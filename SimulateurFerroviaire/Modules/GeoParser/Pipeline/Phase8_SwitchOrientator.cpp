/**
 * @file  Phase8_SwitchOrientator.cpp
 * @brief Implémentation de la phase 7 — orientation des aiguillages.
 *
 * Refactorisation de l'ancien SwitchOrientator.
 * Logique métier conservée — interface adaptée au pipeline v2.
 *
 * @see Phase8_SwitchOrientator
 */
#include "Phase8_SwitchOrientator.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void Phase8_SwitchOrientator::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t swCount = ctx.blocks.switches.size();
    LOG_INFO(logger, "Orientation des switches — "
        + std::to_string(swCount) + " switch(es).");

    // Orientation géométrique root/normal/deviation
    orientGeometric(ctx.blocks, config, logger);

    // Détection et cohérence des crossovers
    const auto crossovers = detectCrossovers(ctx.blocks, logger);
    enforceCrossoverConsistency(ctx.blocks, crossovers, logger);

    size_t oriented = 0;
    for (const auto& sw : ctx.blocks.switches)
        if (sw->isOriented()) ++oriented;

    ctx.endTimer(t0, "Phase8_SwitchOrientator", swCount, oriented);

    LOG_INFO(logger, std::to_string(oriented) + "/" + std::to_string(swCount)
        + " switch(es) orienté(s).");
}


// =============================================================================
// 7a — Orientation géométrique
// =============================================================================

void Phase8_SwitchOrientator::orientGeometric(BlockSet& blocks,
    const ParserConfig& config,
    Logger& logger)
{
    // -----------------------------------------------------------------------
    // Helpers locaux
    // -----------------------------------------------------------------------

    // Vecteur UTM sortant depuis junction vers le premier point interne du
    // bloc adjacent (tangente locale). Supporte StraightBlock et SwitchBlock
    // (cas double-aiguille absorbé en Phase 7).
    auto outgoingVec = [](const CoordinateXY& junction,
        const ShuntingElement* elem) -> CoordinateXY
        {
            if (const auto* st = dynamic_cast<const StraightBlock*>(elem))
            {
                const auto& pts = st->getPointsUTM();
                if (pts.size() < 2) return { 0.0, 0.0 };

                const double dFront = std::hypot(pts.front().x - junction.x,
                    pts.front().y - junction.y);
                const double dBack = std::hypot(pts.back().x - junction.x,
                    pts.back().y - junction.y);
                if (dFront <= dBack)
                    return { pts[1].x - pts[0].x,       pts[1].y - pts[0].y };
                else
                {
                    const size_t n = pts.size();
                    return { pts[n - 2].x - pts[n - 1].x,   pts[n - 2].y - pts[n - 1].y };
                }
            }
            if (const auto* sw2 = dynamic_cast<const SwitchBlock*>(elem))
            {
                const CoordinateXY& j2 = sw2->getJunctionUTM();
                return { j2.x - junction.x,  j2.y - junction.y };
            }
            return { 0.0, 0.0 };
        };

    // Angle en degrés entre deux vecteurs (même logique que Phase5).
    auto angleDeg = [](const CoordinateXY& u,
        const CoordinateXY& v) -> double
        {
            const double lenU = std::hypot(u.x, u.y);
            const double lenV = std::hypot(v.x, v.y);
            if (lenU < 1e-9 || lenV < 1e-9) return 0.0;
            const double dot = u.x * v.x + u.y * v.y;
            return std::acos(std::clamp(dot / (lenU * lenV), -1.0, 1.0))
                * (180.0 / M_PI);
        };

    // Extrémité distale WGS84 du bloc — utilisée pour setTips() et
    // computeTotalLength().
    auto farTip = [](const CoordinateXY& junction,
        const ShuntingElement* elem) -> std::optional<CoordinateLatLon>
        {
            if (const auto* st = dynamic_cast<const StraightBlock*>(elem))
            {
                const auto& ptsUTM = st->getPointsUTM();
                const auto& ptsWGS = st->getPointsWGS84();
                if (ptsUTM.empty() || ptsUTM.size() != ptsWGS.size())
                    return std::nullopt;

                const double dFront = std::hypot(ptsUTM.front().x - junction.x,
                    ptsUTM.front().y - junction.y);
                const double dBack = std::hypot(ptsUTM.back().x - junction.x,
                    ptsUTM.back().y - junction.y);
                return (dFront <= dBack) ? ptsWGS.back() : ptsWGS.front();
            }
            if (const auto* sw2 = dynamic_cast<const SwitchBlock*>(elem))
                return sw2->getJunctionWGS84();   // double-aiguille : jonction partenaire
            return std::nullopt;
        };

    // -----------------------------------------------------------------------
    // Boucle principale
    // -----------------------------------------------------------------------

    int oriented = 0, skipped = 0;

    for (auto& sw : blocks.switches)
    {
        const CoordinateXY junction = sw->getJunctionUTM();

        // Branches dans l'ordre de slot courant (assignées par Phase 9 selon
        // l'ordre d'adjacence topologique — pas encore orientées sémantiquement).
        ShuntingElement* b[3] = {
            sw->getRootBlock(),
            sw->getNormalBlock(),
            sw->getDeviationBlock()
        };

        // --- Préconditions ------------------------------------------------
        if (!b[0] || !b[1] || !b[2])
        {
            LOG_WARNING(logger, "Switch " + sw->getId()
                + " — branche(s) non résolue(s), orientation ignorée.");
            ++skipped;
            continue;
        }

        CoordinateXY v[3];
        for (int k = 0; k < 3; ++k)
            v[k] = outgoingVec(junction, b[k]);

        bool degenerate = false;
        for (int k = 0; k < 3; ++k)
            if (std::hypot(v[k].x, v[k].y) < 1e-6) { degenerate = true; break; }

        if (degenerate)
        {
            LOG_WARNING(logger, "Switch " + sw->getId()
                + " — vecteur(s) dégénéré(s), orientation ignorée.");
            ++skipped;
            continue;
        }

        // --- Identification ROOT ------------------------------------------
        //
        // Root est la branche la plus "opposée" aux deux autres :
        //   pour chaque candidat i, on somme ses angles avec j et k.
        //   En géométrie ferroviaire typique :
        //     angle(root, normal)    ≈ 175°
        //     angle(root, deviation) ≈ 165°
        //     angle(normal, deviation) ≈ 15–30°
        //   → root maximise la somme.
        double sumAngles[3];
        for (int i = 0; i < 3; ++i)
        {
            const int j = (i + 1) % 3;
            const int k = (i + 2) % 3;
            sumAngles[i] = angleDeg(v[i], v[j]) + angleDeg(v[i], v[k]);
        }

        int rootIdx = 0;
        if (sumAngles[1] > sumAngles[0]) rootIdx = 1;
        if (sumAngles[2] > sumAngles[rootIdx]) rootIdx = 2;

        // --- Identification NORMAL / DEVIATION ----------------------------
        //
        // Anti-root = direction prolongeant root en ligne droite.
        // NORMAL    = branche la plus alignée avec anti-root (voie directe).
        // DEVIATION = l'autre branche (bifurcation latérale).
        int ndIdx[2]; int ni = 0;
        for (int k = 0; k < 3; ++k)
            if (k != rootIdx) ndIdx[ni++] = k;

        const CoordinateXY antiRoot = { -v[rootIdx].x, -v[rootIdx].y };
        const double a0 = angleDeg(antiRoot, v[ndIdx[0]]);
        const double a1 = angleDeg(antiRoot, v[ndIdx[1]]);

        const int normalIdx = (a0 <= a1) ? ndIdx[0] : ndIdx[1];
        const int deviationIdx = (a0 <= a1) ? ndIdx[1] : ndIdx[0];

        // Validation CDC : l'angle de déviation doit dépasser minSwitchAngle
        const double deviationAngle = std::max(a0, a1);
        if (deviationAngle < config.minSwitchAngle)
        {
            LOG_WARNING(logger, "Switch " + sw->getId()
                + " — angle déviation (" + std::to_string(static_cast<int>(deviationAngle))
                + "°) < minSwitchAngle ("
                + std::to_string(static_cast<int>(config.minSwitchAngle))
                + "°). Orientation maintenue, vérification terrain recommandée.");
        }

        // --- Application --------------------------------------------------

        // Enregistre les IDs dans m_branchIds (requis par orient())
        sw->addBranchId(b[0]->getId());
        sw->addBranchId(b[1]->getId());
        sw->addBranchId(b[2]->getId());

        // Orientation sémantique — active isOriented()
        sw->orient(b[rootIdx]->getId(),
            b[normalIdx]->getId(),
            b[deviationIdx]->getId());

        // Pointeurs dans le bon ordre sémantique
        sw->setRootPointer(b[rootIdx]);
        sw->setNormalPointer(b[normalIdx]);
        sw->setDeviationPointer(b[deviationIdx]);

        // Extrémités distales pour computeTotalLength()
        sw->setTips(farTip(junction, b[rootIdx]),
            farTip(junction, b[normalIdx]),
            farTip(junction, b[deviationIdx]));

        sw->computeTotalLength();

        LOG_DEBUG(logger, "Switch " + sw->getId()
            + " — root=" + b[rootIdx]->getId()
            + " normal=" + b[normalIdx]->getId()
            + " deviation=" + b[deviationIdx]->getId()
            + " (Δdev=" + std::to_string(static_cast<int>(deviationAngle)) + "°)");

        ++oriented;
    }

    LOG_DEBUG(logger, std::to_string(oriented) + " switch(es) orienté(s) géométriquement, "
        + std::to_string(skipped) + " ignoré(s).");
}


// =============================================================================
// 7b — Détection des crossovers
// =============================================================================

std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
Phase8_SwitchOrientator::detectCrossovers(const BlockSet& blocks,
    Logger& logger)
{
    std::vector<std::pair<SwitchBlock*, SwitchBlock*>> crossovers;

    // Deux switches sont en crossover si leurs branches NORMAL et DEVIATION
    // pointent vers les mêmes deux StraightBlocks (dans n'importe quel ordre).
    //
    // Algorithme :
    //   Pour chaque paire (swA, swB) :
    //     normalA = swA->getNormalBlock()
    //     devA    = swA->getDeviationBlock()
    //     normalB = swB->getNormalBlock()
    //     devB    = swB->getDeviationBlock()
    //     Si {normalA, devA} == {normalB, devB} → crossover

    const size_t n = blocks.switches.size();
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            SwitchBlock* swA = blocks.switches[i].get();
            SwitchBlock* swB = blocks.switches[j].get();

            // Accès via getters de SwitchBlock (inchangés depuis l'ancien)
            ShuntingElement* nA = swA->getNormalBlock();
            ShuntingElement* dA = swA->getDeviationBlock();
            ShuntingElement* nB = swB->getNormalBlock();
            ShuntingElement* dB = swB->getDeviationBlock();

            if (!nA || !dA || !nB || !dB) continue;

            const bool sameSet = (nA == nB && dA == dB)
                || (nA == dB && dA == nB);

            if (!sameSet) continue;

            crossovers.push_back({ swA, swB });
            LOG_DEBUG(logger, "Crossover : " + swA->getId()
                + " ↔ " + swB->getId());
        }
    }

    LOG_DEBUG(logger, std::to_string(crossovers.size()) + " crossover(s) détecté(s).");
    return crossovers;
}


// =============================================================================
// 7c — Cohérence des crossovers
// =============================================================================

void Phase8_SwitchOrientator::enforceCrossoverConsistency(
    BlockSet& blocks,
    const std::vector<std::pair<SwitchBlock*, SwitchBlock*>>& crossovers,
    Logger& logger)
{
    for (const auto& [swA, swB] : crossovers)
    {
        // Pour chaque branche partagée, s'assurer qu'elle est DEVIATION
        // sur les deux switches (convention ferroviaire pour les voies de croisement).
        //
        // Si une branche est NORMAL sur swA mais que swB pointe vers le
        // même straight → la forcer en DEVIATION sur swA.
        //
        // Logique conservée de l'ancien SwitchOrientator::enforceCrossoverConsistency()
        // Adaptée pour accéder via getters SwitchBlock (inchangés).

        ShuntingElement* sharedNormal = nullptr;
        ShuntingElement* sharedDeviation = nullptr;

        ShuntingElement* nA = swA->getNormalBlock();
        ShuntingElement* dA = swA->getDeviationBlock();
        ShuntingElement* nB = swB->getNormalBlock();

        if (nA == nB) { sharedNormal = nA; sharedDeviation = dA; }
        else { sharedNormal = dA; sharedDeviation = nA; }

        if (!sharedNormal) continue;

        // Force les branches partagées en DEVIATION
        if (swA->getNormalBlock() == sharedNormal)
        {
            swA->swapNormalDeviation();
            LOG_DEBUG(logger, swA->getId() + " — branche "
                + sharedNormal->getId() + " forcée en DEVIATION.");
        }
        if (swB->getNormalBlock() == sharedNormal)
        {
            swB->swapNormalDeviation();
            LOG_DEBUG(logger, swB->getId() + " — branche "
                + sharedNormal->getId() + " forcée en DEVIATION.");
        }
    }
}
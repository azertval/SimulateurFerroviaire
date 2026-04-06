/**
 * @file  Phase7_SwitchProcessor.cpp
 * @brief Implémentation du traitement complet des aiguillages.
 *
 * @see Phase7_SwitchProcessor
 */
#include "Phase7_SwitchProcessor.h"

#include <algorithm>
#include <cmath>


namespace
{
    constexpr double EARTH_R = 6371000.0;
    constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;

    double haversine(const CoordinateLatLon& a, const CoordinateLatLon& b)
    {
        const double dLat = (b.latitude - a.latitude) * DEG2RAD;
        const double dLon = (b.longitude - a.longitude) * DEG2RAD;
        const double latA = a.latitude * DEG2RAD;
        const double latB = b.latitude * DEG2RAD;
        const double sinDLat = std::sin(dLat / 2.0);
        const double sinDLon = std::sin(dLon / 2.0);
        const double hav = sinDLat * sinDLat
            + std::cos(latA) * std::cos(latB) * sinDLon * sinDLon;
        return EARTH_R * 2.0 * std::atan2(std::sqrt(hav), std::sqrt(1.0 - hav));
    }
}


// =============================================================================
// Point d'entrée
// =============================================================================

void Phase7_SwitchProcessor::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t swCount = ctx.blocks.switches.size();
    LOG_INFO(logger, "Traitement des switches — "
        + std::to_string(swCount) + " switch(es).");

    // G — Orientation géométrique (root/normal/deviation) — en premier
    // pour que les pointeurs soient corrects lors des étapes suivantes
    orientBranches(ctx.blocks, logger);

    // A/B — Doubles aiguilles
    const auto clusters = detectClusters(ctx.blocks,
        config.doubleSwitchRadius,
        logger);
    LOG_INFO(logger, std::to_string(clusters.size()) + " cluster(s) détecté(s).");

    size_t absorbed = 0;
    for (const auto& [swA, swB] : clusters)
    {
        absorbLinkSegment(ctx.blocks, swA, swB, logger);
        ++absorbed;
    }
    if (absorbed > 0)
        LOG_INFO(logger, std::to_string(absorbed) + " segment(s) absorbé(s).");

    // C — Validation CDC
    validateCDC(ctx.blocks, config.minBranchLength, logger);

    // D/E — Crossovers
    const auto crossovers = detectCrossovers(ctx.blocks, logger);
    enforceCrossoverConsistency(ctx.blocks, crossovers, logger);

    // F — Tips CDC
    computeTips(ctx.blocks, config.switchSideSize, logger);

    size_t oriented = 0;
    for (const auto& sw : ctx.blocks.switches)
        if (sw->isOriented()) ++oriented;

    ctx.endTimer(t0, "Phase7_SwitchProcessor", swCount, oriented);

    LOG_INFO(logger, std::to_string(oriented) + "/" + std::to_string(swCount)
        + " switch(es) orienté(s).");
}


// =============================================================================
// A — Détection des clusters double switch
// =============================================================================

std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
Phase7_SwitchProcessor::detectClusters(const BlockSet& blocks,
    double radius,
    Logger& logger)
{
    std::vector<std::pair<SwitchBlock*, SwitchBlock*>> clusters;
    const size_t n = blocks.switches.size();

    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            SwitchBlock* swA = blocks.switches[i].get();
            SwitchBlock* swB = blocks.switches[j].get();

            const CoordinateXY& jA = swA->getJunctionUTM();
            const CoordinateXY& jB = swB->getJunctionUTM();

            const double dist = std::hypot(jB.x - jA.x, jB.y - jA.y);

            if (dist < radius)
            {
                clusters.push_back({ swA, swB });
                LOG_DEBUG(logger, "Cluster potentiel : "
                    + swA->getId() + " ↔ " + swB->getId()
                    + " (" + std::to_string(static_cast<int>(dist)) + " m)");
            }
        }
    }

    return clusters;
}


// =============================================================================
// B — Recherche du segment de liaison
// =============================================================================

std::vector<StraightBlock*> Phase7_SwitchProcessor::findLinkSegments(
    const BlockSet& blocks,
    const SwitchBlock* swA,
    const SwitchBlock* swB)
{
    auto branchesOf = [](const SwitchBlock* sw)
        -> std::array<ShuntingElement*, 3>
        {
            return { sw->getRootBlock(),
                     sw->getNormalBlock(),
                     sw->getDeviationBlock() };
        };

    const auto brA = branchesOf(swA);
    const auto brB = branchesOf(swB);

    std::vector<StraightBlock*> links;

    for (ShuntingElement* elemA : brA)
    {
        if (!elemA) continue;
        for (ShuntingElement* elemB : brB)
        {
            if (elemA != elemB) continue;
            auto* st = dynamic_cast<StraightBlock*>(elemA);
            // Évite les doublons si une branche root/normal/deviation
            // pointe accidentellement deux fois vers le même bloc
            if (st && std::find(links.begin(), links.end(), st) == links.end())
                links.push_back(st);
        }
    }
    return links;
}


// =============================================================================
// B — Absorption
// =============================================================================

void Phase7_SwitchProcessor::absorbLinkSegment(BlockSet& blocks,
    SwitchBlock* swA,
    SwitchBlock* swB,
    Logger& logger)
{
    // Collecte TOUS les liens avant toute modification :
    // replaceBranchPointer() décale les pointeurs et rendrait une
    // recherche incrémentale aveugle au second lien.
    const auto links = findLinkSegments(blocks, swA, swB);

    if (links.empty())
    {
        LOG_WARNING(logger, "Segment de liaison introuvable entre "
            + swA->getId() + " et " + swB->getId() + " — absorption ignorée.");
        return;
    }

    for (StraightBlock* link : links)
    {
        LOG_DEBUG(logger, "Absorption " + link->getId() + " : "
            + std::to_string(link->getPointsUTM().size()) + " point(s), "
            + swA->getId() + " ↔ " + swB->getId());

        const auto wgs84 = link->getPointsWGS84();
        const auto utm = link->getPointsUTM();

        swA->absorbLink(link->getId(), swB->getId(), wgs84, utm);
        swB->absorbLink(link->getId(), swA->getId(), wgs84, utm);

        swA->replaceBranchPointer(link, swB);
        swB->replaceBranchPointer(link, swA);

        // Purge straightEndpoints (index parallèle à straights)
        const auto idxIt = std::find_if(
            blocks.straights.begin(), blocks.straights.end(),
            [link](const std::unique_ptr<StraightBlock>& s)
            { return s.get() == link; });

        if (idxIt != blocks.straights.end())
        {
            const size_t idx = static_cast<size_t>(
                idxIt - blocks.straights.begin());
            blocks.straightEndpoints.erase(
                blocks.straightEndpoints.begin() + idx);
        }

        LOG_DEBUG(logger, "Absorbé : " + link->getId()
            + "  " + swA->getId() + " ↔ " + swB->getId());
        // Suppression — unique_ptr détruit link ici
        blocks.straights.erase(
            std::remove_if(blocks.straights.begin(), blocks.straights.end(),
                [link](const std::unique_ptr<StraightBlock>& s)
                { return s.get() == link; }),
            blocks.straights.end());

        // Reconstruction après chaque suppression pour garder
        // straightEndpoints cohérent avec straights
        blocks.rebuildStraightIndex();

    }
}



// =============================================================================
// C — Validation CDC
// =============================================================================

void Phase7_SwitchProcessor::validateCDC(const BlockSet& blocks,
    double minLength,
    Logger& logger)
{
    LOG_INFO(logger, "Validation CDC — longueur min = "
        + std::to_string(static_cast<int>(minLength)) + " m.");

    int violations = 0;

    for (const auto& sw : blocks.switches)
    {
        auto checkBranch = [&](const ShuntingElement* elem,
            const std::string& branchName)
            {
                if (!elem) return;
                const auto* st = dynamic_cast<const StraightBlock*>(elem);
                if (!st) return;

                const double len = st->getLengthUTM();
                if (len < minLength)
                {
                    LOG_WARNING(logger, sw->getId()
                        + " : branche " + branchName
                        + " trop courte ("
                        + std::to_string(static_cast<int>(len))
                        + " m < "
                        + std::to_string(static_cast<int>(minLength))
                        + " m min)");
                    ++violations;
                }
            };

        checkBranch(sw->getRootBlock(), "root");
        checkBranch(sw->getNormalBlock(), "normal");
        checkBranch(sw->getDeviationBlock(), "deviation");
    }

    if (violations > 0)
        LOG_WARNING(logger, "Validation CDC : "
            + std::to_string(violations) + " violation(s).");
    else
        LOG_INFO(logger, "Validation CDC : OK.");
}


// =============================================================================
// D — Détection des crossovers
// =============================================================================

std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
Phase7_SwitchProcessor::detectCrossovers(const BlockSet& blocks,
    Logger& logger)
{
    std::vector<std::pair<SwitchBlock*, SwitchBlock*>> crossovers;
    const size_t n = blocks.switches.size();

    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            SwitchBlock* swA = blocks.switches[i].get();
            SwitchBlock* swB = blocks.switches[j].get();

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

    LOG_DEBUG(logger, std::to_string(crossovers.size())
        + " crossover(s) détecté(s).");
    return crossovers;
}


// =============================================================================
// E — Cohérence des crossovers
// =============================================================================

void Phase7_SwitchProcessor::enforceCrossoverConsistency(
    BlockSet& blocks,
    const std::vector<std::pair<SwitchBlock*, SwitchBlock*>>& crossovers,
    Logger& logger)
{
    for (const auto& [swA, swB] : crossovers)
    {
        ShuntingElement* nA = swA->getNormalBlock();
        ShuntingElement* dA = swA->getDeviationBlock();
        ShuntingElement* nB = swB->getNormalBlock();

        ShuntingElement* sharedNormal = (nA == nB) ? nA : dA;
        if (!sharedNormal) continue;

        if (nA == sharedNormal)
        {
            swA->swapNormalDeviation();
            LOG_DEBUG(logger, swA->getId() + " — branche "
                + sharedNormal->getId() + " forcée en DEVIATION.");
        }
        if (nB == sharedNormal)
        {
            swB->swapNormalDeviation();
            LOG_DEBUG(logger, swB->getId() + " — branche "
                + sharedNormal->getId() + " forcée en DEVIATION.");
        }
    }
}


// =============================================================================
// G — Orientation géométrique root/normal/deviation
// =============================================================================

void Phase7_SwitchProcessor::orientBranches(BlockSet& blocks, Logger& logger)
{
    for (const auto& sw : blocks.switches)
    {
        // Récupère les 3 blocs de branches dans l'ordre actuel (arbitraire)
        std::array<ShuntingElement*, 3> elems = {
            sw->getRootBlock(),
            sw->getNormalBlock(),
            sw->getDeviationBlock()
        };

        // Vérifie qu'on a bien 3 branches non-nulles (hors double switch)
        // Pour un double switch, une branche peut pointer vers un SwitchBlock
        int validCount = 0;
        for (auto* e : elems) if (e) ++validCount;
        if (validCount < 3)
        {
            LOG_DEBUG(logger, sw->getId()
                + " — orientation ignorée (branches incomplètes).");
            continue;
        }

        // Calcule les 3 vecteurs UTM unitaires depuis la jonction
        std::array<CoordinateXY, 3> vecs;
        for (size_t i = 0; i < 3; ++i)
            vecs[i] = branchVector(*sw, elems[i]);

        // Identifie le root : branche la plus opposée à la résultante des deux autres
        // Score = dot(v[i], normalize(v[j] + v[k])) — le minimum est le root
        int rootIdx = 0;
        double minScore = std::numeric_limits<double>::max();

        for (int i = 0; i < 3; ++i)
        {
            const int j = (i + 1) % 3;
            const int k = (i + 2) % 3;

            // Résultante des deux autres branches
            CoordinateXY resultant{ vecs[j].x + vecs[k].x,
                                     vecs[j].y + vecs[k].y };
            const double rLen = std::hypot(resultant.x, resultant.y);
            if (rLen < 1e-9) continue;
            resultant.x /= rLen;
            resultant.y /= rLen;

            // Produit scalaire du vecteur i avec la résultante
            const double score = vecs[i].x * resultant.x
                + vecs[i].y * resultant.y;

            if (score < minScore)
            {
                minScore = score;
                rootIdx = i;
            }
        }

        // Les deux branches restantes
        const int idxA = (rootIdx + 1) % 3;
        const int idxB = (rootIdx + 2) % 3;

        // Normal = branche dont l'angle avec root est le plus proche de 180°
        // (continuation directe) → dot product le plus négatif avec root
        const double dotA = vecs[rootIdx].x * vecs[idxA].x
            + vecs[rootIdx].y * vecs[idxA].y;
        const double dotB = vecs[rootIdx].x * vecs[idxB].x
            + vecs[rootIdx].y * vecs[idxB].y;

        // dotA < dotB → A est plus opposé à root → A est le normal
        const int normalIdx = (dotA < dotB) ? idxA : idxB;
        const int deviationIdx = (dotA < dotB) ? idxB : idxA;

        ShuntingElement* newRoot = elems[rootIdx];
        ShuntingElement* newNormal = elems[normalIdx];
        ShuntingElement* newDeviation = elems[deviationIdx];

        // Applique l'orientation
        sw->setRootPointer(newRoot);
        sw->setNormalPointer(newNormal);
        sw->setDeviationPointer(newDeviation);

        if (newRoot)      sw->setRootBranchId(newRoot->getId());
        if (newNormal)    sw->setNormalBranchId(newNormal->getId());
        if (newDeviation) sw->setDeviationBranchId(newDeviation->getId());

        LOG_DEBUG(logger, sw->getId()
            + " — root=" + (newRoot ? newRoot->getId() : "null")
            + " normal=" + (newNormal ? newNormal->getId() : "null")
            + " deviation=" + (newDeviation ? newDeviation->getId() : "null"));
    }
}

CoordinateXY Phase7_SwitchProcessor::branchVector(const SwitchBlock& sw,
    const ShuntingElement* elem)
{
    if (!elem) return { 0.0, 0.0 };

    const CoordinateXY& junc = sw.getJunctionUTM();

    // StraightBlock — direction vers le premier point interne
    const auto* st = dynamic_cast<const StraightBlock*>(elem);
    if (st && st->getPointsUTM().size() >= 2)
    {
        // Détermine le sens : extrémité la plus proche de la jonction
        const CoordinateXY& front = st->getPointsUTM().front();
        const CoordinateXY& back = st->getPointsUTM().back();

        const double dFront = std::hypot(front.x - junc.x, front.y - junc.y);
        const double dBack = std::hypot(back.x - junc.x, back.y - junc.y);

        const CoordinateXY& origin = (dFront <= dBack) ? front : back;
        const CoordinateXY& next = (dFront <= dBack)
            ? st->getPointsUTM()[1]
            : st->getPointsUTM()[st->getPointsUTM().size() - 2];

        const double dx = next.x - origin.x;
        const double dy = next.y - origin.y;
        const double len = std::hypot(dx, dy);
        if (len < 1e-9) return { 0.0, 0.0 };
        return { dx / len, dy / len };
    }

    // SwitchBlock partenaire (double switch) — direction jonction → jonction partenaire
    const auto* swPartner = dynamic_cast<const SwitchBlock*>(elem);
    if (swPartner)
    {
        const CoordinateXY& pJunc = swPartner->getJunctionUTM();
        const double dx = pJunc.x - junc.x;
        const double dy = pJunc.y - junc.y;
        const double len = std::hypot(dx, dy);
        if (len < 1e-9) return { 0.0, 0.0 };
        return { dx / len, dy / len };
    }

    return { 0.0, 0.0 };
}


// =============================================================================
// F — Calcul des tips CDC
// =============================================================================

void Phase7_SwitchProcessor::computeTips(BlockSet& blocks,
    double sideSize,
    Logger& logger)
{
    // Règle universelle : un tip CDC n'est interpolable que sur un StraightBlock.
    // Branche → SwitchBlock  (double switch)  : pas de tip
    // Branche → CrossBlock   (TJD / crossing) : pas de tip
    // Branche → nullptr                        : pas de tip
    auto hasTip = [](const ShuntingElement* branch) -> bool
        {
            return branch && branch->getType() == ElementType::STRAIGHT;
        };

    for (const auto& sw : blocks.switches)
    {
        if (!sw->isOriented()) continue;

        const CoordinateLatLon& junctionWGS84 = sw->getJunctionWGS84();
        const CoordinateXY& junctionUTM = sw->getJunctionUTM();

        const ShuntingElement* root = sw->getRootBlock();
        const ShuntingElement* normal = sw->getNormalBlock();
        const ShuntingElement* deviation = sw->getDeviationBlock();

        // --- Tips WGS84 ---
        auto makeTipWGS84 = [&](const ShuntingElement* elem)
            -> std::optional<CoordinateLatLon>
            {
                if (!hasTip(elem)) return std::nullopt;
                const auto* st = static_cast<const StraightBlock*>(elem);
                if (st->getPointsWGS84().size() < 2) return std::nullopt;
                return interpolateTip(st->getPointsWGS84(), junctionWGS84, sideSize);
            };

        // --- Tips UTM ---
        auto makeTipUTM = [&](const ShuntingElement* elem)
            -> std::optional<CoordinateXY>
            {
                if (!hasTip(elem)) return std::nullopt;
                const auto* st = static_cast<const StraightBlock*>(elem);
                if (st->getPointsUTM().size() < 2) return std::nullopt;
                return interpolateTipUTM(st->getPointsUTM(), junctionUTM, sideSize);
            };

        sw->setTips(
            makeTipWGS84(root),
            makeTipWGS84(normal),
            makeTipWGS84(deviation));

        sw->setTipsUTM(
            makeTipUTM(root),
            makeTipUTM(normal),
            makeTipUTM(deviation));

        sw->computeTotalLength();

        LOG_DEBUG(logger, sw->getId() + " — tips calculés"
            + " root=" + (hasTip(root) ? "✓" : "—")
            + " normal=" + (hasTip(normal) ? "✓" : "—")
            + " deviation=" + (hasTip(deviation) ? "✓" : "—")
            + " (" + std::to_string(static_cast<int>(sideSize)) + " m).");
    }
}

CoordinateLatLon Phase7_SwitchProcessor::interpolateTip(
    const std::vector<CoordinateLatLon>& pts,
    const CoordinateLatLon& junction,
    double targetDist)
{
    if (pts.size() < 2) return junction;

    // Détermine l'extrémité la plus proche de la jonction — c'est le départ
    const double distFront = haversine(pts.front(), junction);
    const double distBack = haversine(pts.back(), junction);
    const bool   fromFront = (distFront <= distBack);

    const int start = fromFront ? 0 : static_cast<int>(pts.size()) - 1;
    const int step = fromFront ? 1 : -1;
    const int end = fromFront ? static_cast<int>(pts.size()) : -1;

    double           accumulated = 0.0;
    CoordinateLatLon prev = pts[static_cast<size_t>(start)];

    for (int i = start + step; i != end; i += step)
    {
        const CoordinateLatLon& curr = pts[static_cast<size_t>(i)];
        const double            segLen = haversine(prev, curr);

        if (accumulated + segLen >= targetDist)
        {
            // Interpolation linéaire entre prev et curr
            const double t = (targetDist - accumulated) / segLen;
            return {
                prev.latitude + t * (curr.latitude - prev.latitude),
                prev.longitude + t * (curr.longitude - prev.longitude)
            };
        }

        accumulated += segLen;
        prev = curr;
    }

    // Branche plus courte que targetDist → extrémité distale
    return fromFront ? pts.back() : pts.front();
}

CoordinateXY Phase7_SwitchProcessor::interpolateTipUTM(
    const std::vector<CoordinateXY>& pts,
    const CoordinateXY& junctionUTM,
    double targetDist)
{
    if (pts.size() < 2) return junctionUTM;

    // Détermine l'extrémité la plus proche de la jonction
    const double dFront = std::hypot(
        pts.front().x - junctionUTM.x,
        pts.front().y - junctionUTM.y);
    const double dBack = std::hypot(
        pts.back().x - junctionUTM.x,
        pts.back().y - junctionUTM.y);
    const bool fromFront = (dFront <= dBack);

    const int start = fromFront ? 0 : static_cast<int>(pts.size()) - 1;
    const int step = fromFront ? 1 : -1;

    double accumulated = 0.0;
    int i = start;

    while (true)
    {
        const int next = i + step;
        if (next < 0 || next >= static_cast<int>(pts.size()))
            // Branche trop courte : retourne l'extrémité distale
            return pts[static_cast<std::size_t>(i)];

        const double segLen = std::hypot(
            pts[static_cast<std::size_t>(next)].x - pts[static_cast<std::size_t>(i)].x,
            pts[static_cast<std::size_t>(next)].y - pts[static_cast<std::size_t>(i)].y);

        if (accumulated + segLen >= targetDist)
        {
            // Interpolation linéaire dans le segment courant
            const double t = (targetDist - accumulated) / segLen;
            return CoordinateXY{
                pts[static_cast<std::size_t>(i)].x
                    + t * (pts[static_cast<std::size_t>(next)].x
                         - pts[static_cast<std::size_t>(i)].x),
                pts[static_cast<std::size_t>(i)].y
                    + t * (pts[static_cast<std::size_t>(next)].y
                         - pts[static_cast<std::size_t>(i)].y)
            };
        }
        accumulated += segLen;
        i = next;
    }
}
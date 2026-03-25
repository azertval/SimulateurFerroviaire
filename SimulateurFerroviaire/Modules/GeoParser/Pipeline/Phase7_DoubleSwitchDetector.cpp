/**
 * @file  Phase7_DoubleSwitchDetector.cpp
 * @brief Implémentation de la phase 8 — doubles aiguilles.
 *
 * @see Phase7_DoubleSwitchDetector
 */
#include "Phase7_DoubleSwitchDetector.h"

#include <algorithm>
#include <cmath>


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void Phase7_DoubleSwitchDetector::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    LOG_INFO(logger, "Détection des doubles aiguilles.");

    // 8a — Détection des clusters
    const auto clusters = detectClusters(ctx.blocks,
        config.doubleSwitchRadius,
        logger);

    LOG_INFO(logger, std::to_string(clusters.size()) + " cluster(s) détecté(s).");

    // 8b — Absorption des segments de liaison
    size_t absorbed = 0;
    for (const auto& [swA, swB] : clusters)
    {
        absorbLinkSegment(ctx.blocks, swA, swB, logger);
        ++absorbed;
        LOG_DEBUG(logger, "Double : " + swA->getId() + " ↔ " + swB->getId());
    }

    if (absorbed > 0)
        LOG_INFO(logger, std::to_string(absorbed) + " segment(s) absorbé(s).");

    // 8c — Validation CDC
    validateCDC(ctx.blocks, config.minBranchLength, logger);

    ctx.endTimer(t0, "Phase7_DoubleSwitchDetector",
        ctx.blocks.switches.size(),
        absorbed);
}


// =============================================================================
// 8a — Détection des clusters
// =============================================================================

std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
Phase7_DoubleSwitchDetector::detectClusters(const BlockSet& blocks,
    double radius,
    Logger& logger)
{
    std::vector<std::pair<SwitchBlock*, SwitchBlock*>> clusters;
    const size_t n = blocks.switches.size();

    // O(n²) — acceptable, peu de switches
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            SwitchBlock* swA = blocks.switches[i].get();
            SwitchBlock* swB = blocks.switches[j].get();

            const CoordinateXY& jA = swA->getJunctionUTM();
            const CoordinateXY& jB = swB->getJunctionUTM();

            const double dx = jB.x - jA.x;
            const double dy = jB.y - jA.y;
            const double dist = std::hypot(dx, dy);

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
// 8b — Recherche du segment de liaison
// =============================================================================

StraightBlock* Phase7_DoubleSwitchDetector::findLinkSegment(
    const BlockSet& blocks,
    const SwitchBlock* swA,
    const SwitchBlock* swB)
{
    // Le segment de liaison est adjacent aux deux switches
    // Il apparaît dans les branches de swA ET dans les branches de swB

    auto branchesOf = [](const SwitchBlock* sw)
        -> std::array<ShuntingElement*, 3>
        {
            return { sw->getRootBlock(),
                     sw->getNormalBlock(),
                     sw->getDeviationBlock() };
        };

    const auto brA = branchesOf(swA);
    const auto brB = branchesOf(swB);

    for (ShuntingElement* elemA : brA)
    {
        if (!elemA) continue;
        for (ShuntingElement* elemB : brB)
        {
            if (elemA == elemB)
            {
                // Même élément dans les branches des deux switches → liaison
                return dynamic_cast<StraightBlock*>(elemA);
            }
        }
    }
    return nullptr;
}


// =============================================================================
// 8b — Absorption
// =============================================================================

void Phase7_DoubleSwitchDetector::absorbLinkSegment(BlockSet& blocks,
    SwitchBlock* swA,
    SwitchBlock* swB,
    Logger& logger)
{
    StraightBlock* link = findLinkSegment(blocks, swA, swB);

    if (!link)
    {
        LOG_WARNING(logger, "Segment de liaison introuvable entre "
            + swA->getId() + " et " + swB->getId() + " — absorption ignorée.");
        return;
    }

    LOG_DEBUG(logger, "Absorption " + link->getId() + " : "
        + std::to_string(link->getPointsUTM().size()) + " point(s), "
        + swA->getId() + " ↔ " + swB->getId());

    // Copie des coordonnées AVANT suppression du segment
    const auto wgs84 = link->getPointsWGS84();
    const auto utm = link->getPointsUTM();

    // Met à jour IDs, tips, doubleOn*, WGS84 et UTM absorbés
    swA->absorbLink(link->getId(), swB->getId(), wgs84, utm);
    swB->absorbLink(link->getId(), swA->getId(), wgs84, utm);

    // Met à jour les raw pointers m_branches
    swA->replaceBranchPointer(link, swB);
    swB->replaceBranchPointer(link, swA);

    // Purge straightByNode — supprime les entrées pointant vers link
    for (auto it = blocks.straightByNode.begin();
        it != blocks.straightByNode.end(); )
    {
        it = (it->second == link)
            ? blocks.straightByNode.erase(it)
            : std::next(it);
    }

    // Purge straightEndpoints — index parallèle à straights
    const auto& straights = blocks.straights;
    const auto idxIt = std::find_if(straights.begin(), straights.end(),
        [link](const std::unique_ptr<StraightBlock>& s) { return s.get() == link; });
    if (idxIt != straights.end())
    {
        const size_t idx = static_cast<size_t>(idxIt - straights.begin());
        blocks.straightEndpoints.erase(blocks.straightEndpoints.begin() + idx);
    }

    // Supprime le segment — unique_ptr détruit link ici
    blocks.straights.erase(
        std::remove_if(blocks.straights.begin(), blocks.straights.end(),
            [link](const std::unique_ptr<StraightBlock>& s)
            { return s.get() == link; }),
        blocks.straights.end());

    LOG_DEBUG(logger, "Absorbé : [lien] (" + swA->getId() + " ↔ " + swB->getId() + ")");
}


// =============================================================================
// 8c — Validation CDC
// =============================================================================

void Phase7_DoubleSwitchDetector::validateCDC(const BlockSet& blocks,
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

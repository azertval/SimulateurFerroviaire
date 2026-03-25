/**
 * @file  Phase8_SwitchOrientator.cpp
 * @brief Implémentation de la phase 7 — orientation des aiguillages.
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
// Détection des crossovers
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
//  Cohérence des crossovers
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
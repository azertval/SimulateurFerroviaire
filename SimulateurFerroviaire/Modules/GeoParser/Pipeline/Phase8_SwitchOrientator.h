/**
 * @file  Phase8_SwitchOrientator.h
 * @brief Phase 7 du pipeline — orientation des aiguillages
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase8_SwitchOrientator
{
public:

    /**
     * @brief Exécute la phase 7 — orientation complète des switches.
     *
     * Enchaîne les sous-phases 7a → 7b → 7c → 7d sur @c ctx.blocks.switches.
     *
     * @param ctx     Contexte pipeline. Lit et modifie blocks.switches + blocks.straights.
     * @param config  Configuration — utilise junctionTrimMargin et minSwitchAngle.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase8_SwitchOrientator() = delete;

private:
    /**
     * @brief Détecte les paires de switches en crossover.
     *
     * Deux switches en crossover partagent exactement les mêmes deux
     * StraightBlocks sur leurs branches NORMAL et DEVIATION.
     *
     * @param blocks  Ensemble des blocs.
     * @param logger  Référence au logger.
     *
     * @return Liste des paires (swA*, swB*) en crossover.
     */
    static std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
        detectCrossovers(const BlockSet& blocks, Logger& logger);

    /**
     * @brief Force la cohérence des crossovers.
     *
     * Pour chaque paire de crossover, s'assure que les deux branches
     * partagées sont toutes deux DEVIATION (convention ferroviaire).
     *
     * @param blocks     Ensemble des blocs.
     * @param crossovers Paires de crossover détectées en 7b.
     * @param logger     Référence au logger.
     */
    static void enforceCrossoverConsistency(
        BlockSet& blocks,
        const std::vector<std::pair<SwitchBlock*, SwitchBlock*>>& crossovers,
        Logger& logger);

    /**
     * @brief Oriente chaque switch géométriquement.
     *
     * Calcule les angles entre les branches et attribue root / normal / deviation
     * selon la géométrie UTM. Utilise @c config.minSwitchAngle comme seuil
     * de discrimination.
     *
     * @param blocks  Ensemble des blocs.
     * @param config  Configuration — utilise minSwitchAngle.
     * @param logger  Référence au logger.
     */
    static void orientGeometric(BlockSet& blocks,
        const ParserConfig& config,
        Logger& logger);
};
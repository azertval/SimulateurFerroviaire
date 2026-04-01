/**
 * @file  Phase6_BlockExtractor.h
 * @brief Phase 6 du pipeline — extraction des blocs ferroviaires.
 *
 * Responsabilité unique : produire les @ref StraightBlock et @ref SwitchBlock
 * non-orientés depuis le graphe classifié.
 *
 * @par Algorithme
 *  -# Les @ref StraightBlock sont extraits en premier par DFS entre
 *     nœuds frontières — les index @c straightsByNode et
 *     @c straightByEndpointPair sont construits au passage.
 *  -# Un @ref SwitchBlock par nœud @c NodeClass::SWITCH.
 *     Les endpoints de chaque branche sont résolus immédiatement via
 *     @c straightByEndpointPair — sans ambiguïté, O(1).
 *
 * @par Libération mémoire
 * @c ctx.topoGraph, @c ctx.classifiedNodes et @c ctx.splitNetwork
 * sont libérés en fin d'exécution.
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase6_BlockExtractor
{
public:

    /**
     * @brief Exécute la phase 6.
     *
     * Ordre interne : extractStraights → extractSwitches.
     * Les straights doivent exister avant les switches pour que
     * straightByEndpointPair soit disponible lors de la résolution
     * des endpoints de branches.
     *
     * @param ctx     Contexte pipeline. Lit topoGraph + classifiedNodes +
     *                splitNetwork, écrit blocks. Libère les trois sources.
     * @param config  Configuration — transmise pour cohérence de signature.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase6_BlockExtractor() = delete;

private:

    /**
     * @brief Extrait tous les StraightBlocks par DFS entre nœuds frontières.
     *
     * Peuple @c ctx.blocks.straightsByNode et
     * @c ctx.blocks.straightByEndpointPair au passage.
     *
     * @param ctx     Contexte pipeline.
     * @param logger  Référence au logger.
     */
    static void extractStraights(PipelineContext& ctx, Logger& logger);

    /**
     * @brief Extrait tous les SwitchBlocks depuis les nœuds SWITCH.
     *
     * Résout les endpoints de branches via @c straightByEndpointPair.
     * Doit être appelé APRÈS @c extractStraights.
     *
     * @param ctx     Contexte pipeline.
     * @param logger  Référence au logger.
     */
    static void extractSwitches(PipelineContext& ctx, Logger& logger);

    /**
     * @brief Vérifie si un nœud est un nœud frontière (non-STRAIGHT).
     *
     * @param ctx     Contexte pipeline.
     * @param nodeId  ID du nœud à tester.
     *
     * @return @c true si TERMINUS, SWITCH, CROSSING, ISOLATED ou AMBIGUOUS.
     */
    static bool isFrontier(const PipelineContext& ctx, size_t nodeId);

    /**
     * @brief Ajoute les points d'un segment atomique à une géométrie accumulée.
     *
     * @param ptsUTM    Vecteur UTM à compléter.
     * @param ptsWGS84  Vecteur WGS84 à compléter.
     * @param seg       Segment à ajouter.
     * @param reversed  @c true si le segment doit être ajouté à l'envers.
     */
    static void appendSegment(std::vector<CoordinateXY>& ptsUTM,
        std::vector<CoordinateLatLon>& ptsWGS84,
        const AtomicSegment& seg,
        bool                           reversed);

    /**
     * @brief Calcule une clé canonique pour une paire de nœuds frontières.
     *
     * @param idA  Premier nœud.
     * @param idB  Second nœud.
     *
     * @return Clé Cantor pairing sur min/max — indépendante de l'ordre.
     */
    static size_t pairKey(size_t idA, size_t idB);
};
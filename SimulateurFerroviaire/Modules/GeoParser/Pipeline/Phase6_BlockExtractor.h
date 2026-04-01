/**
 * @file  Phase6_BlockExtractor.h
 * @brief Phase 6 du pipeline — extraction des blocs ferroviaires.
 *
 * @par Algorithme
 *  -# Les @ref StraightBlock sont extraits en premier par DFS entre nœuds
 *     frontières. La déduplication repose sur les indices d'arêtes (@c usedEdges)
 *     et non plus sur la clé de paire de nœuds, ce qui permet de créer deux
 *     straights distincts entre les mêmes deux switches dans les configurations
 *     crossover (voie double).
 *  -# Si un straight assemblé dépasse @c config.maxSegmentLength, il est
 *     subdivisé en N sous-blocs chaînés via prev/next.
 *  -# Les index directionnels (@c straightByDirectedPair, multi-valués) permettent
 *     à @c extractSwitches de résoudre le sous-bloc adjacent à chaque switch, en
 *     sélectionnant à chaque branche la première entrée non encore utilisée.
 *  -# Un @ref SwitchBlock par nœud @c NodeClass::SWITCH.
 *
 * @par Crossover — correction v2
 * L'ancienne déduplication par @c pairKey(startNode, endNode) empêchait la
 * création d'un second straight entre les mêmes deux nœuds frontières.  La
 * nouvelle déduplication marque les arêtes (@c startEdge et @c lastEdge) comme
 * « utilisées », ce qui permet plusieurs straights entre la même paire de
 * switches tout en évitant les doublons directionnel A→B / B→A.
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
     * @c straightByDirectedPair soit disponible lors de la résolution
     * des endpoints de branches.
     * Libère @c ctx.topoGraph, @c ctx.classifiedNodes et @c ctx.splitNetwork
     * en fin d'exécution.
     *
     * @param ctx     Contexte pipeline. Lit topoGraph + classifiedNodes +
     *                splitNetwork, écrit blocks. Libère les trois sources.
     * @param config  Configuration — utilise @c maxSegmentLength.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase6_BlockExtractor() = delete;

private:

    /**
     * @brief Extrait les StraightBlocks par DFS entre nœuds frontières.
     *
     * @par Déduplication par arêtes (v2)
     * Un ensemble global @c usedEdges mémorise l'indice de l'arête de départ
     * et l'arête d'arrivée de chaque straight créé.  Cela empêche la traversal
     * inverse (B→A) mais autorise deux straights entre les mêmes frontières
     * via des arêtes distinctes (cas crossover).
     *
     * Peuple @c ctx.blocks.straightsByNode, @c straightByEndpointPair et
     * @c straightByDirectedPair au passage.
     *
     * @param ctx     Contexte pipeline.
     * @param config  Configuration — utilise @c maxSegmentLength.
     * @param logger  Référence au logger.
     */
    static void extractStraights(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    /**
     * @brief Extrait les SwitchBlocks et résout leurs endpoints.
     *
     * Pour chaque branche d'un switch, effectue une traversal des nœuds
     * STRAIGHT jusqu'au prochain nœud frontière, puis résout le StraightBlock
     * adjacent via @c straightByDirectedPair (multi-valué).
     *
     * @par Sélection crossover
     * Un ensemble @c usedStraights par switch garantit que deux branches
     * pointant vers le même nœud frontière (crossover) reçoivent des straights
     * distincts — la première branche prend le premier élément du vecteur,
     * la seconde prend le suivant non utilisé.
     *
     * Doit être appelé APRÈS @c extractStraights.
     *
     * @param ctx     Contexte pipeline.
     * @param logger  Référence au logger.
     */
    static void extractSwitches(PipelineContext& ctx, Logger& logger);

    /**
     * @brief Crée un ou plusieurs StraightBlock depuis la géométrie assemblée.
     *
     * Si @c totalLength > @p maxLen, subdivise en @c N sous-blocs chaînés
     * via prev/next. Seuls les premier et dernier sous-blocs sont enregistrés
     * dans @c straightByDirectedPair.
     *
     * @par Endpoints internes
     * Les sous-blocs intermédiaires reçoivent des @ref BlockEndpoint avec
     * @c frontierNodeId = SIZE_MAX et @c neighbourId vide.
     * @ref Phase8_RepositoryTransfer::resolveStraight ne doit PAS appeler
     * @c setNeighbourPrev/Next pour ces entrées, afin de préserver la chaîne.
     *
     * @param ctx       Contexte pipeline.
     * @param ptsUTM    Points UTM du straight assemblé.
     * @param ptsWGS84  Points WGS84 du straight assemblé.
     * @param nodeA     Nœud frontière de départ.
     * @param nodeB     Nœud frontière d'arrivée.
     * @param baseId    Identifiant de base (ex. "s/0").
     * @param maxLen    Longueur maximale par sous-bloc (mètres UTM).
     * @param epA       Endpoint côté A (frontierNodeId = nodeA).
     * @param epB       Endpoint côté B (frontierNodeId = nodeB).
     */
    static void registerStraight(PipelineContext& ctx,
        const std::vector<CoordinateXY>& ptsUTM,
        const std::vector<CoordinateLatLon>& ptsWGS84,
        size_t nodeA,
        size_t nodeB,
        const std::string& baseId,
        double maxLen,
        const BlockEndpoint& epA,
        const BlockEndpoint& epB);

    /** @brief Nœud non-STRAIGHT → frontière. */
    static bool isFrontier(const PipelineContext& ctx, size_t nodeId);

    /** @brief Ajoute les points d'un AtomicSegment à la géométrie accumulée. */
    static void appendSegment(std::vector<CoordinateXY>& ptsUTM,
        std::vector<CoordinateLatLon>& ptsWGS84,
        const AtomicSegment& seg,
        bool                           reversed);

    /**
     * @brief Clé directionnelle : from * 1'000'000 + to.
     *
     * Valide pour < 1 000 000 nœuds.
     */
    static size_t directedKey(size_t from, size_t to)
    {
        return from * 1'000'000ULL + to;
    }

    /**
     * @brief Clé canonique Cantor(min, max) — indépendante de l'ordre.
     *
     * Utilisée uniquement pour @c straightByEndpointPair.
     */
    static size_t pairKey(size_t idA, size_t idB)
    {
        const size_t a = std::min(idA, idB);
        const size_t b = std::max(idA, idB);
        return (a + b) * (a + b + 1) / 2 + b;
    }

    /** @brief Calcule la longueur UTM d'une polyligne. */
    static double computeLength(const std::vector<CoordinateXY>& pts);
};

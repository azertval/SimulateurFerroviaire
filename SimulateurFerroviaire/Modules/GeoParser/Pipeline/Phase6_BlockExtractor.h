/**
 * @file  Phase6_BlockExtractor.h
 * @brief Phase 6 du pipeline — extraction des blocs ferroviaires.
 *
 * Responsabilité unique : produire les @ref StraightBlock et @ref SwitchBlock
 * non-orientés depuis le graphe classifié.
 *
 * @par Algorithme
 *  -# Un @ref SwitchBlock par nœud @c NodeClass::SWITCH.
 *  -# Un @ref StraightBlock par chemin entre deux nœuds frontières,
 *     traversant les nœuds @c NodeClass::STRAIGHT (transparents).
 *  -# Câblage des endpoints (nœud frontière → bloc voisin).
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
     * @param ctx     Contexte pipeline. Lit topoGraph + classifiedNodes +
     *                splitNetwork, écrit blocks. Libère les trois sources.
     * @param config  Configuration — non utilisée directement ici,
     *                transmise pour cohérence de signature.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase6_BlockExtractor() = delete;

private:

    /**
     * @brief Extrait tous les SwitchBlocks depuis les nœuds SWITCH.
     *
     * @param ctx     Contexte pipeline.
     * @param logger  Référence au logger.
     */
    static void extractSwitches(PipelineContext& ctx, Logger& logger);

    /**
     * @brief Extrait tous les StraightBlocks par DFS entre nœuds frontières.
     *
     * @param ctx     Contexte pipeline.
     * @param logger  Référence au logger.
     */
    static void extractStraights(PipelineContext& ctx, Logger& logger);

    /**
     * @brief Vérifie si un nœud est un nœud frontière (non-STRAIGHT).
     *
     * Les nœuds STRAIGHT sont transparents — ils ne délimitent pas de bloc.
     *
     * @param ctx     Contexte pipeline.
     * @param nodeId  ID du nœud à tester.
     *
     * @return @c true si le nœud est TERMINUS, SWITCH, CROSSING, ISOLATED
     *         ou AMBIGUOUS.
     */
    static bool isFrontier(const PipelineContext& ctx, size_t nodeId);

    /**
     * @brief Ajoute les points d'un segment atomique à une géométrie accumulée.
     *
     * Respecte le sens de traversal (normal ou inversé).
     * Saute le premier point pour éviter les doublons de jonction.
     *
     * @param ptsUTM    Vecteur UTM à compléter.
     * @param ptsWGS84  Vecteur WGS84 à compléter.
     * @param seg       Segment à ajouter.
     * @param reversed  @c true si le segment doit être ajouté à l'envers.
     */
    static void appendSegment(std::vector<CoordinateXY>& ptsUTM,
        std::vector<CoordinateLatLon>& ptsWGS84,
        const AtomicSegment& seg,
        bool                       reversed);

    /**
     * @brief Calcule une clé canonique pour une paire de nœuds frontières.
     *
     * Garantit que (A, B) et (B, A) produisent la même clé.
     *
     * @param idA  Premier nœud.
     * @param idB  Second nœud.
     *
     * @return Clé unique via Cantor pairing sur min/max.
     */
    static size_t pairKey(size_t idA, size_t idB);
};
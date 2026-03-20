#pragma once

/**
 * @file  TopologyExtractor.h
 * @brief Phases 3, 4, 5a et 5b — extraction de la topologie ferroviaire depuis le graphe.
 *
 * Phase 3 (detectSwitches) :
 *   Identifie les nœuds de jonction (degré ≥ 3) et crée les objets SwitchBlock.
 *
 * Phase 4 (extractStraights) :
 *   Marche le graphe depuis chaque nœud frontière.
 *   Collecte les chemins droits maximaux entre nœuds frontières.
 *   Déduplique les chemins par empreinte géométrique.
 *
 * Phase 5a (splitLongStraights) :
 *   Subdivise tout StraightBlock dépassant maxStraightLengthMeters.
 *   Les morceaux sont nommés "s/N_c1", "s/N_c2", etc.
 *
 * Phase 5b (wireTopology) :
 *   Peuple SwitchBlock::branchIds avec les IDs des StraightBlocks adjacents.
 *   Peuple StraightBlock::neighbourIds avec les IDs des voisins (Switch ou Straight).
 */

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "GraphBuilder.h"
#include "Modules/Models/StraightBlock.h"
#include "Modules/Models/SwitchBlock.h"
#include "./Enums/GeoParserEnums.h"
#include "Engine/Core/Logger/Logger.h"


/**
 * @brief Résultat complet de l'extraction topologique (Phases 3–5b).
 */
struct TopologyExtractResult
{
    /** Liste des aiguillages détectés. */
    std::vector<SwitchBlock> switches;

    /** Liste des blocs de voie droite extraits. */
    std::vector<StraightBlock> straights;

    /** nodeId → switchId. Peuplée en Phase 3. */
    std::unordered_map<int, std::string> nodeIdToSwitchId;

    /** switchId → nodeId de jonction. Utilisée par les phases suivantes. */
    std::unordered_map<std::string, int> switchIdToNodeId;

    /**
     * straightId → (startNodeId, endNodeId).
     * Nœuds internes de morceaux découpés = TopologySentinel::INTERNAL_CHUNK_NODE (-1).
     */
    std::unordered_map<std::string, std::pair<int, int>> straightEndpointNodeIds;
};


/**
 * @brief Extrait la topologie ferroviaire (Switch + Straight) depuis un TopologyGraph.
 */
class TopologyExtractor
{
public:

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Construit le TopologyExtractor avec les données issues de GraphBuilder.
     *
     * @param logger                  Référence au logger du moteur GeoParser.
     * @param graphResult             Résultat de la Phase 1+2 (modifié en place).
     * @param maxStraightLengthMeters Longueur maximale d'un StraightBlock avant découpe.
     */
    TopologyExtractor(Logger&           logger,
                       GraphBuildResult& graphResult,
                       double            maxStraightLengthMeters
                           = ParserDefaultValues::MAX_STRAIGHT_LENGTH_METERS);

    // -------------------------------------------------------------------------
    // API publique
    // -------------------------------------------------------------------------

    /**
     * @brief Exécute les phases 3, 4, 5a et 5b et retourne le résultat.
     * @return TopologyExtractResult complet.
     */
    TopologyExtractResult extract();

private:

    Logger&           m_logger;
    GraphBuildResult& m_graphResult;
    double            m_maxStraightLengthMeters;

    std::unordered_map<int, std::string>                m_nodeIdToSwitchId;
    std::unordered_map<std::string, int>                m_switchIdToNodeId;
    std::unordered_map<std::string, std::pair<int,int>> m_straightEndpointNodeIds;

    // -------------------------------------------------------------------------
    // Phases internes
    // -------------------------------------------------------------------------

    /** Phase 3 — Crée les SwitchBlock depuis les nœuds de jonction. */
    std::vector<SwitchBlock> detectSwitches();

    /** Phase 4 — Marche du graphe et extraction des StraightBlock maximaux. */
    std::vector<StraightBlock> extractStraights();

    /** Phase 5a — Découpe les StraightBlock trop longs en morceaux. */
    std::vector<StraightBlock> splitLongStraights(
        const std::vector<StraightBlock>& inputStraights);

    /** Phase 5b — Câblage bidirectionnel Switch ↔ Straight. */
    void wireTopology(std::vector<SwitchBlock>&   switches,
                       std::vector<StraightBlock>& straights);

    /**
     * @brief Marche le graphe depuis startNodeId le long de incomingEdgeId
     *        jusqu'au prochain nœud frontière.
     *
     * @return ID du nœud d'arrivée (nœud frontière atteint).
     */
    int walkPathUntilBoundary(int                        startNodeId,
                               const std::string&         incomingEdgeId,
                               std::vector<CoordinateXY>& accumulatedCoords,
                               std::set<std::string>&     visitedEdgeIds);

    /**
     * @brief Regroupe les IDs de morceaux par ID de base (ex. "s/0" → ["s/0","s/0_c1"]).
     */
    std::unordered_map<std::string, std::vector<std::string>> groupChunksByBaseId() const;
};

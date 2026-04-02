/**
 * @file  TopologyGraph.h
 * @brief Structures de données produites par @ref Phase4_TopologyBuilder.
 *
 * Graphe planaire non-orienté : nœuds topologiques + arêtes.
 * Les nœuds sont les points de jonction du réseau (après snap des extrémités).
 * Les arêtes correspondent aux segments atomiques de @ref SplitNetwork.
 */
#pragma once

#include <vector>

#include "Engine/Core/Coordinates/CoordinateLatLon.h"
#include "Engine/Core/Coordinates/CoordinateXY.h"

 /**
  * @struct TopoNode
  * @brief Nœud topologique du graphe planaire.
  *
  * Résultat de la fusion de toutes les extrémités de segments
  * distantes de moins de @c ParserConfig::snapTolerance.
  */
struct TopoNode
{
    size_t       id = 0;   ///< Identifiant unique (index dans TopoGraph::nodes).
    CoordinateXY posUTM;        ///< Position UTM (x = est, y = nord, mètres).
    CoordinateLatLon       posWGS84;      ///< Position WGS84 (lat, lon) — pour rendu Leaflet.
};

/**
 * @struct TopoEdge
 * @brief Arête du graphe planaire — connexion entre deux nœuds.
 *
 * Correspond à un @ref AtomicSegment de @ref SplitNetwork.
 * Les points intermédiaires du segment sont stockés dans @c segmentIndex.
 */
struct TopoEdge
{
    size_t nodeA = 0;  ///< ID du nœud A (extrémité source).
    size_t nodeB = 0;  ///< ID du nœud B (extrémité cible).
    size_t segmentIndex = 0; ///< Indice dans @c SplitNetwork::segments.

    /**
     * @brief Retourne l'ID du nœud opposé.
     *
     * @param fromId  ID du nœud courant (nodeA ou nodeB).
     *
     * @return ID du nœud opposé, ou @c SIZE_MAX si @p fromId est invalide.
     */
    [[nodiscard]] size_t opposite(size_t fromId) const
    {
        if (fromId == nodeA) return nodeB;
        if (fromId == nodeB) return nodeA;
        return SIZE_MAX;
    }
};

/**
 * @struct TopologyGraph
 * @brief Résultat de @ref Phase4_TopologyBuilder — graphe planaire complet.
 *
 * Produit en Phase 4, consommé par @ref Phase5_SwitchClassifier.
 * Libérable après Phase 6 via @c clear().
 *
 * @par Adjacence
 * L'index d'adjacence est construit une fois via @c buildAdjacency()
 * et utilisé par Phase 5 pour calculer les degrés et angles.
 */
struct TopologyGraph
{
    /** Nœuds topologiques — un par point de jonction du réseau. */
    std::vector<TopoNode> nodes;

    /** Arêtes — une par segment atomique (après élimination des dégénérés). */
    std::vector<TopoEdge> edges;

    /**
     * Index d'adjacence : nodeId → liste des arêtes incidentes.
     * Construit par @c buildAdjacency() — ne pas accéder avant l'appel.
     */
    std::vector<std::vector<size_t>> adjacency;

    /**
     * @brief Construit l'index d'adjacence depuis @c edges.
     *
     * À appeler après construction du graphe, avant Phase 5.
     * Complexité O(|edges|).
     */
    void buildAdjacency()
    {
        adjacency.assign(nodes.size(), {});
        for (size_t ei = 0; ei < edges.size(); ++ei)
        {
            adjacency[edges[ei].nodeA].push_back(ei);
            adjacency[edges[ei].nodeB].push_back(ei);
        }
    }

    /**
     * @brief Retourne le degré d'un nœud (nombre d'arêtes incidentes).
     *
     * @param nodeId  ID du nœud.
     *
     * @return Degré du nœud. 0 si nodeId invalide ou adjacency non construite.
     */
    [[nodiscard]] size_t degree(size_t nodeId) const
    {
        if (nodeId >= adjacency.size()) return 0;
        return adjacency[nodeId].size();
    }

    /** @brief Vide le graphe — libère la mémoire après Phase 6. */
    void clear()
    {
        nodes.clear();    nodes.shrink_to_fit();
        edges.clear();    edges.shrink_to_fit();
        adjacency.clear(); adjacency.shrink_to_fit();
    }

    /** @return @c true si le graphe est vide. */
    [[nodiscard]] bool empty() const { return nodes.empty(); }
};
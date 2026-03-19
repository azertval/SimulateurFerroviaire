#pragma once

/**
 * @file  TopologyGraph.h
 * @brief Graphe planaire non-orienté de nœuds et d'arêtes ferroviaires accrochés.
 *
 * Responsabilités :
 *   - Gestion des nœuds    : accrochage sur grille, déduplication, fusion par proximité.
 *   - Gestion des arêtes   : arêtes non-orientées avec géométrie polyligne métrique.
 *   - Fusion des nœuds     : algorithme union-find pour les points quasi-coïncidents.
 *   - Requêtes géométriques: longueur accessible, continuation colinéaire.
 *
 * Ce graphe ne contient aucune logique ferroviaire — c'est un graphe topologique pur.
 * Les nœuds sont identifiés par des entiers consécutifs.
 */

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "TopologyEdge.h"
#include "../Models/CoordinateXY.h"
#include "../Enums/GeoParserEnums.h"


/**
 * @brief Graphe planaire non-orienté utilisé pour la construction de la topologie ferroviaire.
 *
 * Nœuds indexés par entiers. Arêtes indexées par chaînes "e/N".
 * Liste d'adjacence : nodeId → [(voisinId, edgeId), ...].
 */
class TopologyGraph
{
public:

    // -------------------------------------------------------------------------
    // Types internes
    // -------------------------------------------------------------------------

    /** Liste d'adjacence : nodeId → liste de (voisinId, edgeId). */
    using AdjacencyList = std::unordered_map<int, std::vector<std::pair<int, std::string>>>;

    /** Index des arêtes : edgeId → TopologyEdge. */
    using EdgeIndex = std::unordered_map<std::string, TopologyEdge>;


    // -------------------------------------------------------------------------
    // Données publiques
    // -------------------------------------------------------------------------

    /**
     * Positions métriques des nœuds, indexées par ID de nœud.
     * nodePositions[nodeId] = (x, y) en coordonnées UTM.
     */
    std::vector<CoordinateXY> nodePositions;

    /** Liste d'adjacence de tous les nœuds. */
    AdjacencyList adjacency;

    /** Index de toutes les arêtes du graphe. */
    EdgeIndex edges;


    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Construit un graphe vide avec le pas de grille spécifié.
     *
     * @param snapGridMeters  Pas de la grille d'accrochage des nœuds (mètres).
     *                        Utiliser ParserDefaultValues::SNAP_GRID_METERS si inconnu.
     */
    explicit TopologyGraph(double snapGridMeters = ParserDefaultValues::SNAP_GRID_METERS);


    // -------------------------------------------------------------------------
    // Gestion des nœuds
    // -------------------------------------------------------------------------

    /**
     * @brief Retourne l'ID du nœud en (x, y), le créant si absent.
     *
     * La coordonnée est accrochée sur la grille avant recherche/insertion,
     * garantissant la déduplication des nœuds quasi-coïncidents.
     *
     * @param x  Abscisse métrique.
     * @param y  Ordonnée métrique.
     * @return ID du nœud (entier ≥ 0).
     */
    int getOrCreateNode(double x, double y);

    /**
     * @brief Retourne le degré d'un nœud (nombre d'arêtes incidentes).
     * @param nodeId  Identifiant du nœud.
     * @return Degré du nœud, ou 0 si le nœud n'existe pas.
     */
    int getDegreeOfNode(int nodeId) const;


    // -------------------------------------------------------------------------
    // Gestion des arêtes
    // -------------------------------------------------------------------------

    /**
     * @brief Ajoute une arête non-orientée entre les nœuds startNodeId et endNodeId.
     *
     * @param startNodeId   Index du nœud de départ.
     * @param endNodeId     Index du nœud d'arrivée.
     * @param edgeGeometry  Polyligne métrique de l'arête.
     * @return Identifiant de l'arête créée (ex. "e/42").
     */
    std::string addEdge(int startNodeId, int endNodeId, std::vector<CoordinateXY> edgeGeometry);

    /**
     * @brief Supprime l'arête et ses entrées dans la liste d'adjacence.
     * @param edgeId  Identifiant de l'arête à supprimer.
     */
    void removeEdge(const std::string& edgeId);

    /**
     * @brief Retourne l'ID de l'arête reliant nodeIdA et nodeIdB, ou nullopt.
     *
     * @param nodeIdA  Premier nœud.
     * @param nodeIdB  Second nœud.
     * @return ID de l'arête ou nullopt si aucune arête n'existe entre ces nœuds.
     */
    std::optional<std::string> findEdgeBetween(int nodeIdA, int nodeIdB) const;

    /**
     * @brief Retourne l'ID du nœud de l'autre extrémité d'une arête.
     *
     * @param edgeId      Identifiant de l'arête.
     * @param fromNodeId  Nœud depuis lequel on regarde.
     * @return ID du nœud opposé.
     */
    int getOppositeNodeId(const std::string& edgeId, int fromNodeId) const;


    // -------------------------------------------------------------------------
    // Fusion des nœuds proches (union-find)
    // -------------------------------------------------------------------------

    /**
     * @brief Fusionne les nœuds séparés de moins de toleranceMeters.
     *
     * Algorithme union-find avec binning par cellule de grille (O(n) moyen).
     * Supprime les doublons d'arêtes (garde le plus court) et les boucles.
     *
     * @param toleranceMeters  Distance maximale de fusion en mètres.
     */
    void mergeCloseNodes(double toleranceMeters);


    // -------------------------------------------------------------------------
    // Requêtes géométriques
    // -------------------------------------------------------------------------

    /**
     * @brief Marche le graphe depuis fromNodeId vers towardNodeId et accumule les longueurs.
     *
     * S'arrête au premier nœud de degré ≠ PASS_THROUGH ou quand limitMeters est atteint.
     * Utilisé pour valider les longueurs minimales des branches d'aiguillage.
     *
     * @param fromNodeId     Nœud de départ de la marche.
     * @param towardNodeId   Nœud dans la direction de marche initiale.
     * @param limitMeters    Limite de longueur cumulée.
     * @return Longueur accessible totale en mètres.
     */
    double computeReachableLength(int fromNodeId, int towardNodeId, double limitMeters) const;

    /**
     * @brief Retourne l'arête sortante en atNodeId la plus colinéaire à incomingEdgeId.
     *
     * Calcule l'angle entre le vecteur d'arrivée et chaque vecteur sortant potentiel.
     * Retourne l'arête dont l'angle est le plus faible (continuation la plus directe).
     *
     * Utilisé pour linéariser les nœuds de passage (degré 2) lors de l'extraction
     * des blocs Straight.
     *
     * @param atNodeId         Nœud de jonction.
     * @param incomingEdgeId   Arête entrante (référence de direction).
     * @return ID de l'arête la plus colinéaire, ou nullopt si aucune disponible.
     */
    std::optional<std::string> findMostCollinearContinuation(
        int atNodeId, const std::string& incomingEdgeId) const;

private:

    // -------------------------------------------------------------------------
    // Membres privés
    // -------------------------------------------------------------------------

    double      m_snapGridMeters;   ///< Pas de la grille d'accrochage.
    int         m_nextEdgeIndex;    ///< Compteur pour la génération des IDs d'arête.

    /** Clé de hachage pour les nœuds : (x, y) accrochés sur grille. */
    struct SnappedNodeKey
    {
        double x;
        double y;
        bool operator==(const SnappedNodeKey& other) const
        {
            return x == other.x && y == other.y;
        }
    };

    struct SnappedNodeKeyHasher
    {
        std::size_t operator()(const SnappedNodeKey& key) const noexcept
        {
            // Reinterpretation bit-à-bit des doubles pour un hachage cohérent sur grille
            std::uint64_t bitsX, bitsY;
            static_assert(sizeof(double) == sizeof(std::uint64_t), "sizeof mismatch");
            std::memcpy(&bitsX, &key.x, sizeof(bitsX));
            std::memcpy(&bitsY, &key.y, sizeof(bitsY));
            return std::hash<std::uint64_t>{}(bitsX)
                 ^ (std::hash<std::uint64_t>{}(bitsY) * 2654435761ULL + 0x9e3779b9ULL);
        }
    };

    std::unordered_map<SnappedNodeKey, int, SnappedNodeKeyHasher> m_keyToNodeIndex;


    // -------------------------------------------------------------------------
    // Algorithme union-find (privé)
    // -------------------------------------------------------------------------

    /** Remonte la chaîne de parents jusqu'à la racine (avec compression de chemin). */
    static int findRootWithPathCompression(std::vector<int>& parentArray, int nodeIndex);

    /** Fusionne deux ensembles par rang. */
    static void uniteNodeSets(std::vector<int>& parentArray,
                               std::vector<int>& rankArray,
                               int nodeIndexA,
                               int nodeIndexB);
};

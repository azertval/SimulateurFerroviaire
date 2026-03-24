/**
 * @file  Phase4_TopologyBuilder.h
 * @brief Phase 4 du pipeline — construction du graphe planaire.
 *
 * Responsabilité unique : fusionner les extrémités proches (snap) via
 * Union-Find et construire @ref TopologyGraph depuis @ref SplitNetwork.
 *
 * @par Algorithme
 *  -# Parcourt toutes les extrémités des @ref AtomicSegment.
 *  -# Pour chaque extrémité, cherche dans la snap grid si un nœud existe
 *     à moins de @c ParserConfig::snapTolerance.
 *  -# Si oui : Union-Find fusionne les deux extrémités.
 *     Si non  : nouveau nœud créé et inséré dans la grille.
 *  -# Path compression → ID canonique par extrémité.
 *  -# Construction des @ref TopoEdge depuis les segments.
 *  -# Appel de @ref TopologyGraph::buildAdjacency.
 *
 * @par Libération mémoire
 * @c ctx.splitNetwork est libéré en fin d'exécution.
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase4_TopologyBuilder
{
public:

    /**
     * @brief Exécute la phase 4.
     *
     * Lit @c ctx.splitNetwork, construit @c ctx.topoGraph via snap
     * + Union-Find. Libère @c ctx.splitNetwork en fin d'exécution.
     *
     * @param ctx     Contexte pipeline. Lit splitNetwork, écrit topoGraph.
     * @param config  Configuration — utilise @c snapTolerance.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase4_TopologyBuilder() = delete;

private:

    /**
     * @brief Structure Union-Find interne avec path compression + union by rank.
     *
     * Encapsule les vecteurs parent/rank et les opérations find/unite.
     * Instanciée localement dans @ref run() — durée de vie = Phase 4.
     */
    struct UnionFind
    {
        std::vector<size_t> parent;
        std::vector<int>    rank_;

        /**
         * @brief Initialise l'Union-Find pour @p n éléments.
         *
         * Chaque élément est sa propre racine (parent[i] = i, rank[i] = 0).
         *
         * @param n  Nombre d'éléments.
         */
        explicit UnionFind(size_t n)
            : parent(n), rank_(n, 0)
        {
            std::iota(parent.begin(), parent.end(), size_t{ 0 });
        }

        /**
         * @brief Trouve le représentant canonique de @p x — path compression.
         *
         * @param x  Indice de l'élément.
         *
         * @return ID canonique (racine de l'arbre contenant @p x).
         */
        size_t find(size_t x)
        {
            if (parent[x] != x)
                parent[x] = find(parent[x]);  // Raccourcissement récursif
            return parent[x];
        }

        /**
         * @brief Fusionne les ensembles contenant @p a et @p b — union by rank.
         *
         * @param a  Premier élément.
         * @param b  Second élément.
         */
        void unite(size_t a, size_t b)
        {
            a = find(a); b = find(b);
            if (a == b) return;
            if (rank_[a] < rank_[b]) std::swap(a, b);
            parent[b] = a;
            if (rank_[a] == rank_[b]) ++rank_[a];
        }
    };

    /**
     * @brief Cherche un nœud existant dans la snap grid à moins de @p tolerance.
     *
     * Inspecte les 9 cellules voisines (3×3) autour de @p pos.
     *
     * @param pos        Position UTM à tester.
     * @param grid       Grille snap : cellule → liste d'indices de nœuds.
     * @param nodePos    Positions UTM de tous les nœuds créés.
     * @param cellSize   Taille de cellule = snapTolerance.
     * @param tolerance  Distance maximale de snap.
     *
     * @return Indice du nœud le plus proche si distance < @p tolerance,
     *         @c SIZE_MAX sinon.
     */
    static size_t findSnapNeighbour(
        const CoordinateXY& pos,
        const std::unordered_map<GridCell,
        std::vector<size_t>,
        GridCellHash>& grid,
        const std::vector<CoordinateXY>& nodePos,
        double cellSize,
        double tolerance);
};
/**
 * @file  Phase4_TopologyBuilder.cpp
 * @brief Implémentation de la phase 4 — graphe planaire.
 *
 * @see Phase4_TopologyBuilder
 */
#include "Phase4_TopologyBuilder.h"

#include <numeric>
#include <algorithm>
#include <cmath>
#include <unordered_map>


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void Phase4_TopologyBuilder::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t segCount = ctx.splitNetwork.size();
    LOG_INFO(logger, "Construction du graphe — "
        + std::to_string(segCount) + " segment(s) atomique(s).");

    if (segCount == 0)
    {
        LOG_WARNING(logger, "splitNetwork vide — graphe non construit.");
        return;
    }

    // -------------------------------------------------------------------------
    // Structures de travail
    // -------------------------------------------------------------------------

    // 2 extrémités par segment → 2*segCount slots dans l'Union-Find
    // Slot [2*si]   = extrémité A du segment si
    // Slot [2*si+1] = extrémité B du segment si
    UnionFind uf(segCount * 2);

    // Positions UTM et WGS84 de chaque endpoint (avant fusion)
    std::vector<CoordinateXY> endUTM(segCount * 2);
    std::vector<CoordinateLatLon>       endWGS84(segCount * 2);

    for (size_t si = 0; si < segCount; ++si)
    {
        const AtomicSegment& seg = ctx.splitNetwork.segments[si];
        endUTM[2 * si] = seg.endpointA();
        endUTM[2 * si + 1] = seg.endpointB();
        endWGS84[2 * si] = seg.pointsWGS84.front();
        endWGS84[2 * si + 1] = seg.pointsWGS84.back();
    }

    // -------------------------------------------------------------------------
    // Snap grid — cellSize = snapTolerance
    // -------------------------------------------------------------------------
    const double cellSize = config.snapTolerance;

    // Grille : cellule UTM → indices d'endpoints représentants (après snap)
    std::unordered_map<GridCell, std::vector<size_t>, GridCellHash> snapGrid;

    // Position canonique de chaque ensemble Union-Find
    // (position du premier endpoint qui a créé cet ensemble)
    std::vector<CoordinateXY> canonPos(segCount * 2);
    std::vector<CoordinateLatLon>       canonPosWGS84(segCount * 2);

    for (size_t i = 0; i < segCount * 2; ++i)
        canonPos[i] = endUTM[i];   // Position initiale = position propre

    // -------------------------------------------------------------------------
    // Phase de snap : pour chaque endpoint, cherche un voisin dans la grille
    // -------------------------------------------------------------------------
    for (size_t i = 0; i < segCount * 2; ++i)
    {
        const CoordinateXY& pos = endUTM[i];

        // Cherche un nœud existant à moins de snapTolerance
        const size_t neighbour = findSnapNeighbour(
            pos, snapGrid,
            canonPos,          // positions des représentants courants
            cellSize,
            config.snapTolerance);

        if (neighbour != SIZE_MAX)
        {
            // Fusion : i rejoint l'ensemble de neighbour
            uf.unite(i, neighbour);
            // La position canonique reste celle du représentant existant
        }
        else
        {
            // Nouveau nœud — insère dans la grille
            const int col = static_cast<int>(std::floor(pos.x / cellSize));
            const int row = static_cast<int>(std::floor(pos.y / cellSize));
            snapGrid[{col, row}].push_back(i);
            canonPos[i] = pos;
            canonPosWGS84[i] = endWGS84[i];
        }
    }

    // -------------------------------------------------------------------------
    // Construction des nœuds topologiques (représentants canoniques uniques)
    // -------------------------------------------------------------------------
    // Mappe ID Union-Find → index dans topoGraph.nodes
    std::unordered_map<size_t, size_t> canonToNodeIdx;

    for (size_t i = 0; i < segCount * 2; ++i)
    {
        const size_t canon = uf.find(i);
        if (canonToNodeIdx.count(canon)) continue;

        const size_t nodeIdx = ctx.topoGraph.nodes.size();
        canonToNodeIdx[canon] = nodeIdx;

        TopoNode node;
        node.id = nodeIdx;
        node.posUTM = canonPos[canon];
        node.posWGS84 = canonPosWGS84[canon];
        ctx.topoGraph.nodes.push_back(node);
    }

    LOG_DEBUG(logger, std::to_string(ctx.topoGraph.nodes.size())
        + " nœud(s) topologique(s) créé(s).");

    // -------------------------------------------------------------------------
    // Construction des arêtes
    // -------------------------------------------------------------------------
    size_t degenerateCount = 0;

    for (size_t si = 0; si < segCount; ++si)
    {
        const size_t canonA = uf.find(2 * si);
        const size_t canonB = uf.find(2 * si + 1);

        if (canonA == canonB)
        {
            // Segment dégénéré — les deux extrémités ont fusionné
            // (segment plus court que snapTolerance)
            ++degenerateCount;
            continue;
        }

        TopoEdge edge;
        edge.nodeA = canonToNodeIdx[canonA];
        edge.nodeB = canonToNodeIdx[canonB];
        edge.segmentIndex = si;
        ctx.topoGraph.edges.push_back(edge);
    }

    if (degenerateCount > 0)
        LOG_WARNING(logger, std::to_string(degenerateCount)
            + " segment(s) dégénéré(s) ignoré(s) (longueur < snapTolerance).");

    LOG_DEBUG(logger, std::to_string(ctx.topoGraph.edges.size())
        + " arête(s) créée(s).");

    // -------------------------------------------------------------------------
    // Index d'adjacence — nécessaire pour Phase 5
    // -------------------------------------------------------------------------
    ctx.topoGraph.buildAdjacency();

    ctx.endTimer(t0, "Phase4_TopologyBuilder",
        segCount,
        ctx.topoGraph.nodes.size());

    LOG_INFO(logger, "Graphe construit — "
        + std::to_string(ctx.topoGraph.nodes.size()) + " nœud(s), "
        + std::to_string(ctx.topoGraph.edges.size()) + " arête(s).");

    // -------------------------------------------------------------------------
    // Libération mémoire
    // -------------------------------------------------------------------------
    ctx.splitNetwork.clear();
    LOG_DEBUG(logger, "splitNetwork libéré.");
}


// =============================================================================
// Recherche snap grid
// =============================================================================

size_t Phase4_TopologyBuilder::findSnapNeighbour(
    const CoordinateXY& pos,
    const std::unordered_map<GridCell,
    std::vector<size_t>,
    GridCellHash>& grid,
    const std::vector<CoordinateXY>& nodePos,
    double cellSize,
    double tolerance)
{
    const int col = static_cast<int>(std::floor(pos.x / cellSize));
    const int row = static_cast<int>(std::floor(pos.y / cellSize));

    size_t bestIdx = SIZE_MAX;
    double bestDist = tolerance;  // Seuil initial = tolérance

    // Inspection des 9 cellules voisines (3×3)
    // Nécessaire car un voisin peut être dans une cellule adjacente
    for (int dc = -1; dc <= 1; ++dc)
    {
        for (int dr = -1; dr <= 1; ++dr)
        {
            const auto it = grid.find({ col + dc, row + dr });
            if (it == grid.end()) continue;

            for (const size_t idx : it->second)
            {
                const CoordinateXY& candidate = nodePos[idx];
                const double dx = candidate.x - pos.x;
                const double dy = candidate.y - pos.y;
                const double dist = std::sqrt(dx * dx + dy * dy);

                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestIdx = idx;
                }
            }
        }
    }

    return bestIdx;
}
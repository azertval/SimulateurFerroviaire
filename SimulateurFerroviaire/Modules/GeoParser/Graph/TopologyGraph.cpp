/**
 * @file  TopologyGraph.cpp
 * @brief Implémentation du graphe planaire de topologie ferroviaire.
 */

#include "TopologyGraph.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "../Utils/GeometryUtils.h"


// =============================================================================
// Construction
// =============================================================================

TopologyGraph::TopologyGraph(double snapGridMeters)
    : m_snapGridMeters(snapGridMeters)
    , m_nextEdgeIndex(0)
{}


// =============================================================================
// Gestion des nœuds
// =============================================================================

int TopologyGraph::getOrCreateNode(double x, double y)
{
    const CoordinateXY snapped = GeometryUtils::snapToMetricGrid(x, y, m_snapGridMeters);
    const SnappedNodeKey key{ snapped.x, snapped.y };

    auto iterator = m_keyToNodeIndex.find(key);
    if (iterator != m_keyToNodeIndex.end())
    {
        return iterator->second;
    }

    const int newNodeId = static_cast<int>(nodePositions.size());
    nodePositions.push_back(snapped);
    m_keyToNodeIndex[key] = newNodeId;
    adjacency[newNodeId]  = {};
    return newNodeId;
}

int TopologyGraph::getDegreeOfNode(int nodeId) const
{
    auto iterator = adjacency.find(nodeId);
    if (iterator == adjacency.end())
    {
        return 0;
    }
    return static_cast<int>(iterator->second.size());
}


// =============================================================================
// Gestion des arêtes
// =============================================================================

std::string TopologyGraph::addEdge(int startNodeId, int endNodeId,
                                    std::vector<CoordinateXY> edgeGeometry)
{
    const std::string edgeId = "e/" + std::to_string(m_nextEdgeIndex++);
    TopologyEdge newEdge(edgeId, startNodeId, endNodeId, std::move(edgeGeometry));

    edges[edgeId] = std::move(newEdge);
    adjacency[startNodeId].emplace_back(endNodeId, edgeId);
    adjacency[endNodeId].emplace_back(startNodeId, edgeId);
    return edgeId;
}

void TopologyGraph::removeEdge(const std::string& edgeId)
{
    auto edgeIterator = edges.find(edgeId);
    if (edgeIterator == edges.end())
    {
        return;
    }

    const int startNode = edgeIterator->second.startNodeIndex;
    const int endNode   = edgeIterator->second.endNodeIndex;
    edges.erase(edgeIterator);

    auto removeFromAdjacency = [&](int nodeId)
    {
        auto& adjacencyList = adjacency[nodeId];
        adjacencyList.erase(
            std::remove_if(adjacencyList.begin(), adjacencyList.end(),
                [&](const std::pair<int, std::string>& entry)
                {
                    return entry.second == edgeId;
                }),
            adjacencyList.end()
        );
    };

    removeFromAdjacency(startNode);
    removeFromAdjacency(endNode);
}

std::optional<std::string> TopologyGraph::findEdgeBetween(int nodeIdA, int nodeIdB) const
{
    auto iterator = adjacency.find(nodeIdA);
    if (iterator == adjacency.end())
    {
        return std::nullopt;
    }

    for (const auto& [neighbourId, edgeId] : iterator->second)
    {
        if (neighbourId == nodeIdB)
        {
            return edgeId;
        }
    }
    return std::nullopt;
}

int TopologyGraph::getOppositeNodeId(const std::string& edgeId, int fromNodeId) const
{
    const auto& edge = edges.at(edgeId);
    return (edge.startNodeIndex == fromNodeId) ? edge.endNodeIndex : edge.startNodeIndex;
}


// =============================================================================
// Fusion des nœuds proches
// =============================================================================

void TopologyGraph::mergeCloseNodes(double toleranceMeters)
{
    if (toleranceMeters <= 0.0)
    {
        return;
    }

    const int totalNodeCount = static_cast<int>(nodePositions.size());
    if (totalNodeCount == 0)
    {
        return;
    }

    std::vector<int> parentArray(totalNodeCount);
    std::vector<int> rankArray(totalNodeCount, 0);
    for (int index = 0; index < totalNodeCount; ++index)
    {
        parentArray[index] = index;
    }

    // Binning par cellule de grille pour éviter O(n²)
    using GridCell = std::pair<int, int>;
    struct GridCellHasher
    {
        std::size_t operator()(const GridCell& cell) const noexcept
        {
            return std::hash<int>{}(cell.first) ^ (std::hash<int>{}(cell.second) << 16);
        }
    };
    std::unordered_map<GridCell, std::vector<int>, GridCellHasher> cellBins;

    for (int nodeId = 0; nodeId < totalNodeCount; ++nodeId)
    {
        const int cellX = static_cast<int>(std::floor(nodePositions[nodeId].x / toleranceMeters));
        const int cellY = static_cast<int>(std::floor(nodePositions[nodeId].y / toleranceMeters));
        cellBins[{cellX, cellY}].push_back(nodeId);
    }

    // Fusion des nœuds dans les cellules voisines (3×3)
    for (auto& [cell, nodeIdsInCell] : cellBins)
    {
        std::vector<int> neighbourNodeIds;
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                auto neighbourCellIterator = cellBins.find({ cell.first + dx, cell.second + dy });
                if (neighbourCellIterator != cellBins.end())
                {
                    for (int neighbourId : neighbourCellIterator->second)
                    {
                        neighbourNodeIds.push_back(neighbourId);
                    }
                }
            }
        }

        for (int nodeIdI : nodeIdsInCell)
        {
            for (int nodeIdJ : neighbourNodeIds)
            {
                if (nodeIdJ <= nodeIdI)
                {
                    continue;
                }
                const double deltaX = nodePositions[nodeIdI].x - nodePositions[nodeIdJ].x;
                const double deltaY = nodePositions[nodeIdI].y - nodePositions[nodeIdJ].y;
                const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
                if (distanceSquared <= toleranceMeters * toleranceMeters)
                {
                    uniteNodeSets(parentArray, rankArray, nodeIdI, nodeIdJ);
                }
            }
        }
    }

    // Vérifier si des fusions ont eu lieu
    std::vector<int> canonicalIds(totalNodeCount);
    bool anyMerge = false;
    for (int index = 0; index < totalNodeCount; ++index)
    {
        canonicalIds[index] = findRootWithPathCompression(parentArray, index);
        if (canonicalIds[index] != index)
        {
            anyMerge = true;
        }
    }
    if (!anyMerge)
    {
        return;
    }

    // Reconstruire les arêtes avec les IDs canoniques
    // Garder le plus court des doublons, supprimer les boucles
    using EdgeKey = std::pair<int, int>;
    struct EdgeKeyHasher
    {
        std::size_t operator()(const EdgeKey& key) const noexcept
        {
            return std::hash<int>{}(key.first) ^ (std::hash<int>{}(key.second) << 16);
        }
    };
    std::unordered_map<EdgeKey, std::string, EdgeKeyHasher> canonicalEdgeKeys;

    for (const auto& [edgeId, edge] : edges)
    {
        const int canonicalStart = canonicalIds[edge.startNodeIndex];
        const int canonicalEnd   = canonicalIds[edge.endNodeIndex];
        if (canonicalStart == canonicalEnd)
        {
            continue;  // Boucle — supprimer
        }

        const EdgeKey key{ std::min(canonicalStart, canonicalEnd),
                           std::max(canonicalStart, canonicalEnd) };

        auto existingIterator = canonicalEdgeKeys.find(key);
        if (existingIterator == canonicalEdgeKeys.end())
        {
            canonicalEdgeKeys[key] = edgeId;
        }
        else
        {
            // Garder l'arête la plus courte
            if (edge.lengthMeters < edges.at(existingIterator->second).lengthMeters)
            {
                canonicalEdgeKeys[key] = edgeId;
            }
        }
    }

    // Reconstruire la structure complète du graphe
    EdgeIndex rebuiltEdges;
    adjacency.clear();
    for (int index = 0; index < totalNodeCount; ++index)
    {
        adjacency[index] = {};
    }

    for (const auto& [key, edgeId] : canonicalEdgeKeys)
    {
        const TopologyEdge& originalEdge = edges.at(edgeId);
        TopologyEdge rebuiltEdge(originalEdge.id,
                                  key.first,
                                  key.second,
                                  originalEdge.geometry);
        rebuiltEdges[edgeId] = std::move(rebuiltEdge);
        adjacency[key.first].emplace_back(key.second,  edgeId);
        adjacency[key.second].emplace_back(key.first, edgeId);
    }

    edges = std::move(rebuiltEdges);
}


// =============================================================================
// Requêtes géométriques
// =============================================================================

double TopologyGraph::computeReachableLength(int fromNodeId,
                                              int towardNodeId,
                                              double limitMeters) const
{
    double totalLength  = 0.0;
    int    previousNode = fromNodeId;
    int    currentNode  = towardNodeId;

    while (true)
    {
        auto edgeIdOption = findEdgeBetween(previousNode, currentNode);
        if (!edgeIdOption)
        {
            return totalLength;
        }

        totalLength += edges.at(*edgeIdOption).lengthMeters;
        if (totalLength >= limitMeters)
        {
            return totalLength;
        }

        if (getDegreeOfNode(currentNode) != NodeDegreeThresholds::PASS_THROUGH)
        {
            return totalLength;
        }

        // Trouver le prochain nœud dans la direction de marche
        std::optional<int> nextNode;
        for (const auto& [neighbourId, edgeId] : adjacency.at(currentNode))
        {
            if (neighbourId != previousNode)
            {
                nextNode = neighbourId;
                break;
            }
        }

        if (!nextNode)
        {
            return totalLength;
        }

        previousNode = currentNode;
        currentNode  = *nextNode;
    }
}

std::optional<std::string> TopologyGraph::findMostCollinearContinuation(
    int atNodeId, const std::string& incomingEdgeId) const
{
    auto incomingEdgeIterator = edges.find(incomingEdgeId);
    if (incomingEdgeIterator == edges.end())
    {
        return std::nullopt;
    }

    const TopologyEdge& incomingEdge = incomingEdgeIterator->second;
    std::vector<CoordinateXY> incomingCoords = incomingEdge.geometry;

    // Orienter les coordonnées de façon à ce que le dernier point soit atNodeId
    if (incomingEdge.startNodeIndex == atNodeId)
    {
        std::reverse(incomingCoords.begin(), incomingCoords.end());
    }

    if (incomingCoords.size() < 2)
    {
        return std::nullopt;
    }

    // Vecteur d'arrivée (direction de la dernière arête incidente)
    const CoordinateXY arrivalVector{
        incomingCoords.back().x - incomingCoords[incomingCoords.size() - 2].x,
        incomingCoords.back().y - incomingCoords[incomingCoords.size() - 2].y
    };

    std::optional<std::string> bestEdgeId;
    double                     minimumAngle = std::numeric_limits<double>::max();

    auto adjacencyIterator = adjacency.find(atNodeId);
    if (adjacencyIterator == adjacency.end())
    {
        return std::nullopt;
    }

    for (const auto& [neighbourId, candidateEdgeId] : adjacencyIterator->second)
    {
        if (candidateEdgeId == incomingEdgeId)
        {
            continue;  // Ne pas faire demi-tour
        }

        auto candidateEdgeIterator = edges.find(candidateEdgeId);
        if (candidateEdgeIterator == edges.end())
        {
            continue;
        }

        std::vector<CoordinateXY> outgoingCoords = candidateEdgeIterator->second.geometry;
        if (candidateEdgeIterator->second.startNodeIndex != atNodeId)
        {
            std::reverse(outgoingCoords.begin(), outgoingCoords.end());
        }
        if (outgoingCoords.size() < 2)
        {
            continue;
        }

        const CoordinateXY departureVector{
            outgoingCoords[1].x - outgoingCoords[0].x,
            outgoingCoords[1].y - outgoingCoords[0].y
        };

        const double angle = GeometryUtils::unsignedAngleBetweenVectors(arrivalVector, departureVector);
        if (angle < minimumAngle)
        {
            minimumAngle = angle;
            bestEdgeId   = candidateEdgeId;
        }
    }

    return bestEdgeId;
}


// =============================================================================
// Algorithme union-find (statique privé)
// =============================================================================

int TopologyGraph::findRootWithPathCompression(std::vector<int>& parentArray, int nodeIndex)
{
    while (parentArray[nodeIndex] != nodeIndex)
    {
        // Compression de chemin (path halving)
        parentArray[nodeIndex] = parentArray[parentArray[nodeIndex]];
        nodeIndex = parentArray[nodeIndex];
    }
    return nodeIndex;
}

void TopologyGraph::uniteNodeSets(std::vector<int>& parentArray,
                                   std::vector<int>& rankArray,
                                   int nodeIndexA,
                                   int nodeIndexB)
{
    nodeIndexA = findRootWithPathCompression(parentArray, nodeIndexA);
    nodeIndexB = findRootWithPathCompression(parentArray, nodeIndexB);

    if (nodeIndexA == nodeIndexB)
    {
        return;
    }

    // Union par rang
    if (rankArray[nodeIndexA] < rankArray[nodeIndexB])
    {
        std::swap(nodeIndexA, nodeIndexB);
    }
    parentArray[nodeIndexB] = nodeIndexA;
    if (rankArray[nodeIndexA] == rankArray[nodeIndexB])
    {
        ++rankArray[nodeIndexA];
    }
}

/**
 * @file  TopologyExtractor.cpp
 * @brief Implémentation des phases 3, 4, 5a et 5b du pipeline GeoParser.
 */

#include "TopologyExtractor.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "./Utils/GeometryUtils.h"


 // =============================================================================
 // Construction
 // =============================================================================

TopologyExtractor::TopologyExtractor(Logger& logger,
    GraphBuildResult& graphResult,
    double            maxStraightLengthMeters)
    : m_logger(logger)
    , m_graphResult(graphResult)
    , m_maxStraightLengthMeters(maxStraightLengthMeters)
{
}


// =============================================================================
// API publique
// =============================================================================

TopologyExtractResult TopologyExtractor::extract()
{
    LOG_INFO(m_logger, "Démarrage Phase 3 — détection des aiguillages");
    std::vector<SwitchBlock> switches = detectSwitches();
    LOG_INFO(m_logger, std::to_string(switches.size()) + " aiguillage(s) détecté(s)");

    LOG_INFO(m_logger, "Démarrage Phase 4 — extraction des voies droites");
    std::vector<StraightBlock> straights = extractStraights();
    LOG_INFO(m_logger, std::to_string(straights.size()) + " StraightBlock(s) extraits");

    LOG_INFO(m_logger, "Démarrage Phase 5a — découpe des Straight longs");
    straights = splitLongStraights(std::move(straights));
    LOG_INFO(m_logger,
        std::to_string(straights.size()) + " StraightBlock(s) après découpe");

    LOG_INFO(m_logger, "Démarrage Phase 5b — câblage topologique");
    wireTopology(switches, straights);

    TopologyExtractResult result;
    result.switches = std::move(switches);
    result.straights = std::move(straights);
    result.nodeIdToSwitchId = m_nodeIdToSwitchId;
    result.switchIdToNodeId = m_switchIdToNodeId;
    result.straightEndpointNodeIds = m_straightEndpointNodeIds;
    return result;
}


// =============================================================================
// Phase 3 — Détection des aiguillages
// =============================================================================

std::vector<SwitchBlock> TopologyExtractor::detectSwitches()
{
    TopologyGraph& graph = m_graphResult.topologyGraph;
    std::vector<SwitchBlock> switches;
    int switchIndex = 0;

    // Tri des nœuds de jonction pour un ordre déterministe
    std::vector<int> junctionNodeIds;
    for (const auto& [nodeId, adjacencyList] : graph.adjacency)
    {
        if (static_cast<int>(adjacencyList.size()) >= NodeDegreeThresholds::JUNCTION_MINIMUM)
        {
            junctionNodeIds.push_back(nodeId);
        }
    }
    std::sort(junctionNodeIds.begin(), junctionNodeIds.end());

    for (int nodeId : junctionNodeIds)
    {
        const CoordinateXY& metricPosition = graph.nodePositions[nodeId];
        const LatLon junctionCoordinate =
            GeometryUtils::metricUtmToWgs84(metricPosition,
                m_graphResult.utmZoneNumber,
                m_graphResult.isNorthernHemisphere);

        const std::string switchId = "sw/" + std::to_string(switchIndex++);
        SwitchBlock switchBlock(switchId, junctionCoordinate);

        switches.push_back(std::move(switchBlock));
        m_nodeIdToSwitchId[nodeId] = switchId;
        m_switchIdToNodeId[switchId] = nodeId;
    }

    return switches;
}


// =============================================================================
// Phase 4 — Extraction des StraightBlock
// =============================================================================

std::vector<StraightBlock> TopologyExtractor::extractStraights()
{
    TopologyGraph& graph = m_graphResult.topologyGraph;
    const std::set<int>& boundary = m_graphResult.boundaryNodeIds;

    std::set<std::string>  visitedEdgeIds;
    std::set<std::tuple<int, int, int>> seenGeometryHashes;  // hash grossier pour dédup
    std::vector<StraightBlock> straights;
    int straightIndex = 0;

    for (int startNodeId : boundary)
    {
        auto adjacencyIterator = graph.adjacency.find(startNodeId);
        if (adjacencyIterator == graph.adjacency.end())
        {
            continue;
        }

        for (const auto& [neighbourId, edgeId] : adjacencyIterator->second)
        {
            if (visitedEdgeIds.count(edgeId))
            {
                continue;
            }

            std::vector<CoordinateXY> accumulatedCoords;
            std::set<std::string>     pathEdges;

            const int endNodeId = walkPathUntilBoundary(startNodeId, edgeId,
                accumulatedCoords, pathEdges);

            if (accumulatedCoords.size() < 2)
            {
                for (const auto& eid : pathEdges) visitedEdgeIds.insert(eid);
                continue;
            }

            // Empreinte géométrique pour déduplication (premiers/derniers points)
            const auto& front = accumulatedCoords.front();
            const auto& back = accumulatedCoords.back();
            auto hashKey = std::make_tuple(
                static_cast<int>(std::round(front.x)),
                static_cast<int>(std::round(front.y)),
                static_cast<int>(std::round(back.x * 1000.0 + back.y))
            );
            if (seenGeometryHashes.count(hashKey))
            {
                for (const auto& eid : pathEdges) visitedEdgeIds.insert(eid);
                continue;
            }
            seenGeometryHashes.insert(hashKey);

            // Conversion métrique → WGS-84
            std::vector<LatLon> wgs84Coords =
                GeometryUtils::convertPolylineToWgs84(accumulatedCoords,
                    m_graphResult.utmZoneNumber,
                    m_graphResult.isNorthernHemisphere);

            const std::string straightId = "s/" + std::to_string(straightIndex++);
            StraightBlock straight(straightId, std::move(wgs84Coords));

            m_straightEndpointNodeIds[straightId] = { startNodeId, endNodeId };
            straights.push_back(std::move(straight));

            for (const auto& eid : pathEdges) visitedEdgeIds.insert(eid);
        }
    }

    return straights;
}

int TopologyExtractor::walkPathUntilBoundary(int                        startNodeId,
    const std::string& incomingEdgeId,
    std::vector<CoordinateXY>& accumulatedCoords,
    std::set<std::string>& visitedEdgeIds)
{
    TopologyGraph& graph = m_graphResult.topologyGraph;
    const std::set<int>& boundary = m_graphResult.boundaryNodeIds;

    int         currentNodeId = startNodeId;
    std::string currentEdgeId = incomingEdgeId;
    bool        firstIteration = true;

    while (true)
    {
        visitedEdgeIds.insert(currentEdgeId);

        auto edgeIterator = graph.edges.find(currentEdgeId);
        if (edgeIterator == graph.edges.end())
        {
            break;
        }

        const TopologyEdge& edge = edgeIterator->second;
        std::vector<CoordinateXY> edgeCoords = edge.geometry;

        // Orienter les coordonnées de l'arête dans le sens de la marche
        if (edge.startNodeIndex != currentNodeId)
        {
            std::reverse(edgeCoords.begin(), edgeCoords.end());
        }

        if (firstIteration)
        {
            accumulatedCoords.insert(accumulatedCoords.end(),
                edgeCoords.begin(), edgeCoords.end());
            firstIteration = false;
        }
        else
        {
            // Éviter le doublon du point de jonction
            accumulatedCoords.insert(accumulatedCoords.end(),
                edgeCoords.begin() + 1, edgeCoords.end());
        }

        const int nextNodeId = graph.getOppositeNodeId(currentEdgeId, currentNodeId);

        // Arrêt sur nœud frontière
        if (boundary.count(nextNodeId))
        {
            return nextNodeId;
        }

        // Nœud de degré != 2 : terminus ou jonction non identifiée comme frontière
        if (graph.getDegreeOfNode(nextNodeId) != NodeDegreeThresholds::PASS_THROUGH)
        {
            return nextNodeId;
        }

        // Continuation colinéaire sur les nœuds de passage (degré 2)
        auto continuationEdgeOpt =
            graph.findMostCollinearContinuation(nextNodeId, currentEdgeId);
        if (!continuationEdgeOpt)
        {
            return nextNodeId;
        }

        currentNodeId = nextNodeId;
        currentEdgeId = *continuationEdgeOpt;
    }

    return currentNodeId;
}


// =============================================================================
// Phase 5a — Découpe des StraightBlock trop longs
// =============================================================================

std::vector<StraightBlock> TopologyExtractor::splitLongStraights(
    std::vector<StraightBlock> inputStraights)
{
    if (m_maxStraightLengthMeters <= 0.0)
    {
        return inputStraights;
    }

    std::vector<StraightBlock>                         outputStraights;
    std::unordered_map<std::string, std::pair<int, int>> newEndpoints;

    for (StraightBlock& straight : inputStraights)
    {
        const auto endpointIterator = m_straightEndpointNodeIds.find(straight.id);
        const int startNodeId = (endpointIterator != m_straightEndpointNodeIds.end())
            ? endpointIterator->second.first : -1;
        const int endNodeId = (endpointIterator != m_straightEndpointNodeIds.end())
            ? endpointIterator->second.second : -1;

        if (straight.lengthMeters <= m_maxStraightLengthMeters)
        {
            // Sauvegarder l'ID avant le move — straight.id est invalide après.
            const std::string savedId = straight.id;
            newEndpoints[savedId] = { startNodeId, endNodeId };
            outputStraights.push_back(std::move(straight));
            continue;
        }

        // Nombre de morceaux nécessaires
        const int chunkCount = static_cast<int>(
            std::ceil(straight.lengthMeters / m_maxStraightLengthMeters));

        LOG_DEBUG(m_logger,
            straight.id + " (" + std::to_string(static_cast<int>(straight.lengthMeters))
            + " m) → découpe en " + std::to_string(chunkCount) + " morceaux");

        // Projection de la polyligne en métrique pour la découpe
        std::vector<CoordinateXY> metricCoords =
            GeometryUtils::convertPolylineToMetric(straight.coordinates,
                m_graphResult.utmZoneNumber,
                m_graphResult.isNorthernHemisphere);

        const double totalMetricLength =
            GeometryUtils::computePolylineLengthMeters(metricCoords);

        if (totalMetricLength <= 0.0)
        {
            const std::string savedId = straight.id;
            newEndpoints[savedId] = { startNodeId, endNodeId };
            outputStraights.push_back(std::move(straight));
            continue;
        }

        const double chunkLengthMeters = totalMetricLength / static_cast<double>(chunkCount);

        // Subdivision de la polyligne en morceaux de longueur égale
        std::size_t coordIndex = 0;
        double      accumulated = 0.0;
        std::vector<CoordinateXY> currentChunkCoords;

        if (!metricCoords.empty())
        {
            currentChunkCoords.push_back(metricCoords[0]);
        }

        int chunkIndex = 0;
        while (coordIndex + 1 < metricCoords.size() && chunkIndex < chunkCount)
        {
            const double deltaX = metricCoords[coordIndex + 1].x - metricCoords[coordIndex].x;
            const double deltaY = metricCoords[coordIndex + 1].y - metricCoords[coordIndex].y;
            const double segLength = std::hypot(deltaX, deltaY);

            const double targetLength = (chunkIndex + 1) * chunkLengthMeters;

            if (accumulated + segLength < targetLength - 1e-9)
            {
                accumulated += segLength;
                ++coordIndex;
                if (coordIndex < metricCoords.size())
                {
                    currentChunkCoords.push_back(metricCoords[coordIndex]);
                }
            }
            else
            {
                // Point de coupure intermédiaire
                const double ratio = (targetLength - accumulated) / (segLength < 1e-12 ? 1e-12 : segLength);
                const CoordinateXY cutPoint{
                    metricCoords[coordIndex].x + ratio * deltaX,
                    metricCoords[coordIndex].y + ratio * deltaY
                };
                currentChunkCoords.push_back(cutPoint);

                if (currentChunkCoords.size() >= 2)
                {
                    const std::string chunkId = (chunkIndex == 0)
                        ? straight.id
                        : straight.id + "_c" + std::to_string(chunkIndex);

                    std::vector<LatLon> chunkWgs84 = GeometryUtils::convertPolylineToWgs84(
                        currentChunkCoords,
                        m_graphResult.utmZoneNumber,
                        m_graphResult.isNorthernHemisphere);

                    outputStraights.emplace_back(chunkId, std::move(chunkWgs84));
                    newEndpoints[chunkId] = {
                        (chunkIndex == 0) ? startNodeId : TopologySentinel::INTERNAL_CHUNK_NODE,
                        (chunkIndex == chunkCount - 1) ? endNodeId : TopologySentinel::INTERNAL_CHUNK_NODE
                    };
                }

                currentChunkCoords = { cutPoint };
                ++chunkIndex;
            }
        }

        // Dernier morceau
        if (chunkIndex < chunkCount && currentChunkCoords.size() >= 2)
        {
            if (!metricCoords.empty())
            {
                currentChunkCoords.push_back(metricCoords.back());
            }
            const std::string chunkId = (chunkIndex == 0)
                ? straight.id
                : straight.id + "_c" + std::to_string(chunkIndex);

            std::vector<LatLon> chunkWgs84 = GeometryUtils::convertPolylineToWgs84(
                currentChunkCoords,
                m_graphResult.utmZoneNumber,
                m_graphResult.isNorthernHemisphere);

            outputStraights.emplace_back(chunkId, std::move(chunkWgs84));
            newEndpoints[chunkId] = {
                (chunkIndex == 0) ? startNodeId : TopologySentinel::INTERNAL_CHUNK_NODE,
                endNodeId
            };
        }
    }

    m_straightEndpointNodeIds = std::move(newEndpoints);
    return outputStraights;
}


// =============================================================================
// Phase 5b — Câblage topologique
// =============================================================================

void TopologyExtractor::wireTopology(std::vector<SwitchBlock>& switches,
    std::vector<StraightBlock>& straights)
{
    // Index rapide par ID
    std::unordered_map<std::string, SwitchBlock*>   switchById;
    std::unordered_map<std::string, StraightBlock*> straightById;
    for (auto& sw : switches)   switchById[sw.id] = &sw;
    for (auto& st : straights)  straightById[st.id] = &st;

    // Map nodeId → liste de StraightBlock IDs touchant ce nœud
    std::unordered_map<int, std::vector<std::string>> nodeToStraightIds;
    for (const auto& [straightId, endpoints] : m_straightEndpointNodeIds)
    {
        const auto [startNode, endNode] = endpoints;
        if (startNode >= 0) nodeToStraightIds[startNode].push_back(straightId);
        if (endNode >= 0) nodeToStraightIds[endNode].push_back(straightId);
    }

    // Peuplement des branchIds des Switch
    for (auto& switchBlock : switches)
    {
        const auto nodeIterator = m_switchIdToNodeId.find(switchBlock.id);
        if (nodeIterator == m_switchIdToNodeId.end()) continue;
        const int junctionNodeId = nodeIterator->second;

        std::set<std::string> addedBranches;
        for (const auto& straightId : nodeToStraightIds[junctionNodeId])
        {
            if (!addedBranches.count(straightId))
            {
                switchBlock.branchIds.push_back(straightId);
                addedBranches.insert(straightId);
            }
        }
    }

    // Peuplement des neighbourIds des StraightBlock
    std::unordered_map<std::string, std::set<std::string>> neighbourSets;
    for (const auto& straight : straights)
    {
        neighbourSets[straight.id] = {};
    }

    // Voisins depuis les nœuds Switch
    for (const auto& [straightId, endpoints] : m_straightEndpointNodeIds)
    {
        for (int nodeId : { endpoints.first, endpoints.second })
        {
            if (nodeId < 0) continue;
            auto switchIterator = m_nodeIdToSwitchId.find(nodeId);
            if (switchIterator != m_nodeIdToSwitchId.end())
            {
                neighbourSets[straightId].insert(switchIterator->second);
            }
        }
    }

    // Voisins entre morceaux consécutifs d'un même Straight découpé
    const auto chunkGroups = groupChunksByBaseId();
    for (const auto& [baseId, chunkIds] : chunkGroups)
    {
        for (std::size_t index = 0; index < chunkIds.size(); ++index)
        {
            if (index > 0)
            {
                neighbourSets[chunkIds[index]].insert(chunkIds[index - 1]);
            }
            if (index + 1 < chunkIds.size())
            {
                neighbourSets[chunkIds[index]].insert(chunkIds[index + 1]);
            }
        }
    }

    // Application des sets triés sur chaque StraightBlock
    for (auto& straight : straights)
    {
        auto setIterator = neighbourSets.find(straight.id);
        if (setIterator == neighbourSets.end()) continue;

        straight.neighbourIds.assign(setIterator->second.begin(),
            setIterator->second.end());
        std::sort(straight.neighbourIds.begin(), straight.neighbourIds.end());
    }
}

std::unordered_map<std::string, std::vector<std::string>>
TopologyExtractor::groupChunksByBaseId() const
{
    std::unordered_map<std::string, std::vector<std::string>> groups;
    for (const auto& [straightId, endpoints] : m_straightEndpointNodeIds)
    {
        const std::string baseId = straightId.substr(0, straightId.find("_c"));
        groups[baseId].push_back(straightId);
    }
    for (auto& [baseId, chunkIds] : groups)
    {
        std::sort(chunkIds.begin(), chunkIds.end(),
            [](const std::string& idA, const std::string& idB)
            {
                const auto posA = idA.find("_c");
                const auto posB = idB.find("_c");
                const int  indexA = (posA != std::string::npos)
                    ? std::stoi(idA.substr(posA + 2)) : 0;
                const int  indexB = (posB != std::string::npos)
                    ? std::stoi(idB.substr(posB + 2)) : 0;
                return indexA < indexB;
            });
    }
    return groups;
}
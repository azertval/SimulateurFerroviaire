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

    std::vector<int> junctionNodeIds;
    for (const auto& [nodeId, adjacencyList] : graph.adjacency)
        if (static_cast<int>(adjacencyList.size()) >= NodeDegreeThresholds::JUNCTION_MINIMUM)
            junctionNodeIds.push_back(nodeId);

    std::sort(junctionNodeIds.begin(), junctionNodeIds.end());

    for (int nodeId : junctionNodeIds)
    {
        const CoordinateXY& metricPosition = graph.nodePositions[nodeId];
        const LatLon junctionCoordinate =
            GeometryUtils::metricUtmToWgs84(metricPosition,
                m_graphResult.utmZoneNumber,
                m_graphResult.isNorthernHemisphere);

        const std::string switchId = "sw/" + std::to_string(switchIndex++);
        switches.emplace_back(switchId, junctionCoordinate);

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

    std::set<std::string>              visitedEdgeIds;
    std::set<std::tuple<int, int, int>> seenGeometryHashes;
    std::vector<StraightBlock>         straights;
    int straightIndex = 0;

    for (int startNodeId : boundary)
    {
        auto adjIt = graph.adjacency.find(startNodeId);
        if (adjIt == graph.adjacency.end()) continue;

        for (const auto& [neighbourId, edgeId] : adjIt->second)
        {
            if (visitedEdgeIds.count(edgeId)) continue;

            std::vector<CoordinateXY> accumulatedCoords;
            std::set<std::string>     pathEdges;

            const int endNodeId = walkPathUntilBoundary(startNodeId, edgeId,
                accumulatedCoords, pathEdges);

            if (accumulatedCoords.size() < 2)
            {
                for (const auto& eid : pathEdges) visitedEdgeIds.insert(eid);
                continue;
            }

            const auto& front = accumulatedCoords.front();
            const auto& back = accumulatedCoords.back();
            auto hashKey = std::make_tuple(
                static_cast<int>(std::round(front.x)),
                static_cast<int>(std::round(front.y)),
                static_cast<int>(std::round(back.x * 1000.0 + back.y)));

            if (seenGeometryHashes.count(hashKey))
            {
                for (const auto& eid : pathEdges) visitedEdgeIds.insert(eid);
                continue;
            }
            seenGeometryHashes.insert(hashKey);

            std::vector<LatLon> wgs84Coords =
                GeometryUtils::convertPolylineToWgs84(accumulatedCoords,
                    m_graphResult.utmZoneNumber,
                    m_graphResult.isNorthernHemisphere);

            const std::string straightId = "s/" + std::to_string(straightIndex++);
            straights.emplace_back(straightId, std::move(wgs84Coords));
            m_straightEndpointNodeIds[straightId] = { startNodeId, endNodeId };

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

        auto edgeIt = graph.edges.find(currentEdgeId);
        if (edgeIt == graph.edges.end()) break;

        const TopologyEdge& edge = edgeIt->second;
        std::vector<CoordinateXY> edgeCoords = edge.geometry;

        if (edge.startNodeIndex != currentNodeId)
            std::reverse(edgeCoords.begin(), edgeCoords.end());

        if (firstIteration)
        {
            accumulatedCoords.insert(accumulatedCoords.end(),
                edgeCoords.begin(), edgeCoords.end());
            firstIteration = false;
        }
        else
        {
            accumulatedCoords.insert(accumulatedCoords.end(),
                edgeCoords.begin() + 1, edgeCoords.end());
        }

        const int nextNodeId = graph.getOppositeNodeId(currentEdgeId, currentNodeId);

        if (boundary.count(nextNodeId)) return nextNodeId;

        if (graph.getDegreeOfNode(nextNodeId) != NodeDegreeThresholds::PASS_THROUGH)
            return nextNodeId;

        auto continuationOpt =
            graph.findMostCollinearContinuation(nextNodeId, currentEdgeId);
        if (!continuationOpt) return nextNodeId;

        currentNodeId = nextNodeId;
        currentEdgeId = *continuationOpt;
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
        return inputStraights;

    std::vector<StraightBlock>                           outputStraights;
    std::unordered_map<std::string, std::pair<int, int>> newEndpoints;

    for (StraightBlock& straight : inputStraights)
    {
        const auto endpointIt = m_straightEndpointNodeIds.find(straight.getId());
        const int startNodeId = (endpointIt != m_straightEndpointNodeIds.end())
            ? endpointIt->second.first : -1;
        const int endNodeId = (endpointIt != m_straightEndpointNodeIds.end())
            ? endpointIt->second.second : -1;

        if (straight.getLengthMeters() <= m_maxStraightLengthMeters)
        {
            const std::string savedId = straight.getId();
            newEndpoints[savedId] = { startNodeId, endNodeId };
            outputStraights.push_back(std::move(straight));
            continue;
        }

        const int chunkCount = static_cast<int>(
            std::ceil(straight.getLengthMeters() / m_maxStraightLengthMeters));

        LOG_DEBUG(m_logger,
            straight.getId() + " (" + std::to_string(static_cast<int>(straight.getLengthMeters()))
            + " m) → découpe en " + std::to_string(chunkCount) + " morceaux");

        std::vector<CoordinateXY> metricCoords =
            GeometryUtils::convertPolylineToMetric(straight.getCoordinates(),
                m_graphResult.utmZoneNumber,
                m_graphResult.isNorthernHemisphere);

        const double totalMetricLength = GeometryUtils::computePolylineLengthMeters(metricCoords);

        if (totalMetricLength <= 0.0)
        {
            const std::string savedId = straight.getId();
            newEndpoints[savedId] = { startNodeId, endNodeId };
            outputStraights.push_back(std::move(straight));
            continue;
        }

        const double chunkLength = totalMetricLength / static_cast<double>(chunkCount);

        std::size_t coordIdx = 0;
        double      accumulated = 0.0;
        std::vector<CoordinateXY> currentChunk;
        if (!metricCoords.empty())
            currentChunk.push_back(metricCoords[0]);

        int chunkIdx = 0;
        const std::string baseId = straight.getId();   // save before potential move

        while (coordIdx + 1 < metricCoords.size() && chunkIdx < chunkCount)
        {
            const double dx = metricCoords[coordIdx + 1].x - metricCoords[coordIdx].x;
            const double dy = metricCoords[coordIdx + 1].y - metricCoords[coordIdx].y;
            const double segLen = std::hypot(dx, dy);
            const double target = (chunkIdx + 1) * chunkLength;

            if (accumulated + segLen < target - 1e-9)
            {
                accumulated += segLen;
                ++coordIdx;
                if (coordIdx < metricCoords.size())
                    currentChunk.push_back(metricCoords[coordIdx]);
            }
            else
            {
                const double ratio = (target - accumulated) / (segLen < 1e-12 ? 1e-12 : segLen);
                const CoordinateXY cutPoint{
                    metricCoords[coordIdx].x + ratio * dx,
                    metricCoords[coordIdx].y + ratio * dy };
                currentChunk.push_back(cutPoint);

                if (currentChunk.size() >= 2)
                {
                    const std::string chunkId = (chunkIdx == 0)
                        ? baseId
                        : baseId + "_c" + std::to_string(chunkIdx);

                    std::vector<LatLon> chunkWgs84 = GeometryUtils::convertPolylineToWgs84(
                        currentChunk, m_graphResult.utmZoneNumber, m_graphResult.isNorthernHemisphere);

                    outputStraights.emplace_back(chunkId, std::move(chunkWgs84));
                    newEndpoints[chunkId] = {
                        (chunkIdx == 0) ? startNodeId : TopologySentinel::INTERNAL_CHUNK_NODE,
                        (chunkIdx == chunkCount - 1) ? endNodeId : TopologySentinel::INTERNAL_CHUNK_NODE
                    };
                }

                currentChunk = { cutPoint };
                ++chunkIdx;
            }
        }

        // Dernier morceau
        if (chunkIdx < chunkCount && currentChunk.size() >= 2)
        {
            if (!metricCoords.empty())
                currentChunk.push_back(metricCoords.back());

            const std::string chunkId = (chunkIdx == 0)
                ? baseId
                : baseId + "_c" + std::to_string(chunkIdx);

            std::vector<LatLon> chunkWgs84 = GeometryUtils::convertPolylineToWgs84(
                currentChunk, m_graphResult.utmZoneNumber, m_graphResult.isNorthernHemisphere);

            outputStraights.emplace_back(chunkId, std::move(chunkWgs84));
            newEndpoints[chunkId] = {
                (chunkIdx == 0) ? startNodeId : TopologySentinel::INTERNAL_CHUNK_NODE,
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
    std::unordered_map<std::string, SwitchBlock*>   switchById;
    std::unordered_map<std::string, StraightBlock*> straightById;
    for (auto& sw : switches)  switchById[sw.getId()] = &sw;
    for (auto& st : straights) straightById[st.getId()] = &st;

    // nodeId → liste des StraightBlock IDs touchant ce nœud
    std::unordered_map<int, std::vector<std::string>> nodeToStraightIds;
    for (const auto& [straightId, endpoints] : m_straightEndpointNodeIds)
    {
        const auto [startNode, endNode] = endpoints;
        if (startNode >= 0) nodeToStraightIds[startNode].push_back(straightId);
        if (endNode >= 0) nodeToStraightIds[endNode].push_back(straightId);
    }

    // Peuplement des branchIds des switches
    for (auto& sw : switches)
    {
        const auto nodeIt = m_switchIdToNodeId.find(sw.getId());
        if (nodeIt == m_switchIdToNodeId.end()) continue;
        const int junctionNodeId = nodeIt->second;

        for (const auto& straightId : nodeToStraightIds[junctionNodeId])
            sw.addBranchId(straightId);
    }

    // Peuplement des neighbourIds des straights (via les nœuds de switch)
    for (const auto& [straightId, endpoints] : m_straightEndpointNodeIds)
    {
        auto stIt = straightById.find(straightId);
        if (stIt == straightById.end()) continue;
        StraightBlock& st = *stIt->second;

        for (int nodeId : { endpoints.first, endpoints.second })
        {
            if (nodeId < 0) continue;
            auto swIt = m_nodeIdToSwitchId.find(nodeId);
            if (swIt != m_nodeIdToSwitchId.end())
                st.addNeighbourId(swIt->second);
        }
    }

    // Voisins entre morceaux consécutifs d'un même Straight découpé
    const auto chunkGroups = groupChunksByBaseId();
    for (const auto& [baseId, chunkIds] : chunkGroups)
    {
        for (std::size_t i = 0; i < chunkIds.size(); ++i)
        {
            auto stIt = straightById.find(chunkIds[i]);
            if (stIt == straightById.end()) continue;
            StraightBlock& st = *stIt->second;

            if (i > 0)                      st.addNeighbourId(chunkIds[i - 1]);
            if (i + 1 < chunkIds.size())    st.addNeighbourId(chunkIds[i + 1]);
        }
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
            [](const std::string& a, const std::string& b)
            {
                const auto posA = a.find("_c");
                const auto posB = b.find("_c");
                const int  idxA = (posA != std::string::npos) ? std::stoi(a.substr(posA + 2)) : 0;
                const int  idxB = (posB != std::string::npos) ? std::stoi(b.substr(posB + 2)) : 0;
                return idxA < idxB;
            });
    }
    return groups;
}
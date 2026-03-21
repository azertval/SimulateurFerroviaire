/**
 * @file  GraphBuilder.cpp
 * @brief Implémentation des phases 1 et 2 — chargement GeoJSON et construction du graphe.
 */

#include "GraphBuilder.h"

#include <fstream>
#include <sstream>

#include "./Exceptions/GeoParserException.h"
#include "./Utils/GeometryUtils.h"

#if __has_include("../../External/nlohmann/json.hpp")
#  include "../../External/nlohmann/json.hpp"
using JsonDocument = nlohmann::json;
#  define NLOHMANN_JSON_AVAILABLE 1
#else
#  pragma message("GraphBuilder : External/nlohmann/json.hpp introuvable.")
#  define NLOHMANN_JSON_AVAILABLE 0
#endif


 // =============================================================================
 // Construction
 // =============================================================================

GraphBuilder::GraphBuilder(Logger& logger,
    const std::string& geoJsonFilePath,
    double             snapGridMeters,
    double             endpointSnapMeters)
    : m_logger(logger)
    , m_geoJsonFilePath(geoJsonFilePath)
    , m_snapGridMeters(snapGridMeters)
    , m_endpointSnapMeters(endpointSnapMeters)
{
}


// =============================================================================
// API publique
// =============================================================================

GraphBuildResult GraphBuilder::build()
{
    LOG_INFO(m_logger, "Démarrage Phase 1+2 — fichier : " + m_geoJsonFilePath);

    std::vector<RawPolyline> rawPolylines;
    int  estimatedUtmZone = 32;
    bool detectedNorthernHemisphere = true;

    loadGeoJsonFile(rawPolylines, estimatedUtmZone, detectedNorthernHemisphere);

    LOG_INFO(m_logger,
        std::to_string(rawPolylines.size()) + " polyligne(s) chargée(s) — zone UTM : "
        + std::to_string(estimatedUtmZone)
        + (detectedNorthernHemisphere ? "N" : "S"));

    TopologyGraph graph = buildTopologyGraph(rawPolylines,
        estimatedUtmZone,
        detectedNorthernHemisphere);

    LOG_INFO(m_logger,
        "Graphe construit — " + std::to_string(graph.nodePositions.size())
        + " nœud(s), " + std::to_string(graph.edges.size()) + " arête(s)");

    std::set<int> boundaryIds = computeBoundaryNodeIds(graph);

    LOG_DEBUG(m_logger,
        std::to_string(boundaryIds.size()) + " nœud(s) frontière(s) identifié(s)");

    GraphBuildResult result;
    result.topologyGraph = std::move(graph);
    result.boundaryNodeIds = std::move(boundaryIds);
    result.utmZoneNumber = estimatedUtmZone;
    result.isNorthernHemisphere = detectedNorthernHemisphere;
    return result;
}


// =============================================================================
// Phase 1 — Chargement du GeoJSON
// =============================================================================

void GraphBuilder::loadGeoJsonFile(std::vector<RawPolyline>& rawPolylines,
    int& estimatedUtmZone,
    bool& detectedNorthernHemisphere)
{
#if !NLOHMANN_JSON_AVAILABLE
    throw InvalidGeoJsonFormatException(
        "nlohmann/json.hpp introuvable. "
        "Télécharger depuis https://github.com/nlohmann/json/releases "
        "et placer dans External/nlohmann/json.hpp");
#else
    std::ifstream fileStream(m_geoJsonFilePath);
    if (!fileStream.is_open())
        throw InvalidGeoJsonFormatException(
            "Impossible d'ouvrir le fichier GeoJSON : " + m_geoJsonFilePath);

    JsonDocument document;
    try { fileStream >> document; }
    catch (const JsonDocument::exception& e)
    {
        throw InvalidGeoJsonFormatException(
            "Fichier GeoJSON malformé : " + std::string(e.what()));
    }

    if (!document.contains("features") || !document["features"].is_array())
        throw InvalidGeoJsonFormatException(
            "GeoJSON invalide : champ 'features' absent ou non-tableau");

    bool utmZoneDetected = false;
    int  featureIndex = 0;

    for (const auto& feature : document["features"])
    {
        if (!feature.contains("geometry") || feature["geometry"].is_null()) continue;

        const auto& geometry = feature["geometry"];
        if (!geometry.contains("type") || !geometry.contains("coordinates")) continue;

        const std::string geometryType = geometry["type"].get<std::string>();

        std::string featureId = "feature/" + std::to_string(featureIndex++);
        if (feature.contains("id") && !feature["id"].is_null())
            featureId = feature["id"].get<std::string>();

        std::vector<std::vector<std::pair<double, double>>> coordinateSets;

        if (geometryType == "LineString")
        {
            std::vector<std::pair<double, double>> cs;
            for (const auto& pt : geometry["coordinates"])
                if (pt.size() >= 2)
                    cs.emplace_back(pt[0].get<double>(), pt[1].get<double>());
            if (cs.size() >= 2) coordinateSets.push_back(std::move(cs));
        }
        else if (geometryType == "MultiLineString")
        {
            for (const auto& line : geometry["coordinates"])
            {
                std::vector<std::pair<double, double>> cs;
                for (const auto& pt : line)
                    if (pt.size() >= 2)
                        cs.emplace_back(pt[0].get<double>(), pt[1].get<double>());
                if (cs.size() >= 2) coordinateSets.push_back(std::move(cs));
            }
        }
        else
        {
            LOG_DEBUG(m_logger,
                "Géométrie ignorée (type non supporté) : " + geometryType
                + " sur feature " + featureId);
            continue;
        }

        for (const auto& cs : coordinateSets)
        {
            RawPolyline polyline;
            polyline.featureId = featureId;
            for (const auto& [lon, lat] : cs)
                polyline.wgs84Coords.push_back(LatLon(lat, lon));

            if (!utmZoneDetected && !polyline.wgs84Coords.empty())
            {
                estimatedUtmZone = GeometryUtils::estimateUtmZone(
                    polyline.wgs84Coords.front().longitude);
                detectedNorthernHemisphere = (polyline.wgs84Coords.front().latitude >= 0.0);
                utmZoneDetected = true;
                LOG_DEBUG(m_logger,
                    "Zone UTM estimée : " + std::to_string(estimatedUtmZone)
                    + (detectedNorthernHemisphere ? "N" : "S"));
            }
            rawPolylines.push_back(std::move(polyline));
        }
    }

    if (rawPolylines.empty())
        throw InvalidGeoJsonFormatException(
            "Le fichier GeoJSON ne contient aucune géométrie LineString exploitable");
#endif
}


// =============================================================================
// Phase 2 — Construction du TopologyGraph
// =============================================================================

TopologyGraph GraphBuilder::buildTopologyGraph(const std::vector<RawPolyline>& rawPolylines,
    int  utmZoneNumber,
    bool isNorthernHemisphere)
{
    TopologyGraph graph(m_snapGridMeters);

    for (const auto& polyline : rawPolylines)
    {
        std::vector<CoordinateXY> metricCoords =
            GeometryUtils::convertPolylineToMetric(polyline.wgs84Coords,
                utmZoneNumber,
                isNorthernHemisphere);
        if (metricCoords.size() < 2) continue;

        for (auto& coord : metricCoords)
            coord = GeometryUtils::snapToMetricGrid(coord.x, coord.y, m_snapGridMeters);

        for (std::size_t i = 1; i < metricCoords.size(); ++i)
        {
            if (metricCoords[i] == metricCoords[i - 1]) continue;
            const int startId = graph.getOrCreateNode(metricCoords[i - 1].x, metricCoords[i - 1].y);
            const int endId = graph.getOrCreateNode(metricCoords[i].x, metricCoords[i].y);
            graph.addEdge(startId, endId, { metricCoords[i - 1], metricCoords[i] });
        }
    }

    LOG_DEBUG(m_logger,
        "Graphe brut : " + std::to_string(graph.nodePositions.size())
        + " nœud(s), " + std::to_string(graph.edges.size()) + " arête(s)");

    graph.mergeCloseNodes(m_endpointSnapMeters);

    if (graph.edges.empty())
        throw InvalidTopologyException(
            "Aucune arête construite depuis le GeoJSON — vérifier le fichier source");

    LOG_DEBUG(m_logger,
        "Après fusion (" + std::to_string(m_endpointSnapMeters) + " m) : "
        + std::to_string(graph.nodePositions.size()) + " nœud(s), "
        + std::to_string(graph.edges.size()) + " arête(s)");

    return graph;
}


// =============================================================================
// Calcul des nœuds frontières
// =============================================================================

std::set<int> GraphBuilder::computeBoundaryNodeIds(const TopologyGraph& graph) const
{
    std::set<int> boundaryIds;

    int terminus = 0;
    int switches = 0;
    int crossings = 0;

    for (const auto& [nodeId, adjacencyList] : graph.adjacency)
    {
        const int degree = static_cast<int>(adjacencyList.size());

        if (degree == NodeDegreeThresholds::TERMINUS)
        {
            boundaryIds.insert(nodeId);
            ++terminus;
        }
        else if (degree == NodeDegreeThresholds::SWITCH_PORT_COUNT)
        {
            // Aiguillage en Y : exactement 3 branches
            boundaryIds.insert(nodeId);
            ++switches;
        }
        else if (degree > NodeDegreeThresholds::SWITCH_PORT_COUNT)
        {
            // Croisement (degré 4+) : frontière pour découper les Straights,
            // mais PAS un aiguillage — sera ignoré par detectSwitches.
            boundaryIds.insert(nodeId);
            ++crossings;
            LOG_DEBUG(m_logger,
                "Nœud frontière degré " + std::to_string(degree)
                + " (croisement, non-switch) — nodeId=" + std::to_string(nodeId));
        }
    }

    LOG_INFO(m_logger,
        "Frontières : " + std::to_string(terminus) + " terminus, "
        + std::to_string(switches) + " aiguillage(s) potentiel(s), "
        + std::to_string(crossings) + " croisement(s) ignoré(s)");

    return boundaryIds;
}
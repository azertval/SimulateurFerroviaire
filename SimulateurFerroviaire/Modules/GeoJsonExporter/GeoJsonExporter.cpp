/**
 * @file  GeoJsonExporter.cpp
 * @brief Implémentation de l'exporteur GeoJSON.
 *
 * @see GeoJsonExporter
 */

#include "GeoJsonExporter.h"

#include "Modules/Models/TopologyRepository.h"

#include <fstream>
#include <iomanip>
#include <stdexcept>

JsonDocument GeoJsonExporter::convertStraightToFeature(const StraightBlock& straight)
{
    JsonDocument feature;
    feature["type"] = "Feature";

    // ---------------------------------------------------------------------
    // Properties
    // ---------------------------------------------------------------------
    feature["properties"] = {
        { "id", straight.id },
        { "feature_type", "straight" },
        { "length_m", straight.lengthMeters }
    };

    // ---------------------------------------------------------------------
    // Geometry
    // ---------------------------------------------------------------------
    JsonDocument coordinates = JsonDocument::array();

    for (const auto& coordinate : straight.coordinates)
    {
        // GeoJSON requires [longitude, latitude]
        coordinates.push_back({
            coordinate.longitude,
            coordinate.latitude
            });
    }

    feature["geometry"] = {
        { "type", "LineString" },
        { "coordinates", coordinates }
    };

    feature["properties"]["neighbour_count"] = static_cast<int>(straight.neighbourIds.size());
    feature["properties"]["neighbour_ids"] = straight.neighbourIds;

    return feature;
}

JsonDocument GeoJsonExporter::convertSwitchToFeature(const SwitchBlock& switchBlock)
{
    JsonDocument feature;
    feature["type"] = "Feature";

    // ---------------------------------------------------------------------
    // Properties
    // ---------------------------------------------------------------------
    feature["properties"] = {
        { "id", switchBlock.id },
        { "feature_type", "switch" },
        { "is_double_switch", switchBlock.isDoubleSwitch }
    };

    feature["properties"]["root_branch_id"] = switchBlock.rootBranchId;
    feature["properties"]["normal_branch_id"] = switchBlock.normalBranchId;
    feature["properties"]["deviation_branch_id"] = switchBlock.deviationBranchId;

    // ---------------------------------------------------------------------
    // Geometry
    // ---------------------------------------------------------------------
    feature["geometry"] = {
        { "type", "Point" },
        { "coordinates", {
            switchBlock.junctionCoordinate.longitude,
            switchBlock.junctionCoordinate.latitude
        }}
    };

    return feature;
}

void GeoJsonExporter::exportToFile(
    const std::string& outputPath)
{
    const std::vector<StraightBlock>& straights = TopologyRepository::instance().data().straights;
    const std::vector<SwitchBlock>& switches = TopologyRepository::instance().data().switches;
    JsonDocument root;
    root["type"] = "FeatureCollection";
    root["features"] = JsonDocument::array();

    // -------------------------------------------------------------------------
    // Export all straight blocks
    // -------------------------------------------------------------------------
    for (const auto& straight : straights)
    {
        root["features"].push_back(convertStraightToFeature(straight));
    }

    // -------------------------------------------------------------------------
    // Export all switch blocks
    // -------------------------------------------------------------------------
    for (const auto& switchBlock : switches)
    {
        root["features"].push_back(convertSwitchToFeature(switchBlock));
    }

    // -------------------------------------------------------------------------
    // Write to disk
    // -------------------------------------------------------------------------
    std::ofstream outputFile(outputPath);
    if (!outputFile.is_open())
    {
        return;
    }

    outputFile << std::setw(2) << root;
}

std::wstring GeoJsonExporter::loadGeoJsonToWebView()
{
    // Récupérer les données de topologie ferroviaire depuis le repository
    const std::vector<StraightBlock>& straights = TopologyRepository::instance().data().straights;
    const std::vector<SwitchBlock>& switches = TopologyRepository::instance().data().switches;

    // Build GeoJSON FeatureCollection
    JsonDocument root;
    root["type"] = "FeatureCollection";
    root["features"] = JsonDocument::array();

    // -------------------------------------------------------------------------
    // Export all straight blocks
    // -------------------------------------------------------------------------
    for (const auto& straight : straights)
    {
        root["features"].push_back(convertStraightToFeature(straight));
    }

    // -------------------------------------------------------------------------
    // Export all switch blocks
    // -------------------------------------------------------------------------
    for (const auto& switchBlock : switches)
    {
        root["features"].push_back(convertSwitchToFeature(switchBlock));
    }
    // serialize to string and escape for JavaScript injection
    std::string jsonString = root.dump();
    std::wstring escaped = escapeForJavaScript(jsonString);

    // Build JS script
    std::wstring script =
        L"window.loadGeoJson(JSON.parse(\"" + escaped + L"\"));";

    return script;
}

std::wstring GeoJsonExporter::escapeForJavaScript(const std::string& input)
{
    std::wstring output;
    output.reserve(input.size());

    for (char c : input)
    {
        switch (c)
        {
        case '\"': output += L"\\\""; break;
        case '\\': output += L"\\\\"; break;
        case '\n': output += L"\\n"; break;
        case '\r': output += L"\\r"; break;
        case '\t': output += L"\\t"; break;
        default: output += static_cast<wchar_t>(c);
        }
    }

    return output;
}
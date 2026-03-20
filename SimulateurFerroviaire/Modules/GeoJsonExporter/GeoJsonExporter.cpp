/**
 * @file  GeoJsonExporter.cpp
 * @brief Implémentation de l'exporteur GeoJSON.
 *
 * @see GeoJsonExporter
 */

#include "GeoJsonExporter.h"

#include "Modules/Stockages/TopologyRepository.h"

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
        { "id",           straight.id          },
        { "feature_type", "straight"            },
        { "length_m",     straight.lengthMeters }
    };

    // ---------------------------------------------------------------------
    // Geometry
    // ---------------------------------------------------------------------
    JsonDocument coordinates = JsonDocument::array();

    for (const auto& coordinate : straight.coordinates)
    {
        // GeoJSON : [longitude, latitude]
        coordinates.push_back({ coordinate.longitude, coordinate.latitude });
    }

    feature["geometry"] = {
        { "type",        "LineString" },
        { "coordinates", coordinates  }
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
        { "id",               switchBlock.id             },
        { "feature_type",     "switch"                   },
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

void GeoJsonExporter::exportToFile(const std::string& outputPath)
{
    const auto& straights = TopologyRepository::instance().data().straights;
    const auto& switches = TopologyRepository::instance().data().switches;

    JsonDocument root;
    root["type"] = "FeatureCollection";
    root["features"] = JsonDocument::array();

    for (const auto& straight : straights)
        root["features"].push_back(convertStraightToFeature(*straight));

    for (const auto& sw : switches)
        root["features"].push_back(convertSwitchToFeature(*sw));

    std::ofstream outputFile(outputPath);
    if (!outputFile.is_open()) return;

    outputFile << std::setw(2) << root;
}

std::wstring GeoJsonExporter::loadGeoJsonToWebView()
{
    const auto& straights = TopologyRepository::instance().data().straights;
    const auto& switches = TopologyRepository::instance().data().switches;

    JsonDocument root;
    root["type"] = "FeatureCollection";
    root["features"] = JsonDocument::array();

    for (const auto& straight : straights)
        root["features"].push_back(convertStraightToFeature(*straight));

    for (const auto& sw : switches)
        root["features"].push_back(convertSwitchToFeature(*sw));

    std::string  jsonString = root.dump();
    std::wstring escaped = escapeForJavaScript(jsonString);

    return L"window.loadGeoJson(JSON.parse(\"" + escaped + L"\"));";
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
        case '\n': output += L"\\n";  break;
        case '\r': output += L"\\r";  break;
        case '\t': output += L"\\t";  break;
        default:   output += static_cast<wchar_t>(c);
        }
    }

    return output;
}

std::wstring GeoJsonExporter::renderStraightBlock(const StraightBlock& straightBlock)
{
    if (straightBlock.coordinates.size() < 2)
        return L"";

    std::wstring script = L"renderStraightBlock(";

    script += L"\"";
    script += std::wstring(straightBlock.id.begin(), straightBlock.id.end());
    script += L"\",";

    script += L"[";
    for (std::size_t i = 0; i < straightBlock.coordinates.size(); ++i)
    {
        const LatLon& coord = straightBlock.coordinates[i];
        script += L"[";
        script += std::to_wstring(coord.latitude);
        script += L",";
        script += std::to_wstring(coord.longitude);
        script += L"]";
        if (i + 1 < straightBlock.coordinates.size())
            script += L",";
    }
    script += L"]";
    script += L");";

    return script;
}

std::wstring GeoJsonExporter::renderAllStraightBlocks()
{
    const auto& straights = TopologyRepository::instance().data().straights;

    std::wstring script;
    script += L"clearStraightBlocks();";

    for (const auto& straight : straights)
        script += renderStraightBlock(*straight);

    script += L"zoomToStraights();";
    return script;
}

std::wstring GeoJsonExporter::renderSwitchBlock(const SwitchBlock& sw)
{
    std::wstring script = L"renderSwitch(";

    script += L"\"";
    script += std::wstring(sw.id.begin(), sw.id.end());
    script += L"\",";

    script += std::to_wstring(sw.junctionCoordinate.latitude);
    script += L",";
    script += std::to_wstring(sw.junctionCoordinate.longitude);
    script += L",";

    script += (sw.isDoubleSwitch ? L"true" : L"false");
    script += L");";

    return script;
}

std::wstring GeoJsonExporter::renderAllSwitchBlocksJunctions()
{
    const auto& switches = TopologyRepository::instance().data().switches;

    std::wstring script;
    script += L"clearSwitches();";

    for (const auto& sw : switches)
        script += renderSwitchBlock(*sw);

    return script;
}


// =============================================================================
// Branches d'aiguillage (root / normal / deviation)
// =============================================================================

/**
 * Encode une coordonnée optionnelle en wstring.
 * Si le tip est absent, émet "NaN,NaN" — ignoré côté JavaScript.
 */
static std::wstring encodeTip(const std::optional<LatLon>& tip)
{
    if (!tip.has_value())
        return L"NaN,NaN";

    return std::to_wstring(tip->latitude) + L"," + std::to_wstring(tip->longitude);
}

std::wstring GeoJsonExporter::renderSwitchBranches(const SwitchBlock& sw)
{
    if (!sw.isOriented()) return L"";

    std::wstring script = L"renderSwitchBranches(";

    script += L"\"";
    script += std::wstring(sw.id.begin(), sw.id.end());
    script += L"\",";

    script += std::to_wstring(sw.junctionCoordinate.latitude);
    script += L",";
    script += std::to_wstring(sw.junctionCoordinate.longitude);
    script += L",";

    script += encodeTip(sw.tipOnRoot);      script += L",";
    script += encodeTip(sw.tipOnNormal);    script += L",";
    script += encodeTip(sw.tipOnDeviation);

    script += L");";
    return script;
}

std::wstring GeoJsonExporter::renderAllSwitchBranches()
{
    const auto& switches = TopologyRepository::instance().data().switches;

    std::wstring script;
    script += L"clearSwitchBranches();";

    for (const auto& sw : switches)
        script += renderSwitchBranches(*sw);

    return script;
}
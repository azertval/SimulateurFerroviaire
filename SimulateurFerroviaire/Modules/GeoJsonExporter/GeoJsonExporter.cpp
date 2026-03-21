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
#include <numbers>


 // =============================================================================
 // Helpers locaux
 // =============================================================================

namespace
{
    /**
     * Encode un tip optionnel en "lat,lon" ou "NaN,NaN" si absent.
     * Utilisé pour les branches simples (non absorbées).
     */
    std::wstring encodeTip(const std::optional<LatLon>& tip)
    {
        if (!tip.has_value())
            return L"NaN,NaN";
        return std::to_wstring(tip->latitude) + L"," + std::to_wstring(tip->longitude);
    }

    /** Convertit un std::string ASCII en std::wstring. */
    std::wstring toWide(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }

    /**
     * Encode une polyligne WGS-84 en tableau JS : [[lat,lon],[lat,lon],…]
     * Retourne "null" si la polyligne est vide.
     */
    std::wstring encodePolyline(const std::vector<LatLon>& coords)
    {
        if (coords.empty())
            return L"null";

        std::wstring s = L"[";
        for (std::size_t i = 0; i < coords.size(); ++i)
        {
            s += L"[";
            s += std::to_wstring(coords[i].latitude);
            s += L",";
            s += std::to_wstring(coords[i].longitude);
            s += L"]";
            if (i + 1 < coords.size()) s += L",";
        }
        s += L"]";
        return s;
    }
}


// =============================================================================
// Conversion GeoJSON
// =============================================================================

JsonDocument GeoJsonExporter::convertStraightToFeature(const StraightBlock& straight)
{
    JsonDocument feature;
    feature["type"] = "Feature";

    feature["properties"] = {
        { "id",           straight.getId()          },
        { "feature_type", "straight"                 },
        { "length_m",     straight.getLengthMeters() }
    };

    JsonDocument coordinates = JsonDocument::array();
    for (const auto& coord : straight.getCoordinates())
        coordinates.push_back({ coord.longitude, coord.latitude });

    feature["geometry"] = {
        { "type",        "LineString" },
        { "coordinates", coordinates  }
    };

    feature["properties"]["neighbour_count"] = static_cast<int>(straight.getNeighbourIds().size());
    feature["properties"]["neighbour_ids"] = straight.getNeighbourIds();

    return feature;
}

JsonDocument GeoJsonExporter::convertSwitchToFeature(const SwitchBlock& sw)
{
    JsonDocument feature;
    feature["type"] = "Feature";

    feature["properties"] = {
        { "id",               sw.getId()    },
        { "feature_type",     "switch"      },
        { "is_double_switch", sw.isDouble() }
    };

    feature["properties"]["root_branch_id"] = sw.getRootBranchId().value_or("");
    feature["properties"]["normal_branch_id"] = sw.getNormalBranchId().value_or("");
    feature["properties"]["deviation_branch_id"] = sw.getDeviationBranchId().value_or("");

    feature["geometry"] = {
        { "type", "Point" },
        { "coordinates", {
            sw.getJunctionCoordinate().longitude,
            sw.getJunctionCoordinate().latitude
        }}
    };

    return feature;
}


// =============================================================================
// Export fichier
// =============================================================================

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


// =============================================================================
// Injection WebView (GeoJSON brut)
// =============================================================================

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

    return L"window.loadGeoJson(JSON.parse(\""
        + escapeForJavaScript(root.dump())
        + L"\"));";
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


// =============================================================================
// Straights — rendu WebView
// =============================================================================

std::wstring GeoJsonExporter::renderStraightBlock(const StraightBlock& straight)
{
    if (straight.getCoordinates().size() < 2)
        return L"";

    std::wstring script = L"renderStraightBlock(\"";
    script += toWide(straight.getId());
    script += L"\",[";

    const auto& coords = straight.getCoordinates();
    for (std::size_t i = 0; i < coords.size(); ++i)
    {
        script += L"[";
        script += std::to_wstring(coords[i].latitude);
        script += L",";
        script += std::to_wstring(coords[i].longitude);
        script += L"]";
        if (i + 1 < coords.size()) script += L",";
    }
    script += L"]);";
    return script;
}

std::wstring GeoJsonExporter::renderAllStraightBlocks()
{
    const auto& straights = TopologyRepository::instance().data().straights;

    std::wstring script = L"clearStraightBlocks();";
    for (const auto& straight : straights)
        script += renderStraightBlock(*straight);
    script += L"zoomToStraights();";
    return script;
}


// =============================================================================
// Switches — jonction (rendu WebView)
// =============================================================================

double GeoJsonExporter::bearingDeg(const LatLon& a, const LatLon& b)
{
    constexpr double DEG2RAD = std::numbers::pi / 180.0;
    constexpr double RAD2DEG = 180.0 / std::numbers::pi;

    const double lat1 = a.latitude * DEG2RAD;
    const double lat2 = b.latitude * DEG2RAD;
    const double dLon = (b.longitude - a.longitude) * DEG2RAD;

    const double x = std::cos(lat2) * std::sin(dLon);
    const double y = std::cos(lat1) * std::sin(lat2)
        - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);

    double result = std::fmod(std::atan2(x, y) * RAD2DEG + 360.0, 360.0);
    if (std::isnan(result)) result = 0.0;
    return result;
}

std::wstring GeoJsonExporter::renderSwitchBlock(const SwitchBlock& sw)
{
    const LatLon& junction = sw.getJunctionCoordinate();

    const double bearingNormal = sw.getTipOnNormal()
        ? bearingDeg(junction, *sw.getTipOnNormal())
        : 0.0;

    const double bearingDeviation = sw.getTipOnDeviation()
        ? bearingDeg(junction, *sw.getTipOnDeviation())
        : bearingNormal;

    const std::string partnerId = sw.getPartnerId().value_or("");

    std::wstring script = L"renderSwitch(\"";
    script += toWide(sw.getId());
    script += L"\",";
    script += std::to_wstring(junction.latitude);
    script += L",";
    script += std::to_wstring(junction.longitude);
    script += L",";
    script += sw.isDouble() ? L"true" : L"false";
    script += L",";
    script += std::to_wstring(bearingNormal);
    script += L",";
    script += std::to_wstring(bearingDeviation);
    script += L",\"";
    script += toWide(partnerId);
    script += L"\");";
    return script;
}

std::wstring GeoJsonExporter::renderAllSwitchBlocksJunctions()
{
    const auto& switches = TopologyRepository::instance().data().switches;

    std::wstring script = L"clearSwitches();";
    for (const auto& sw : switches)
        script += renderSwitchBlock(*sw);
    return script;
}


// =============================================================================
// Switches — branches (rendu WebView)
// =============================================================================

/**
 * Génère l'appel JS renderSwitchBranches pour un SwitchBlock.
 *
 * Signature JS :
 *   renderSwitchBranches(id,
 *     jLat, jLon,
 *     rootCoords,      // [[lat,lon],...] ou null
 *     normalCoords,    // [[lat,lon],...] ou null
 *     devCoords)       // [[lat,lon],...] ou null
 *
 * Pour les branches simples (non absorbées) : tableau à 1 point [tip].
 * Pour les branches absorbées (double switch) : polyligne complète.
 * null si le tip est absent.
 */
std::wstring GeoJsonExporter::renderSwitchBranches(const SwitchBlock& sw)
{
    if (!sw.isOriented()) return L"";

    const LatLon& junction = sw.getJunctionCoordinate();

    // --- Branche root : toujours un simple tip (jamais absorbée) ---
    std::wstring rootCoords = L"null";
    if (sw.getTipOnRoot())
        rootCoords = L"[["
        + std::to_wstring(sw.getTipOnRoot()->latitude) + L","
        + std::to_wstring(sw.getTipOnRoot()->longitude) + L"]]";

    // --- Branche normal ---
    std::wstring normalCoords;
    if (!sw.getAbsorbedNormalCoords().empty())
    {
        // Double switch : polyligne complète du segment absorbé
        // On skip le premier point (≈ jonction de ce switch, déjà connue côté JS)
        const auto& pts = sw.getAbsorbedNormalCoords();
        normalCoords = encodePolyline(
            std::vector<LatLon>(pts.begin() + (pts.size() > 1 ? 1 : 0), pts.end()));
    }
    else if (sw.getTipOnNormal())
    {
        // Switch simple : un seul point tip
        normalCoords = L"[["
            + std::to_wstring(sw.getTipOnNormal()->latitude) + L","
            + std::to_wstring(sw.getTipOnNormal()->longitude) + L"]]";
    }
    else
    {
        normalCoords = L"null";
    }

    // --- Branche deviation ---
    std::wstring devCoords;
    if (!sw.getAbsorbedDeviationCoords().empty())
    {
        const auto& pts = sw.getAbsorbedDeviationCoords();
        devCoords = encodePolyline(
            std::vector<LatLon>(pts.begin() + (pts.size() > 1 ? 1 : 0), pts.end()));
    }
    else if (sw.getTipOnDeviation())
    {
        devCoords = L"[["
            + std::to_wstring(sw.getTipOnDeviation()->latitude) + L","
            + std::to_wstring(sw.getTipOnDeviation()->longitude) + L"]]";
    }
    else
    {
        devCoords = L"null";
    }

    std::wstring script = L"renderSwitchBranches(\"";
    script += toWide(sw.getId());
    script += L"\",";
    script += std::to_wstring(junction.latitude);
    script += L",";
    script += std::to_wstring(junction.longitude);
    script += L",";
    script += rootCoords;
    script += L",";
    script += normalCoords;
    script += L",";
    script += devCoords;
    script += L");";
    return script;
}

std::wstring GeoJsonExporter::renderAllSwitchBranches()
{
    const auto& switches = TopologyRepository::instance().data().switches;

    std::wstring script = L"clearSwitchBranches();";
    for (const auto& sw : switches)
        script += renderSwitchBranches(*sw);
    return script;
}
/**
 * @file  TopologyRenderer.cpp
 * @brief Implémentation de l'exporteur GeoJSON.
 *
 * @see TopologyRenderer
 */

#include "TopologyRenderer.h"

#include "Engine/Core/Topology/TopologyRepository.h"

#include <fstream>
#include <iomanip>

 // =============================================================================
 // Helpers locaux
 // =============================================================================

namespace
{
    /**
     * Encode un tip optionnel en "lat,lon" ou "NaN,NaN" si absent.
     * Utilisé pour les branches simples (non absorbées).
     */
    std::wstring encodeTip(const std::optional<CoordinateLatLon>& tip)
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
    std::wstring encodePolyline(const std::vector<CoordinateLatLon>& Coordinates)
    {
        if (Coordinates.empty())
            return L"null";

        std::wstring s = L"[";
        for (std::size_t i = 0; i < Coordinates.size(); ++i)
        {
            s += L"[";
            s += std::to_wstring(Coordinates[i].latitude);
            s += L",";
            s += std::to_wstring(Coordinates[i].longitude);
            s += L"]";
            if (i + 1 < Coordinates.size()) s += L",";
        }
        s += L"]";
        return s;
    }
}


// =============================================================================
// Conversion GeoJSON
// =============================================================================

JsonDocument TopologyRenderer::convertStraightToFeature(const StraightBlock& straight)
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

JsonDocument TopologyRenderer::convertSwitchToFeature(const SwitchBlock& sw)
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

void TopologyRenderer::exportToFile(const std::string& outputPath)
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

std::wstring TopologyRenderer::loadGeoJsonToWebView()
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

std::wstring TopologyRenderer::escapeForJavaScript(const std::string& input)
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

std::wstring TopologyRenderer::renderStraightBlock(const StraightBlock& straight)
{
    if (straight.getCoordinates().size() < 2)
        return L"";

    std::wstring script = L"renderStraightBlock(\"";
    script += toWide(straight.getId());
    script += L"\",[";

    const auto& Coordinates = straight.getCoordinates();
    for (std::size_t i = 0; i < Coordinates.size(); ++i)
    {
        script += L"[";
        script += std::to_wstring(Coordinates[i].latitude);
        script += L",";
        script += std::to_wstring(Coordinates[i].longitude);
        script += L"]";
        if (i + 1 < Coordinates.size()) script += L",";
    }
    script += L"]);";
    return script;
}

std::wstring TopologyRenderer::renderAllStraightBlocks()
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

std::wstring TopologyRenderer::renderSwitchBlock(const SwitchBlock& sw)
{
    const CoordinateLatLon& junction = sw.getJunctionCoordinate();

    std::wstring script = L"renderSwitch(\"";
    script += toWide(sw.getId());
    script += L"\",";
    script += std::to_wstring(junction.latitude);
    script += L",";
    script += std::to_wstring(junction.longitude);
    script += L");";
    return script;
}

std::wstring TopologyRenderer::renderAllSwitchBlocksJunctions()
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
 *     rootCoordinates,      // [[lat,lon],...] ou null
 *     normalCoordinates,    // [[lat,lon],...] ou null
 *     devCoordinates)       // [[lat,lon],...] ou null
 *
 * Pour les branches simples (non absorbées) : tableau à 1 point [tip].
 * Pour les branches absorbées (double switch) : polyligne complète.
 * null si le tip est absent.
 */
std::wstring TopologyRenderer::renderSwitchBranches(const SwitchBlock& sw)
{
    if (!sw.isOriented()) return L"";

    const CoordinateLatLon& junction = sw.getJunctionCoordinate();

    // --- Branche root : toujours un simple tip (jamais absorbée) ---
    std::wstring rootCoordinates = L"null";
    if (sw.getTipOnRoot())
        rootCoordinates = L"[["
        + std::to_wstring(sw.getTipOnRoot()->latitude) + L","
        + std::to_wstring(sw.getTipOnRoot()->longitude) + L"]]";

    // --- Branche normal ---
    std::wstring normalCoordinates;
    if (!sw.getAbsorbedNormalCoordinates().empty())
    {
        // Double switch : polyligne complète du segment absorbé
        // On skip le premier point (≈ jonction de ce switch, déjà connue côté JS)
        const auto& pts = sw.getAbsorbedNormalCoordinates();
        normalCoordinates = encodePolyline(
            std::vector<CoordinateLatLon>(pts.begin() + (pts.size() > 1 ? 1 : 0), pts.end()));
    }
    else if (sw.getTipOnNormal())
    {
        // Switch simple : un seul point tip
        normalCoordinates = L"[["
            + std::to_wstring(sw.getTipOnNormal()->latitude) + L","
            + std::to_wstring(sw.getTipOnNormal()->longitude) + L"]]";
    }
    else
    {
        normalCoordinates = L"null";
    }

    // --- Branche deviation ---
    std::wstring devCoordinates;
    if (!sw.getAbsorbedDeviationCoordinates().empty())
    {
        const auto& pts = sw.getAbsorbedDeviationCoordinates();
        devCoordinates = encodePolyline(
            std::vector<CoordinateLatLon>(pts.begin() + (pts.size() > 1 ? 1 : 0), pts.end()));
    }
    else if (sw.getTipOnDeviation())
    {
        devCoordinates = L"[["
            + std::to_wstring(sw.getTipOnDeviation()->latitude) + L","
            + std::to_wstring(sw.getTipOnDeviation()->longitude) + L"]]";
    }
    else
    {
        devCoordinates = L"null";
    }

    std::wstring script = L"renderSwitchBranches(\"";
    script += toWide(sw.getId());
    script += L"\",";
    script += std::to_wstring(junction.latitude);
    script += L",";
    script += std::to_wstring(junction.longitude);
    script += L",";
    script += rootCoordinates;
    script += L",";
    script += normalCoordinates;
    script += L",";
    script += devCoordinates;
    script += L");";
    return script;
}

std::wstring TopologyRenderer::renderAllSwitchBranches()
{
    const auto& switches = TopologyRepository::instance().data().switches;

    std::wstring script = L"clearSwitchBranches();";
    for (const auto& sw : switches)
        script += renderSwitchBranches(*sw);
    return script;
}

std::wstring TopologyRenderer::updateSwitchBlocks(const SwitchBlock& sw)
{
    const bool toDeviation = sw.isDeviationActive();

    auto applyState = [&](const std::string& id) -> std::wstring
        {
            return L"window.switchApplyState(\""
                + toWide(id)
                + L"\"," + (toDeviation ? L"true" : L"false") + L");";
        };

    std::wstring script = applyState(sw.getId());

    if (auto* p = sw.getPartnerOnNormal())
        script += applyState(p->getId());
    if (auto* p = sw.getPartnerOnDeviation())
        script += applyState(p->getId());

    return script;
}
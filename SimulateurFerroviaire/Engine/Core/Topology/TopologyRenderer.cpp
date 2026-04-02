/**
 * @file  TopologyRenderer.cpp
 * @brief Implémentation de l'exporteur GeoJSON et du générateur de scripts JS.
 *
 * Modification v2 :
 *  - ScriptBuilder remplace la concaténation naïve wstring+= (allocation amortie).
 *  - renderAllTopology() fusionne les trois anciens renderAll*() en une seule passe.
 *  - encodeTip() supprimé — jamais utilisé.
 *
 * @see TopologyRenderer
 */

#include "TopologyRenderer.h"

#include "Engine/Core/Topology/TopologyRepository.h"

#include <fstream>
#include <iomanip>
#include <sstream>


 // =============================================================================
 // Helpers locaux
 // =============================================================================

namespace
{
    /**
     * Wrapper autour de wostringstream pour la génération de scripts JS.
     *
     * Avantage vs wstring+= : une seule allocation amortie pour toute la
     * séquence de concaténations. fixed/setprecision configurés une fois.
     *
     * Précision 6 décimales ≈ 11 cm — largement suffisant pour Leaflet.
     */
    struct ScriptBuilder
    {
        /**
         * @brief Initialise le stream avec le format de coordonnées fixe.
         */
        ScriptBuilder()
        {
            m_oss << std::fixed << std::setprecision(6);
        }

        /**
         * @brief Ajoute "lat,lon" (sans crochets).
         * @param lat  Latitude WGS84.
         * @param lon  Longitude WGS84.
         * @return *this pour chaînage.
         */
        ScriptBuilder& appendCoord(double lat, double lon)
        {
            m_oss << lat << L"," << lon;
            return *this;
        }

        /**
         * @brief Ajoute "[[lat,lon],...]" ou "null" si vide.
         * @param pts  Polyligne WGS84.
         * @return *this pour chaînage.
         */
        ScriptBuilder& appendPolyline(const std::vector<CoordinateLatLon>& pts)
        {
            if (pts.empty())
            {
                m_oss << L"null";
                return *this;
            }
            m_oss << L"[";
            for (std::size_t i = 0; i < pts.size(); ++i)
            {
                m_oss << L"[" << pts[i].latitude << L"," << pts[i].longitude << L"]";
                if (i + 1 < pts.size()) m_oss << L",";
            }
            m_oss << L"]";
            return *this;
        }

        /**
         * @brief Opérateur de flux générique — délègue au stream interne.
         * @param val  Valeur à sérialiser (wstring, wchar_t*, bool, int…).
         * @return *this pour chaînage.
         */
        template<typename T>
        ScriptBuilder& operator<<(const T& val)
        {
            m_oss << val;
            return *this;
        }

        /**
         * @brief Retourne le script complet sous forme de wstring.
         * @return Chaîne construite — une seule allocation finale.
         */
        [[nodiscard]] std::wstring str() const { return m_oss.str(); }

    private:
        std::wostringstream m_oss;
    };

    /**
     * Convertit un std::string ASCII en std::wstring.
     * Utilisé pour les identifiants de blocs (ASCII garanti — "s/0", "sw/3"…).
     */
    std::wstring toWide(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }

    /**
     * Encode une polyligne WGS-84 en tableau JS : [[lat,lon],[lat,lon],…]
     * Retourne "null" si la polyligne est vide.
     * Délègue à ScriptBuilder pour cohérence du format.
     */
    std::wstring encodePolyline(const std::vector<CoordinateLatLon>& coords)
    {
        ScriptBuilder sb;
        sb.appendPolyline(coords);
        return sb.str();
    }

} // namespace


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
    for (const auto& coord : straight.getPointsWGS84())
        coordinates.push_back({ coord.longitude, coord.latitude });

    feature["geometry"] = {
        { "type",        "LineString" },
        { "coordinates", coordinates  }
    };

    feature["properties"]["neighbour_count"] =
        static_cast<int>(straight.getNeighbourIds().size());
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
            sw.getJunctionWGS84().longitude,
            sw.getJunctionWGS84().latitude
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
// Rendu complet — point d'entrée principal 
// =============================================================================

std::wstring TopologyRenderer::renderAllTopology()
{
    const auto& straights = TopologyRepository::instance().data().straights;
    const auto& switches = TopologyRepository::instance().data().switches;

    // Un seul ScriptBuilder pour l'intégralité du rendu.
    // Ordre : straights → branches switches → jonctions switches.
    // Les jonctions (cercles Leaflet) sont rendues en dernier pour
    // apparaître par-dessus les branches.
    ScriptBuilder sb;

    sb << L"clearStraightBlocks();";
    for (const auto& st : straights)
        sb << renderStraightBlock(*st);
    sb << L"zoomToStraights();";

    sb << L"clearSwitchBranches();";
    for (const auto& sw : switches)
        sb << renderSwitchBranches(*sw);   // retourne "" si non orienté

    sb << L"clearSwitches();";
    for (const auto& sw : switches)
        sb << renderSwitchBlock(*sw);

    return sb.str();
}


// =============================================================================
// Straights — rendu WebView 
// =============================================================================

std::wstring TopologyRenderer::renderStraightBlock(const StraightBlock& straight)
{
    if (straight.getPointsWGS84().size() < 2)
        return L"";

    const auto& pts = straight.getPointsWGS84();
    ScriptBuilder sb;

    sb << L"renderStraightBlock(\"" << toWide(straight.getId()) << L"\",[";
    for (std::size_t i = 0; i < pts.size(); ++i)
    {
        sb << L"[";
        sb.appendCoord(pts[i].latitude, pts[i].longitude);
        sb << L"]";
        if (i + 1 < pts.size()) sb << L",";
    }
    sb << L"]);";

    return sb.str();
}


// =============================================================================
// Switches — jonction (rendu WebView) 
// =============================================================================

std::wstring TopologyRenderer::renderSwitchBlock(const SwitchBlock& sw)
{
    const CoordinateLatLon& j = sw.getJunctionWGS84();

    ScriptBuilder sb;
    sb << L"renderSwitch(\"" << toWide(sw.getId()) << L"\",";
    sb.appendCoord(j.latitude, j.longitude);
    sb << L");";

    return sb.str();
}


// =============================================================================
// Switches — branches (rendu WebView) 
// =============================================================================

std::wstring TopologyRenderer::renderSwitchBranches(const SwitchBlock& sw)
{
    if (!sw.isOriented()) return L"";

    const CoordinateLatLon& j = sw.getJunctionWGS84();

    // --- Branche root : toujours un simple tip (jamais absorbée) ---
    std::wstring rootCoordinates = L"null";
    if (sw.getTipOnRoot())
    {
        ScriptBuilder sb;
        sb << L"[[";
        sb.appendCoord(sw.getTipOnRoot()->latitude, sw.getTipOnRoot()->longitude);
        sb << L"]]";
        rootCoordinates = sb.str();
    }

    // --- Helper : encode une branche (absorbée ou tip simple) ---
    auto encodeBranch = [&](const std::optional<CoordinateLatLon>& tip,
        const std::vector<CoordinateLatLon>& absorbed) -> std::wstring
        {
            if (!absorbed.empty())
            {
                // Double switch : polyligne complète sans le premier point
                // (jonction de ce switch, déjà connue côté JS)
                const auto begin = absorbed.begin() + (absorbed.size() > 1 ? 1 : 0);
                return encodePolyline(std::vector<CoordinateLatLon>(begin, absorbed.end()));
            }
            if (tip)
            {
                ScriptBuilder sb;
                sb << L"[[";
                sb.appendCoord(tip->latitude, tip->longitude);
                sb << L"]]";
                return sb.str();
            }
            return L"null";
        };

    const std::wstring normalCoordinates =
        encodeBranch(sw.getTipOnNormal(), sw.getAbsorbedNormalCoordinates());
    const std::wstring devCoordinates =
        encodeBranch(sw.getTipOnDeviation(), sw.getAbsorbedDeviationCoordinates());

    ScriptBuilder sb;
    sb << L"renderSwitchBranches(\""
        << toWide(sw.getId()) << L"\",";
    sb.appendCoord(j.latitude, j.longitude);
    sb << L"," << rootCoordinates
        << L"," << normalCoordinates
        << L"," << devCoordinates
        << L");";

    return sb.str();
}


// =============================================================================
// Mise à jour état switch (runtime)
// =============================================================================

std::wstring TopologyRenderer::updateSwitchBlocks(const SwitchBlock& sw)
{
    const bool toDeviation = sw.isDeviationActive();

    auto applyState = [&](const std::string& id) -> std::wstring
        {
            ScriptBuilder sb;
            sb << L"window.switchApplyState(\""
                << toWide(id) << L"\","
                << (toDeviation ? L"true" : L"false") << L");";
            return sb.str();
        };

    std::wstring script = applyState(sw.getId());
    if (auto* p = sw.getPartnerOnNormal())    script += applyState(p->getId());
    if (auto* p = sw.getPartnerOnDeviation()) script += applyState(p->getId());

    return script;
}
/**
 * @file  GeoJsonExporter.h
 * @brief Exporteur de topologie ferroviaire au format GeoJSON.
 *
 * @see GeoJsonExporter
 */
#pragma once
#include <string>
#include <vector>

#include "Modules/Models/StraightBlock.h"
#include "Modules/Models/SwitchBlock.h"
#include "Engine/Core/Logger/Logger.h"
#include "External/nlohmann/json.hpp"

using JsonDocument = nlohmann::json;

/**
 * @class GeoJsonExporter
 * @brief Utilitaire statique d'export de la topologie ferroviaire en GeoJSON.
 *
 * Convertit :
 *  - StraightBlock → feature GeoJSON de type LineString.
 *  - SwitchBlock   → feature GeoJSON de type Point.
 *
 * Le fichier résultant est une FeatureCollection GeoJSON valide, lisible
 * par Leaflet, geojson.io ou QGIS.
 *
 * Usage :
 * @code
 *   GeoJsonExporter::exportToFile("output.geojson");
 * @endcode
 */
class GeoJsonExporter
{
public:
    /**
    * @brief Exporte la topologie ferroviaire dans un fichier GeoJSON.
    *
    * Lit les données depuis @ref TopologyRepository.
    *
    * @param outputPath  Chemin du fichier de sortie (ex. "output.geojson").
    */
    static void exportToFile(const std::string& outputPath);

    /**
   * @brief Génère le script JavaScript d'injection GeoJSON dans le WebView.
   *
   *  Lit les données depuis @ref TopologyRepository.
   * 
   * @return Instruction @c window.loadGeoJson(...) prête à être passée à
   *         @c executeScript.
   */
    static std::wstring loadGeoJsonToWebView();

private : 
    /**
     * @brief Convertit un StraightBlock en feature GeoJSON LineString.  
     *
     * Les coordonnées sont émises au format GeoJSON [longitude, latitude].
     *
     * @param straight  StraightBlock à convertir.
     * @return Feature GeoJSON sérialisable.
     */
     static JsonDocument convertStraightToFeature(const StraightBlock& straight);

     /**
     * @brief Convertit un SwitchBlock en feature GeoJSON Point.
     *
     * Les coordonnées sont émises au format GeoJSON [longitude, latitude].
     *
     * @param switchBlock  SwitchBlock à convertir.                   // ← espace parasite supprimé
     * @return Feature GeoJSON sérialisable.
     */
     static JsonDocument convertSwitchToFeature(const SwitchBlock& switchBlock);

     /**
     * @brief Échappe une chaîne pour injection sécurisée dans du JavaScript.
     *
     * @param input  Chaîne à échapper.
     * @return Chaîne échappée prête pour @c executeScript.
     */
     static std::wstring escapeForJavaScript(const std::string& input);

    /** @brief Classe non instanciable — constructeur supprimé. */
    GeoJsonExporter() = delete;
};
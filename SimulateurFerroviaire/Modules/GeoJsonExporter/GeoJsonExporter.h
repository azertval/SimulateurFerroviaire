/**
 * @file  GeoJsonExporter.h
 * @brief Exporteur de topologie ferroviaire au format GeoJSON.
 *
 * @see GeoJsonExporter
 */
#pragma once

#include <string>
#include <vector>

#include "Modules/InteractiveElements/ShuntingElements/StraightBlock.h"
#include "Modules/InteractiveElements/ShuntingElements/SwitchBlock.h"
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
     * @param outputPath  Chemin du fichier de sortie.
     */
    static void exportToFile(const std::string& outputPath);

    /**
     * @brief Génère le script JavaScript d'injection GeoJSON dans le WebView.
     * @return Instruction @c window.loadGeoJson(...) prête pour @c executeScript.
     */
    static std::wstring loadGeoJsonToWebView();

    // -------------------------------------------------------------------------
    // Straights
    // -------------------------------------------------------------------------

    /**
     * @brief Construit l'appel JS @c renderStraightBlock(id, coords) pour un bloc.
     * @param straightBlock  Bloc à rendre (ignoré si < 2 coordonnées).
     * @return Instruction JavaScript.
     */
    static std::wstring renderStraightBlock(const StraightBlock& straightBlock);

    /**
     * @brief Efface puis redessine tous les StraightBlocks.
     * @return Instruction JavaScript.
     */
    static std::wstring renderAllStraightBlocks();

    // -------------------------------------------------------------------------
    // Switches — jonction
    // -------------------------------------------------------------------------

    /**
     * @brief Construit l'appel JS @c renderSwitch(id, lat, lon, isDouble).
     * @param sw  SwitchBlock à rendre.
     * @return Instruction JavaScript.
     */
    static std::wstring renderSwitchBlock(const SwitchBlock& sw);

    /**
     * @brief Efface puis redessine tous les marqueurs de jonction.
     * @return Instruction JavaScript.
     */
    static std::wstring renderAllSwitchBlocksJunctions();

    // -------------------------------------------------------------------------
    // Switches — branches (root / normal / deviation)
    // -------------------------------------------------------------------------

    /**
     * @brief Construit l'appel JS @c renderSwitchBranches(...) pour un switch.
     *
     * Émet les coordonnées de la jonction et des trois tips CDC.
     * Un tip absent (nullopt) est encodé comme @c NaN,NaN et silencieusement
     * ignoré côté JavaScript.
     *
     * Retourne une chaîne vide si le switch n'est pas orienté.
     *
     * @param sw  SwitchBlock orienté.
     * @return Instruction JavaScript.
     */
    static std::wstring renderSwitchBranches(const SwitchBlock& sw);

    /**
     * @brief Efface puis redessine les branches de tous les switches.
     * @return Instruction JavaScript.
     */
    static std::wstring renderAllSwitchBranches();

private:
    /**
    * @brief Convertit un StraightBlock en feature GeoJSON de type LineString.
     * @param straight  Bloc à convertir.
     * @return Objet JSON représentant la feature GeoJSON.
    */
    static JsonDocument convertStraightToFeature(const StraightBlock& straight);

    /**
    * @brief Convertit un SwitchBlock en feature GeoJSON de type Point.
     * @param switchBlock  Bloc à convertir.
     * @return Objet JSON représentant la feature GeoJSON.
    */
    static JsonDocument convertSwitchToFeature(const SwitchBlock& switchBlock);

    /**
    * @brief Échappe une chaîne JSON pour l'injection dans JavaScript.
     * @param input Chaîne JSON brute.
     * @return Chaîne échappée prête à être insérée dans un string JS.
     *
     * Caractères échappés :
     *   - " → \"
     *   - \ → \\
     *   - \n → \\n
     *   - \r → \\r
     *   - \t → \\t
    */
    static std::wstring escapeForJavaScript(const std::string& input);

    /**
    * @brief Calcule le bearing en degrés entre deux points géographiques.
     * @param a Point de départ (latitude, longitude).
     * @param b Point d'arrivée (latitude, longitude).
     * @return Bearing en degrés (0° = nord, 90° = est, 180° = sud, 270° = ouest).
     *
     * Utilise la formule de l'azimut géodésique pour calculer le bearing
     * entre les deux points sur la sphère terrestre.
     *
     * Retourne 0 si les coordonnées sont invalides ou identiques.
    */
    static double bearingDeg(const LatLon& a, const LatLon& b);


    GeoJsonExporter() = delete;
};
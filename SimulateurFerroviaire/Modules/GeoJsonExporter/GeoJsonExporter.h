#pragma once
#include <string>
#include <vector>

#include "Modules/Models/StraightBlock.h"
#include "Modules/Models/SwitchBlock.h"
#include "Engine/Core/Logger/Logger.h"
#include "External/nlohmann/json.hpp"

using JsonDocument = nlohmann::json;

/**
* @brief Utility class utilisée pour exporter la topologie ferroviaire parsée en GeoJSON
* 
* L'exporteur convertit :
*  - StraightBlock en des features GeoJSON de type LineString
*  - SwitchBlock en des features GeoJSON de type Point
* 
* Le fichier résultant est une FeatureCollection GeoJSON valide qui peut être ouverte 
* dans des outils tels que Leaflet, geojson.io ou QGIS.
*/

class GeoJsonExporter
{
public:
    /**
    * @brief Exporte les données de topologie ferroviaire en format GeoJSON
    * 
    * @param straights  Liste des StraightBlock à exporter
    * @param switches   Liste des SwitchBlock à exporter
    * @param outputPath Chemin du fichier de sortie (ex: "output.geojson")
    * @param logger  Fonction de logging pour les messages d'information et d'erreur
    */
    static void exportToFile(const std::string& outputPath);

private : 
    /**
    * @brief Convertit un SwitchBlock en une feature GeoJSON de type Point
    * GeoJSON format [longitude, latitude].
    * 
    * @param straight StraightBlock à convertir
    * @return JsonDocument Représentation GeoJSON de la feature correspondante
    */
     static JsonDocument convertStraightToFeature(const StraightBlock& straight);

     /** 
     * @brieft Convertit un SwitchBlock en une feature GeoJSON de type Point
     * GeoJSON format [longitude, latitude].
     * 
     * @param switch SwitchBlock à convertir
     * @ return JsonDocument Représentation GeoJSON de la feature correspondante
     */
     static JsonDocument convertSwitchToFeature(const SwitchBlock& switchBlock);
};
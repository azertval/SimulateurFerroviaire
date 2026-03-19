#pragma once

/**
 * @file  GraphBuilder.h
 * @brief Phases 1 et 2 du pipeline — chargement GeoJSON et construction du graphe métrique.
 *
 * Phase 1 (loadGeoJsonFile) :
 *   Lit le fichier GeoJSON via la bibliothèque nlohmann/json.
 *   Normalise toutes les géométries en LineStrings simples (éclate les MultiLineString).
 *   Estime la zone UTM optimale pour la zone couverte.
 *
 * Phase 2 (buildTopologyGraph) :
 *   Construit le TopologyGraph : une arête par paire de points consécutifs.
 *   Accroche toutes les coordonnées sur la grille de snap.
 *   Fusionne les nœuds proches via TopologyGraph::mergeCloseNodes().
 *
 * Résultat retourné par build() :
 *   GraphBuildResult contenant le graphe, la zone UTM, et les nœuds frontières.
 *
 * Dépendance externe :
 *   nlohmann/json.hpp — à placer dans External/nlohmann/json.hpp
 *   Télécharger : https://github.com/nlohmann/json/releases (single header)
 */

#include <set>
#include <string>
#include <vector>

#include "./Graph/TopologyGraph.h"
#include "./Enums/GeoParserEnums.h"
#include "Modules/Models/LatLon.h"
#include "Modules/Models/CoordinateXY.h"
#include "Engine/Core/Logger/Logger.h"

/**
 * @brief Données de résultat de la Phase 1+2.
 *
 * Transmises au TopologyExtractor pour la Phase 3+4+5.
 */
struct GraphBuildResult
{
    /** Graphe topologique métrique construit depuis le GeoJSON. */
    TopologyGraph topologyGraph{ ParserDefaultValues::SNAP_GRID_METERS };

    /**
     * Nœuds frontières : jonctions (degré ≥ 3) et terminus (degré 1).
     * Ces nœuds délimitent les blocs StraightBlock lors de la marche du graphe.
     */
    std::set<int> boundaryNodeIds;

    /** Zone UTM estimée depuis les coordonnées du premier feature. */
    int  utmZoneNumber       = 32;

    /** True si les coordonnées sont dans l'hémisphère nord. */
    bool isNorthernHemisphere = true;
};


/**
 * @brief Charge un fichier GeoJSON et construit le TopologyGraph métrique.
 *
 * Un seul appel à build() suffit pour obtenir le GraphBuildResult complet.
 */
class GraphBuilder
{
public:

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Construit un GraphBuilder pour le fichier et les paramètres donnés.
     *
     * @param logger                 Référence au logger du moteur GeoParser.
     * @param geoJsonFilePath        Chemin vers le fichier GeoJSON à parser.
     * @param snapGridMeters         Pas de grille d'accrochage des coordonnées (mètres).
     * @param endpointSnapMeters     Distance de fusion des nœuds quasi-coïncidents (mètres).
     */
    GraphBuilder(Logger&           logger,
                  const std::string& geoJsonFilePath,
                  double             snapGridMeters     = ParserDefaultValues::SNAP_GRID_METERS,
                  double             endpointSnapMeters = ParserDefaultValues::ENDPOINT_SNAP_METERS);


    // -------------------------------------------------------------------------
    // API publique
    // -------------------------------------------------------------------------

    /**
     * @brief Exécute les phases 1 et 2 et retourne le résultat.
     *
     * @return GraphBuildResult contenant le graphe, les nœuds frontières et la zone UTM.
     * @throws InvalidGeoJsonFormatException Si le fichier est absent, malformé ou vide.
     * @throws InvalidTopologyException       Si aucune arête n'est construite.
     */
    GraphBuildResult build();

private:

    // -------------------------------------------------------------------------
    // Membres privés
    // -------------------------------------------------------------------------

    Logger&     m_logger;
    std::string m_geoJsonFilePath;
    double      m_snapGridMeters;
    double      m_endpointSnapMeters;

    // -------------------------------------------------------------------------
    // Structure interne pour les polylignes brutes extraites du GeoJSON
    // -------------------------------------------------------------------------

    /**
     * @brief Polyligne brute extraite d'un feature GeoJSON (avant projection).
     */
    struct RawPolyline
    {
        std::string       featureId;      ///< ID du feature source.
        std::vector<LatLon> wgs84Coords;  ///< Coordonnées WGS-84 (lat, lon).
    };

    // -------------------------------------------------------------------------
    // Phases internes
    // -------------------------------------------------------------------------

    /**
     * @brief Phase 1 — Lit le GeoJSON et extrait les polylignes brutes WGS-84.
     *
     * Utilise nlohmann/json pour parser le fichier.
     * Éclate les MultiLineString en LineStrings simples.
     * Estime la zone UTM depuis la première coordonnée rencontrée.
     *
     * @param[out] rawPolylines          Polylignes extraites du fichier.
     * @param[out] estimatedUtmZone      Zone UTM estimée.
     * @param[out] detectedNorthernHemisphere  True si hémisphère nord.
     */
    void loadGeoJsonFile(std::vector<RawPolyline>& rawPolylines,
                          int&                      estimatedUtmZone,
                          bool&                     detectedNorthernHemisphere);

    /**
     * @brief Phase 2 — Construit le TopologyGraph depuis les polylignes métriques.
     *
     * Pour chaque polyligne brute :
     *   - Projette les coordonnées WGS-84 en métrique UTM.
     *   - Accroche chaque point sur la grille.
     *   - Ajoute une arête par paire de points consécutifs.
     * Appelle mergeCloseNodes() pour fusionner les nœuds quasi-coïncidents.
     *
     * @param rawPolylines           Polylignes brutes issues de Phase 1.
     * @param utmZoneNumber          Zone UTM à utiliser pour la projection.
     * @param isNorthernHemisphere   True pour l'hémisphère nord.
     * @return TopologyGraph construit et fusionné.
     */
    TopologyGraph buildTopologyGraph(const std::vector<RawPolyline>& rawPolylines,
                                      int                             utmZoneNumber,
                                      bool                            isNorthernHemisphere);

    /**
     * @brief Calcule et retourne l'ensemble des nœuds frontières du graphe.
     *
     * Frontières = jonctions (degré ≥ JUNCTION_MINIMUM) ∪ terminus (degré = TERMINUS).
     *
     * @param graph  Graphe topologique construit.
     * @return Ensemble des IDs de nœuds frontières.
     */
    std::set<int> computeBoundaryNodeIds(const TopologyGraph& graph) const;
};

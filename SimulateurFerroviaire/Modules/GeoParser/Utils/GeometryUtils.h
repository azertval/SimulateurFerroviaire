#pragma once

/**
 * @file  GeometryUtils.h
 * @brief Utilitaires géométriques métriques pour le pipeline ferroviaire.
 *
 * Classe statique regroupant toutes les fonctions géométriques partagées :
 *
 *   Projection géographique :
 *     estimateUtmZone()          Estimation de la zone UTM depuis la longitude.
 *     wgs84ToMetricUtm()         Conversion LatLon WGS-84 → CoordinateXY UTM.
 *     metricUtmToWgs84()         Conversion CoordinateXY UTM → LatLon WGS-84.
 *     convertPolylineToMetric()  Conversion d'une polyligne WGS-84 en métrique.
 *     convertPolylineToWgs84()   Conversion d'une polyligne métrique en WGS-84.
 *
 *   Géométrie pure :
 *     snapToMetricGrid()         Accrochage d'une coordonnée sur grille régulière.
 *     pointAtDistanceAlongLine() Interpolation d'un point à distance sur polyligne.
 *     computePolylineLengthMeters() Longueur planaire d'une polyligne.
 *     unsignedAngleBetweenVectors() Angle non-signé (rad) entre deux vecteurs 2-D.
 *     angleBetweenVectorsPiFallback() Variante "pire cas" — retourne π si vecteur nul.
 *
 * Précision de la projection UTM : meilleure que 0.1 m dans un rayon de 100 km
 * autour du méridien central de la zone — suffisant pour un réseau ferroviaire régional.
 */

#include <cmath>
#include <optional>
#include <vector>

#include "../Models/LatLon.h"
#include "../Models/CoordinateXY.h"
#include "../Enums/GeoParserEnums.h"


/**
 * @brief Fonctions géométriques statiques utilisées par le pipeline GeoParser.
 */
class GeometryUtils
{
public:

    // Classe purement statique — pas d'instances
    GeometryUtils() = delete;

    // =========================================================================
    // Projection géographique WGS-84 ↔ UTM
    // =========================================================================

    /**
     * @brief Estime le numéro de zone UTM (1–60) depuis la longitude centre.
     *
     * @param centerLongitude  Longitude centrale de la zone en degrés décimaux.
     * @return Numéro de zone UTM entre 1 et 60.
     */
    static int estimateUtmZone(double centerLongitude);

    /**
     * @brief Retourne le méridien central (degrés) d'une zone UTM.
     * @param utmZoneNumber  Numéro de zone UTM (1–60).
     * @return Longitude du méridien central en degrés décimaux.
     */
    static double utmCentralMeridianDegrees(int utmZoneNumber);

    /**
     * @brief Convertit une coordonnée WGS-84 en mètres UTM.
     *
     * Implémentation de la projection transverse de Mercator (Karney 2011).
     * Précision < 0.1 m à moins de 100 km du méridien central.
     *
     * @param geographicCoord     Coordonnée WGS-84 (lat, lon) à convertir.
     * @param utmZoneNumber       Zone UTM cible (utiliser estimateUtmZone si inconnu).
     * @param isNorthernHemisphere True pour l'hémisphère nord (faux nord = 0 m).
     * @return Coordonnée métrique UTM (x = est, y = nord).
     */
    static CoordinateXY wgs84ToMetricUtm(const LatLon& geographicCoord,
                                          int           utmZoneNumber,
                                          bool          isNorthernHemisphere = true);

    /**
     * @brief Convertit une coordonnée métrique UTM en WGS-84.
     *
     * Inversion de la projection transverse de Mercator.
     *
     * @param metricCoord          Coordonnée métrique UTM (x = est, y = nord).
     * @param utmZoneNumber        Zone UTM source.
     * @param isNorthernHemisphere True pour l'hémisphère nord.
     * @return Coordonnée WGS-84 (lat, lon).
     */
    static LatLon metricUtmToWgs84(const CoordinateXY& metricCoord,
                                    int                 utmZoneNumber,
                                    bool                isNorthernHemisphere = true);

    /**
     * @brief Convertit une polyligne WGS-84 en coordonnées métriques UTM.
     *
     * @param geographicPolyline  Séquence de points WGS-84.
     * @param utmZoneNumber       Zone UTM cible.
     * @param isNorthernHemisphere True pour l'hémisphère nord.
     * @return Polyligne en coordonnées métriques UTM.
     */
    static std::vector<CoordinateXY> convertPolylineToMetric(
        const std::vector<LatLon>& geographicPolyline,
        int                        utmZoneNumber,
        bool                       isNorthernHemisphere = true);

    /**
     * @brief Convertit une polyligne métrique UTM en WGS-84.
     *
     * @param metricPolyline       Séquence de points métriques UTM.
     * @param utmZoneNumber        Zone UTM source.
     * @param isNorthernHemisphere True pour l'hémisphère nord.
     * @return Polyligne en coordonnées WGS-84.
     */
    static std::vector<LatLon> convertPolylineToWgs84(
        const std::vector<CoordinateXY>& metricPolyline,
        int                              utmZoneNumber,
        bool                             isNorthernHemisphere = true);


    // =========================================================================
    // Géométrie pure (sans projection)
    // =========================================================================

    /**
     * @brief Accroche une coordonnée métrique au sommet le plus proche d'une grille.
     *
     * Élimine le bruit flottant entre coordonnées quasi-coïncidentes.
     * Passer gridSizeMeters ≤ 0 pour désactiver l'accrochage.
     *
     * @param x              Abscisse métrique en entrée.
     * @param y              Ordonnée métrique en entrée.
     * @param gridSizeMeters Pas de la grille en mètres.
     * @return Coordonnée accrochée.
     */
    static CoordinateXY snapToMetricGrid(double x, double y, double gridSizeMeters);

    /**
     * @brief Interpole un point à distanceMeters du premier sommet d'une polyligne.
     *
     * - Si distanceMeters ≤ 0 : retourne le premier sommet.
     * - Si distanceMeters > longueur totale : retourne le dernier sommet.
     * - Si la polyligne est vide : retourne nullopt.
     *
     * @param polylineVertices  Sommets ordonnés de la polyligne métrique.
     * @param distanceMeters    Distance d'arc depuis le premier sommet.
     * @return Point interpolé ou nullopt.
     */
    static std::optional<CoordinateXY> pointAtDistanceAlongLine(
        const std::vector<CoordinateXY>& polylineVertices,
        double                           distanceMeters);

    /**
     * @brief Calcule la longueur planaire totale d'une polyligne métrique.
     *
     * Somme des distances euclidiennes entre chaque paire de sommets consécutifs.
     *
     * @param polylineVertices  Sommets de la polyligne.
     * @return Longueur totale en mètres.
     */
    static double computePolylineLengthMeters(const std::vector<CoordinateXY>& polylineVertices);

    /**
     * @brief Calcule l'angle non-signé (radians) entre deux vecteurs 2-D.
     *
     * Retourne 0.0 si l'un des vecteurs a une magnitude nulle (comportement neutre).
     * Pour le comportement "pire cas" (π sur vecteur nul), utiliser
     * angleBetweenVectorsPiFallback().
     *
     * @param firstVector   Premier vecteur 2-D.
     * @param secondVector  Second vecteur 2-D.
     * @return Angle en radians dans [0, π].
     */
    static double unsignedAngleBetweenVectors(const CoordinateXY& firstVector,
                                               const CoordinateXY& secondVector);

    /**
     * @brief Variante "pire cas" de l'angle entre deux vecteurs 2-D.
     *
     * Retourne π si l'un des vecteurs a une magnitude nulle.
     * Garantit qu'une paire contenant un vecteur dégénéré ne sera jamais
     * choisie comme paire de sorties dans l'orientation des aiguillages.
     *
     * Diffère de unsignedAngleBetweenVectors() qui retourne 0.0 (comportement neutre).
     *
     * @param firstVector   Premier vecteur 2-D.
     * @param secondVector  Second vecteur 2-D.
     * @return Angle en radians dans [0, π].
     */
    static double angleBetweenVectorsPiFallback(const CoordinateXY& firstVector,
                                                 const CoordinateXY& secondVector);

private:

    // Constantes de l'ellipsoïde WGS-84
    static constexpr double WGS84_SEMI_MAJOR_AXIS = GeographicProjection::WGS84_SEMI_MAJOR_AXIS;
    static constexpr double WGS84_FLATTENING      = GeographicProjection::WGS84_FLATTENING;
    static constexpr double WGS84_ECCENTRICITY_SQUARED =
        2.0 * GeographicProjection::WGS84_FLATTENING
        - GeographicProjection::WGS84_FLATTENING * GeographicProjection::WGS84_FLATTENING;

    static constexpr double UTM_SCALE_FACTOR  = GeographicProjection::UTM_SCALE_FACTOR;
    static constexpr double UTM_FALSE_EASTING = GeographicProjection::UTM_FALSE_EASTING;
    static constexpr double UTM_FALSE_NORTHING_SOUTH = GeographicProjection::UTM_FALSE_NORTHING_SOUTH;

    static constexpr double PI            = 3.14159265358979323846;
    static constexpr double DEGREES_TO_RADIANS = PI / 180.0;
    static constexpr double RADIANS_TO_DEGREES = 180.0 / PI;
};

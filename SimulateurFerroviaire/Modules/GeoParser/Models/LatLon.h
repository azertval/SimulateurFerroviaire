#pragma once

/**
 * @file  LatLon.h
 * @brief Représentation d'une coordonnée géographique WGS-84 (latitude, longitude).
 *
 * Convention : latitude en premier, longitude en second.
 * Toutes les coordonnées géographiques du pipeline sont exprimées dans ce format.
 *
 * Exemple :
 * @code
 *   LatLon junctionPoint{ 48.8566, 2.3522 };  // Paris
 *   double latitude  = junctionPoint.latitude;
 *   double longitude = junctionPoint.longitude;
 * @endcode
 */

/**
 * @brief Coordonnée géographique WGS-84 exprimée en degrés décimaux.
 */
struct LatLon
{
    double latitude  = 0.0;  ///< Latitude en degrés décimaux (positif = nord).
    double longitude = 0.0;  ///< Longitude en degrés décimaux (positif = est).

    /** Constructeur par défaut — coordonnée à l'origine (0°N, 0°E). */
    LatLon() = default;

    /**
     * @brief Construit une coordonnée avec les valeurs fournies.
     * @param lat  Latitude en degrés décimaux.
     * @param lon  Longitude en degrés décimaux.
     */
    LatLon(double lat, double lon)
        : latitude(lat), longitude(lon)
    {}

    bool operator==(const LatLon& other) const
    {
        return latitude == other.latitude && longitude == other.longitude;
    }

    bool operator!=(const LatLon& other) const
    {
        return !(*this == other);
    }
};

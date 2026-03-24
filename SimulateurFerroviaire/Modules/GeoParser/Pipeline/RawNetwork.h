/**
 * @file  RawNetwork.h
 * @brief Structures de données produites par @ref Phase1_GeoLoader.
 *
 * Contient les polylignes WGS84 brutes issues du GeoJSON et leur
 * projection UTM précalculée — conversion effectuée une seule fois
 * en Phase 1.
 */
#pragma once

#include <string>
#include <vector>

#include "Engine/Core/Coords/LatLon.h"
#include "Engine/Core/Coords/CoordinateXY.h"

 /**
  * @struct RawPolyline
  * @brief Une polyligne GeoJSON avec ses coordonnées en double système.
  *
  * WGS84 conservé pour le rendu Leaflet.
  * UTM précalculé pour tous les calculs métriques des phases suivantes.
  */
struct RawPolyline
{
    /** Identifiant GeoJSON optionnel (@c id ou @c name de la feature). */
    std::string id;

    /** Points WGS84 (latitude, longitude) — conservés pour le rendu. */
    std::vector<LatLon> pointsWGS84;

    /**
     * Points projetés en UTM (x = est, y = nord, en mètres).
     * Même taille que @c pointsWGS84 — index identique.
     */
    std::vector<CoordinateXY> pointsUTM;
};

/**
 * @struct RawNetwork
 * @brief Résultat de @ref Phase1_GeoLoader — ensemble des polylignes du GeoJSON.
 *
 * Produit en Phase 1, consommé par les phases 2 à 4.
 * Libérable après Phase 4 via @c clear().
 */
struct RawNetwork
{
    /** Zone UTM détectée depuis les coordonnées du fichier. */
    std::string utmZone;

    /** Toutes les polylignes extraites du GeoJSON. */
    std::vector<RawPolyline> polylines;

    /** @brief Vide le réseau — libère la mémoire après Phase 4. */
    void clear()
    {
        polylines.clear();
        polylines.shrink_to_fit();
        utmZone.clear();
    }

    /** @return Nombre total de points UTM sur toutes les polylignes. */
    [[nodiscard]] size_t totalPointCount() const
    {
        size_t n = 0;
        for (const auto& p : polylines) n += p.pointsUTM.size();
        return n;
    }
};
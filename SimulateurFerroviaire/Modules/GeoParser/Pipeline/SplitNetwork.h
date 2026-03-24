/**
 * @file  SplitNetwork.h
 * @brief Structures de données produites par @ref Phase3_NetworkSplitter.
 *
 * Contient les segments atomiques — chaque segment ne contient aucune
 * intersection en son milieu. Prêt pour la construction du graphe en Phase 4.
 */
#pragma once

#include <vector>

#include "Engine/Core/Coordinates/CoordinateLatLon.h"
#include "Engine/Core/Coordinates/CoordinateXY.h"

 /**
  * @struct AtomicSegment
  * @brief Segment atomique entre deux nœuds topologiques potentiels.
  *
  * Un segment atomique ne contient aucun point d'intersection en son milieu.
  * Ses deux extrémités (index 0 et dernier) sont candidats à devenir des
  * nœuds du graphe topologique en Phase 4.
  *
  * @par Redondance WGS84/UTM intentionnelle
  * WGS84 est conservé pour le rendu Leaflet (@ref TopologyRenderer).
  * UTM est utilisé pour tous les calculs métriques des phases 4-9.
  * Évite les reconversions répétées.
  */
struct AtomicSegment
{
    /**
     * Points du segment en WGS84 (latitude, longitude).
     * Au minimum 2 points (extrémités). Peut en avoir plus si le segment
     * original avait des points intermédiaires entre deux intersections.
     */
    std::vector<CoordinateLatLon> pointsWGS84;

    /**
     * Points du segment en UTM (x = est, y = nord, mètres).
     * Même taille que @c pointsWGS84 — index identique.
     */
    std::vector<CoordinateXY> pointsUTM;

    /**
     * Indice de la polyligne parente dans @c RawNetwork::polylines.
     * Utilisé par Phase 4 pour la traçabilité et la reconstruction des
     * features GeoJSON d'origine.
     */
    size_t parentPolylineIndex = 0;

    /** @return Longueur planaire du segment (distance A→B en UTM). */
    [[nodiscard]] double lengthUTM() const
    {
        if (pointsUTM.size() < 2) return 0.0;
        double len = 0.0;
        for (size_t i = 0; i + 1 < pointsUTM.size(); ++i)
        {
            const double dx = pointsUTM[i + 1].x - pointsUTM[i].x;
            const double dy = pointsUTM[i + 1].y - pointsUTM[i].y;
            len += std::sqrt(dx * dx + dy * dy);
        }
        return len;
    }

    /** @return Extrémité A du segment (premier point UTM). */
    [[nodiscard]] const CoordinateXY& endpointA() const { return pointsUTM.front(); }

    /** @return Extrémité B du segment (dernier point UTM). */
    [[nodiscard]] const CoordinateXY& endpointB() const { return pointsUTM.back(); }
};

/**
 * @struct SplitNetwork
 * @brief Résultat de @ref Phase3_NetworkSplitter — ensemble des segments atomiques.
 *
 * Produit en Phase 3, consommé par Phase 4.
 * Libérable après Phase 4 via @c clear().
 */
struct SplitNetwork
{
    /** Tous les segments atomiques du réseau. */
    std::vector<AtomicSegment> segments;

    /** @brief Vide le réseau — libère la mémoire après Phase 4. */
    void clear()
    {
        segments.clear();
        segments.shrink_to_fit();
    }

    /** @return Nombre de segments atomiques. */
    [[nodiscard]] size_t size() const { return segments.size(); }

    /** @return @c true si aucun segment n'a été produit. */
    [[nodiscard]] bool empty() const { return segments.empty(); }
};
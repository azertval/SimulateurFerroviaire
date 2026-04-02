/**
 * @file  IntersectionMap.h
 * @brief Structures de données produites par @ref Phase2_GeometricIntersector.
 *
 * Contient les identifiants de segments, les points d'intersection UTM
 * et la grille spatiale de binning utilisée pour O(n·k).
 */
#pragma once

#include <unordered_map>
#include <vector>

#include "Engine/Core/Coordinates/CoordinateXY.h"

 /**
  * @struct SegmentId
  * @brief Identifiant unique d'un segment dans le @ref RawNetwork.
  *
  * Un segment est défini par l'indice de sa polyligne parente et
  * l'indice de son premier point. Le second point est à @c pointIndex+1.
  */
struct SegmentId
{
    size_t polylineIndex = 0; ///< Indice dans @c RawNetwork::polylines.
    size_t pointIndex = 0; ///< Indice du premier point du segment.

    bool operator==(const SegmentId& o) const
    {
        return polylineIndex == o.polylineIndex
            && pointIndex == o.pointIndex;
    }
};

/**
 * @struct IntersectionPoint
 * @brief Point d'intersection entre deux segments, avec paramètres de position.
 *
 * @par Paramètres t et u
 * @c t est la position relative de l'intersection sur le segment source :
 *  - @c t = 0.0 → extrémité A du segment
 *  - @c t = 1.0 → extrémité B du segment
 *  - @c t = 0.5 → milieu du segment
 *
 * Utilisés par @ref Phase3_NetworkSplitter pour interpoler le point de découpe.
 */
struct IntersectionPoint
{
    CoordinateXY point; ///< Coordonnées UTM du point d'intersection.
    double       t = 0; ///< Position relative sur le segment source (0..1).
    double       u = 0; ///< Position relative sur le segment croisé (0..1).
    SegmentId    other; ///< Identifiant du segment croisé.
};

/**
 * @struct GridCell
 * @brief Clé de cellule de la grille spatiale de binning.
 *
 * Identifie une cellule par ses indices colonne/ligne dans la grille UTM.
 */
struct GridCell
{
    int col = 0; ///< Indice de colonne (axe X UTM).
    int row = 0; ///< Indice de ligne  (axe Y UTM).

    bool operator==(const GridCell& o) const
    {
        return col == o.col && row == o.row;
    }
};

/**
 * @struct GridCellHash
 * @brief Fonction de hachage pour @ref GridCell — Cantor pairing.
 *
 * Bijection Z²→Z sans collision pour des indices dans [-10000, 10000].
 */
struct GridCellHash
{
    size_t operator()(const GridCell& c) const
    {
        // Décalage pour gérer les indices négatifs
        const size_t a = static_cast<size_t>(c.col + 10000);
        const size_t b = static_cast<size_t>(c.row + 10000);
        return (a + b) * (a + b + 1) / 2 + b;
    }
};

/**
 * @brief Grille spatiale : cellule UTM → liste des segments la traversant.
 *
 * Construite en Phase 2, utilisée pour limiter les tests d'intersection
 * aux segments candidats (même cellule → potentiellement croisants).
 */
using SpatialGrid = std::unordered_map<GridCell,
    std::vector<SegmentId>,
    GridCellHash>;

/**
 * @struct IntersectionData
 * @brief Résultat de @ref Phase2_GeometricIntersector.
 *
 * Produit en Phase 2, consommé par @ref Phase3_NetworkSplitter.
 * Libérable après Phase 3 via @c clear().
 */
struct IntersectionData
{
    /**
     * Index global du segment → liste de ses points d'intersection.
     * Index global = somme des (taille-1) des polylignes précédentes + pointIndex.
     */
    std::unordered_map<size_t, std::vector<IntersectionPoint>> intersections;

    /** Grille spatiale de binning — réutilisable en Phase 4 pour le snap. */
    SpatialGrid grid;

    /** Taille de cellule utilisée (m UTM). */
    double cellSize = 500.0;

    /** Nombre total d'intersections détectées. */
    size_t totalIntersections = 0;

    /** @brief Vide les données — libère la mémoire après Phase 3. */
    void clear()
    {
        intersections.clear();
        grid.clear();
        totalIntersections = 0;
    }
};
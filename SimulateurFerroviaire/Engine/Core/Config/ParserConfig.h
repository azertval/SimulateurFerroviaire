/**
 * @file  ParserConfig.h
 * @brief Paramètres de configuration du pipeline GeoParser.
 *
 * Struct POD pur — aucune dépendance externe.
 * Copiable librement, passé par valeur dans @ref GeoParser
 * pour garantir l'immutabilité pendant un parsing.
 */
#pragma once

 /**
  * @struct ParserConfig
  * @brief Configuration complète du pipeline GeoParser — POD sans logique.
  */
struct ParserConfig
{
    // =========================================================================
    // [Topology]
    // =========================================================================

    /**
     * @brief Tolérance de snap/fusion des nœuds en mètres (UTM).
     *
     * Deux nœuds distants de moins de @c snapTolerance sont fusionnés
     * en un seul nœud topologique.
     *
     * @par Valeur typique
     * 3.0 m.
     */
    double snapTolerance = 3.0;

    /**
     * @brief Longueur maximale d'un segment avant découpe automatique (m).
     *
     * @par Valeur typique
     * 1000.0 m.
     */
    double maxSegmentLength = 1000.0;

    // =========================================================================
    // [Intersection]
    // =========================================================================

    /**
     * @brief Tolérance epsilon pour la détection d'intersection géométrique (m, UTM).
     *
     * @par Valeur typique
     * 1.5 m — inférieur à snapTolerance pour éviter les faux positifs.
     */
    double intersectionEpsilon = 1.5;

    // =========================================================================
    // [Switch]
    // =========================================================================

    /**
     * @brief Angle minimal (degrés) pour qu'une bifurcation soit classée
     *        comme aiguillage (SwitchBlock).
     *
     * @par Valeur typique
     * 15.0°.
     */
    double minSwitchAngle = 15.0;

    /**
     * @brief Marge de trim junction/straight en mètres.
     *
     * Longueur retirée à chaque extrémité d'un @ref StraightBlock adjacent
     * à un switch pour éviter le chevauchement visuel avec la jonction.
     *
     * @par Valeur typique
     * 25.0 m.
     */
    double junctionTrimMargin = 25.0;

    /**
     * @brief Rayon de détection du segment de liaison double switch (m).
     *
     * @par Valeur typique
     * 100.0 m.
     */
    double doubleSwitchRadius = 100.0;

    /**
     * @brief Longueur des branches CDC de l'aiguillage depuis la jonction (m).
     *
     * Définit la distance à laquelle les tips CDC (root, normal, deviation)
     * sont interpolés sur la géométrie WGS84 de chaque branche.
     * Correspond à la longueur physique d'un côté de l'aiguille.
     *
     * @par Valeur typique
     * 25.0 m — longueur standard d'un côté d'aiguille ferroviaire.
     */
    double switchSideSize = 25.0;

    // =========================================================================
    // [CDC]
    // =========================================================================

    /**
     * @brief Longueur minimale d'une branche switch pour validation CDC (m).
     *
     * @par Valeur typique
     * 25.0 m.
     */
    double minBranchLength = 25.0;
};
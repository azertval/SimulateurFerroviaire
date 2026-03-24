/**
 * @file  ParserConfig.h
 * @brief Paramètres de configuration du pipeline GeoParser.
 *
 * Struct POD pur — aucune dépendance externe.
 * Copiable librement, passé par valeur dans @ref GeoParser
 * pour garantir l'immutabilité pendant un parsing.
 *
 * Les valeurs par défaut correspondent à un réseau ferroviaire standard
 * (voies métriques / standard gauge, données OSM).
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
     * en un seul nœud topologique. Compense les imprécisions de
     * numérisation OSM.
     *
     * @par Valeur typique
     * 3.0 m — couvre les erreurs GPS courantes sans fusionner des nœuds distincts.
     */
    double snapTolerance = 3.0;

    /**
     * @brief Longueur maximale d'un segment avant découpe automatique (m).
     *
     * Les polylignes plus longues que cette valeur sont découpées en
     * segments atomiques de longueur ≤ @c maxSegmentLength.
     * Permet de détecter les intersections sur de longs segments droits.
     *
     * @par Valeur typique
     * 1000.0 m — 1 km.
     */
    double maxSegmentLength = 1000.0;

    // =========================================================================
    // [Intersection]
    // =========================================================================

    /**
     * @brief Tolérance epsilon pour la détection d'intersection géométrique (m, UTM).
     *
     * Deux segments sont considérés comme se croisant si leur point
     * d'intersection calculé est à moins de @c intersectionEpsilon de
     * chacun d'eux. Absorbe les erreurs numériques de floating point.
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
     * Une bifurcation dont l'angle entre les branches est inférieur à
     * @c minSwitchAngle est ignorée (bruit topologique).
     *
     * @par Valeur typique
     * 15.0° — exclut les micro-déviations géométriques.
     */
    double minSwitchAngle = 15.0;

    /**
     * @brief Marge de trim junction/straight en mètres.
     *
     * Longueur retirée à chaque extrémité d'un @ref StraightBlock adjacent
     * à un switch, pour éviter le chevauchement visuel avec la jonction.
     *
     * @par Valeur typique
     * 25.0 m.
     */
    double junctionTrimMargin = 25.0;

    /**
     * @brief Rayon de détection du segment de liaison double switch (m).
     *
     * Deux switches distants de moins de @c doubleSwitchRadius et reliés
     * par un segment court sont candidats à la détection double aiguille.
     *
     * @par Valeur typique
     * 50.0 m.
     */
    double doubleSwitchRadius = 50.0;

    // =========================================================================
    // [CDC]
    // =========================================================================

    /**
     * @brief Longueur minimale d'une branche switch pour validation CDC (m).
     *
     * Une branche plus courte génère un WARNING de validation CDC.
     * Ne bloque pas le parsing.
     *
     * @par Valeur typique
     * 100.0 m.
     */
    double minBranchLength = 100.0;
};

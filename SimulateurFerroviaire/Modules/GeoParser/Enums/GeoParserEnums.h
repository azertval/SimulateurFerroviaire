#pragma once

/**
 * @file  GeoParserEnums.h
 * @brief Constantes nommées et énumérations du pipeline ferroviaire GeoParser.
 *
 * Ce fichier centralise toutes les valeurs numériques littérales ("magic numbers")
 * du pipeline GeoJSON ferroviaire. Aucune dépendance extérieure requise.
 *
 * Groupes :
 *   NodeDegreeThresholds   — seuils de degré topologique des nœuds du graphe
 *   ParserDefaultValues    — valeurs par défaut des paramètres de GeoParser
 *   GeographicProjection   — codes EPSG utilisés
 *   TopologySentinel       — valeurs sentinelles de la topologie
 *   GeometricTolerances    — tolérances numériques des calculs géométriques
 */

// =============================================================================
// NodeDegreeThresholds — seuils de degré topologique
// =============================================================================

/**
 * @brief Seuils de degré utilisés pour classifier les nœuds du graphe topologique.
 *
 * Le degré d'un nœud est le nombre d'arêtes qui lui sont incidentes.
 */
class NodeDegreeThresholds
{
public:
    /** Degré 1 — extrémité de voie sans continuation (terminus). */
    static constexpr int TERMINUS                     = 1;

    /** Degré 2 — nœud de passage sans divergence. */
    static constexpr int PASS_THROUGH                 = 2;

    /** Degré minimal pour qu'un nœud soit classifié comme jonction (aiguillage). */
    static constexpr int JUNCTION_MINIMUM             = 3;

    /** Nombre exact de branches d'un aiguillage standard à 3 voies. */
    static constexpr int SWITCH_PORT_COUNT            = 3;

    /**
     * Nombre de branches partagées entre deux aiguillages formant un croisement.
     * Si deux aiguillages partagent exactement 2 branches → croisement mécanique.
     */
    static constexpr int CROSSOVER_SHARED_BRANCH_COUNT = 2;

    /** Taille minimale d'un cluster d'aiguillages pour être qualifié de double aiguille. */
    static constexpr int DOUBLE_SWITCH_MINIMUM_CLUSTER = 2;
};


// =============================================================================
// ParserDefaultValues — valeurs par défaut des paramètres de GeoParser
// =============================================================================

/**
 * @brief Valeurs par défaut des paramètres de construction de GeoParser.
 *
 * Calibrées pour un réseau ferroviaire typique en projection WGS-84.
 * Représentent un compromis entre robustesse au bruit GPS et précision topologique.
 */
class ParserDefaultValues
{
public :
    /**
     * Pas de grille d'accrochage des coordonnées brutes (mètres).
     * Élimine le bruit flottant entre segments quasi-coïncidents.
     */
    static constexpr double SNAP_GRID_METERS              = 0.2;

    /**
     * Distance de fusion pour les extrémités de segments quasi-jointifs (mètres).
     * Compense les micro-écarts dans les fichiers GeoJSON source.
     */
    static constexpr double ENDPOINT_SNAP_METERS          = 3.0;

    /**
     * Longueur maximale d'un bloc Straight avant découpe automatique (mètres).
     * Au-delà, le Straight est subdivisé en morceaux de longueur égale.
     */
    static constexpr double MAX_STRAIGHT_LENGTH_METERS    = 270.0;

    /**
     * Longueur minimale requise pour chaque branche d'un aiguillage (mètres).
     * En dessous, la branche est signalée comme invalide par la validation CDC.
     */
    static constexpr double MIN_BRANCH_LENGTH_METERS      = 15.0;

    /**
     * Longueur maximale du segment de liaison interne d'un double aiguille (mètres).
     * Au-delà, deux aiguillages adjacents sont considérés indépendants.
     */
    static constexpr double DOUBLE_LINK_MAX_METERS        = 50.0;

    /**
     * Distance depuis la jonction pour interpoler les points tip CDC (mètres).
     * Ces points servent aux vérifications d'écartement de voies.
     */
    static constexpr double BRANCH_TIP_DISTANCE_METERS    = 15.0;

    /** Si true, le parseur écrit des traces DEBUG détaillées en fin de pipeline. */
    static constexpr bool   ENABLE_FULL_DEBUG_MODE         = false;
};


// =============================================================================
// GeographicProjection — codes EPSG
// =============================================================================

/**
 * @brief Codes de projection géographique utilisés dans le pipeline.
 */
class GeographicProjection
{
public :
    /** Code EPSG du système WGS-84 (coordonnées géographiques lat/lon). */
    static constexpr int  WGS84_EPSG_CODE     = 4326;

    /** Faux est UTM (décalage est standard de la projection transverse de Mercator). */
    static constexpr double UTM_FALSE_EASTING = 500000.0;

    /** Faux nord pour l'hémisphère sud (mètres). */
    static constexpr double UTM_FALSE_NORTHING_SOUTH = 10000000.0;

    /** Facteur d'échelle central de la projection UTM. */
    static constexpr double UTM_SCALE_FACTOR  = 0.9996;

    /** Grand demi-axe de l'ellipsoïde WGS-84 (mètres). */
    static constexpr double WGS84_SEMI_MAJOR_AXIS = 6378137.0;

    /** Aplatissement de l'ellipsoïde WGS-84. */
    static constexpr double WGS84_FLATTENING     = 1.0 / 298.257223563;
};


// =============================================================================
// TopologySentinel — valeurs sentinelles
// =============================================================================

/**
 * @brief Valeurs sentinelles utilisées dans les structures topologiques.
 */
class TopologySentinel
{
public :
    /**
     * Index de nœud sentinelle pour les joints internes entre morceaux
     * (chunks) d'un Straight découpé. Pas de nœud réel dans le graphe.
     */
    static constexpr int INTERNAL_CHUNK_NODE  = -1;

    /** Valeur signalant l'absence d'un bloc voisin (slot vide). */
    static constexpr int ABSENT_NEIGHBOUR     = -1;
};


// =============================================================================
// GeometricTolerances — tolérances numériques
// =============================================================================

/**
 * @brief Tolérances pour les comparaisons numériques en virgule flottante.
 *
 * Évitent les divisions par zéro et les artefacts de précision lors des
 * calculs géométriques et vectoriels.
 */
class GeometricTolerances
{
public :
    /**
     * Longueur en dessous de laquelle un segment est considéré dégénéré (quasi-point).
     * Utilisé dans pointAtDistance pour ignorer les segments nuls.
     */
    static constexpr double DEGENERATE_SEGMENT_METERS = 1e-9;

    /**
     * Magnitude en dessous de laquelle un vecteur 2-D est considéré nul.
     * Utilisé dans les calculs d'angle et d'orientation des aiguillages.
     */
    static constexpr double ZERO_VECTOR_MAGNITUDE     = 1e-12;

    /**
     * Longueur en dessous de laquelle une polyligne métrique est considérée vide.
     * Utilisé dans l'interpolation des points tip CDC.
     */
    static constexpr double EMPTY_LINE_LENGTH_METERS  = 1e-6;
};

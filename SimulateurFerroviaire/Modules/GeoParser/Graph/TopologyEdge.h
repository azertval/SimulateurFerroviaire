#pragma once

/**
 * @file  TopologyEdge.h
 * @brief Arête non-orientée du graphe planaire de topologie ferroviaire.
 *
 * Chaque TopologyEdge représente un segment de voie ferrée entre deux nœuds
 * du graphe. Elle est créée lors de la Phase 2 (construction du graphe).
 *
 * La géométrie est stockée en coordonnées métriques UTM (CoordinateXY).
 * La longueur planaire est calculée automatiquement depuis la géométrie.
 */

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "Modules/Models/CoordinateXY.h"


/**
 * @brief Arête non-orientée entre deux nœuds du graphe topologique.
 */
class TopologyEdge
{
public:

    // -------------------------------------------------------------------------
    // Données publiques
    // -------------------------------------------------------------------------

    /** Identifiant unique (ex. "e/0", "e/1"). */
    std::string id;

    /** Index du nœud de départ dans la liste des positions. */
    int startNodeIndex = -1;

    /** Index du nœud d'arrivée dans la liste des positions. */
    int endNodeIndex = -1;

    /**
     * Polyligne métrique de l'arête (au moins 2 points).
     * Exprimée en coordonnées UTM (CoordinateXY).
     */
    std::vector<CoordinateXY> geometry;

    /**
     * Longueur planaire en mètres (calculée depuis geometry).
     * Mise à jour automatiquement par recomputeLength().
     */
    double lengthMeters = 0.0;


    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /** Constructeur par défaut. */
    TopologyEdge() = default;

    /**
     * @brief Construit une arête complète avec calcul automatique de longueur.
     *
     * @param edgeId         Identifiant unique de l'arête.
     * @param startIndex     Index du nœud de départ.
     * @param endIndex       Index du nœud d'arrivée.
     * @param edgeGeometry   Polyligne métrique de l'arête.
     */
    TopologyEdge(std::string               edgeId,
                  int                       startIndex,
                  int                       endIndex,
                  std::vector<CoordinateXY> edgeGeometry);


    // -------------------------------------------------------------------------
    // Méthodes publiques
    // -------------------------------------------------------------------------

    /**
     * @brief Recalcule et met à jour lengthMeters depuis la géométrie actuelle.
     *
     * À appeler si geometry est modifié après construction.
     */
    void recomputeLength();

    /**
     * @brief Retourne une représentation textuelle pour le débogage.
     *
     * Format : TopologyEdge(id=e/0, 3<->7, len=12.4m)
     *
     * @return Chaîne de représentation.
     */
    std::string toString() const;

private:

    /**
     * @brief Calcule la longueur planaire de geometry.
     * @return Somme des distances euclidiennes entre sommets consécutifs.
     */
    double computePlanarLength() const;
};

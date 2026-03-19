#pragma once

/**
 * @file  StraightBlock.h
 * @brief Modèle de domaine d'un bloc de voie droite (Straight).
 *
 * Un StraightBlock représente un tronçon rectiligne de voie ferrée compris
 * entre deux nœuds frontières (jonctions ou terminus) du graphe topologique.
 *
 * Identifiants :
 *   - Format standard :   "s/0", "s/1", …
 *   - Format morceau :    "s/0_c1", "s/0_c2", … (après découpe Phase 5a)
 *
 * La longueur géodésique est calculée automatiquement à la construction
 * via la formule de Haversine sur les coordonnées WGS-84.
 */

#include <sstream>
#include <string>
#include <vector>

#include "LatLon.h"


/**
 * @brief Bloc de voie droite produit par le parseur GeoJSON.
 *
 * Cycle de vie :
 *   Phase 4 (extraction) — créé avec coordonnées WGS-84 et longueur calculée.
 *   Phase 5a (découpe)   — peut être subdivisé ; les morceaux héritent du suffixe _cN.
 *   Phase 5b (câblage)   — neighbours peuplé avec les IDs adjacents.
 */
class StraightBlock
{
public:

    // -------------------------------------------------------------------------
    // Données publiques
    // -------------------------------------------------------------------------

    /** Identifiant unique (ex. "s/0", "s/0_c1" pour les morceaux). */
    std::string id;

    /**
     * Polyligne WGS-84 ordonnée du bloc.
     * Premier point : extrémité de départ. Dernier point : extrémité d'arrivée.
     */
    std::vector<LatLon> coordinates;

    /**
     * Identifiants des blocs adjacents (autres StraightBlock ou SwitchBlock).
     * Peuplé par la Phase 5b (câblage topologique).
     */
    std::vector<std::string> neighbourIds;

    /**
     * Longueur géodésique en mètres.
     * Calculée automatiquement à la construction via la formule de Haversine.
     */
    double lengthMeters = 0.0;


    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /** Constructeur par défaut. */
    StraightBlock() = default;

    /**
     * @brief Construit un StraightBlock avec ses coordonnées.
     *
     * La longueur géodésique est calculée immédiatement à partir de
     * l'ensemble des coordonnées fournies.
     *
     * @param blockId      Identifiant unique du bloc.
     * @param blockCoords  Polyligne WGS-84 ordonnée (≥ 2 points pour une longueur non nulle).
     * @param initialNeighbourIds  Identifiants des voisins connus à la construction (optionnel).
     */
    StraightBlock(std::string                blockId,
                  std::vector<LatLon>        blockCoords,
                  std::vector<std::string>   initialNeighbourIds = {});

    // -------------------------------------------------------------------------
    // Méthodes publiques
    // -------------------------------------------------------------------------

    /**
     * @brief Recalcule et met à jour lengthMeters depuis les coordonnées actuelles.
     *
     * À appeler si coordinates est modifié après construction.
     */
    void recomputeGeodesicLength();

    /**
     * @brief Retourne une représentation textuelle du bloc pour le débogage.
     *
     * Format : Straight(id=s/0, len=342.5m, coords=7, neighbours=[sw/0, sw/1])
     *
     * @return Chaîne de représentation.
     */
    std::string toString() const;

private:

    /**
     * @brief Calcule la longueur géodésique totale via la formule de Haversine.
     *
     * Somme des distances Haversine entre chaque paire de points consécutifs
     * de la polyligne.
     *
     * @return Longueur en mètres.
     */
    double computeGeodesicLength() const;

    /**
     * @brief Calcule la distance de Haversine entre deux points WGS-84.
     *
     * Précision sub-métrique pour les distances inférieures à quelques centaines
     * de kilomètres — largement suffisant pour un réseau ferroviaire.
     *
     * @param pointA  Premier point WGS-84.
     * @param pointB  Second point WGS-84.
     * @return Distance en mètres.
     */
    static double haversineDistanceMeters(const LatLon& pointA, const LatLon& pointB);
};

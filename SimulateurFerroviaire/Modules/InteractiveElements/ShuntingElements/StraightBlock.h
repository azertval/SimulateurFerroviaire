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
 * Hiérarchie :
 *   InteractiveElement → ShuntingElement → StraightBlock
 */

#include <sstream>
#include <string>
#include <vector>

#include "ShuntingElement.h"
#include "Modules/Coordinates/LatLon.h"


 /**
  * @brief Bloc de voie droite produit par le parseur GeoJSON.
  *
  */
class StraightBlock : public ShuntingElement
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
     * @param blockId             Identifiant unique du bloc.
     * @param blockCoords         Polyligne WGS-84 ordonnée (≥ 2 points pour une longueur non nulle).
     * @param initialNeighbourIds Identifiants des voisins connus à la construction (optionnel).
     */
    StraightBlock(std::string              blockId,
        std::vector<LatLon>      blockCoords,
        std::vector<std::string> initialNeighbourIds = {});


    // -------------------------------------------------------------------------
    // Interface ShuntingElement — implémentation
    // -------------------------------------------------------------------------

    /** @brief Retourne l'identifiant unique du bloc (ex. "s/0"). */
    [[nodiscard]] std::string getId() const override { return id; }

    /** @brief Retourne le type STRAIGHT. */
    [[nodiscard]] InteractiveElementType getType() const override
    {
        return InteractiveElementType::STRAIGHT;
    }

    /**
     * @brief Retourne l'état opérationnel courant du bloc.
     * @return m_state (FREE par défaut).
     */
    [[nodiscard]] ShuntingState getState() const override { return m_state; }

    /**
     * @brief Met à jour l'état opérationnel du bloc.
     * @param state Nouvel état.
     */
    void setState(ShuntingState state) { m_state = state; }


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

    /** État opérationnel courant (FREE par défaut). */
    ShuntingState m_state = ShuntingState::FREE;

    /**
     * @brief Calcule la longueur géodésique totale via la formule de Haversine.
     * @return Longueur en mètres.
     */
    double computeGeodesicLength() const;

    /**
     * @brief Calcule la distance de Haversine entre deux points WGS-84.
     * @param pointA  Premier point WGS-84.
     * @param pointB  Second point WGS-84.
     * @return Distance en mètres.
     */
    static double haversineDistanceMeters(const LatLon& pointA, const LatLon& pointB);
};
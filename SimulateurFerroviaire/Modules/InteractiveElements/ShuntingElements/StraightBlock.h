#pragma once

/**
 * @file  StraightBlock.h
 * @brief Modèle de domaine d'un bloc de voie droite (Straight).
 *
 * Encapsulation : tous les champs sont privés. Les mutations passent par des
 * méthodes à intent explicite, appelées uniquement par la phase du pipeline
 * qui en a la charge :
 *   Phase 3/4 → constructeur
 *   Phase 5b  → addNeighbourId / replaceNeighbourId
 *   Phase 6d  → setCoordinates  (retaille le Straight côté jonction)
 *
 * Identifiants :
 *   Format standard : "s/0", "s/1", …
 *   Format morceau  : "s/0_c1", "s/0_c2", … (après découpe Phase 5a)
 */

#include <string>
#include <vector>

#include "ShuntingElement.h"
#include "Modules/Coordinates/LatLon.h"


class StraightBlock : public ShuntingElement
{
public:

    // =========================================================================
    // Construction
    // =========================================================================

    StraightBlock() = default;

    /**
     * @brief Construit un StraightBlock et calcule immédiatement sa longueur géodésique.
     * @param blockId             Identifiant unique (ex. "s/0").
     * @param blockCoords         Polyligne WGS-84 ordonnée (≥ 2 points).
     * @param initialNeighbourIds Voisins connus à la construction (optionnel).
     */
    StraightBlock(std::string              blockId,
        std::vector<LatLon>      blockCoords,
        std::vector<std::string> initialNeighbourIds = {});


    // =========================================================================
    // Interface ShuntingElement
    // =========================================================================

    [[nodiscard]] std::string            getId()    const override { return m_id; }
    [[nodiscard]] InteractiveElementType getType()  const override { return InteractiveElementType::STRAIGHT; }
    [[nodiscard]] ShuntingState          getState() const override { return m_state; }

    void setState(ShuntingState state) { m_state = state; }


    // =========================================================================
    // Requêtes
    // =========================================================================

    /** Polyligne WGS-84 ordonnée. Premier point : extrémité A. Dernier : extrémité B. */
    [[nodiscard]] const std::vector<LatLon>& getCoordinates()  const { return m_coordinates; }

    /**
     * Identifiants des blocs adjacents (StraightBlock ou SwitchBlock).
     * Toujours triés lexicographiquement — addNeighbourId maintient l'ordre.
     */
    [[nodiscard]] const std::vector<std::string>& getNeighbourIds() const { return m_neighbourIds; }

    /** Longueur géodésique en mètres (Haversine), mise à jour par setCoordinates(). */
    [[nodiscard]] double getLengthMeters() const { return m_lengthMeters; }

    /**
     * @brief Représentation textuelle pour le débogage.
     * Format : Straight(id=s/0, len=342.5m, coords=7, neighbours=[sw/0, sw/1])
     */
    [[nodiscard]] std::string toString() const;


    // =========================================================================
    // Mutations — Phase 5b
    // =========================================================================

    /**
     * @brief Ajoute un voisin en maintenant l'ordre lexicographique.
     * Pas de doublon.
     */
    void addNeighbourId(const std::string& id);

    /**
     * @brief Remplace un ID de voisin par un autre.
     * Utilisé en Phase 7 lors de l'absorption du segment de liaison.
     */
    void replaceNeighbourId(const std::string& oldId, const std::string& newId);


    // =========================================================================
    // Mutations — Phase 6d
    // =========================================================================

    /**
     * @brief Remplace la polyligne et recalcule la longueur géodésique.
     *
     * Appelé par trimStraightOverlaps() pour retirer le chevauchement
     * entre la branche du switch et le début (ou la fin) du Straight.
     */
    void setCoordinates(std::vector<LatLon> coords);

private:

    std::string              m_id;
    std::vector<LatLon>      m_coordinates;
    std::vector<std::string> m_neighbourIds;   ///< Toujours trié.
    double                   m_lengthMeters = 0.0;
    ShuntingState            m_state = ShuntingState::FREE;

    [[nodiscard]] double computeGeodesicLength() const;
    static double haversineDistanceMeters(const LatLon& a, const LatLon& b);
};
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
#include "Engine/Core/Coords/LatLon.h"


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

    /**
     * @brief Voisins topologiques d'un StraightBlock.
     *
     * Un straight est toujours borné par exactement deux extrémités.
     * Chaque extrémité est soit un SwitchBlock, soit un autre StraightBlock
     * (morceau découpé), soit nullptr (terminus).
     */
    struct StraightNeighbours
    {
        /** Bloc adjacent à l'extrémité A (front des coordonnées). */
        ShuntingElement* prev = nullptr;

        /** Bloc adjacent à l'extrémité B (back des coordonnées). */
        ShuntingElement* next = nullptr;
    };

    /**
     * @brief Enregistre les pointeurs prev/next après parsing.
     * @param neighbours  Struct contenant les deux extrémités résolues.
     */
    void setNeighbourPointers(StraightNeighbours neighbours);

    /** @brief Retourne les voisins résolus. Nullptr si non encore initialisé. */
    [[nodiscard]] const StraightNeighbours& getNeighbours() const { return m_neighbours; }

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
    /**
     * Polyligne WGS-84 ordonnée du bloc.
     * Premier point : extrémité A. Dernier point : extrémité B.
     * Modifiable uniquement via setCoordinates() (Phase 6d) — recalcule automatiquement m_lengthMeters.
     */
    std::vector<LatLon> m_coordinates;

    /**
     * IDs des blocs adjacents (StraightBlock ou SwitchBlock).
     * Toujours trié lexicographiquement — addNeighbourId() maintient l'invariant.
     * Peuplé en Phase 5b, muté en Phase 7 via replaceNeighbourId().
     */
    std::vector<std::string> m_neighbourIds;

    /**
     * Pointeurs non-propriétaires vers les blocs adjacents (SwitchBlock ou StraightBlock).
     * Parallèle à m_neighbourIds — même ordre, même contenu sous forme de pointeurs.
     * Propriété du TopologyRepository — ne pas delete.
     */
    StraightNeighbours m_neighbours;

    /**
     * Longueur géodésique totale en mètres, calculée par Haversine.
     * Mise à jour automatiquement à la construction et à chaque appel à setCoordinates().
     * Ne pas modifier directement — utiliser setCoordinates() pour garantir la cohérence.
     */
    double m_lengthMeters = 0.0;

    /**
     * @brief Calcule la longueur géodésique totale depuis m_coordinates.
     *
     * Somme des distances Haversine entre chaque paire de points consécutifs.
     * Retourne 0 si m_coordinates contient moins de 2 points.
     */
    [[nodiscard]] double computeGeodesicLength() const;

    /**
     * @brief Calcule la distance de Haversine entre deux points WGS-84.
     *
     * Formule exacte sur sphère de rayon 6 371 000 m.
     * Utilisée en interne par computeGeodesicLength().
     *
     * @param a  Premier point (latitude, longitude en degrés décimaux).
     * @param b  Second point.
     * @return   Distance en mètres.
     */
    static double haversineDistanceMeters(const LatLon& a, const LatLon& b);
};
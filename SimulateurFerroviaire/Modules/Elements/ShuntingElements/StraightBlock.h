/**
 * @file  StraightBlock.h
 * @brief Modèle de domaine d'un bloc de voie droite (Straight).
 *
 * Identifiants :
 *   Format standard : "s/0", "s/1", …
 *   Format morceau  : "s/0_c1", "s/0_c2", … (après découpe)
 *
 * Système de coordonnées :
 *   WGS84 — conservé pour le rendu Leaflet (TopologyRenderer).
 *   UTM   — utilisé pour tous les calculs métriques du pipeline.
 */
#pragma once

#include <string>
#include <vector>

#include "ShuntingElement.h"
#include "Engine/Core/Coordinates/CoordinateLatLon.h"
#include "Engine/Core/Coordinates/CoordinateXY.h"


class StraightBlock : public ShuntingElement
{
public:

    // =========================================================================
    // Construction
    // =========================================================================

    StraightBlock() = default;

    /**
     * @brief Construit un StraightBlock avec géométrie WGS84 et calcule sa longueur.
     *
     * @param blockId          Identifiant unique (ex. "s/0").
     * @param pointsWGS84      Polyligne WGS-84 ordonnée (≥ 2 points).
     * @param neighbourIds     Voisins connus à la construction (optionnel).
     */
    StraightBlock(std::string                   blockId,
        std::vector<CoordinateLatLon> pointsWGS84,
        std::vector<std::string>      neighbourIds = {});

    // =========================================================================
    // Interface ShuntingElement
    // =========================================================================

    [[nodiscard]] std::string            getId()    const override { return m_id; }
    [[nodiscard]] ElementType getType()  const override { return ElementType::STRAIGHT; }
    [[nodiscard]] ShuntingState          getState() const override { return m_state; }

    void setState(ShuntingState state) { m_state = state; }

    // =========================================================================
    // Requêtes géométriques
    // =========================================================================

    /**
     * @brief Polyligne WGS-84 ordonnée.
     * Premier point = extrémité A. Dernier point = extrémité B.
     */
    [[nodiscard]] const std::vector<CoordinateLatLon>& getPointsWGS84() const { return m_pointsWGS84; }

    /**
     * @brief Polyligne projetée en UTM (x = est, y = nord, mètres).
     * Même taille et même index que getPointsWGS84().
     * Vide si non renseigné par le pipeline v2.
     */
    [[nodiscard]] const std::vector<CoordinateXY>& getPointsUTM() const { return m_pointsUTM; }

    /**
     * @brief Référence modifiable sur la polyligne UTM.
     * Utilisée par Phase8_SwitchOrientator::trimStraightOverlaps().
     */
    [[nodiscard]] std::vector<CoordinateXY>& getPointsUTMRef() { return m_pointsUTM; }

    /**
     * @brief Longueur géodésique Haversine en mètres (depuis pointsWGS84).
     * Mise à jour par setPointsWGS84().
     */
    [[nodiscard]] double getLengthMeters() const { return m_lengthMeters; }

    /**
     * @brief Longueur euclidienne en mètres depuis les coordonnées UTM.
     * Retourne 0 si pointsUTM est vide ou contient moins de 2 points.
     */
    [[nodiscard]] double getLengthUTM() const;

    // =========================================================================
    // Requêtes topologiques
    // =========================================================================

    /**
     * @brief Identifiants des blocs adjacents (StraightBlock ou SwitchBlock).
     * Triés lexicographiquement — addNeighbourId() maintient l'ordre.
     */
    [[nodiscard]] const std::vector<std::string>& getNeighbourIds() const { return m_neighbourIds; }

    /**
     * @brief Voisins topologiques résolus (pointeurs non-propriétaires).
     *
     * Renseignés par Phase9_RepositoryTransfer::resolve() via
     * setNeighbourPrev() / setNeighbourNext().
     */
    struct StraightNeighbours
    {
        ShuntingElement* prev = nullptr;  ///< Bloc adjacent à l'extrémité A.
        ShuntingElement* next = nullptr;  ///< Bloc adjacent à l'extrémité B.
    };

    /** @brief Retourne les voisins résolus. nullptr si non encore initialisé. */
    [[nodiscard]] const StraightNeighbours& getNeighbours() const { return m_neighbours; }

    /** @brief Représentation textuelle pour le débogage. */
    [[nodiscard]] std::string toString() const;

    // =========================================================================
    // Mutations — identifiant
    // =========================================================================

    /**
     * @brief Assigne l'identifiant du bloc.
     * Appelé par Phase6_BlockExtractor lors de la création du bloc.
     *
     * @param id  Identifiant unique (ex. "s/0").
     */
    void setId(std::string id) { m_id = std::move(id); }

    // =========================================================================
    // Mutations — géométrie
    // =========================================================================

    /**
     * @brief Remplace la polyligne WGS84 et recalcule la longueur géodésique.
     *
     * @param points  Polyligne WGS-84 ordonnée (≥ 2 points).
     */
    void setPointsWGS84(std::vector<CoordinateLatLon> points);

    /**
     * @brief Assigne la polyligne UTM.
     *
     * @param points  Points projetés en UTM (même taille que pointsWGS84).
     */
    void setPointsUTM(std::vector<CoordinateXY> points) { m_pointsUTM = std::move(points); }

    // =========================================================================
    // Mutations — topologie IDs
    // =========================================================================

    /**
     * @brief Ajoute un voisin en maintenant l'ordre lexicographique.
     * Pas de doublon.
     */
    void addNeighbourId(const std::string& id);

    /**
     * @brief Remplace un ID de voisin par un autre.
     * Utilisé lors de l'absorption du segment de liaison double switch.
     */
    void replaceNeighbourId(const std::string& oldId, const std::string& newId);

    // =========================================================================
    // Mutations — pointeurs résolus (Phase9_RepositoryTransfer)
    // =========================================================================

    /**
     * @brief Assigne le voisin côté extrémité A.
     *
     * @param elem  Pointeur non-propriétaire. nullptr si terminus.
     */
    void setNeighbourPrev(ShuntingElement* elem) { m_neighbours.prev = elem; }

    /**
     * @brief Assigne le voisin côté extrémité B.
     *
     * @param elem  Pointeur non-propriétaire. nullptr si terminus.
     */
    void setNeighbourNext(ShuntingElement* elem) { m_neighbours.next = elem; }

    /**
     * @brief Enregistre les pointeurs prev/next en une seule opération.
     *
     * @param neighbours  Struct contenant les deux extrémités résolues.
     */
    void setNeighbourPointers(StraightNeighbours neighbours);

private:

    /**
     * Polyligne WGS-84 ordonnée (latitude, longitude).
     * Premier point = extrémité A. Dernier point = extrémité B.
     * Modifiable via setPointsWGS84() — recalcule automatiquement m_lengthMeters.
     */
    std::vector<CoordinateLatLon> m_pointsWGS84;

    /**
     * Polyligne UTM (x = est, y = nord, mètres).
     * Même taille et même index que m_pointsWGS84.
     * Renseigné par le pipeline v2 (Phase6_BlockExtractor).
     */
    std::vector<CoordinateXY> m_pointsUTM;

    /**
     * IDs des blocs adjacents (StraightBlock ou SwitchBlock).
     * Trié lexicographiquement — addNeighbourId() maintient l'invariant.
     */
    std::vector<std::string> m_neighbourIds;

    /**
     * Pointeurs non-propriétaires vers les blocs adjacents.
     * Renseignés par Phase9_RepositoryTransfer::resolve().
     * Propriété de TopologyRepository — ne pas delete.
     */
    StraightNeighbours m_neighbours;

    /**
     * Longueur géodésique totale en mètres (Haversine sur m_pointsWGS84).
     * Mise à jour automatiquement par setPointsWGS84().
     */
    double m_lengthMeters = 0.0;

    /**
     * @brief Calcule la longueur géodésique totale depuis m_pointsWGS84.
     * Somme des distances Haversine entre chaque paire de points consécutifs.
     */
    [[nodiscard]] double computeGeodesicLength() const;
};
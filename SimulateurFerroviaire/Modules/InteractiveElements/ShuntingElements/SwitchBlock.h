/**
 * @file  SwitchBlock.h
 * @brief Modèle de domaine d'un aiguillage ferroviaire à 3 branches.
 *
 * Encapsulation : tous les champs sont privés. Les mutations passent par des
 * méthodes à intent explicite.
 *
 * Double aiguille :
 *   Un aiguillage peut absorber le segment de liaison côté branche normale
 *   (doubleOnNormal) ou côté branche déviée (doubleOnDeviation).
 *   Un seul des deux est renseigné à la fois.
 *
 * Système de coordonnées :
 *   WGS84 — jonction et tips pour le rendu Leaflet.
 *   UTM   — jonction UTM pour les calculs métriques du pipeline.
 */
#pragma once

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Engine/Core/Coordinates/CoordinateLatLon.h"
#include "Engine/Core/Coordinates/CoordinateXY.h"
#include "ShuntingElement.h"

 /**
  * @brief Branche active d'un aiguillage.
  *
  * NORMAL    : train sur la voie directe (position repos).
  * DEVIATION : train sur la voie déviée (position basculée).
  *
  * Modifié en runtime par l'opérateur via clic Leaflet.
  */
enum class ActiveBranch { NORMAL, DEVIATION };


class SwitchBlock : public ShuntingElement
{
public:

    // =========================================================================
    // Construction
    // =========================================================================

    SwitchBlock() = default;

    /**
     * @brief Construit un SwitchBlock positionné à junctionWGS84.
     *
     * @param switchId        Identifiant unique (ex. "sw/0").
     * @param junctionWGS84   Coordonnée WGS-84 du point de jonction.
     * @param branchIds       Branches connues à la création (optionnel).
     */
    SwitchBlock(std::string              switchId,
        CoordinateLatLon         junctionWGS84,
        std::vector<std::string> branchIds = {});

    // =========================================================================
    // Interface ShuntingElement
    // =========================================================================

    [[nodiscard]] std::string            getId()    const override { return m_id; }
    [[nodiscard]] InteractiveElementType getType()  const override { return InteractiveElementType::SWITCH; }
    [[nodiscard]] ShuntingState          getState() const override { return m_state; }

    void setState(ShuntingState state) { m_state = state; }

    // =========================================================================
    // Requêtes — géométrie
    // =========================================================================

    /** @brief Coordonnée WGS-84 du point de jonction physique. */
    [[nodiscard]] const CoordinateLatLon& getJunctionWGS84() const { return m_junctionWGS84; }

    /** @brief Coordonnée UTM du point de jonction (x = est, y = nord, mètres). */
    [[nodiscard]] const CoordinateXY& getJunctionUTM() const { return m_junctionUTM; }

    /** Point CDC côté root. Absent si non orienté. */
    [[nodiscard]] const std::optional<CoordinateLatLon>& getTipOnRoot()      const { return m_tipOnRoot; }

    /** Point CDC côté normal. Absent si non orienté. */
    [[nodiscard]] const std::optional<CoordinateLatLon>& getTipOnNormal()    const { return m_tipOnNormal; }

    /** Point CDC côté deviation. Absent si non orienté. */
    [[nodiscard]] const std::optional<CoordinateLatLon>& getTipOnDeviation() const { return m_tipOnDeviation; }

    /** @brief Polyligne absorbée côté normal (double switch). Vide si non applicable. */
    [[nodiscard]] const std::vector<CoordinateLatLon>& getAbsorbedNormalCoordinates()    const { return m_absorbedNormalCoords; }

    /** @brief Polyligne absorbée côté deviation (double switch). Vide si non applicable. */
    [[nodiscard]] const std::vector<CoordinateLatLon>& getAbsorbedDeviationCoordinates() const { return m_absorbedDeviationCoords; }

    /** @brief UTM absorbée côté deviation (double switch). Vide si non applicable. */
    [[nodiscard]] const std::vector<CoordinateXY>& getAbsorbedNormalCoordsUTM()     const { return m_absorbedNormalCoordsUTM; }

    /** @brief UTM absorbée côté deviation (double switch). Vide si non applicable. */
    [[nodiscard]] const std::vector<CoordinateXY>& getAbsorbedDeviationCoordsUTM()  const { return m_absorbedDeviationCoordsUTM; }

    /** @brief Longueur physique de traversée en mètres. Absent si tips manquants. */
    [[nodiscard]] const std::optional<double>& getTotalLengthMeters() const { return m_totalLengthMeters; }

    // =========================================================================
    // Requêtes — topologie IDs
    // =========================================================================

    /**
     * @brief IDs des StraightBlocks connectés à la jonction.
     * Peuplé lors de la construction du graphe.
     */
    [[nodiscard]] const std::vector<std::string>& getBranchIds() const { return m_branchIds; }

    /** @brief True si les rôles root/normal/deviation sont assignés. */
    [[nodiscard]] bool isOriented() const { return m_rootBranchId.has_value(); }

    /** @brief ID de la branche root. Absent si non orienté. */
    [[nodiscard]] const std::optional<std::string>& getRootBranchId()      const { return m_rootBranchId; }

    /** @brief ID de la branche normale. Absent si non orienté. */
    [[nodiscard]] const std::optional<std::string>& getNormalBranchId()    const { return m_normalBranchId; }

    /** @brief ID de la branche déviée. Absent si non orienté. */
    [[nodiscard]] const std::optional<std::string>& getDeviationBranchId() const { return m_deviationBranchId; }

    /** @brief ID du partenaire double switch côté normal. Absent si non applicable. */
    [[nodiscard]] const std::optional<std::string>& getDoubleOnNormal()    const { return m_doubleOnNormal; }

    /** @brief ID du partenaire double switch côté deviation. Absent si non applicable. */
    [[nodiscard]] const std::optional<std::string>& getDoubleOnDeviation() const { return m_doubleOnDeviation; }

    /** @brief True si un segment de liaison a été absorbé (double aiguille). */
    [[nodiscard]] bool isDouble() const
    {
        return m_doubleOnNormal.has_value() || m_doubleOnDeviation.has_value();
    }

    // =========================================================================
    // Requêtes — pointeurs résolus
    // =========================================================================

    /**
     * @brief Branches topologiques résolues (pointeurs non-propriétaires).
     */
    struct SwitchBranches
    {
        ShuntingElement* root = nullptr;  ///< Tronc entrant.
        ShuntingElement* normal = nullptr;  ///< Sortie directe.
        ShuntingElement* deviation = nullptr;  ///< Sortie déviée.
    };

    /** @brief Retourne les branches résolues. nullptr si non initialisé. */
    [[nodiscard]] const SwitchBranches& getBranches() const { return m_branches; }

    /** @brief Accès direct à la branche root. nullptr si non résolue. */
    [[nodiscard]] ShuntingElement* getRootBlock()      const { return m_branches.root; }

    /** @brief Accès direct à la branche normale. nullptr si non résolue. */
    [[nodiscard]] ShuntingElement* getNormalBlock()    const { return m_branches.normal; }

    /** @brief Accès direct à la branche déviée. nullptr si non résolue. */
    [[nodiscard]] ShuntingElement* getDeviationBlock() const { return m_branches.deviation; }

    /**
     * @brief Retourne le switch partenaire côté normal, ou nullptr.
     * Cast valide uniquement si isDouble() && getDoubleOnNormal().
     */
    [[nodiscard]] SwitchBlock* getPartnerOnNormal() const
    {
        return m_doubleOnNormal
            ? static_cast<SwitchBlock*>(m_branches.normal)
            : nullptr;
    }

    /**
     * @brief Retourne le switch partenaire côté deviation, ou nullptr.
     */
    [[nodiscard]] SwitchBlock* getPartnerOnDeviation() const
    {
        return m_doubleOnDeviation
            ? static_cast<SwitchBlock*>(m_branches.deviation)
            : nullptr;
    }

    // =========================================================================
    // Requêtes — état opérationnel
    // =========================================================================

    /** @brief Branche actuellement active (NORMAL par défaut). */
    [[nodiscard]] ActiveBranch getActiveBranch() const { return m_activeBranch; }

    /** @brief Raccourci — évite la comparaison explicite. */
    [[nodiscard]] bool isDeviationActive() const { return m_activeBranch == ActiveBranch::DEVIATION; }

    /** @brief Convertit ActiveBranch en chaîne lisible pour les logs. */
    [[nodiscard]] std::string activeBranchToString() const
    {
        return m_activeBranch == ActiveBranch::DEVIATION ? "DEVIATION" : "NORMAL";
    }

    /** @brief Représentation textuelle pour le débogage. */
    [[nodiscard]] std::string toString() const;

    // =========================================================================
    // Mutations — identifiant
    // =========================================================================

    /**
     * @brief Assigne l'identifiant du bloc.
     * Appelé par Phase6_BlockExtractor lors de la création du bloc.
     *
     * @param id  Identifiant unique (ex. "sw/0").
     */
    void setId(std::string id) { m_id = std::move(id); }

    // =========================================================================
    // Mutations — géométrie
    // =========================================================================

    /**
     * @brief Assigne la position de jonction en WGS84.
     *
     * @param coord  Coordonnée WGS-84 (latitude, longitude).
     */
    void setJunctionWGS84(CoordinateLatLon coord) { m_junctionWGS84 = coord; }

    /**
     * @brief Assigne la position de jonction en UTM.
     *
     * @param coord  Coordonnée UTM (x = est, y = nord, mètres).
     */
    void setJunctionUTM(CoordinateXY coord) { m_junctionUTM = coord; }

    /**
     * @brief Assigne les trois tips CDC en une seule opération.
     *
     * @param tipRoot      Tip côté root.
     * @param tipNormal    Tip côté normal.
     * @param tipDeviation Tip côté deviation.
     */
    void setTips(std::optional<CoordinateLatLon> tipRoot,
        std::optional<CoordinateLatLon> tipNormal,
        std::optional<CoordinateLatLon> tipDeviation);

    /**
     * @brief Stocke les coordonnées absorbées d'un double switch.
     *
     * Appelé par Phase8_DoubleSwitchDetector::absorbLinkSegment().
     *
     * @param side   Branche absorbée ("normal" ou "deviation").
     * @param coords Polyligne WGS84 du segment absorbé.
     */
    void setAbsorbedCoords(const std::string& side,
        std::vector<CoordinateLatLon> coords);

    // =========================================================================
    // Mutations — topologie IDs
    // =========================================================================

    /**
     * @brief Ajoute un ID de branche. Pas de doublon.
     */
    void addBranchId(const std::string& id);

    /**
     * @brief Assigne les rôles root / normal / deviation.
     *
     * @throws std::invalid_argument Si un ID est absent de m_branchIds.
     */
    void orient(std::string rootId, std::string normalId, std::string deviationId);

    /**
     * @brief Échange normal ↔ deviation (rôles + tips + polylignes absorbées + doubles).
     */
    void swapNormalDeviation();

    /**
     * @brief Absorbe le segment de liaison d'un double aiguille.
     *
     * Remplace linkId par partnerId dans m_branchIds,
     * met à jour la branchId correspondante et les tips CDC.
     *
     * @param linkId          ID du StraightBlock absorbé.
     * @param partnerId       ID du SwitchBlock partenaire.
     * @param linkCoordsWGS84 Polyligne du segment absorbé (orientée depuis cette jonction).
     * @param linkCoordsUTM   Coordonnée XY du segment absorbé
     */
    void absorbLink(const std::string& linkId,
        const std::string& partnerId,
        std::vector<CoordinateLatLon> linkCoordsWGS84,
        std::vector<CoordinateXY>     linkCoordsUTM);

    /**
     * @brief Remplace un pointeur de branche par un autre.
     *
     * Appelé par Phase8_DoubleSwitchDetector::absorbLinkSegment()
     * après suppression du segment de liaison.
     *
     * @param oldElem  Ancien bloc pointé (ex. le StraightBlock absorbé).
     * @param newElem  Nouveau bloc (ex. le SwitchBlock partenaire).
     */
    void replaceBranchPointer(ShuntingElement* oldElem, ShuntingElement* newElem);

    /**
     * @brief Calcule et mémorise la longueur totale de traversée.
     * Formule : root_leg + max(normal_leg, deviation_leg).
     * No-op si non orienté ou tips absents.
     */
    void computeTotalLength();

    // =========================================================================
    // Mutations — pointeurs résolus (Phase9_RepositoryTransfer)
    // =========================================================================

    /**
     * @brief Assigne le pointeur de la branche root.
     *
     * @param elem  Pointeur non-propriétaire. nullptr si non résolu.
     */
    void setRootPointer(ShuntingElement* elem) { m_branches.root = elem; }

    /**
     * @brief Assigne le pointeur de la branche normale.
     *
     * @param elem  Pointeur non-propriétaire. nullptr si non résolu.
     */
    void setNormalPointer(ShuntingElement* elem) { m_branches.normal = elem; }

    /**
     * @brief Assigne le pointeur de la branche déviée.
     *
     * @param elem  Pointeur non-propriétaire. nullptr si non résolu.
     */
    void setDeviationPointer(ShuntingElement* elem) { m_branches.deviation = elem; }

    /**
     * @brief Enregistre les branches en une seule opération.
     *
     * @param branches  Struct contenant les trois pointeurs résolus.
     */
    void setBranchPointers(SwitchBranches branches);

    // =========================================================================
    // Mutations — état opérationnel
    // =========================================================================

    /**
     * @brief Assigne la branche active.
     *
     * @param branch    Branche à activer.
     * @param propagate Si true, propage aux partenaires double switch.
     */
    void setActiveBranch(ActiveBranch branch, bool propagate = true);

    /**
     * @brief Alterne entre NORMAL et DEVIATION.
     *
     * @param propagate Si true, propage aux partenaires double switch.
     *
     * @return Nouvelle valeur de m_activeBranch.
     */
    ActiveBranch toggleActiveBranch(bool propagate = true);

private:

    // =========================================================================
    // Champs — géométrie
    // =========================================================================

    /** Coordonnée WGS-84 du point de jonction physique. */
    CoordinateLatLon m_junctionWGS84;

    /** Coordonnée UTM du point de jonction (x = est, y = nord, mètres). */
    CoordinateXY m_junctionUTM;

    /** Tip CDC côté root. Absent si non orienté. */
    std::optional<CoordinateLatLon> m_tipOnRoot;

    /** Tip CDC côté normal. Absent si non orienté. */
    std::optional<CoordinateLatLon> m_tipOnNormal;

    /** Tip CDC côté deviation. Absent si non orienté. */
    std::optional<CoordinateLatLon> m_tipOnDeviation;

    /**
     * Longueur physique de traversée (root_leg + max(normal, deviation)).
     * Absent si tips non disponibles.
     */
    std::optional<double> m_totalLengthMeters;

    /** Polyligne absorbée côté normal (double switch). */
    std::vector<CoordinateLatLon> m_absorbedNormalCoords;

    /** Polyligne absorbée côté deviation (double switch). */
    std::vector<CoordinateLatLon> m_absorbedDeviationCoords;

    std::vector<CoordinateXY>     m_absorbedNormalCoordsUTM;
    std::vector<CoordinateXY>     m_absorbedDeviationCoordsUTM;

    // =========================================================================
    // Champs — topologie IDs
    // =========================================================================

    /** IDs des StraightBlocks connectés. */
    std::vector<std::string> m_branchIds;

    /** ID de la branche root. Absent si non orienté. */
    std::optional<std::string> m_rootBranchId;

    /** ID de la branche normale. Absent si non orienté. */
    std::optional<std::string> m_normalBranchId;

    /** ID de la branche déviée. Absent si non orienté. */
    std::optional<std::string> m_deviationBranchId;

    /** ID du partenaire double switch côté normal. */
    std::optional<std::string> m_doubleOnNormal;

    /** ID du partenaire double switch côté deviation. */
    std::optional<std::string> m_doubleOnDeviation;

    // =========================================================================
    // Champs — pointeurs résolus
    // =========================================================================

    /** Branches topologiques résolues. nullptr si non initialisées. */
    SwitchBranches m_branches;

    // =========================================================================
    // Champs — état opérationnel
    // =========================================================================

    /** Branche actuellement active. NORMAL par défaut. */
    ActiveBranch m_activeBranch = ActiveBranch::NORMAL;

    // =========================================================================
    // Helpers privés
    // =========================================================================

    /**
     * @brief Calcule la distance de Haversine entre deux points WGS-84.
     *
     * @param a  Premier point.
     * @param b  Second point.
     *
     * @return Distance en mètres.
     */
    static double haversineDistanceMeters(const CoordinateLatLon& a,
        const CoordinateLatLon& b);
};
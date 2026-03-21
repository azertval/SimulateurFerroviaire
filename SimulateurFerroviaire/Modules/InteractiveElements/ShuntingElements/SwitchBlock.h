#pragma once

/**
 * @file  SwitchBlock.h
 * @brief Modèle de domaine d'un aiguillage ferroviaire à 3 branches.
 *
 * Encapsulation : tous les champs sont privés. Les mutations passent par des
 * méthodes à intent explicite, appelées uniquement par la phase du pipeline
 * qui en a la charge :
 *   Phase 3   → constructeur
 *   Phase 5b  → addBranchId
 *   Phase 6   → orient / setTips / swapNormalDeviation / computeTotalLength
 *   Phase 7   → absorbLink
 *
 * Double aiguille :
 *   Un aiguillage peut absorber le segment de liaison côté branche normale
 *   (doubleOnNormal) ou côté branche déviée (doubleOnDeviation).
 *   Un seul des deux est renseigné à la fois.
 *
 *   absorbLink() stocke la polyligne COMPLÈTE du segment absorbé, orientée
 *   depuis la jonction de ce switch vers celle du partenaire.
 *   Cette polyligne est utilisée par le rendu WebView pour tracer la branche
 *   fusionnée avec fidélité géométrique.
 */

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Modules/Coordinates/LatLon.h"


class SwitchBlock
{
public:

    // =========================================================================
    // Construction
    // =========================================================================

    SwitchBlock() = default;

    /**
     * @brief Construit un SwitchBlock positionné à junctionCoord.
     * @param switchId          Identifiant unique (ex. "sw/0").
     * @param junctionCoord     Coordonnée WGS-84 du point de jonction.
     * @param initialBranchIds  Branches connues à la création (optionnel).
     */
    SwitchBlock(std::string              switchId,
        LatLon                   junctionCoord,
        std::vector<std::string> initialBranchIds = {});


    // =========================================================================
    // Requêtes
    // =========================================================================

    [[nodiscard]] const std::string& getId()                 const { return m_id; }
    [[nodiscard]] const LatLon& getJunctionCoordinate() const { return m_junctionCoordinate; }
    [[nodiscard]] const std::vector<std::string>& getBranchIds()          const { return m_branchIds; }

    /** True si Phase 6 a assigné root / normal / deviation. */
    [[nodiscard]] bool isOriented() const { return m_rootBranchId.has_value(); }

    /** True si Phase 7 a absorbé un segment de liaison (double aiguille). */
    [[nodiscard]] bool isDouble() const
    {
        return m_doubleOnNormal.has_value() || m_doubleOnDeviation.has_value();
    }

    [[nodiscard]] const std::optional<std::string>& getRootBranchId()      const { return m_rootBranchId; }
    [[nodiscard]] const std::optional<std::string>& getNormalBranchId()    const { return m_normalBranchId; }
    [[nodiscard]] const std::optional<std::string>& getDeviationBranchId() const { return m_deviationBranchId; }

    /** Point CDC à ~branchTipDistance depuis la jonction sur la branche root. */
    [[nodiscard]] const std::optional<LatLon>& getTipOnRoot()      const { return m_tipOnRoot; }

    /**
     * Point CDC sur la branche normal.
     * Pour un double switch côté normal : extrémité distale de la polyligne absorbée
     * (= jonction du partenaire).
     */
    [[nodiscard]] const std::optional<LatLon>& getTipOnNormal()    const { return m_tipOnNormal; }

    /**
     * Point CDC sur la branche deviation.
     * Pour un double switch côté deviation : extrémité distale de la polyligne absorbée.
     */
    [[nodiscard]] const std::optional<LatLon>& getTipOnDeviation() const { return m_tipOnDeviation; }

    /**
     * @brief ID du partenaire si le lien absorbé était côté branche NORMALE.
     */
    [[nodiscard]] const std::optional<std::string>& getDoubleOnNormal()    const { return m_doubleOnNormal; }

    /**
     * @brief ID du partenaire si le lien absorbé était côté branche DEVIEE.
     */
    [[nodiscard]] const std::optional<std::string>& getDoubleOnDeviation() const { return m_doubleOnDeviation; }

    /**
     * @brief Retourne l'ID du partenaire (peu importe la branche). Nullopt si pas un double.
     */
    [[nodiscard]] std::optional<std::string> getPartnerId() const
    {
        if (m_doubleOnNormal)    return m_doubleOnNormal;
        if (m_doubleOnDeviation) return m_doubleOnDeviation;
        return std::nullopt;
    }

    /**
     * @brief Polyligne complète du segment absorbé côté normal.
     *
     * Orientée depuis la jonction de CE switch vers celle du partenaire.
     * Vide si ce switch n'est pas un double côté normal.
     * Utilisée par le rendu WebView pour tracer la branche fusionnée.
     */
    [[nodiscard]] const std::vector<LatLon>& getAbsorbedNormalCoords()    const { return m_absorbedNormalCoords; }

    /**
     * @brief Polyligne complète du segment absorbé côté deviation.
     *
     * Même sémantique que getAbsorbedNormalCoords().
     */
    [[nodiscard]] const std::vector<LatLon>& getAbsorbedDeviationCoords() const { return m_absorbedDeviationCoords; }

    [[nodiscard]] const std::optional<double>& getTotalLengthMeters() const { return m_totalLengthMeters; }

    /**
     * @brief Représentation textuelle pour le débogage.
     */
    [[nodiscard]] std::string toString() const;


    // =========================================================================
    // Mutations — Phase 5b
    // =========================================================================

    /** Ajoute un ID de branche (StraightBlock adjacent). Pas de doublon. */
    void addBranchId(const std::string& id);


    // =========================================================================
    // Mutations — Phase 6
    // =========================================================================

    /**
     * @brief Assigne les rôles root / normal / deviation.
     * @throws std::invalid_argument Si un ID est absent de m_branchIds.
     */
    void orient(std::string rootId, std::string normalId, std::string deviationId);

    /** @brief Assigne les trois tips CDC en une seule opération. */
    void setTips(std::optional<LatLon> tipRoot,
        std::optional<LatLon> tipNormal,
        std::optional<LatLon> tipDeviation);

    /**
     * @brief Échange normal ↔ deviation (rôles + tips + polylignes absorbées).
     */
    void swapNormalDeviation();

    /**
     * @brief Calcule et mémorise totalLengthMeters.
     * Formule : root_leg + max(normal_leg, deviation_leg). No-op si non orienté.
     */
    void computeTotalLength();


    // =========================================================================
    // Mutations — Phase 7
    // =========================================================================

    /**
     * @brief Absorbe le segment de liaison d'un double aiguille.
     *
     * - Remplace linkId par partnerId dans m_branchIds.
     * - Si linkId était la branche normale :
     *     • met à jour normalBranchId
     *     • stocke linkCoords (orientée depuis cette jonction) dans m_absorbedNormalCoords
     *     • tipOnNormal = linkCoords.back() (jonction du partenaire)
     *     • renseigne m_doubleOnNormal
     * - Idem côté deviation.
     *
     * @param linkId     ID du StraightBlock intermédiaire absorbé.
     * @param partnerId  ID de l'aiguillage partenaire.
     * @param linkCoords Polyligne complète du segment absorbé, orientée depuis
     *                   LA JONCTION DE CE SWITCH vers celle du partenaire.
     *                   Premier point ≈ jonction de ce switch.
     *                   Dernier point  = jonction du partenaire (nouveau tip CDC).
     */
    void absorbLink(const std::string& linkId,
        const std::string& partnerId,
        std::vector<LatLon>       linkCoords);

private:

    // =========================================================================
    // Champs
    // =========================================================================

    std::string m_id;
    LatLon      m_junctionCoordinate;

    std::vector<std::string> m_branchIds;

    // --- Phase 6 ---
    std::optional<std::string> m_rootBranchId;
    std::optional<std::string> m_normalBranchId;
    std::optional<std::string> m_deviationBranchId;

    std::optional<LatLon> m_tipOnRoot;
    std::optional<LatLon> m_tipOnNormal;
    std::optional<LatLon> m_tipOnDeviation;

    std::optional<double> m_totalLengthMeters;

    // --- Phase 7 : double aiguille ---

    /** ID du partenaire si le lien absorbé était côté branche NORMALE. */
    std::optional<std::string> m_doubleOnNormal;

    /** ID du partenaire si le lien absorbé était côté branche DEVIEE. */
    std::optional<std::string> m_doubleOnDeviation;

    /**
     * Polyligne complète du segment absorbé côté normal,
     * orientée depuis la jonction de ce switch vers le partenaire.
     * Vide si non applicable.
     */
    std::vector<LatLon> m_absorbedNormalCoords;

    /**
     * Polyligne complète du segment absorbé côté deviation,
     * orientée depuis la jonction de ce switch vers le partenaire.
     */
    std::vector<LatLon> m_absorbedDeviationCoords;


    // =========================================================================
    // Helpers privés
    // =========================================================================

    static double haversineDistanceMeters(const LatLon& a, const LatLon& b);
};
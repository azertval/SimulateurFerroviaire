#pragma once

/**
 * @file  SwitchOrientator.h
 * @brief Phases 6, 6b, 6c et 6d — orientation géométrique des aiguillages.
 *
 * Phase 6 (orientAllSwitches) :
 *   Pour chaque SwitchBlock à 3 branches, assigne root / normal / deviation.
 *   Règle : la paire de branches avec l'angle mutuel le plus faible = sorties.
 *   La branche restante = root. Normal = sortie la plus alignée avec l'anti-root.
 *   Calcule les points tip CDC à branchTipDistanceMeters depuis la jonction.
 *
 * Phase 6b (alignDoubleSwitchRoles) :
 *   Garantit que le segment de liaison entre deux futurs demi-doubles porte
 *   le même rôle (normal ou deviation) des deux côtés.
 *   L'aiguillage de référence est celui dont l'ID est le plus petit.
 *
 * Phase 6c (enforceCrossoverConsistency) :
 *   Si deux aiguillages partagent exactement 2 branches (croisement mécanique),
 *   force la branche partagée au rôle DEVIATION sur les deux.
 *
 * Phase 6d (trimStraightOverlaps) :
 *   Supprime le chevauchement géométrique entre les branches de l'aiguillage et
 *   les StraightBlock adjacents. Après cette phase :
 *     - SwitchBlock possède 4 coordonnées : junctionCoordinate, tipOnRoot,
 *       tipOnNormal, tipOnDeviation  (à branchTipDistanceMeters depuis la jonction).
 *     - StraightBlock.coordinates.front() (côté jonction) = tip CDC correspondant.
 *   Un même bout de Straight n'est jamais retaillé deux fois (garde via
 *   un ensemble (straightId, junctionAtFront)).
 */

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "TopologyExtractor.h"
#include "Modules/InteractiveElements/ShuntingElements/StraightBlock.h"
#include "Modules/InteractiveElements/ShuntingElements/SwitchBlock.h"
#include "./Enums/GeoParserEnums.h"
#include "Engine/Core/Logger/Logger.h"


 /**
  * @brief Oriente les aiguillages ferroviaires (root / normal / deviation / tips CDC)
  *        et supprime les chevauchements junction/straight.
  */
class SwitchOrientator
{
public:

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /**
     * @brief Construit le SwitchOrientator.
     *
     * @param logger                  Référence au logger du moteur GeoParser.
     * @param topoResult              Résultat de TopologyExtractor (modifié en place).
     * @param utmZoneNumber           Zone UTM pour la reprojection des tips.
     * @param isNorthernHemisphere    True pour l'hémisphère nord.
     * @param doubleLinkMaxMeters     Longueur max du segment de liaison d'un double aiguille.
     * @param branchTipDistanceMeters Distance tip CDC depuis la jonction.
     *                                Détermine aussi la longueur des branches du switch
     *                                et la quantité retaillée sur les Straights adjacents.
     */
    SwitchOrientator(Logger& logger,
        TopologyExtractResult& topoResult,
        int                     utmZoneNumber,
        bool                    isNorthernHemisphere,
        double                  doubleLinkMaxMeters = ParserDefaultValues::DOUBLE_LINK_MAX_METERS,
        double                  branchTipDistanceMeters = ParserDefaultValues::BRANCH_TIP_DISTANCE_METERS);


    // -------------------------------------------------------------------------
    // API publique
    // -------------------------------------------------------------------------

    /**
     * @brief Exécute les phases 6, 6b, 6c et 6d en place sur topoResult.
     *
     * Après cet appel :
     *   - Chaque SwitchBlock orienté dispose de junctionCoordinate + 3 tips CDC.
     *   - Chaque StraightBlock adjacent à une jonction débute (ou se termine)
     *     au point tip CDC — aucun chevauchement avec les branches du switch.
     */
    void orient();

private:

    Logger& m_logger;
    TopologyExtractResult& m_topoResult;
    int                    m_utmZoneNumber;
    bool                   m_isNorthernHemisphere;
    double                 m_doubleLinkMaxMeters;
    double                 m_branchTipDistanceMeters;

    // -------------------------------------------------------------------------
    // Phase 6 — Orientation d'un aiguillage 3-branches
    // -------------------------------------------------------------------------

    /**
     * @brief Oriente un SwitchBlock à 3 branches en place.
     *
     * @param switchBlock    Aiguillage à orienter.
     * @param straightById   Index des StraightBlock pour accès par ID.
     */
    void orientThreePortSwitch(
        SwitchBlock& switchBlock,
        const std::unordered_map<std::string, StraightBlock*>& straightById);

    /**
     * @brief Calcule et assigne les points tip CDC sur un SwitchBlock orienté.
     *
     * @param switchBlock  Aiguillage orienté (root/normal/deviation déjà assignés).
     * @param straightById Index des StraightBlock.
     */
    void computeBranchTipPoints(
        SwitchBlock& switchBlock,
        const std::unordered_map<std::string, StraightBlock*>& straightById);

    /**
     * @brief Interpole un point tip à distanceMeters depuis la jonction le long d'un Straight.
     *
     * @param straightId       Identifiant du StraightBlock.
     * @param junctionCoord    Coordonnée WGS-84 de la jonction.
     * @param distanceMeters   Distance depuis la jonction.
     * @param straightById     Index des StraightBlock.
     * @return Point tip WGS-84, ou nullopt si impossible à calculer.
     */
    std::optional<LatLon> interpolateTipPoint(
        const std::string& straightId,
        const LatLon& junctionCoord,
        double                                                  distanceMeters,
        const std::unordered_map<std::string, StraightBlock*>& straightById);

    // -------------------------------------------------------------------------
    // Phase 6b — Alignement des rôles pour les futurs doubles
    // -------------------------------------------------------------------------

    /**
     * @brief Échange normal↔deviation sur l'aiguillage non-référence si les rôles diffèrent.
     *
     * @param straightById Index des StraightBlock.
     */
    void alignDoubleSwitchRoles(
        const std::unordered_map<std::string, StraightBlock*>& straightById);

    // -------------------------------------------------------------------------
    // Phase 6c — Cohérence des croisements mécaniques
    // -------------------------------------------------------------------------

    /**
     * @brief Force la branche partagée au rôle DEVIATION sur les deux aiguillages
     *        formant un croisement mécanique (2 branches partagées).
     */
    void enforceCrossoverConsistency();

    // -------------------------------------------------------------------------
    // Phase 6d — Suppression des chevauchements junction / straight
    // -------------------------------------------------------------------------

    /**
     * @brief Retaille chaque StraightBlock adjacent à une jonction pour qu'il
     *        commence (ou se termine) exactement au point tip CDC.
     *
     * Algorithme :
     *   Pour chaque switch orienté, pour chaque branche (root / normal / deviation) :
     *     1. Identifier quel bout du Straight est côté jonction.
     *     2. Convertir la polyligne en métrique UTM, jonction en tête.
     *     3. Parcourir les segments jusqu'à cumuler branchTipDistanceMeters ;
     *        interpoler le point de coupure exact.
     *     4. Reconstituer la polyligne à partir de ce point, reconvertir en WGS-84.
     *     5. Recalculer StraightBlock::lengthMeters.
     *
     * Un ensemble (straightId, junctionAtFront) garantit qu'un même bout n'est
     * jamais retaillé deux fois (croisements mécaniques, doubles aiguilles).
     *
     * @param straightById  Index mutable des StraightBlock.
     */
    void trimStraightOverlaps(
        std::unordered_map<std::string, StraightBlock*>& straightById);

    // -------------------------------------------------------------------------
    // Utilitaires géométriques
    // -------------------------------------------------------------------------

    /**
     * @brief Retourne la direction (vecteur lat/lon) depuis junctionCoord vers
     *        l'extrémité distale du StraightBlock.
     *
     * @param straightId    Identifiant du StraightBlock.
     * @param junctionCoord Coordonnée de la jonction WGS-84.
     * @param straightById  Index des StraightBlock.
     * @return Vecteur direction CoordinateXY (composantes lat/lon).
     */
    CoordinateXY computeBranchDirectionVector(
        const std::string& straightId,
        const LatLon& junctionCoord,
        const std::unordered_map<std::string, StraightBlock*>& straightById) const;
};
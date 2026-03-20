#pragma once

/**
 * @file  SwitchBlock.h
 * @brief Modèle de domaine d'un aiguillage ferroviaire à 3 branches (Switch).
 *
 * Un SwitchBlock représente une jonction topologique de degré ≥ 3 dans
 * le graphe ferroviaire. Après orientation (Phase 6), il expose :
 *   - La branche root  : Straight entrant dans la jonction.
 *   - La branche normal: premier Straight de sortie (continuation directe).
 *   - La branche deviation : second Straight de sortie (branche déviée).
 * 
 * Hiérarchie :
 *   InteractiveElement → ShuntingElement → SwitchBlock
 */

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Modules/Coordinates/LatLon.h"


/**
 * @brief Aiguillage ferroviaire à 3 branches produit par le parseur GeoJSON.
 */
class SwitchBlock
{
public:

    // -------------------------------------------------------------------------
    // Identification
    // -------------------------------------------------------------------------

    /** Identifiant unique de l'aiguillage (ex. "sw/0"). */
    std::string id;

    /** Coordonnée WGS-84 du point de jonction physique. */
    LatLon junctionCoordinate;

    /**
     * Identifiants des 3 branches de l'aiguillage.
     * Phase 3  : IDs d'arêtes brutes du graphe.
     * Phase 5b : IDs de StraightBlock.
     * Phase 6  : ordonnés [rootBranchId, normalBranchId, deviationBranchId].
     */
    std::vector<std::string> branchIds;


    // -------------------------------------------------------------------------
    // Orientation — peuplé par Phase 6
    // -------------------------------------------------------------------------

    /** Identifiant du StraightBlock entrant dans la jonction (branche root). */
    std::optional<std::string> rootBranchId;

    /** Identifiant du StraightBlock de sortie principale (continuation directe). */
    std::optional<std::string> normalBranchId;

    /** Identifiant du StraightBlock de sortie déviée. */
    std::optional<std::string> deviationBranchId;

    /**
     * Point CDC WGS-84 à ~branch_tip_distance_m depuis la jonction sur la branche root.
     * Sert aux vérifications d'écartement de voies.
     */
    std::optional<LatLon> tipOnRoot;

    /** Point CDC WGS-84 à ~branch_tip_distance_m sur la branche normal. */
    std::optional<LatLon> tipOnNormal;

    /** Point CDC WGS-84 à ~branch_tip_distance_m sur la branche deviation. */
    std::optional<LatLon> tipOnDeviation;


    // -------------------------------------------------------------------------
    // Double aiguille — peuplé par Phase 7
    // -------------------------------------------------------------------------

    /** True si cet aiguillage fait partie d'un double aiguille (TJD ou similaire). */
    bool isDoubleSwitch = false;

    /** Identifiant du partenaire (premier demi-double dans l'ordre de la chaîne). */
    std::optional<std::string> nextSwitchId;

    /** Identifiant du partenaire (second demi-double dans l'ordre de la chaîne). */
    std::optional<std::string> prevSwitchId;

    /** Longueur physique totale de traversée (root_leg + max(normal_leg, deviation_leg)). */
    std::optional<double> totalLengthMeters;

    /**
     * Coordonnée du milieu du segment de liaison dans un double aiguille.
     * Absente si l'aiguillage n'est pas un double.
     */
    std::optional<LatLon> doubleLinkMidCoordinate;


    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /** Constructeur par défaut. */
    SwitchBlock() = default;

    /**
     * @brief Construit un SwitchBlock avec son identifiant et sa position.
     *
     * @param switchId           Identifiant unique.
     * @param junctionCoord      Coordonnée WGS-84 du point de jonction.
     * @param initialBranchIds   Identifiants des branches (optionnel à la création).
     */
    SwitchBlock(std::string              switchId,
                LatLon                   junctionCoord,
                std::vector<std::string> initialBranchIds = {});


    // -------------------------------------------------------------------------
    // Requêtes
    // -------------------------------------------------------------------------

    /**
     * @brief Indique si l'aiguillage a été orienté (Phase 6 complète).
     * @return true dès que rootBranchId contient une valeur.
     */
    bool isOriented() const;

    /**
     * @brief Calcule et mémorise totalLengthMeters.
     *
     * Formule : root_leg + max(normal_leg, deviation_leg)
     * où les jambes sont les distances Haversine entre la jonction et chaque tip.
     *
     * Ne fait rien si l'aiguillage n'est pas orienté ou si un tip est absent.
     */
    void computeTotalLength();

    /**
     * @brief Retourne une représentation textuelle pour le débogage.
     *
     * Format orienté  : Switch(id=sw/0, root=s/0, normal=s/1, deviation=s/2, len=45.3m)
     * Format brut      : Switch(id=sw/0, junction=(48.85,2.35), degree=3)
     *
     * @return Chaîne de représentation.
     */
    std::string toString() const;

private:

    /**
     * @brief Calcule la distance de Haversine entre deux points WGS-84.
     * @param pointA  Premier point WGS-84.
     * @param pointB  Second point WGS-84.
     * @return Distance en mètres.
     */
    static double haversineDistanceMeters(const LatLon& pointA, const LatLon& pointB);
};

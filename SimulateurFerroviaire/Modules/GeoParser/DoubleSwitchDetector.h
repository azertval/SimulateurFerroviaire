#pragma once
/**
 * @file  DoubleSwitchDetector.h
 * @brief Phases 7 et 8 — détection des doubles aiguilles et validation CDC.
 *
 * Phase 7 (detectAndAbsorb) : Identifie les clusters d'aiguilles composées reliées
 *   par un court segment non-root. Absorbe le StraightBlock intermédiaire :
 *   son ID est remplacé par l'ID du partenaire dans les pointeurs de branche.
 *
 * Phase 8 (validateSwitches) : Vérifie les contraintes CDC géométriques
 *   (longueurs minimales de branches). Les violations sont LOG_WARNING — non-bloquant.
 */
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "./Graph/TopologyGraph.h"
#include "./Models/StraightBlock.h"
#include "./Models/SwitchBlock.h"
#include "./Enums/GeoParserEnums.h"
#include "Engine/Core/Logger/Logger.h"

class DoubleSwitchDetector
{
public:
    DoubleSwitchDetector(
        Logger&                                         logger,
        std::vector<SwitchBlock>&                       switches,
        std::vector<StraightBlock>&                     straights,
        const std::unordered_map<std::string, int>&     switchIdToNodeId,
        const TopologyGraph&                            topologyGraph,
        double                                          doubleLinkMaxMeters   = ParserDefaultValues::DOUBLE_LINK_MAX_METERS,
        double                                          minBranchLengthMeters = ParserDefaultValues::MIN_BRANCH_LENGTH_METERS);

    /** Phase 7 — détection et absorption (modifie les listes en place). */
    void detectAndAbsorb();

    /** Phase 8 — validation CDC (LOG_WARNING uniquement, non-bloquant). */
    void validateSwitches();

private:
    Logger&                     m_logger;
    std::vector<SwitchBlock>&   m_switches;
    std::vector<StraightBlock>& m_straights;
    const std::unordered_map<std::string, int>& m_switchIdToNodeId;
    const TopologyGraph&        m_topologyGraph;
    double                      m_doubleLinkMaxMeters;
    double                      m_minBranchLengthMeters;

    std::vector<std::vector<std::string>> findDoubleSwitchClusters(
        const std::unordered_map<std::string, SwitchBlock*>&   switchIndex,
        const std::unordered_map<std::string, StraightBlock*>& straightIndex);

    double estimateAccessibleChainLength(
        const std::string&                                     branchId,
        int                                                    junctionNodeId,
        const std::unordered_map<std::string, StraightBlock*>& straightIndex) const;

    void absorbLinkSegment(
        const std::string&                               switchIdA,
        const std::string&                               switchIdB,
        const std::string&                               linkSegmentId,
        std::unordered_map<std::string, SwitchBlock*>&   switchIndex,
        std::unordered_map<std::string, StraightBlock*>& straightIndex,
        std::set<std::string>&                           segmentsToRemove);
};

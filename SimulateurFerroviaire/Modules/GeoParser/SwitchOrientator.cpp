/**
 * @file  SwitchOrientator.cpp
 * @brief Implémentation des phases 6, 6b, 6c et 6d — orientation des aiguillages.
 */

#include "SwitchOrientator.h"

#include <algorithm>
#include <set>
#include <utility>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <cmath>

#include "./Utils/GeometryUtils.h"


 // =============================================================================
 // Construction
 // =============================================================================

SwitchOrientator::SwitchOrientator(Logger& logger,
    TopologyExtractResult& topoResult,
    int                    utmZoneNumber,
    bool                   isNorthernHemisphere,
    double                 doubleLinkMaxMeters,
    double                 branchTipDistanceMeters)
    : m_logger(logger)
    , m_topoResult(topoResult)
    , m_utmZoneNumber(utmZoneNumber)
    , m_isNorthernHemisphere(isNorthernHemisphere)
    , m_doubleLinkMaxMeters(doubleLinkMaxMeters)
    , m_branchTipDistanceMeters(branchTipDistanceMeters)
{
}


// =============================================================================
// API publique
// =============================================================================

void SwitchOrientator::orient()
{
    std::unordered_map<std::string, StraightBlock*> straightById;
    for (auto& straight : m_topoResult.straights)
        straightById[straight.getId()] = &straight;

    int orientedCount = 0;
    int skippedCount = 0;

    for (auto& sw : m_topoResult.switches)
    {
        if (static_cast<int>(sw.getBranchIds().size()) != NodeDegreeThresholds::SWITCH_PORT_COUNT)
        {
            LOG_WARNING(m_logger,
                "Aiguillage " + sw.getId() + " ignoré : degré "
                + std::to_string(sw.getBranchIds().size()) + " ≠ 3");
            ++skippedCount;
            continue;
        }

        orientThreePortSwitch(sw, straightById);
        computeBranchTipPoints(sw, straightById);
        sw.computeTotalLength();
        ++orientedCount;
    }

    LOG_INFO(m_logger,
        "Phase 6 terminée — " + std::to_string(orientedCount)
        + " orienté(s), " + std::to_string(skippedCount) + " ignoré(s)");

    alignDoubleSwitchRoles(straightById);
    enforceCrossoverConsistency();

    LOG_INFO(m_logger, "Phases 6b+6c terminées");

    trimStraightOverlaps(straightById);

    LOG_INFO(m_logger, "Phase 6d terminée — chevauchements junction/straight supprimés");
}


// =============================================================================
// Phase 6 — Orientation 3-branches
// =============================================================================

void SwitchOrientator::orientThreePortSwitch(
    SwitchBlock& sw,
    const std::unordered_map<std::string, StraightBlock*>& straightById)
{
    const LatLon& junction = sw.getJunctionCoordinate();

    struct BranchInfo { std::string id; CoordinateXY dir; };

    std::vector<BranchInfo> branches;
    branches.reserve(3);
    for (const auto& branchId : sw.getBranchIds())
        branches.push_back({ branchId, computeBranchDirectionVector(branchId, junction, straightById) });

    // Paire de sorties = angle mutuel minimal
    std::size_t exitA = 0, exitB = 1;
    double minAngle = GeometryUtils::angleBetweenVectorsPiFallback(
        branches[0].dir, branches[1].dir);

    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = i + 1; j < 3; ++j)
        {
            const double a = GeometryUtils::angleBetweenVectorsPiFallback(
                branches[i].dir, branches[j].dir);
            if (a < minAngle) { minAngle = a; exitA = i; exitB = j; }
        }

    const std::size_t rootIdx = 3 - exitA - exitB;
    const CoordinateXY antiRoot{
        -branches[rootIdx].dir.x,
        -branches[rootIdx].dir.y
    };

    const double dotA = branches[exitA].dir.x * antiRoot.x + branches[exitA].dir.y * antiRoot.y;
    const double dotB = branches[exitB].dir.x * antiRoot.x + branches[exitB].dir.y * antiRoot.y;

    const std::string normalId = (dotA >= dotB) ? branches[exitA].id : branches[exitB].id;
    const std::string deviationId = (dotA >= dotB) ? branches[exitB].id : branches[exitA].id;

    sw.orient(branches[rootIdx].id, normalId, deviationId);
}

void SwitchOrientator::computeBranchTipPoints(
    SwitchBlock& sw,
    const std::unordered_map<std::string, StraightBlock*>& straightById)
{
    const LatLon& junction = sw.getJunctionCoordinate();

    sw.setTips(
        interpolateTipPoint(sw.getRootBranchId().value_or(""), junction, m_branchTipDistanceMeters, straightById),
        interpolateTipPoint(sw.getNormalBranchId().value_or(""), junction, m_branchTipDistanceMeters, straightById),
        interpolateTipPoint(sw.getDeviationBranchId().value_or(""), junction, m_branchTipDistanceMeters, straightById)
    );
}

std::optional<LatLon> SwitchOrientator::interpolateTipPoint(
    const std::string& straightId,
    const LatLon& junctionCoord,
    double distanceMeters,
    const std::unordered_map<std::string, StraightBlock*>& straightById)
{
    if (straightId.empty()) return std::nullopt;

    auto it = straightById.find(straightId);
    if (it == straightById.end()) return std::nullopt;

    const StraightBlock& straight = *it->second;
    if (straight.getCoordinates().size() < 2) return std::nullopt;

    const LatLon& first = straight.getCoordinates().front();
    const LatLon& last = straight.getCoordinates().back();

    const double dFirst = (first.latitude - junctionCoord.latitude) * (first.latitude - junctionCoord.latitude)
        + (first.longitude - junctionCoord.longitude) * (first.longitude - junctionCoord.longitude);
    const double dLast = (last.latitude - junctionCoord.latitude) * (last.latitude - junctionCoord.latitude)
        + (last.longitude - junctionCoord.longitude) * (last.longitude - junctionCoord.longitude);

    std::vector<LatLon> oriented = straight.getCoordinates();
    if (dFirst > dLast) std::reverse(oriented.begin(), oriented.end());

    std::vector<CoordinateXY> metric =
        GeometryUtils::convertPolylineToMetric(oriented, m_utmZoneNumber, m_isNorthernHemisphere);

    const double totalLength = GeometryUtils::computePolylineLengthMeters(metric);
    if (totalLength <= GeometricTolerances::EMPTY_LINE_LENGTH_METERS) return std::nullopt;

    auto tipOpt = GeometryUtils::pointAtDistanceAlongLine(metric, std::min(distanceMeters, totalLength));
    if (!tipOpt) return std::nullopt;

    return GeometryUtils::metricUtmToWgs84(*tipOpt, m_utmZoneNumber, m_isNorthernHemisphere);
}


// =============================================================================
// Phase 6b — Alignement des rôles pour les futurs doubles
// =============================================================================

void SwitchOrientator::alignDoubleSwitchRoles(
    const std::unordered_map<std::string, StraightBlock*>& straightById)
{
    std::unordered_map<std::string, SwitchBlock*> switchById;
    for (auto& sw : m_topoResult.switches)
        if (sw.isOriented()) switchById[sw.getId()] = &sw;

    // segmentId → liste des switches qui l'ont en branche (non-root)
    std::unordered_map<std::string, std::vector<SwitchBlock*>> segToSwitches;
    for (auto& [swId, swPtr] : switchById)
        for (const auto& bid : swPtr->getBranchIds())
            segToSwitches[bid].push_back(swPtr);

    for (auto& [segId, switchList] : segToSwitches)
    {
        if (static_cast<int>(switchList.size()) != NodeDegreeThresholds::CROSSOVER_SHARED_BRANCH_COUNT)
            continue;

        auto stIt = straightById.find(segId);
        if (stIt == straightById.end() || stIt->second->getLengthMeters() > m_doubleLinkMaxMeters)
            continue;

        SwitchBlock* refPtr = (switchList[0]->getId() < switchList[1]->getId()) ? switchList[0] : switchList[1];
        SwitchBlock* otherPtr = (switchList[0]->getId() < switchList[1]->getId()) ? switchList[1] : switchList[0];

        if (refPtr->getRootBranchId() == segId || otherPtr->getRootBranchId() == segId) continue;

        const bool refIsNormal = (refPtr->getNormalBranchId() == segId);
        const bool otherIsNormal = (otherPtr->getNormalBranchId() == segId);

        if (refIsNormal != otherIsNormal)
        {
            otherPtr->swapNormalDeviation();
            LOG_DEBUG(m_logger,
                "Phase 6b — rôles échangés sur " + otherPtr->getId()
                + " pour segment " + segId);
        }
    }
}


// =============================================================================
// Phase 6c — Cohérence des croisements mécaniques
// =============================================================================

void SwitchOrientator::enforceCrossoverConsistency()
{
    std::vector<SwitchBlock*> oriented;
    for (auto& sw : m_topoResult.switches)
        if (sw.isOriented()) oriented.push_back(&sw);

    for (std::size_t i = 0; i < oriented.size(); ++i)
    {
        for (std::size_t j = i + 1; j < oriented.size(); ++j)
        {
            SwitchBlock* swA = oriented[i];
            SwitchBlock* swB = oriented[j];

            std::vector<std::string> shared;
            for (const auto& bId : swA->getBranchIds())
                if (std::find(swB->getBranchIds().begin(), swB->getBranchIds().end(), bId)
                    != swB->getBranchIds().end())
                    shared.push_back(bId);

            if (static_cast<int>(shared.size()) != NodeDegreeThresholds::CROSSOVER_SHARED_BRANCH_COUNT)
                continue;
            if (swA->getRootBranchId() == swB->getRootBranchId()) continue;

            for (const auto& sharedId : shared)
            {
                if (swA->getRootBranchId() == sharedId || swB->getRootBranchId() == sharedId)
                    continue;

                if (swA->getNormalBranchId() == sharedId) swA->swapNormalDeviation();
                if (swB->getNormalBranchId() == sharedId) swB->swapNormalDeviation();

                LOG_DEBUG(m_logger,
                    "Phase 6c — croisement " + swA->getId() + "↔" + swB->getId()
                    + " : " + sharedId + " forcé en DEVIATION");
            }
        }
    }
}


// =============================================================================
// Phase 6d — Suppression des chevauchements junction / straight
// =============================================================================

void SwitchOrientator::trimStraightOverlaps(
    std::unordered_map<std::string, StraightBlock*>& straightById)
{
    // Garde : éviter de retailler deux fois le même bout d'un Straight
    std::set<std::pair<std::string, bool>> processed;

    int trimCount = 0;

    for (auto& sw : m_topoResult.switches)
    {
        if (!sw.isOriented()) continue;

        const LatLon& junction = sw.getJunctionCoordinate();

        const std::optional<std::string>* branchOpts[3] = {
            &sw.getRootBranchId(),
            &sw.getNormalBranchId(),
            &sw.getDeviationBranchId()
        };

        for (const auto* branchOpt : branchOpts)
        {
            if (!branchOpt || !branchOpt->has_value()) continue;
            const std::string& branchId = branchOpt->value();

            auto it = straightById.find(branchId);
            if (it == straightById.end()) continue;
            StraightBlock& straight = *it->second;
            if (straight.getCoordinates().size() < 2) continue;

            // 1. Quel bout est côté jonction ?
            const LatLon& front = straight.getCoordinates().front();
            const LatLon& back = straight.getCoordinates().back();

            const double dFront = (front.latitude - junction.latitude) * (front.latitude - junction.latitude)
                + (front.longitude - junction.longitude) * (front.longitude - junction.longitude);
            const double dBack = (back.latitude - junction.latitude) * (back.latitude - junction.latitude)
                + (back.longitude - junction.longitude) * (back.longitude - junction.longitude);

            const bool junctionAtFront = (dFront <= dBack);
            const auto key = std::make_pair(branchId, junctionAtFront);
            if (processed.count(key)) continue;
            processed.insert(key);

            // 2. Mise en métrique, jonction en tête
            std::vector<LatLon> workCoords = straight.getCoordinates();
            if (!junctionAtFront) std::reverse(workCoords.begin(), workCoords.end());

            std::vector<CoordinateXY> metric =
                GeometryUtils::convertPolylineToMetric(workCoords, m_utmZoneNumber, m_isNorthernHemisphere);

            // 3. Parcourir jusqu'à branchTipDistanceMeters
            double       accumulated = 0.0;
            std::size_t  trimSegIdx = 0;
            CoordinateXY trimPoint = metric.front();
            bool         foundTrim = false;

            for (std::size_t k = 1; k < metric.size(); ++k)
            {
                const double dx = metric[k].x - metric[k - 1].x;
                const double dy = metric[k].y - metric[k - 1].y;
                const double segLen = std::hypot(dx, dy);

                if (accumulated + segLen >= m_branchTipDistanceMeters)
                {
                    const double ratio = (m_branchTipDistanceMeters - accumulated) / segLen;
                    trimPoint = CoordinateXY(metric[k - 1].x + ratio * dx,
                        metric[k - 1].y + ratio * dy);
                    trimSegIdx = k - 1;
                    foundTrim = true;
                    break;
                }
                accumulated += segLen;
                trimSegIdx = k;
            }

            if (!foundTrim)
            {
                LOG_WARNING(m_logger,
                    "Phase 6d — Straight " + branchId + " trop court pour être trimé côté "
                    + sw.getId() + " (" + std::to_string(static_cast<int>(straight.getLengthMeters()))
                    + " m < " + std::to_string(static_cast<int>(m_branchTipDistanceMeters)) + " m tip)");
                continue;
            }

            // 4. Polyligne retaillée
            std::vector<CoordinateXY> trimmedMetric;
            trimmedMetric.push_back(trimPoint);
            for (std::size_t k = trimSegIdx + 1; k < metric.size(); ++k)
                trimmedMetric.push_back(metric[k]);

            if (trimmedMetric.size() < 2)
            {
                LOG_WARNING(m_logger,
                    "Phase 6d — Straight " + branchId
                    + " réduit à un seul point après trim côté " + sw.getId() + " — ignoré");
                continue;
            }

            // 5. Reconversion WGS-84 + restauration de l'orientation d'origine
            std::vector<LatLon> trimmedCoords =
                GeometryUtils::convertPolylineToWgs84(trimmedMetric, m_utmZoneNumber, m_isNorthernHemisphere);

            if (!junctionAtFront) std::reverse(trimmedCoords.begin(), trimmedCoords.end());

            straight.setCoordinates(std::move(trimmedCoords));  // recalcule lengthMeters

            ++trimCount;
            LOG_DEBUG(m_logger,
                "Phase 6d — Straight " + branchId + " : "
                + std::to_string(static_cast<int>(m_branchTipDistanceMeters))
                + " m retaillés côté " + sw.getId()
                + " (jonction " + (junctionAtFront ? "front" : "back") + ")");
        }
    }

    LOG_INFO(m_logger,
        "Phase 6d — " + std::to_string(trimCount) + " extrémité(s) retaillée(s)");
}


// =============================================================================
// Utilitaire géométrique
// =============================================================================

CoordinateXY SwitchOrientator::computeBranchDirectionVector(
    const std::string& straightId,
    const LatLon& junctionCoord,
    const std::unordered_map<std::string, StraightBlock*>& straightById) const
{
    auto it = straightById.find(straightId);
    if (it == straightById.end() || it->second->getCoordinates().size() < 2)
        return CoordinateXY(0.0, 0.0);

    const auto& coords = it->second->getCoordinates();
    const LatLon& first = coords.front();
    const LatLon& last = coords.back();

    const double dFirst = (first.latitude - junctionCoord.latitude) * (first.latitude - junctionCoord.latitude)
        + (first.longitude - junctionCoord.longitude) * (first.longitude - junctionCoord.longitude);
    const double dLast = (last.latitude - junctionCoord.latitude) * (last.latitude - junctionCoord.latitude)
        + (last.longitude - junctionCoord.longitude) * (last.longitude - junctionCoord.longitude);

    const LatLon& distal = (dFirst < dLast) ? last : first;
    return CoordinateXY(distal.latitude - junctionCoord.latitude,
        distal.longitude - junctionCoord.longitude);
}
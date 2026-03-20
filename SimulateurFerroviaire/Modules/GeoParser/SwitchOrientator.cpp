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
        straightById[straight.id] = &straight;

    int orientedCount = 0;
    int skippedCount = 0;

    for (auto& switchBlock : m_topoResult.switches)
    {
        if (static_cast<int>(switchBlock.branchIds.size())
            != NodeDegreeThresholds::SWITCH_PORT_COUNT)
        {
            LOG_WARNING(m_logger,
                "Aiguillage " + switchBlock.id + " ignoré : degré "
                + std::to_string(switchBlock.branchIds.size()) + " ≠ 3");
            ++skippedCount;
            continue;
        }

        orientThreePortSwitch(switchBlock, straightById);
        computeBranchTipPoints(switchBlock, straightById);
        switchBlock.computeTotalLength();
        ++orientedCount;
    }

    LOG_INFO(m_logger,
        "Phase 6 terminée — " + std::to_string(orientedCount)
        + " orienté(s), " + std::to_string(skippedCount) + " ignoré(s)");

    alignDoubleSwitchRoles(straightById);
    enforceCrossoverConsistency();

    LOG_INFO(m_logger, "Phases 6b+6c terminées");

    // Phase 6d — doit être exécutée après que toutes les orientations
    // (6, 6b, 6c) sont définitives, car elle utilise les branchIds finaux.
    trimStraightOverlaps(straightById);

    LOG_INFO(m_logger, "Phase 6d terminée — chevauchements junction/straight supprimés");
}


// =============================================================================
// Phase 6 — Orientation 3-branches
// =============================================================================

void SwitchOrientator::orientThreePortSwitch(
    SwitchBlock& switchBlock,
    const std::unordered_map<std::string, StraightBlock*>& straightById)
{
    const LatLon& junctionCoord = switchBlock.junctionCoordinate;

    struct BranchInfo
    {
        std::string  branchId;
        CoordinateXY directionVector;
    };

    std::vector<BranchInfo> branchInfos;
    branchInfos.reserve(3);
    for (const auto& branchId : switchBlock.branchIds)
    {
        branchInfos.push_back({
            branchId,
            computeBranchDirectionVector(branchId, junctionCoord, straightById)
            });
    }

    // Paire de sorties = angle mutuel minimal
    std::size_t exitA = 0, exitB = 1;
    double minimumAngle = GeometryUtils::angleBetweenVectorsPiFallback(
        branchInfos[0].directionVector, branchInfos[1].directionVector);

    for (std::size_t i = 0; i < 3; ++i)
    {
        for (std::size_t j = i + 1; j < 3; ++j)
        {
            const double angle = GeometryUtils::angleBetweenVectorsPiFallback(
                branchInfos[i].directionVector, branchInfos[j].directionVector);
            if (angle < minimumAngle)
            {
                minimumAngle = angle;
                exitA = i;
                exitB = j;
            }
        }
    }

    const std::size_t rootIndex = 3 - exitA - exitB;
    const CoordinateXY antiRootVector{
        -branchInfos[rootIndex].directionVector.x,
        -branchInfos[rootIndex].directionVector.y
    };

    const double dotA = branchInfos[exitA].directionVector.x * antiRootVector.x
        + branchInfos[exitA].directionVector.y * antiRootVector.y;
    const double dotB = branchInfos[exitB].directionVector.x * antiRootVector.x
        + branchInfos[exitB].directionVector.y * antiRootVector.y;

    std::string normalId, deviationId;
    if (std::abs(dotA - dotB) < GeometricTolerances::ZERO_VECTOR_MAGNITUDE)
    {
        if (branchInfos[exitA].branchId < branchInfos[exitB].branchId)
        {
            normalId = branchInfos[exitA].branchId; deviationId = branchInfos[exitB].branchId;
        }
        else
        {
            normalId = branchInfos[exitB].branchId; deviationId = branchInfos[exitA].branchId;
        }
    }
    else if (dotA >= dotB)
    {
        normalId = branchInfos[exitA].branchId; deviationId = branchInfos[exitB].branchId;
    }
    else
    {
        normalId = branchInfos[exitB].branchId; deviationId = branchInfos[exitA].branchId;
    }

    switchBlock.rootBranchId = branchInfos[rootIndex].branchId;
    switchBlock.normalBranchId = normalId;
    switchBlock.deviationBranchId = deviationId;
    switchBlock.branchIds = { branchInfos[rootIndex].branchId, normalId, deviationId };

    LOG_DEBUG(m_logger,
        switchBlock.id + " orienté — root=" + branchInfos[rootIndex].branchId
        + ", normal=" + normalId + ", deviation=" + deviationId);
}

void SwitchOrientator::computeBranchTipPoints(
    SwitchBlock& switchBlock,
    const std::unordered_map<std::string, StraightBlock*>& straightById)
{
    if (!switchBlock.isOriented()) return;

    switchBlock.tipOnRoot = interpolateTipPoint(*switchBlock.rootBranchId,
        switchBlock.junctionCoordinate, m_branchTipDistanceMeters, straightById);
    switchBlock.tipOnNormal = interpolateTipPoint(*switchBlock.normalBranchId,
        switchBlock.junctionCoordinate, m_branchTipDistanceMeters, straightById);
    switchBlock.tipOnDeviation = interpolateTipPoint(*switchBlock.deviationBranchId,
        switchBlock.junctionCoordinate, m_branchTipDistanceMeters, straightById);
}

std::optional<LatLon> SwitchOrientator::interpolateTipPoint(
    const std::string& straightId,
    const LatLon& junctionCoord,
    double                                                distanceMeters,
    const std::unordered_map<std::string, StraightBlock*>& straightById)
{
    auto it = straightById.find(straightId);
    if (it == straightById.end() || it->second->coordinates.empty()) return std::nullopt;

    const StraightBlock& straight = *it->second;
    const LatLon& first = straight.coordinates.front();
    const LatLon& last = straight.coordinates.back();

    const double dFirst = (first.latitude - junctionCoord.latitude) * (first.latitude - junctionCoord.latitude)
        + (first.longitude - junctionCoord.longitude) * (first.longitude - junctionCoord.longitude);
    const double dLast = (last.latitude - junctionCoord.latitude) * (last.latitude - junctionCoord.latitude)
        + (last.longitude - junctionCoord.longitude) * (last.longitude - junctionCoord.longitude);

    std::vector<LatLon> orientedCoords = straight.coordinates;
    if (dFirst > dLast) std::reverse(orientedCoords.begin(), orientedCoords.end());

    std::vector<CoordinateXY> metric =
        GeometryUtils::convertPolylineToMetric(orientedCoords, m_utmZoneNumber, m_isNorthernHemisphere);

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
        if (sw.isOriented()) switchById[sw.id] = &sw;

    std::unordered_map<std::string, std::vector<SwitchBlock*>> segmentToSwitches;
    for (auto& [swId, swPtr] : switchById)
        for (const auto& branchId : swPtr->branchIds)
            segmentToSwitches[branchId].push_back(swPtr);

    for (auto& [segmentId, switchList] : segmentToSwitches)
    {
        if (static_cast<int>(switchList.size()) != NodeDegreeThresholds::CROSSOVER_SHARED_BRANCH_COUNT)
            continue;

        auto stIt = straightById.find(segmentId);
        if (stIt == straightById.end() || stIt->second->lengthMeters > m_doubleLinkMaxMeters)
            continue;

        SwitchBlock* refPtr = (switchList[0]->id < switchList[1]->id) ? switchList[0] : switchList[1];
        SwitchBlock* otherPtr = (switchList[0]->id < switchList[1]->id) ? switchList[1] : switchList[0];

        if (refPtr->rootBranchId == segmentId || otherPtr->rootBranchId == segmentId) continue;

        const bool refIsNormal = (refPtr->normalBranchId == segmentId);
        const bool otherIsNormal = (otherPtr->normalBranchId == segmentId);

        if (refIsNormal != otherIsNormal)
        {
            std::swap(otherPtr->normalBranchId, otherPtr->deviationBranchId);
            std::swap(otherPtr->tipOnNormal, otherPtr->tipOnDeviation);
            otherPtr->branchIds = {
                *otherPtr->rootBranchId, *otherPtr->normalBranchId, *otherPtr->deviationBranchId
            };
            LOG_DEBUG(m_logger,
                "Phase 6b — rôles échangés sur " + otherPtr->id
                + " pour segment " + segmentId);
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
            SwitchBlock* switchA = oriented[i];
            SwitchBlock* switchB = oriented[j];

            std::vector<std::string> shared;
            for (const auto& bId : switchA->branchIds)
                if (std::find(switchB->branchIds.begin(), switchB->branchIds.end(), bId)
                    != switchB->branchIds.end())
                    shared.push_back(bId);

            if (static_cast<int>(shared.size()) != NodeDegreeThresholds::CROSSOVER_SHARED_BRANCH_COUNT)
                continue;
            if (switchA->rootBranchId == switchB->rootBranchId) continue;

            for (const auto& sharedId : shared)
            {
                if (switchA->rootBranchId == sharedId || switchB->rootBranchId == sharedId)
                    continue;

                if (switchA->normalBranchId == sharedId)
                {
                    std::swap(switchA->normalBranchId, switchA->deviationBranchId);
                    std::swap(switchA->tipOnNormal, switchA->tipOnDeviation);
                }
                if (switchB->normalBranchId == sharedId)
                {
                    std::swap(switchB->normalBranchId, switchB->deviationBranchId);
                    std::swap(switchB->tipOnNormal, switchB->tipOnDeviation);
                }
                LOG_DEBUG(m_logger,
                    "Phase 6c — croisement " + switchA->id + "↔" + switchB->id
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
    // Garde pour éviter de retailler deux fois le même bout d'un Straight.
    // Clé : (straightId, junctionAtFront)
    //   junctionAtFront = true  → la jonction était à coordinates.front()
    //   junctionAtFront = false → la jonction était à coordinates.back()
    std::set<std::pair<std::string, bool>> processed;

    int trimCount = 0;

    for (auto& switchBlock : m_topoResult.switches)
    {
        if (!switchBlock.isOriented()) continue;

        const LatLon& junction = switchBlock.junctionCoordinate;

        // Itérer sur les 3 branches orientées
        const std::optional<std::string>* branches[3] = {
            &switchBlock.rootBranchId,
            &switchBlock.normalBranchId,
            &switchBlock.deviationBranchId
        };

        for (const auto* branchOpt : branches)
        {
            if (!branchOpt || !branchOpt->has_value()) continue;
            const std::string& branchId = branchOpt->value();

            auto it = straightById.find(branchId);
            if (it == straightById.end()) continue;
            StraightBlock& straight = *it->second;
            if (straight.coordinates.size() < 2) continue;

            // -----------------------------------------------------------------
            // 1. Déterminer quel bout est côté jonction
            // -----------------------------------------------------------------
            const LatLon& front = straight.coordinates.front();
            const LatLon& back = straight.coordinates.back();

            const double dFront = (front.latitude - junction.latitude) * (front.latitude - junction.latitude)
                + (front.longitude - junction.longitude) * (front.longitude - junction.longitude);
            const double dBack = (back.latitude - junction.latitude) * (back.latitude - junction.latitude)
                + (back.longitude - junction.longitude) * (back.longitude - junction.longitude);

            const bool junctionAtFront = (dFront <= dBack);

            auto key = std::make_pair(branchId, junctionAtFront);
            if (processed.count(key)) continue;
            processed.insert(key);

            // -----------------------------------------------------------------
            // 2. Travailler en métrique, jonction en tête
            // -----------------------------------------------------------------
            std::vector<LatLon> workCoords = straight.coordinates;
            if (!junctionAtFront)
                std::reverse(workCoords.begin(), workCoords.end());

            std::vector<CoordinateXY> metric =
                GeometryUtils::convertPolylineToMetric(
                    workCoords, m_utmZoneNumber, m_isNorthernHemisphere);

            // -----------------------------------------------------------------
            // 3. Parcourir les segments jusqu'à branchTipDistanceMeters
            // -----------------------------------------------------------------
            double       accumulated = 0.0;
            std::size_t  trimSegIdx = 0;
            CoordinateXY trimPoint = metric.front();
            bool         foundTrim = false;

            for (std::size_t i = 1; i < metric.size(); ++i)
            {
                const double dx = metric[i].x - metric[i - 1].x;
                const double dy = metric[i].y - metric[i - 1].y;
                const double segLen = std::hypot(dx, dy);

                if (accumulated + segLen >= m_branchTipDistanceMeters)
                {
                    const double ratio = (m_branchTipDistanceMeters - accumulated) / segLen;
                    trimPoint = CoordinateXY(
                        metric[i - 1].x + ratio * dx,
                        metric[i - 1].y + ratio * dy);
                    trimSegIdx = i - 1;
                    foundTrim = true;
                    break;
                }
                accumulated += segLen;
                trimSegIdx = i;
            }

            if (!foundTrim)
            {
                LOG_WARNING(m_logger,
                    "Phase 6d — Straight " + branchId
                    + " trop court pour être trimé côté " + switchBlock.id
                    + " (" + std::to_string(static_cast<int>(straight.lengthMeters)) + " m < "
                    + std::to_string(static_cast<int>(m_branchTipDistanceMeters)) + " m tip)");
                continue;
            }

            // -----------------------------------------------------------------
            // 4. Construire la polyligne retaillée : trimPoint + sommets restants
            // -----------------------------------------------------------------
            std::vector<CoordinateXY> trimmedMetric;
            trimmedMetric.push_back(trimPoint);
            for (std::size_t i = trimSegIdx + 1; i < metric.size(); ++i)
                trimmedMetric.push_back(metric[i]);

            if (trimmedMetric.size() < 2)
            {
                LOG_WARNING(m_logger,
                    "Phase 6d — Straight " + branchId
                    + " réduit à un seul point après trim côté " + switchBlock.id
                    + " — ignoré");
                continue;
            }

            // -----------------------------------------------------------------
            // 5. Reconvertir en WGS-84 et restaurer l'orientation d'origine
            // -----------------------------------------------------------------
            std::vector<LatLon> trimmedCoords =
                GeometryUtils::convertPolylineToWgs84(
                    trimmedMetric, m_utmZoneNumber, m_isNorthernHemisphere);

            // La jonction était au back → le résultat doit rester jonction au back
            if (!junctionAtFront)
                std::reverse(trimmedCoords.begin(), trimmedCoords.end());

            straight.coordinates = std::move(trimmedCoords);
            straight.recomputeGeodesicLength();

            ++trimCount;
            LOG_DEBUG(m_logger,
                "Phase 6d — Straight " + branchId + " : "
                + std::to_string(static_cast<int>(m_branchTipDistanceMeters))
                + " m retaillés côté " + switchBlock.id
                + " (jonction " + (junctionAtFront ? "front" : "back") + ")");
        }
    }

    LOG_INFO(m_logger,
        "Phase 6d — " + std::to_string(trimCount) + " extrémité(s) retaillée(s)");
}


// =============================================================================
// Utilitaires
// =============================================================================

CoordinateXY SwitchOrientator::computeBranchDirectionVector(
    const std::string& straightId,
    const LatLon& junctionCoord,
    const std::unordered_map<std::string, StraightBlock*>& straightById) const
{
    auto it = straightById.find(straightId);
    if (it == straightById.end() || it->second->coordinates.size() < 2)
        return CoordinateXY(0.0, 0.0);

    const StraightBlock& straight = *it->second;
    const LatLon& first = straight.coordinates.front();
    const LatLon& last = straight.coordinates.back();

    const double dFirst = (first.latitude - junctionCoord.latitude) * (first.latitude - junctionCoord.latitude)
        + (first.longitude - junctionCoord.longitude) * (first.longitude - junctionCoord.longitude);
    const double dLast = (last.latitude - junctionCoord.latitude) * (last.latitude - junctionCoord.latitude)
        + (last.longitude - junctionCoord.longitude) * (last.longitude - junctionCoord.longitude);

    const LatLon& distal = (dFirst < dLast) ? last : first;
    return CoordinateXY(distal.latitude - junctionCoord.latitude,
        distal.longitude - junctionCoord.longitude);
}
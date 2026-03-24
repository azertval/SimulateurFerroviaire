/**
 * @file  SwitchBlock.cpp
 * @brief Implémentation du modèle d'aiguillage SwitchBlock.
 */

#include "SwitchBlock.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <stdexcept>


namespace
{
    constexpr double EARTH_RADIUS_METERS = 6371000.0;
    constexpr double DEGREES_TO_RADIANS = 3.14159265358979323846 / 180.0;
}


// =============================================================================
// Construction
// =============================================================================

SwitchBlock::SwitchBlock(std::string              switchId,
    CoordinateLatLon                   junctionCoord,
    std::vector<std::string> initialBranchIds)
    : m_junctionCoordinate(junctionCoord)
    , m_branchIds(std::move(initialBranchIds))
{
    m_id = std::move(switchId);
}

void SwitchBlock::setBranchPointers(SwitchBranches branches)
{
    m_branches = branches;
    LOG_DEBUG(m_logger, m_id + " — root="
        + (m_branches.root ? m_branches.root->getId() : "null")
        + " normal="
        + (m_branches.normal ? m_branches.normal->getId() : "null")
        + " deviation="
        + (m_branches.deviation ? m_branches.deviation->getId() : "null"));
}


// =============================================================================
// Phase 5b
// =============================================================================

void SwitchBlock::addBranchId(const std::string& id)
{
    if (std::find(m_branchIds.begin(), m_branchIds.end(), id) == m_branchIds.end())
        m_branchIds.push_back(id);
}


// =============================================================================
// Phase 6
// =============================================================================

void SwitchBlock::orient(std::string rootId, std::string normalId, std::string deviationId)
{
    auto has = [&](const std::string& id) {
        return std::find(m_branchIds.begin(), m_branchIds.end(), id) != m_branchIds.end();
        };
    if (!has(rootId) || !has(normalId) || !has(deviationId))
        throw std::invalid_argument(
            "SwitchBlock::orient — ID absent de branchIds sur " + m_id);

    m_rootBranchId = std::move(rootId);
    m_normalBranchId = std::move(normalId);
    m_deviationBranchId = std::move(deviationId);
}

void SwitchBlock::setTips(std::optional<CoordinateLatLon> tipRoot,
    std::optional<CoordinateLatLon> tipNormal,
    std::optional<CoordinateLatLon> tipDeviation)
{
    m_tipOnRoot = std::move(tipRoot);
    m_tipOnNormal = std::move(tipNormal);
    m_tipOnDeviation = std::move(tipDeviation);
}

void SwitchBlock::swapNormalDeviation()
{
    std::swap(m_normalBranchId, m_deviationBranchId);
    std::swap(m_tipOnNormal, m_tipOnDeviation);
    std::swap(m_absorbedNormalCoordinates, m_absorbedDeviationCoordinates);
    std::swap(m_doubleOnNormal, m_doubleOnDeviation);
}

void SwitchBlock::computeTotalLength()
{
    if (!isOriented() || !m_tipOnRoot || !m_tipOnNormal || !m_tipOnDeviation)
        return;

    const double rootLeg = haversineDistanceMeters(*m_tipOnRoot, m_junctionCoordinate);
    const double normalLeg = haversineDistanceMeters(m_junctionCoordinate, *m_tipOnNormal);
    const double deviationLeg = haversineDistanceMeters(m_junctionCoordinate, *m_tipOnDeviation);

    m_totalLengthMeters = rootLeg + (((normalLeg) > (deviationLeg)) ? (normalLeg) : (deviationLeg));
}


// =============================================================================
// Phase 7
// =============================================================================

void SwitchBlock::absorbLink(const std::string& linkId,
    const std::string& partnerId,
    std::vector<CoordinateLatLon> linkCoordinates)
{
    // Remplace le segment de liaison dans la liste de branches
    for (auto& bid : m_branchIds)
        if (bid == linkId) { bid = partnerId; break; }

    // Le dernier point de la polyligne orientée = jonction du partenaire = nouveau tip CDC
    const CoordinateLatLon tipFar = linkCoordinates.empty() ? m_junctionCoordinate : linkCoordinates.back();

    if (m_normalBranchId == linkId)
    {
        m_normalBranchId = partnerId;
        m_tipOnNormal = tipFar;
        m_doubleOnNormal = partnerId;
        m_absorbedNormalCoordinates = std::move(linkCoordinates);
    }
    else if (m_deviationBranchId == linkId)
    {
        m_deviationBranchId = partnerId;
        m_tipOnDeviation = tipFar;
        m_doubleOnDeviation = partnerId;
        m_absorbedDeviationCoordinates = std::move(linkCoordinates);
    }
}

void SwitchBlock::setActiveBranch(ActiveBranch branch, bool propagate)
{
    m_activeBranch = branch;
    LOG_DEBUG(m_logger, m_id + " set → " + activeBranchToString());

    if (!propagate) return;

    if (auto* p = getPartnerOnNormal())    p->setActiveBranch(branch, false);
    if (auto* p = getPartnerOnDeviation()) p->setActiveBranch(branch, false);
}

ActiveBranch SwitchBlock::toggleActiveBranch(bool propagate)
{
    m_activeBranch = (m_activeBranch == ActiveBranch::NORMAL)
        ? ActiveBranch::DEVIATION : ActiveBranch::NORMAL;

    LOG_DEBUG(m_logger, m_id + " toggled → " + activeBranchToString());

    if (propagate)
    {
        if (auto* p = getPartnerOnNormal())    p->setActiveBranch(m_activeBranch, false);
        if (auto* p = getPartnerOnDeviation()) p->setActiveBranch(m_activeBranch, false);
    }

    return m_activeBranch;
}

// =============================================================================
// Affichage
// =============================================================================

std::string SwitchBlock::toString() const
{
    std::ostringstream s;
    s << "Switch(id=" << m_id;

    if (isDouble())
    {
        s << " [DOUBLE:";
        if (m_doubleOnNormal)
            s << "normal→" << *m_doubleOnNormal
            << " (" << m_absorbedNormalCoordinates.size() << " pts)";
        if (m_doubleOnDeviation)
            s << "deviation→" << *m_doubleOnDeviation
            << " (" << m_absorbedDeviationCoordinates.size() << " pts)";
        s << "]";
    }

    if (isOriented())
    {
        s << ", root=" << m_rootBranchId.value_or("?")
            << ", normal=" << m_normalBranchId.value_or("?")
            << ", deviation=" << m_deviationBranchId.value_or("?");

        if (m_totalLengthMeters)
        {
            s << std::fixed;
            s.precision(1);
            s << ", len=" << *m_totalLengthMeters << "m";
        }
    }
    else
    {
        s << std::fixed;
        s.precision(6);
        s << ", junction=(" << m_junctionCoordinate.latitude
            << ", " << m_junctionCoordinate.longitude << ")"
            << ", degree=" << m_branchIds.size();
    }

    s << ")";
    return s.str();
}


// =============================================================================
// Helpers privés
// =============================================================================

double SwitchBlock::haversineDistanceMeters(const CoordinateLatLon& a, const CoordinateLatLon& b)
{
    const double dLat = (b.latitude - a.latitude) * DEGREES_TO_RADIANS;
    const double dLon = (b.longitude - a.longitude) * DEGREES_TO_RADIANS;
    const double latA = a.latitude * DEGREES_TO_RADIANS;
    const double latB = b.latitude * DEGREES_TO_RADIANS;

    const double sinDLat = std::sin(dLat / 2.0);
    const double sinDLon = std::sin(dLon / 2.0);

    const double hav = sinDLat * sinDLat
        + std::cos(latA) * std::cos(latB) * sinDLon * sinDLon;

    return EARTH_RADIUS_METERS * 2.0 * std::atan2(std::sqrt(hav), std::sqrt(1.0 - hav));
}
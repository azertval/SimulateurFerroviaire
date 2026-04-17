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

SwitchBlock::SwitchBlock()
{
	LOG_INFO(m_logger, "Switch created");
}

// =============================================================================
// Construction
// =============================================================================

SwitchBlock::SwitchBlock(std::string              switchId,
    CoordinateLatLon         junctionWGS84,
    std::vector<std::string> branchIds)
    : m_junctionWGS84(junctionWGS84)
    , m_branchIds(std::move(branchIds))
{
    m_id = std::move(switchId);
}


// =============================================================================
// Mutations — géométrie
// =============================================================================

void SwitchBlock::setTips(std::optional<CoordinateLatLon> tipRoot,
    std::optional<CoordinateLatLon> tipNormal,
    std::optional<CoordinateLatLon> tipDeviation)
{
    m_tipOnRoot = std::move(tipRoot);
    m_tipOnNormal = std::move(tipNormal);
    m_tipOnDeviation = std::move(tipDeviation);
}

void SwitchBlock::setAbsorbedCoords(const std::string& side,
    std::vector<CoordinateLatLon> coords)
{
    if (side == "normal")
        m_absorbedNormalCoords = std::move(coords);
    else if (side == "deviation")
        m_absorbedDeviationCoords = std::move(coords);
}

void SwitchBlock::computeTotalLength()
{
    if (!isOriented() || !m_tipOnRoot || !m_tipOnNormal || !m_tipOnDeviation)
        return;

    const double rootLeg = haversineDistanceMeters(*m_tipOnRoot, m_junctionWGS84);
    const double normalLeg = haversineDistanceMeters(m_junctionWGS84, *m_tipOnNormal);
    const double deviationLeg = haversineDistanceMeters(m_junctionWGS84, *m_tipOnDeviation);

    m_totalLengthMeters = rootLeg + std::max(normalLeg, deviationLeg);
}


// =============================================================================
// Mutations — topologie IDs
// =============================================================================

void SwitchBlock::addBranchId(const std::string& id)
{
    if (std::find(m_branchIds.begin(), m_branchIds.end(), id) == m_branchIds.end())
        m_branchIds.push_back(id);
}

void SwitchBlock::orient(std::string rootId,
    std::string normalId,
    std::string deviationId)
{
    auto has = [&](const std::string& id)
        {
            return std::find(m_branchIds.begin(), m_branchIds.end(), id)
                != m_branchIds.end();
        };

    if (!has(rootId) || !has(normalId) || !has(deviationId))
        throw std::invalid_argument(
            "SwitchBlock::orient — ID absent de branchIds sur " + m_id);

    m_rootBranchId = std::move(rootId);
    m_normalBranchId = std::move(normalId);
    m_deviationBranchId = std::move(deviationId);
}

void SwitchBlock::swapNormalDeviation()
{
    std::swap(m_normalBranchId, m_deviationBranchId);
    std::swap(m_tipOnNormal, m_tipOnDeviation);
    std::swap(m_tipOnNormalUTM, m_tipOnDeviationUTM);
    std::swap(m_absorbedNormalCoords, m_absorbedDeviationCoords);
    std::swap(m_absorbedNormalCoordsUTM, m_absorbedDeviationCoordsUTM);
    std::swap(m_doubleOnNormal, m_doubleOnDeviation);
}

void SwitchBlock::absorbLink(const std::string& linkId,
    const std::string& partnerId,
    std::vector<CoordinateLatLon> linkCoordsWGS84,
    std::vector<CoordinateXY>     linkCoordsUTM)
{
    m_branchIds.erase(
        std::remove(m_branchIds.begin(), m_branchIds.end(), linkId),
        m_branchIds.end());
    addBranchId(partnerId);

    const CoordinateLatLon tipFar = linkCoordsWGS84.empty()
        ? m_junctionWGS84
        : linkCoordsWGS84.back();

    if (m_normalBranchId == linkId)
    {
        m_normalBranchId = partnerId;
        m_tipOnNormal = tipFar;
        m_doubleOnNormal = partnerId;
        m_absorbedNormalCoords = std::move(linkCoordsWGS84);
        m_absorbedNormalCoordsUTM = std::move(linkCoordsUTM);   // ← UTM
    }
    else if (m_deviationBranchId == linkId)
    {
        m_deviationBranchId = partnerId;
        m_tipOnDeviation = tipFar;
        m_doubleOnDeviation = partnerId;
        m_absorbedDeviationCoords = std::move(linkCoordsWGS84);
        m_absorbedDeviationCoordsUTM = std::move(linkCoordsUTM); // ← UTM
    }
}


// =============================================================================
// Mutations — pointeurs résolus
// =============================================================================

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

void SwitchBlock::replaceBranchPointer(ShuntingElement* oldElem,
    ShuntingElement* newElem)
{
    if (m_branches.root == oldElem) { m_branches.root = newElem; return; }
    if (m_branches.normal == oldElem) { m_branches.normal = newElem; return; }
    if (m_branches.deviation == oldElem) { m_branches.deviation = newElem; }
}


// =============================================================================
// Mutations — état opérationnel
// =============================================================================

void SwitchBlock::setActiveBranch(ActiveBranch branch, SwitchBlock* origin)
{
    m_activeBranch = branch;
    LOG_DEBUG(m_logger, m_id + " set → " + activeBranchToString());

    if (auto* p = getPartnerOnNormal())
        if (p != origin) p->setActiveBranch(branch, this);

    if (auto* p = getPartnerOnDeviation())
        if (p != origin) p->setActiveBranch(branch, this);
}

ActiveBranch SwitchBlock::toggleActiveBranch(SwitchBlock* origin)
{
    m_activeBranch = (m_activeBranch == ActiveBranch::NORMAL)
        ? ActiveBranch::DEVIATION
        : ActiveBranch::NORMAL;

    LOG_DEBUG(m_logger, m_id + " toggled → " + activeBranchToString());

    SwitchBlock* pN = getPartnerOnNormal();
    SwitchBlock* pD = getPartnerOnDeviation();

    if (pN && pN != origin) pN->setActiveBranch(m_activeBranch, this);
    if (pD && pD != origin && pD != pN) pD->setActiveBranch(m_activeBranch, this);

    return m_activeBranch;
}

// =============================================================================
// Affichage
// =============================================================================

std::string SwitchBlock::toString() const
{
    std::ostringstream s;
    s << std::fixed;
    s.precision(6);

    // -------------------------------------------------------------------------
    // Identifiant + jonction
    // -------------------------------------------------------------------------
    s << "Switch(id=" << m_id
        << ", junction=(" << m_junctionWGS84.latitude
        << ", " << m_junctionWGS84.longitude << ")";

    // -------------------------------------------------------------------------
    // Double aiguille
    // -------------------------------------------------------------------------
    if (isDouble())
    {
        s << ", [DOUBLE:";
        if (m_doubleOnNormal)
            s << " normal→" << *m_doubleOnNormal;
        if (m_doubleOnDeviation)
            s << " deviation→" << *m_doubleOnDeviation;
        s << "]";
    }

    // -------------------------------------------------------------------------
    // Orienté : branches + tips + longueur
    // -------------------------------------------------------------------------
    if (isOriented())
    {
        // Branches — ID + pointeur résolu
        auto branch = [](const std::optional<std::string>& id,
            const ShuntingElement* ptr) -> std::string
            {
                const std::string idStr = id.value_or("?");
                const std::string ptrStr = ptr ? ptr->getId() : "null";
                return idStr == ptrStr ? idStr : idStr + "→" + ptrStr;
            };

        s << ", root=" << branch(m_rootBranchId, m_branches.root)
            << ", normal=" << branch(m_normalBranchId, m_branches.normal)
            << ", deviation=" << branch(m_deviationBranchId, m_branches.deviation);

        // Tips CDC
        auto tip = [](const std::optional<CoordinateLatLon>& t) -> std::string
            {
                if (!t) return "—";
                std::ostringstream b;
                b << std::fixed;
                b.precision(6);
                b << "(" << t->latitude << ", " << t->longitude << ")";
                return b.str();
            };

        s << ", tipRoot=" << tip(m_tipOnRoot)
            << ", tipNormal=" << tip(m_tipOnNormal)
            << ", tipDeviation=" << tip(m_tipOnDeviation);

        auto tipUTM = [](const std::optional<CoordinateXY>& t) -> std::string
            {
                if (!t) return "—";
                std::ostringstream b;
                b << std::fixed;
                b.precision(1);
                b << "(" << t->x << ", " << t->y << ")";
                return b.str();
            };

        s << ", tipRootUTM=" << tipUTM(m_tipOnRootUTM)
            << ", tipNormalUTM=" << tipUTM(m_tipOnNormalUTM)
            << ", tipDeviationUTM=" << tipUTM(m_tipOnDeviationUTM);

        // Longueur totale
        if (m_totalLengthMeters)
        {
            s.precision(1);
            s << ", len=" << *m_totalLengthMeters << "m";
        }
    }
    // -------------------------------------------------------------------------
    // Non orienté : degré seulement
    // -------------------------------------------------------------------------
    else
    {
        s << ", degree=" << m_branchIds.size();
    }

    s << ")";
    return s.str();
}

void SwitchBlock::setTipsUTM(
    std::optional<CoordinateXY> tipRoot,
    std::optional<CoordinateXY> tipNormal,
    std::optional<CoordinateXY> tipDeviation)
{
    // Même structure que setTips() — assignation directe sans calcul.
    // Les valeurs viennent de Phase7_SwitchProcessor::interpolateTipUTM.
    m_tipOnRootUTM = std::move(tipRoot);
    m_tipOnNormalUTM = std::move(tipNormal);
    m_tipOnDeviationUTM = std::move(tipDeviation);
}

// =============================================================================
// Helpers privés
// =============================================================================

double SwitchBlock::haversineDistanceMeters(const CoordinateLatLon& a,
    const CoordinateLatLon& b)
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
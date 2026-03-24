/**
 * @file  StraightBlock.cpp
 * @brief Implémentation du bloc de voie droite StraightBlock.
 */

#include "StraightBlock.h"

#include <algorithm>
#include <cmath>
#include <sstream>


namespace
{
    constexpr double EARTH_RADIUS_METERS = 6371000.0;
    constexpr double DEGREES_TO_RADIANS = 3.14159265358979323846 / 180.0;

    double haversine(const CoordinateLatLon& a, const CoordinateLatLon& b)
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
}


// =============================================================================
// Construction
// =============================================================================

StraightBlock::StraightBlock(std::string                   blockId,
    std::vector<CoordinateLatLon> pointsWGS84,
    std::vector<std::string>      neighbourIds)
    : m_pointsWGS84(std::move(pointsWGS84))
    , m_neighbourIds(std::move(neighbourIds))
    , m_lengthMeters(computeGeodesicLength())
{
    std::sort(m_neighbourIds.begin(), m_neighbourIds.end());
    m_id = std::move(blockId);
}


// =============================================================================
// Mutations — géométrie
// =============================================================================

void StraightBlock::setPointsWGS84(std::vector<CoordinateLatLon> points)
{
    m_pointsWGS84 = std::move(points);
    m_lengthMeters = computeGeodesicLength();
}


// =============================================================================
// Mutations — topologie IDs
// =============================================================================

void StraightBlock::addNeighbourId(const std::string& id)
{
    const auto pos = std::lower_bound(m_neighbourIds.begin(), m_neighbourIds.end(), id);
    if (pos != m_neighbourIds.end() && *pos == id)
        return;
    m_neighbourIds.insert(pos, id);
}

void StraightBlock::replaceNeighbourId(const std::string& oldId,
    const std::string& newId)
{
    const auto pos = std::find(m_neighbourIds.begin(), m_neighbourIds.end(), oldId);
    if (pos == m_neighbourIds.end())
        return;
    m_neighbourIds.erase(pos);
    addNeighbourId(newId);
}


// =============================================================================
// Mutations — pointeurs résolus
// =============================================================================

void StraightBlock::setNeighbourPointers(StraightNeighbours neighbours)
{
    m_neighbours = neighbours;
    LOG_DEBUG(m_logger, m_id + " — prev="
        + (m_neighbours.prev ? m_neighbours.prev->getId() : "null")
        + " next="
        + (m_neighbours.next ? m_neighbours.next->getId() : "null"));
}


// =============================================================================
// Requêtes
// =============================================================================

double StraightBlock::getLengthUTM() const
{
    if (m_pointsUTM.size() < 2) return 0.0;

    double total = 0.0;
    for (size_t i = 1; i < m_pointsUTM.size(); ++i)
    {
        const double dx = m_pointsUTM[i].x - m_pointsUTM[i - 1].x;
        const double dy = m_pointsUTM[i].y - m_pointsUTM[i - 1].y;
        total += std::sqrt(dx * dx + dy * dy);
    }
    return total;
}

std::string StraightBlock::toString() const
{
    std::ostringstream s;
    s << "Straight(id=" << m_id
        << ", len=" << std::fixed;
    s.precision(1);
    s << m_lengthMeters << "m"
        << ", pts=" << m_pointsWGS84.size()
        << ", neighbours=[";
    for (size_t i = 0; i < m_neighbourIds.size(); ++i)
    {
        if (i > 0) s << ", ";
        s << m_neighbourIds[i];
    }
    s << "])";
    return s.str();
}


// =============================================================================
// Helpers privés
// =============================================================================

double StraightBlock::computeGeodesicLength() const
{
    if (m_pointsWGS84.size() < 2) return 0.0;

    double total = 0.0;
    for (size_t i = 1; i < m_pointsWGS84.size(); ++i)
        total += haversine(m_pointsWGS84[i - 1], m_pointsWGS84[i]);
    return total;
}
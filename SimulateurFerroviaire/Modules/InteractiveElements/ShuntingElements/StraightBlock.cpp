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
}


// =============================================================================
// Construction
// =============================================================================

StraightBlock::StraightBlock(std::string              blockId,
    std::vector<CoordinateLatLon>      blockCoordinates,
    std::vector<std::string> initialNeighbourIds)
    : m_coordinates(std::move(blockCoordinates))
    , m_neighbourIds(std::move(initialNeighbourIds))
    , m_lengthMeters(computeGeodesicLength())
{
    std::sort(m_neighbourIds.begin(), m_neighbourIds.end());
    m_id = std::move(blockId);
}

void StraightBlock::setNeighbourPointers(StraightNeighbours neighbours)
{
    m_neighbours = neighbours;
    LOG_DEBUG(m_logger, m_id + " — prev="
        + (m_neighbours.prev ? m_neighbours.prev->getId() : "null")
        + " next="
        + (m_neighbours.next ? m_neighbours.next->getId() : "null"));
}

// =============================================================================
// Phase 5b
// =============================================================================

void StraightBlock::addNeighbourId(const std::string& id)
{
    // Insertion triée sans doublon
    const auto pos = std::lower_bound(m_neighbourIds.begin(), m_neighbourIds.end(), id);
    if (pos != m_neighbourIds.end() && *pos == id)
        return;
    m_neighbourIds.insert(pos, id);
}

void StraightBlock::replaceNeighbourId(const std::string& oldId, const std::string& newId)
{
    const auto pos = std::find(m_neighbourIds.begin(), m_neighbourIds.end(), oldId);
    if (pos == m_neighbourIds.end())
        return;

    m_neighbourIds.erase(pos);
    addNeighbourId(newId);   // ré-insère en position triée
}


// =============================================================================
// Phase 6d
// =============================================================================

void StraightBlock::setCoordinates(std::vector<CoordinateLatLon> Coordinates)
{
    m_coordinates = std::move(Coordinates);
    m_lengthMeters = computeGeodesicLength();
}


// =============================================================================
// Affichage
// =============================================================================

std::string StraightBlock::toString() const
{
    std::ostringstream s;
    s << "Straight(id=" << m_id
        << ", len=" << std::fixed;
    s.precision(1);
    s << m_lengthMeters << "m"
        << ", Coordinates=" << m_coordinates.size()
        << ", neighbours=[";

    for (std::size_t i = 0; i < m_neighbourIds.size(); ++i)
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
    if (m_coordinates.size() < 2)
        return 0.0;

    double total = 0.0;
    for (std::size_t i = 1; i < m_coordinates.size(); ++i)
        total += haversineDistanceMeters(m_coordinates[i - 1], m_coordinates[i]);
    return total;
}

double StraightBlock::haversineDistanceMeters(const CoordinateLatLon& a, const CoordinateLatLon& b)
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
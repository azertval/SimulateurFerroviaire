/**
 * @file  StraightBlock.cpp
 * @brief Implémentation du bloc de voie droite StraightBlock.
 */

#include "StraightBlock.h"

#include <cmath>


// =============================================================================
// Constantes locales
// =============================================================================

namespace
{
    /** Rayon moyen de la Terre en mètres (utilisé dans la formule de Haversine). */
    constexpr double EARTH_RADIUS_METERS = 6371000.0;

    /** Facteur de conversion degrés → radians. */
    constexpr double DEGREES_TO_RADIANS = 3.14159265358979323846 / 180.0;
}


// =============================================================================
// Construction
// =============================================================================

StraightBlock::StraightBlock(std::string              blockId,
                              std::vector<LatLon>      blockCoords,
                              std::vector<std::string> initialNeighbourIds)
    : id(std::move(blockId))
    , coordinates(std::move(blockCoords))
    , neighbourIds(std::move(initialNeighbourIds))
    , lengthMeters(0.0)
{
    lengthMeters = computeGeodesicLength();
}


// =============================================================================
// Méthodes publiques
// =============================================================================

void StraightBlock::recomputeGeodesicLength()
{
    lengthMeters = computeGeodesicLength();
}

std::string StraightBlock::toString() const
{
    std::ostringstream stream;
    stream << "Straight(id=" << id
           << ", len=" << std::fixed;
    stream.precision(1);
    stream << lengthMeters << "m"
           << ", coords=" << coordinates.size()
           << ", neighbours=[";

    for (std::size_t index = 0; index < neighbourIds.size(); ++index)
    {
        if (index > 0)
        {
            stream << ", ";
        }
        stream << neighbourIds[index];
    }
    stream << "])";
    return stream.str();
}


// =============================================================================
// Méthodes privées
// =============================================================================

double StraightBlock::computeGeodesicLength() const
{
    if (coordinates.size() < 2)
    {
        return 0.0;
    }

    double totalLength = 0.0;
    for (std::size_t index = 1; index < coordinates.size(); ++index)
    {
        totalLength += haversineDistanceMeters(coordinates[index - 1], coordinates[index]);
    }
    return totalLength;
}

double StraightBlock::haversineDistanceMeters(const LatLon& pointA, const LatLon& pointB)
{
    const double deltaLatitude  = (pointB.latitude  - pointA.latitude)  * DEGREES_TO_RADIANS;
    const double deltaLongitude = (pointB.longitude - pointA.longitude) * DEGREES_TO_RADIANS;

    const double latitudeA = pointA.latitude * DEGREES_TO_RADIANS;
    const double latitudeB = pointB.latitude * DEGREES_TO_RADIANS;

    const double sinHalfDeltaLatitude  = std::sin(deltaLatitude  / 2.0);
    const double sinHalfDeltaLongitude = std::sin(deltaLongitude / 2.0);

    const double haversineValue =
        sinHalfDeltaLatitude  * sinHalfDeltaLatitude +
        std::cos(latitudeA)   * std::cos(latitudeB) *
        sinHalfDeltaLongitude * sinHalfDeltaLongitude;

    const double centralAngle = 2.0 * std::atan2(std::sqrt(haversineValue),
                                                   std::sqrt(1.0 - haversineValue));
    return EARTH_RADIUS_METERS * centralAngle;
}

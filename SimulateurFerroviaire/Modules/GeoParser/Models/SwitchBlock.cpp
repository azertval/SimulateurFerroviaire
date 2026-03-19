/**
 * @file  SwitchBlock.cpp
 * @brief Implémentation du modèle d'aiguillage SwitchBlock.
 */

#include "SwitchBlock.h"

#include <algorithm>
#include <cmath>
#include <iomanip>


// =============================================================================
// Constantes locales
// =============================================================================

namespace
{
    constexpr double EARTH_RADIUS_METERS = 6371000.0;
    constexpr double DEGREES_TO_RADIANS  = 3.14159265358979323846 / 180.0;
}


// =============================================================================
// Construction
// =============================================================================

SwitchBlock::SwitchBlock(std::string              switchId,
                          LatLon                   junctionCoord,
                          std::vector<std::string> initialBranchIds)
    : id(std::move(switchId))
    , junctionCoordinate(junctionCoord)
    , branchIds(std::move(initialBranchIds))
{}


// =============================================================================
// Requêtes
// =============================================================================

bool SwitchBlock::isOriented() const
{
    return rootBranchId.has_value();
}

void SwitchBlock::computeTotalLength()
{
    if (!isOriented())
    {
        return;
    }
    if (!tipOnRoot || !tipOnNormal || !tipOnDeviation)
    {
        return;
    }

    const double rootLegLength      = haversineDistanceMeters(*tipOnRoot,      junctionCoordinate);
    const double normalLegLength    = haversineDistanceMeters(junctionCoordinate, *tipOnNormal);
    const double deviationLegLength = haversineDistanceMeters(junctionCoordinate, *tipOnDeviation);

    totalLengthMeters = rootLegLength + std::max(normalLegLength, deviationLegLength);
}

std::string SwitchBlock::toString() const
{
    std::ostringstream stream;
    stream << "Switch(id=" << id;

    if (isDoubleSwitch)
    {
        stream << " [DOUBLE]";
    }

    if (isOriented())
    {
        stream << ", root=" << rootBranchId.value_or("?")
               << ", normal=" << normalBranchId.value_or("?")
               << ", deviation=" << deviationBranchId.value_or("?");

        if (totalLengthMeters.has_value())
        {
            stream << std::fixed;
            stream.precision(1);
            stream << ", len=" << *totalLengthMeters << "m";
        }
    }
    else
    {
        stream << std::fixed;
        stream.precision(6);
        stream << ", junction=(" << junctionCoordinate.latitude
               << ", " << junctionCoordinate.longitude << ")"
               << ", degree=" << branchIds.size();
    }

    stream << ")";
    return stream.str();
}


// =============================================================================
// Méthodes privées
// =============================================================================

double SwitchBlock::haversineDistanceMeters(const LatLon& pointA, const LatLon& pointB)
{
    const double deltaLatitude  = (pointB.latitude  - pointA.latitude)  * DEGREES_TO_RADIANS;
    const double deltaLongitude = (pointB.longitude - pointA.longitude) * DEGREES_TO_RADIANS;

    const double latitudeA = pointA.latitude * DEGREES_TO_RADIANS;
    const double latitudeB = pointB.latitude * DEGREES_TO_RADIANS;

    const double sinHalfDeltaLatitude  = std::sin(deltaLatitude  / 2.0);
    const double sinHalfDeltaLongitude = std::sin(deltaLongitude / 2.0);

    const double haversineValue =
        sinHalfDeltaLatitude  * sinHalfDeltaLatitude +
        std::cos(latitudeA)   * std::cos(latitudeB)  *
        sinHalfDeltaLongitude * sinHalfDeltaLongitude;

    const double centralAngle = 2.0 * std::atan2(std::sqrt(haversineValue),
                                                   std::sqrt(1.0 - haversineValue));
    return EARTH_RADIUS_METERS * centralAngle;
}

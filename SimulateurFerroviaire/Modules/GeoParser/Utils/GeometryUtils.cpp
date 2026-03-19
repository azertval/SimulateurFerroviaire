/**
 * @file  GeometryUtils.cpp
 * @brief Implémentation des utilitaires géométriques — projection UTM et géométrie pure.
 */

#include "GeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>


// =============================================================================
// Projection — estimation de la zone UTM
// =============================================================================

int GeometryUtils::estimateUtmZone(double centerLongitude)
{
    // La zone UTM est déterminée par la longitude : une zone couvre 6 degrés.
    return static_cast<int>(std::floor((centerLongitude + 180.0) / 6.0)) % 60 + 1;
}

double GeometryUtils::utmCentralMeridianDegrees(int utmZoneNumber)
{
    return static_cast<double>(utmZoneNumber * 6 - 183);
}


// =============================================================================
// Projection — WGS-84 → UTM
// =============================================================================

CoordinateXY GeometryUtils::wgs84ToMetricUtm(const LatLon& geographicCoord,
                                               int           utmZoneNumber,
                                               bool          isNorthernHemisphere)
{
    const double latitudeRadians   = geographicCoord.latitude  * DEGREES_TO_RADIANS;
    const double longitudeRadians  = geographicCoord.longitude * DEGREES_TO_RADIANS;
    const double centralMeridianRadians = utmCentralMeridianDegrees(utmZoneNumber) * DEGREES_TO_RADIANS;

    // Paramètres intermédiaires de la transverse de Mercator
    const double sinLatitude = std::sin(latitudeRadians);
    const double cosLatitude = std::cos(latitudeRadians);
    const double tanLatitude = std::tan(latitudeRadians);

    const double primeVerticalRadius = WGS84_SEMI_MAJOR_AXIS
        / std::sqrt(1.0 - WGS84_ECCENTRICITY_SQUARED * sinLatitude * sinLatitude);

    const double tangentSquared  = tanLatitude * tanLatitude;
    const double etaSquared      = WGS84_ECCENTRICITY_SQUARED / (1.0 - WGS84_ECCENTRICITY_SQUARED)
                                    * cosLatitude * cosLatitude;
    const double longitudeDelta  = longitudeRadians - centralMeridianRadians;

    // Méridienne — arc de méridien depuis l'équateur
    const double e2 = WGS84_ECCENTRICITY_SQUARED;
    const double meridionalArc = WGS84_SEMI_MAJOR_AXIS * (
        (1.0 - e2 / 4.0 - 3.0 * e2 * e2 / 64.0) * latitudeRadians
        - (3.0 * e2 / 8.0 + 3.0 * e2 * e2 / 32.0) * std::sin(2.0 * latitudeRadians)
        + (15.0 * e2 * e2 / 256.0) * std::sin(4.0 * latitudeRadians));

    // Coordonnée est (x)
    const double eastingMeters = UTM_SCALE_FACTOR * primeVerticalRadius * (
        longitudeDelta
        + (1.0 - tangentSquared + etaSquared) * std::pow(longitudeDelta, 3) * cosLatitude * cosLatitude / 6.0
    ) * cosLatitude + UTM_FALSE_EASTING;

    // Coordonnée nord (y)
    double northingMeters = UTM_SCALE_FACTOR * (
        meridionalArc
        + primeVerticalRadius * tanLatitude * (
            longitudeDelta * longitudeDelta * cosLatitude * cosLatitude / 2.0
            + std::pow(longitudeDelta, 4) * std::pow(cosLatitude, 4)
              * (5.0 - tangentSquared + 9.0 * etaSquared) / 24.0
        )
    );

    if (!isNorthernHemisphere)
    {
        northingMeters += UTM_FALSE_NORTHING_SOUTH;
    }

    return CoordinateXY(eastingMeters, northingMeters);
}


// =============================================================================
// Projection — UTM → WGS-84
// =============================================================================

LatLon GeometryUtils::metricUtmToWgs84(const CoordinateXY& metricCoord,
                                        int                 utmZoneNumber,
                                        bool                isNorthernHemisphere)
{
    double eastingMeters  = metricCoord.x - UTM_FALSE_EASTING;
    double northingMeters = isNorthernHemisphere
                            ? metricCoord.y
                            : metricCoord.y - UTM_FALSE_NORTHING_SOUTH;

    const double centralMeridianRadians =
        utmCentralMeridianDegrees(utmZoneNumber) * DEGREES_TO_RADIANS;

    // Paramètres intermédiaires
    const double e1 = (1.0 - std::sqrt(1.0 - WGS84_ECCENTRICITY_SQUARED))
                    / (1.0 + std::sqrt(1.0 - WGS84_ECCENTRICITY_SQUARED));

    const double e2 = WGS84_ECCENTRICITY_SQUARED;
    const double mu = northingMeters / (UTM_SCALE_FACTOR * WGS84_SEMI_MAJOR_AXIS
        * (1.0 - e2 / 4.0 - 3.0 * e2 * e2 / 64.0));

    // Latitude de pied de méridienne (série de Fourier)
    const double phi1 = mu
        + (3.0 * e1 / 2.0 - 27.0 * std::pow(e1, 3) / 32.0) * std::sin(2.0 * mu)
        + (21.0 * e1 * e1 / 16.0)                            * std::sin(4.0 * mu)
        - (151.0 * std::pow(e1, 3) / 96.0)                   * std::sin(6.0 * mu);

    const double sinPhi1 = std::sin(phi1);
    const double cosPhi1 = std::cos(phi1);

    const double primeVerticalRadius1 = WGS84_SEMI_MAJOR_AXIS
        / std::sqrt(1.0 - e2 * sinPhi1 * sinPhi1);

    const double curvatureRadius1 = WGS84_SEMI_MAJOR_AXIS * (1.0 - e2)
        / std::pow(1.0 - e2 * sinPhi1 * sinPhi1, 1.5);

    const double tangentSquared1 = std::tan(phi1) * std::tan(phi1);
    const double etaSquared1     = e2 / (1.0 - e2) * cosPhi1 * cosPhi1;

    const double scaledEasting = eastingMeters / (UTM_SCALE_FACTOR * primeVerticalRadius1);

    // Latitude (degrés)
    const double latitudeRadians = phi1
        - (primeVerticalRadius1 * std::tan(phi1) / curvatureRadius1) * (
            scaledEasting * scaledEasting / 2.0
            - std::pow(scaledEasting, 4) * (5.0 + 3.0 * tangentSquared1 + 10.0 * etaSquared1) / 24.0
        );

    // Longitude (degrés)
    const double longitudeRadians = centralMeridianRadians
        + (scaledEasting
           - (1.0 + 2.0 * tangentSquared1 + etaSquared1) * std::pow(scaledEasting, 3) / 6.0)
          / cosPhi1;

    return LatLon(latitudeRadians  * RADIANS_TO_DEGREES,
                   longitudeRadians * RADIANS_TO_DEGREES);
}


// =============================================================================
// Conversion de polylignes
// =============================================================================

std::vector<CoordinateXY> GeometryUtils::convertPolylineToMetric(
    const std::vector<LatLon>& geographicPolyline,
    int                        utmZoneNumber,
    bool                       isNorthernHemisphere)
{
    std::vector<CoordinateXY> result;
    result.reserve(geographicPolyline.size());
    for (const auto& point : geographicPolyline)
    {
        result.push_back(wgs84ToMetricUtm(point, utmZoneNumber, isNorthernHemisphere));
    }
    return result;
}

std::vector<LatLon> GeometryUtils::convertPolylineToWgs84(
    const std::vector<CoordinateXY>& metricPolyline,
    int                              utmZoneNumber,
    bool                             isNorthernHemisphere)
{
    std::vector<LatLon> result;
    result.reserve(metricPolyline.size());
    for (const auto& point : metricPolyline)
    {
        result.push_back(metricUtmToWgs84(point, utmZoneNumber, isNorthernHemisphere));
    }
    return result;
}


// =============================================================================
// Géométrie pure
// =============================================================================

CoordinateXY GeometryUtils::snapToMetricGrid(double x, double y, double gridSizeMeters)
{
    if (gridSizeMeters <= 0.0)
    {
        return CoordinateXY(x, y);
    }
    const double snappedX = std::round(x / gridSizeMeters) * gridSizeMeters;
    const double snappedY = std::round(y / gridSizeMeters) * gridSizeMeters;
    return CoordinateXY(snappedX, snappedY);
}

std::optional<CoordinateXY> GeometryUtils::pointAtDistanceAlongLine(
    const std::vector<CoordinateXY>& polylineVertices,
    double                           distanceMeters)
{
    if (polylineVertices.empty())
    {
        return std::nullopt;
    }
    if (distanceMeters <= 0.0)
    {
        return polylineVertices.front();
    }

    double accumulatedLength = 0.0;
    for (std::size_t index = 1; index < polylineVertices.size(); ++index)
    {
        const double deltaX    = polylineVertices[index].x - polylineVertices[index - 1].x;
        const double deltaY    = polylineVertices[index].y - polylineVertices[index - 1].y;
        const double segmentLength = std::hypot(deltaX, deltaY);

        if (segmentLength <= GeometricTolerances::DEGENERATE_SEGMENT_METERS)
        {
            continue;
        }

        if (accumulatedLength + segmentLength >= distanceMeters)
        {
            const double interpolationRatio = (distanceMeters - accumulatedLength) / segmentLength;
            return CoordinateXY(
                polylineVertices[index - 1].x + interpolationRatio * deltaX,
                polylineVertices[index - 1].y + interpolationRatio * deltaY
            );
        }
        accumulatedLength += segmentLength;
    }

    return polylineVertices.back();
}

double GeometryUtils::computePolylineLengthMeters(const std::vector<CoordinateXY>& polylineVertices)
{
    double totalLength = 0.0;
    for (std::size_t index = 1; index < polylineVertices.size(); ++index)
    {
        const double deltaX = polylineVertices[index].x - polylineVertices[index - 1].x;
        const double deltaY = polylineVertices[index].y - polylineVertices[index - 1].y;
        totalLength += std::hypot(deltaX, deltaY);
    }
    return totalLength;
}

double GeometryUtils::unsignedAngleBetweenVectors(const CoordinateXY& firstVector,
                                                    const CoordinateXY& secondVector)
{
    const double magnitude1 = std::hypot(firstVector.x,  firstVector.y);
    const double magnitude2 = std::hypot(secondVector.x, secondVector.y);

    if (magnitude1 < GeometricTolerances::ZERO_VECTOR_MAGNITUDE
     || magnitude2 < GeometricTolerances::ZERO_VECTOR_MAGNITUDE)
    {
        return 0.0;  // Comportement neutre
    }

    const double dotProduct  = firstVector.x * secondVector.x + firstVector.y * secondVector.y;
    const double cosineValue = std::max(-1.0, std::min(1.0, dotProduct / (magnitude1 * magnitude2)));
    return std::acos(cosineValue);
}

double GeometryUtils::angleBetweenVectorsPiFallback(const CoordinateXY& firstVector,
                                                      const CoordinateXY& secondVector)
{
    const double magnitude1 = std::hypot(firstVector.x,  firstVector.y);
    const double magnitude2 = std::hypot(secondVector.x, secondVector.y);

    if (magnitude1 < GeometricTolerances::ZERO_VECTOR_MAGNITUDE
     || magnitude2 < GeometricTolerances::ZERO_VECTOR_MAGNITUDE)
    {
        return PI;  // Pire cas : paire dégénérée jamais choisie comme exits
    }

    const double dotProduct  = firstVector.x * secondVector.x + firstVector.y * secondVector.y;
    const double cosineValue = std::max(-1.0, std::min(1.0, dotProduct / (magnitude1 * magnitude2)));
    return std::acos(cosineValue);
}

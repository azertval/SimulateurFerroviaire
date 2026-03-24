/**
 * @file  Phase1_GeoLoader.cpp
 * @brief Implémentation de la phase 1 — GeoJSON → RawNetwork.
 *
 * @see Phase1_GeoLoader
 */
#include "Phase1_GeoLoader.h"

#include <fstream>
#include <stdexcept>
#include <cmath>

#include <nlohmann/json.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

 // =============================================================================
 // Point d'entrée
 // =============================================================================

void Phase1_GeoLoader::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    LOG_INFO(logger, "Chargement GeoJSON : " + ctx.filePath);

    // Ouverture du fichier
    std::ifstream file(ctx.filePath);
    if (!file.is_open())
        throw std::runtime_error("Fichier introuvable : " + ctx.filePath);

    // Parse JSON
    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(file);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        throw std::runtime_error("GeoJSON invalide : " + std::string(e.what()));
    }

    if (!root.contains("features") || !root["features"].is_array())
        throw std::runtime_error("GeoJSON : champ 'features' absent ou invalide.");

    // Détection de la zone UTM depuis le premier point du fichier
    ctx.rawNetwork.utmZone.clear();

    size_t featureCount = 0;

    for (const auto& feature : root.at("features"))
    {
        // On ne traite que les LineString
        if (!feature.contains("geometry")) continue;
        const auto& geom = feature.at("geometry");
        if (!geom.contains("type")) continue;
        if (geom.at("type").get<std::string>() != "LineString") continue;
        if (!geom.contains("coordinates")) continue;

        const auto& coords = geom.at("coordinates");
        if (coords.size() < 2) continue;
        // ^ Une polyligne avec un seul point est invalide

        // Détection UTM sur le premier point du premier feature
        if (ctx.rawNetwork.utmZone.empty())
        {
            const double lat = coords[0][1].get<double>();
            const double lon = coords[0][0].get<double>();
            ctx.rawNetwork.utmZone = detectUtmZone(lat, lon);
            LOG_DEBUG(logger, "Zone UTM estimée : " + ctx.rawNetwork.utmZone);
        }

        RawPolyline polyline;

        // Identifiant optionnel depuis les propriétés
        if (feature.contains("properties") && feature["properties"].contains("name"))
            polyline.id = feature["properties"]["name"].get<std::string>();

        // Extraction et projection des coordonnées
        for (const auto& coord : coords)
        {
            if (!coord.is_array() || coord.size() < 2) continue;

            const double lon = coord[0].get<double>();
            const double lat = coord[1].get<double>();

            polyline.pointsWGS84.push_back({ lat, lon });
            polyline.pointsUTM.push_back(
                project(lat, lon, ctx.rawNetwork.utmZone));
        }

        if (polyline.pointsWGS84.size() >= 2)
        {
            ctx.rawNetwork.polylines.push_back(std::move(polyline));
            ++featureCount;
        }
    }

    ctx.endTimer(t0, "Phase1_GeoLoader",
        root.at("features").size(),
        ctx.rawNetwork.polylines.size());

    LOG_INFO(logger, std::to_string(ctx.rawNetwork.polylines.size())
        + " polyligne(s) chargée(s) — zone UTM : "
        + ctx.rawNetwork.utmZone);
}


// =============================================================================
// Détection zone UTM
// =============================================================================

std::string Phase1_GeoLoader::detectUtmZone(double lat, double lon)
{
    // Zone UTM = floor((longitude + 180) / 6) + 1
    const int zone = static_cast<int>(std::floor((lon + 180.0) / 6.0)) + 1;
    const std::string hemi = (lat >= 0.0) ? "N" : "S";
    return std::to_string(zone) + hemi;
}


// =============================================================================
// Projection WGS84 → UTM
// =============================================================================

CoordinateXY Phase1_GeoLoader::project(double lat, double lon,
    const std::string& utmZone)
{
    // Constantes ellipsoïde WGS84
    constexpr double a = 6378137.0;           // demi-grand axe (m)
    constexpr double f = 1.0 / 298.257223563; // aplatissement
    constexpr double b = a * (1.0 - f);
    constexpr double e2 = 2.0 * f - f * f;     // excentricité au carré
    constexpr double k0 = 0.9996;              // facteur d'échelle UTM
    constexpr double E0 = 500000.0;            // fausse est (m)
    constexpr double N0 = 10000000.0;          // fausse nord (m), hémisphère S

    // Numéro de zone depuis l'identifiant (ex. "30N" → 30)
    const int zoneNum = std::stoi(utmZone.substr(0, utmZone.size() - 1));
    const bool northHemi = (utmZone.back() == 'N');

    const double latRad = lat * M_PI / 180.0;
    const double lonRad = lon * M_PI / 180.0;
    const double lon0Rad = ((zoneNum - 1) * 6.0 - 180.0 + 3.0) * M_PI / 180.0;

    const double n = a / std::sqrt(1.0 - e2 * std::sin(latRad) * std::sin(latRad));
    const double t = std::tan(latRad);
    const double c = e2 / (1.0 - e2) * std::cos(latRad) * std::cos(latRad);
    const double A = std::cos(latRad) * (lonRad - lon0Rad);

    const double e4 = e2 * e2;
    const double e6 = e4 * e2;
    const double M = a * (
        (1.0 - e2 / 4.0 - 3.0 * e4 / 64.0 - 5.0 * e6 / 256.0) * latRad
        - (3.0 * e2 / 8.0 + 3.0 * e4 / 32.0 + 45.0 * e6 / 1024.0) * std::sin(2.0 * latRad)
        + (15.0 * e4 / 256.0 + 45.0 * e6 / 1024.0) * std::sin(4.0 * latRad)
        - (35.0 * e6 / 3072.0) * std::sin(6.0 * latRad));

    const double A2 = A * A, A3 = A2 * A, A4 = A3 * A, A5 = A4 * A, A6 = A5 * A;
    const double t2 = t * t, c2 = c * c;

    const double easting = k0 * n * (
        A
        + (1.0 - t2 + c) * A3 / 6.0
        + (5.0 - 18.0 * t2 + t2 * t2 + 72.0 * c - 58.0 * e2 / (1.0 - e2)) * A5 / 120.0
        ) + E0;

    const double M0 = 0.0; // méridien d'origine à lat=0
    const double northing = k0 * (M + n * std::tan(latRad) * (
        A2 / 2.0
        + (5.0 - t2 + 9.0 * c + 4.0 * c2) * A4 / 24.0
        + (61.0 - 58.0 * t2 + t2 * t2 + 600.0 * c - 330.0 * e2 / (1.0 - e2)) * A6 / 720.0
        )) + (northHemi ? 0.0 : N0);

    return { easting, northing };
}

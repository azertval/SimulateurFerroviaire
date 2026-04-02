/**
 * @file  Phase1_GeoLoader.h
 * @brief Phase 1 du pipeline — chargement GeoJSON → RawNetwork.
 *
 * Responsabilité unique : lire le fichier GeoJSON, extraire les features
 * de type LineString, et projeter chaque point en UTM.
 *
 * @par Ce que Phase1 ne fait PAS
 *  - Elle ne détecte pas les intersections (@ref Phase2_GeometricIntersector).
 *  - Elle ne snap pas les nœuds (@ref Phase4_TopologyBuilder).
 *  - Elle ne classe pas les nœuds (@ref Phase5_SwitchClassifier).
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase1_GeoLoader
{
public:

    /**
     * @brief Exécute la phase 1.
     *
     * Lit @c ctx.filePath, extrait les features LineString, projette en UTM,
     * et écrit le résultat dans @c ctx.rawNetwork.
     * Enregistre les stats via @c ctx.endTimer().
     *
     * @param ctx     Contexte pipeline. Lit filePath, écrit rawNetwork.
     * @param config  Configuration — utilise maxSegmentLength pour filtrage.
     * @param logger  Référence au logger GeoParser.
     *
     * @throws std::runtime_error Si le fichier est introuvable ou invalide.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase1_GeoLoader() = delete;

private:

    /**
     * @brief Détecte la zone UTM depuis le premier point WGS84 de la polyligne.
     *
     * @param lat  Latitude du point de référence.
     * @param lon  Longitude du point de référence.
     *
     * @return Identifiant de zone UTM (ex. "30N").
     */
    static std::string detectUtmZone(double lat, double lon);

    /**
     * @brief Projette un point WGS84 en UTM.
     *
     * @param lat      Latitude WGS84.
     * @param lon      Longitude WGS84.
     * @param utmZone  Zone UTM cible (ex. "30N").
     *
     * @return Coordonnées métriques UTM (x = est, y = nord).
     */
    static CoordinateXY project(double lat, double lon,
        const std::string& utmZone);
};
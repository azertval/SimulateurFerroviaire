#include "GeoParser.h"

/**
 * @file GeoParser.cpp
 * @brief Implémentation du pipeline GeoParser.
 */

#include "GraphBuilder.h"
#include "TopologyExtractor.h"
#include "SwitchOrientator.h"
#include "DoubleSwitchDetector.h"
#include "Modules/GeoJsonExporter/GeoJsonExporter.h"
#include "Modules/Models/TopologyRepository.h"

 /**
  * @brief Constructeur.
  */
GeoParser::GeoParser(Logger& logger,
    const std::string& geoJsonFilePath,
    double snapGridMeters,
    double endpointSnapMeters,
    double maxStraightLengthMeters,
    double minBranchLengthMeters,
    double doubleLinkMaxMeters,
    double branchTipDistanceMeters)
    : m_logger(logger)
    , m_geoJsonFilePath(geoJsonFilePath)
    , m_snapGridMeters(snapGridMeters)
    , m_endpointSnapMeters(endpointSnapMeters)
    , m_maxStraightLengthMeters(maxStraightLengthMeters)
    , m_minBranchLengthMeters(minBranchLengthMeters)
    , m_doubleLinkMaxMeters(doubleLinkMaxMeters)
    , m_branchTipDistanceMeters(branchTipDistanceMeters)
{
}

/**
 * @brief Exécute toutes les phases du pipeline.
 */
void GeoParser::parse(bool enableDebugDump)
{
    switches.clear();
    straights.clear();

    LOG_INFO(m_logger, "=== GeoParser START ===");

    reportProgress(5);

    GraphBuilder graphBuilder(m_logger, m_geoJsonFilePath,
        m_snapGridMeters, m_endpointSnapMeters);

    GraphBuildResult graphResult = graphBuilder.build();

    reportProgress(20);

    TopologyExtractor extractor(m_logger, graphResult, m_maxStraightLengthMeters);
    TopologyExtractResult topo = extractor.extract();

    reportProgress(40);

    SwitchOrientator orientator(
        m_logger,
        topo,
        graphResult.utmZoneNumber,
        graphResult.isNorthernHemisphere,
        m_doubleLinkMaxMeters,
        m_branchTipDistanceMeters);

    orientator.orient();

    reportProgress(60);

    DoubleSwitchDetector detector(
        m_logger,
        topo.switches,
        topo.straights,
        topo.switchIdToNodeId,
        graphResult.topologyGraph,
        m_doubleLinkMaxMeters,
        m_minBranchLengthMeters);

    detector.detectAndAbsorb();

    reportProgress(80);

    detector.validateSwitches();

    reportProgress(90);

    TopologyRepository::instance().data().clear();
    TopologyRepository::instance().data().switches = std::move(topo.switches);
    TopologyRepository::instance().data().straights = std::move(topo.straights);

    reportProgress(100);

    if (enableDebugDump)
        dumpDebugOutput();

    LOG_INFO (m_logger, "Nombre de SwitchBlocks : " + std::to_string(switches.size()));
    LOG_INFO(m_logger, "Nombre de StraightBlocks : " + std::to_string(straights.size()));

    LOG_INFO(m_logger, "=== GeoParser COMPLETED ===");
}

/**
 * @brief Dump détaillé des résultats.
 */
void GeoParser::dumpDebugOutput() const
{
    LOG_DEBUG(m_logger, "===== SWITCHES =====");
    for (const auto& sw : switches)
        LOG_DEBUG(m_logger, sw.toString());

    LOG_DEBUG(m_logger, "===== STRAIGHTS =====");
    for (const auto& st : straights)
        LOG_DEBUG(m_logger, st.toString());
}
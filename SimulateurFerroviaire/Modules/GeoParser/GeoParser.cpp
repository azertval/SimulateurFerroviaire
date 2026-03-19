#include "GeoParser.h"

/**
 * @file GeoParser.cpp
 * @brief Implémentation du pipeline GeoParser.
 */

#include "GraphBuilder.h"
#include "TopologyExtractor.h"
#include "SwitchOrientator.h"
#include "DoubleSwitchDetector.h"

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

    reportProgress(50);

    SwitchOrientator orientator(
        m_logger,
        topo,
        graphResult.utmZoneNumber,
        graphResult.isNorthernHemisphere,
        m_doubleLinkMaxMeters,
        m_branchTipDistanceMeters);

    orientator.orient();

    reportProgress(70);

    DoubleSwitchDetector detector(
        m_logger,
        topo.switches,
        topo.straights,
        topo.switchIdToNodeId,
        graphResult.topologyGraph,
        m_doubleLinkMaxMeters,
        m_minBranchLengthMeters);

    detector.detectAndAbsorb();

    reportProgress(90);

    detector.validateSwitches();

    reportProgress(100);

    switches = std::move(topo.switches);
    straights = std::move(topo.straights);

    LOG_INFO(m_logger, "=== GeoParser END ===");

    if (enableDebugDump)
        dumpDebugOutput();
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
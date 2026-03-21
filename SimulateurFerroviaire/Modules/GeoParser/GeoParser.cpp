/**
 * @file  GeoParser.cpp
 * @brief Implémentation du pipeline GeoParser.
 *
 * @see GeoParser
 */

#include "GeoParser.h"

#include "GraphBuilder.h"
#include "TopologyExtractor.h"
#include "SwitchOrientator.h"
#include "DoubleSwitchDetector.h"
#include "Engine/Core/Topology/TopologyRenderer.h"
#include "Engine/Core/Topology/TopologyRepository.h"

 /**
  * @brief Constructeur.
  */
GeoParser::GeoParser(Logger& logger,
    const std::string& geoJsonFilePath,
    double             snapGridMeters,
    double             endpointSnapMeters,
    double             maxStraightLengthMeters,
    double             minBranchLengthMeters,
    double             doubleLinkMaxMeters,
    double             branchTipDistanceMeters)
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
    LOG_INFO(m_logger, "=== GeoParser START ===");

    reportProgress(5);

    GraphBuilder      graphBuilder(m_logger, m_geoJsonFilePath,
        m_snapGridMeters, m_endpointSnapMeters);
    GraphBuildResult  graphResult = graphBuilder.build();

    reportProgress(10);

    TopologyExtractor    extractor(m_logger, graphResult, m_maxStraightLengthMeters);
    TopologyExtractResult topo = extractor.extract();

    reportProgress(30);

    SwitchOrientator orientator(
        m_logger,
        topo,
        graphResult.utmZoneNumber,
        graphResult.isNorthernHemisphere,
        m_doubleLinkMaxMeters,
        m_branchTipDistanceMeters);

    orientator.orient();

    reportProgress(40);

    DoubleSwitchDetector detector(
        m_logger,
        topo.switches,
        topo.straights,
        topo.switchIdToNodeId,
        graphResult.topologyGraph,
        m_doubleLinkMaxMeters,
        m_minBranchLengthMeters);

    detector.detectAndAbsorb();

    reportProgress(60);

    detector.validateSwitches();

    reportProgress(80);

    // -------------------------------------------------------------------------
    // Transfert dans le repository global
    //
    // TopologyExtractResult stocke encore par valeur (vecteurs de blocs concrets).
    // On convertit ici en unique_ptr pour alimenter TopologyData.
    //
    // NOTE : une fois TopologyExtractor migré vers unique_ptr, remplacer ces
    //        boucles par un simple std::move() des vecteurs.
    // -------------------------------------------------------------------------
    TopologyData& repoData = TopologyRepository::instance().data();
    repoData.clear();

    repoData.switches.reserve(topo.switches.size());
    for (auto& sw : topo.switches)
        repoData.switches.push_back(std::make_unique<SwitchBlock>(std::move(sw)));

    repoData.straights.reserve(topo.straights.size());
    for (auto& st : topo.straights)
        repoData.straights.push_back(std::make_unique<StraightBlock>(std::move(st)));

    // Index rapide id → ShuntingElement* couvrant switches ET straights
    std::unordered_map<std::string, ShuntingElement*> elementIndex;
    for (auto& sw : repoData.switches)
        elementIndex[sw->getId()] = sw.get();
    for (auto& st : repoData.straights)
        elementIndex[st->getId()] = st.get();

    // --- Straights : résolution prev/next ---
    for (auto& st : repoData.straights)
    {
        StraightBlock::StraightNeighbours nb;

        const auto& ids = st->getNeighbourIds();
        if (ids.size() >= 1)
        {
            const auto it = elementIndex.find(ids[0]);
            if (it != elementIndex.end()) nb.prev = it->second;
        }
        if (ids.size() >= 2)
        {
            const auto it = elementIndex.find(ids[1]);
            if (it != elementIndex.end()) nb.next = it->second;
        }

        st->setNeighbourPointers(nb);
    }

    // --- Switches : résolution root/normal/deviation ---
    for (auto& sw : repoData.switches)
    {
        if (!sw->isOriented()) continue;

        SwitchBlock::SwitchBranches br;

        auto resolve = [&](const std::optional<std::string>& id) -> ShuntingElement*
            {
                if (!id) return nullptr;
                const auto it = elementIndex.find(*id);
                return (it != elementIndex.end()) ? it->second : nullptr;
            };

        br.root = resolve(sw->getRootBranchId());
        br.normal = resolve(sw->getNormalBranchId());
        br.deviation = resolve(sw->getDeviationBranchId());

        sw->setBranchPointers(br);
    }

    TopologyRepository::instance().data().buildIndex();

    if (enableDebugDump)
        dumpDebugOutput();

    reportProgress(85);

    LOG_INFO(m_logger, "Nombre de SwitchBlocks  : " + std::to_string(repoData.switches.size()));
    LOG_INFO(m_logger, "Nombre de StraightBlocks: " + std::to_string(repoData.straights.size()));

    LOG_INFO(m_logger, "=== GeoParser COMPLETED ===");
}

/**
 * @brief Dump détaillé des résultats.
 */
void GeoParser::dumpDebugOutput() const
{
    const TopologyData& repoData = TopologyRepository::instance().data();

    LOG_DEBUG(m_logger, "===== SWITCHES =====");
    for (const auto& sw : repoData.switches)
        LOG_DEBUG(m_logger, sw->toString());

    LOG_DEBUG(m_logger, "===== STRAIGHTS =====");
    for (const auto& st : repoData.straights)
        LOG_DEBUG(m_logger, st->toString());
}
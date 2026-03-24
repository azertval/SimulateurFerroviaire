/**
 * @file  GeoParser.cpp
 * @brief Implémentation de l'orchestrateur GeoParser v2.
 *
 * @see GeoParser
 */
#include "GeoParser.h"

#include "Pipeline/Phase1_GeoLoader.h"
#include "Pipeline/Phase2_GeometricIntersector.h"
#include "Pipeline/Phase3_NetworkSplitter.h"
#include "Pipeline/Phase4_TopologyBuilder.h"
#include "Pipeline/Phase5_SwitchClassifier.h"


 // =============================================================================
 // Construction
 // =============================================================================

GeoParser::GeoParser(ParserConfig config,
    Logger& logger,
    std::function<void(int)> onProgress)
    : m_config(std::move(config))
    , m_logger(logger)
    , m_onProgress(std::move(onProgress))
{
}


// =============================================================================
// Pipeline
// =============================================================================

void GeoParser::parse(const std::string& filePath)
{
    LOG_INFO(m_logger, "GeoParser START : " + filePath);
    m_ctx = PipelineContext{};   // Reset complet
    m_ctx.filePath = filePath;


    Phase1_GeoLoader::run(m_ctx, m_config, m_logger);
    reportProgress(8, "Phase 1/9 — Chargement GeoJSON");

    Phase2_GeometricIntersector::run(m_ctx, m_config, m_logger);
    reportProgress(33, "Phase 2/9 — Intersections géométriques");

    Phase3_NetworkSplitter::run(m_ctx, m_config, m_logger);
    reportProgress(43, "Phase 3/9 — Découpe des segments");

    Phase4_TopologyBuilder::run(m_ctx, m_config, m_logger);
    reportProgress(58, "Phase 4/9 — Construction du graphe");

    Phase5_SwitchClassifier::run(m_ctx, m_config, m_logger);
    reportProgress(68, "Phase 5/9 — Classification des nœuds");

    logPerformanceSummary();
    LOG_INFO(m_logger, "GeoParser COMPLETED");
}


// =============================================================================
// Helpers
// =============================================================================

void GeoParser::reportProgress(int progress, std::string msg)
{
    // Log la dernière phase mesurée
    if (!m_ctx.stats.empty())
    {
        const auto& s = m_ctx.stats.back();
        LOG_DEBUG(m_logger, s.name + " — "
            + std::to_string(s.outputCount) + " éléments produits en "
            + std::to_string(static_cast<int>(s.durationMs)) + " ms.");
    }
    LOG_INFO(m_logger, msg);

    if (m_onProgress)
        m_onProgress(progress);
}

void GeoParser::logPerformanceSummary() const
{
    LOG_INFO(m_logger, "--- Performance pipeline ---");
    double total = 0.0;
    for (const auto& s : m_ctx.stats)
    {
        LOG_INFO(m_logger, "  " + s.name
            + " : " + std::to_string(static_cast<int>(s.durationMs)) + " ms"
            + " | in=" + std::to_string(s.inputCount)
            + " out=" + std::to_string(s.outputCount));
        total += s.durationMs;
    }
    LOG_INFO(m_logger, "  TOTAL : " + std::to_string(static_cast<int>(total)) + " ms");
}

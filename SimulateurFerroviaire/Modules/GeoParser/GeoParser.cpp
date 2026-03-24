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
#include "Pipeline/Phase6_BlockExtractor.h"
#include "Pipeline/Phase7_SwitchOrientator.h"
#include "Pipeline/Phase8_DoubleSwitchDetector.h"
#include "Pipeline/Phase9_RepositoryTransfer.h"

 // =============================================================================
 // Construction
 // =============================================================================

GeoParser::GeoParser(ParserConfig config, Logger& logger, std::function<void(int, const std::wstring&)> onProgress)
    : m_config(std::move(config))
    , m_logger(logger)
    , m_onProgress(std::move(onProgress))
{
}


// =============================================================================
// Pipeline
// =============================================================================

void GeoParser::parse(const std::string& filePath, std::shared_ptr<std::atomic<bool>> cancelToken)
{
    LOG_INFO(m_logger, "GeoParser START : " + filePath);
    m_ctx = PipelineContext{};   // Reset complet
    m_ctx.filePath = filePath;


    Phase1_GeoLoader::run(m_ctx, m_config, m_logger);
    reportProgress(8, L"Phase 1/9 — Chargement GeoJSON");
    checkCancel();

    Phase2_GeometricIntersector::run(m_ctx, m_config, m_logger);
    reportProgress(33, L"Phase 2/9 — Intersections géométriques");
    checkCancel();

    Phase3_NetworkSplitter::run(m_ctx, m_config, m_logger);
    reportProgress(43, L"Phase 3/9 — Découpe des segments");
    checkCancel();

    Phase4_TopologyBuilder::run(m_ctx, m_config, m_logger);
    reportProgress(58, L"Phase 4/9 — Construction du graphe");
    checkCancel();

    Phase5_SwitchClassifier::run(m_ctx, m_config, m_logger);
    reportProgress(65, L"Phase 5/9 — Classification des nœuds");
    checkCancel();

    Phase6_BlockExtractor::run(m_ctx, m_config, m_logger);
    reportProgress(75, L"Phase 6/9 — Extraction des blocs");
    checkCancel();

    // Nota : Phase7_SwitchOrientator est appelée APRÈS Phase 9a_resolutionDesPointeurs
    // SwitchOrientator a besoin des pointeurs réels(getRootBlock(), getNormalBlock()) pour calculer les angles.
    Phase9_RepositoryTransfer::resolve(m_ctx, m_logger);
    checkCancel();

    Phase8_DoubleSwitchDetector::run(m_ctx, m_config, m_logger);
    reportProgress(85, L"Phase 7/9 — Doubles aiguilles");
    checkCancel();

    Phase7_SwitchOrientator::run(m_ctx, m_config, m_logger);
    reportProgress(95, L"Phase 8/9 — Orientation des switches");
    checkCancel();

    Phase9_RepositoryTransfer::transfer(m_ctx, m_logger);
    reportProgress(100, L"Phase 9/9 — Transfert TopologyRepository");
    checkCancel();
    
    logPerformanceSummary();
    LOG_INFO(m_logger, "GeoParser COMPLETED");
}


// =============================================================================
// Helpers
// =============================================================================

void GeoParser::checkCancel() const
{
    if (m_cancelToken && m_cancelToken->load())
        throw CancelledException{};
}

void GeoParser::reportProgress(int progress, const std::wstring& label)
{
    if (!m_ctx.stats.empty())
    {
        const auto& s = m_ctx.stats.back();
        LOG_DEBUG(m_logger, s.name + " — "
            + std::to_string(s.outputCount) + " éléments en "
            + std::to_string(static_cast<int>(s.durationMs)) + " ms.");
    }
    if (m_onProgress)
        m_onProgress(progress, label);
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

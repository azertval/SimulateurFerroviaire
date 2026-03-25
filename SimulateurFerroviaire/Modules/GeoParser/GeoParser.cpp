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
#include "Pipeline/Phase7_DoubleSwitchDetector.h"
#include "Pipeline/Phase8_SwitchOrientator.h"
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

    reportProgress(0, L"Phase 1/9 — Chargement GeoJSON");
    Phase1_GeoLoader::run(m_ctx, m_config, m_logger);    
    checkCancel();

    reportProgress(5, L"Phase 2/9 — Intersections géométriques");
    Phase2_GeometricIntersector::run(m_ctx, m_config, m_logger);
    checkCancel();

    reportProgress(30, L"Phase 3/9 — Découpe des segments");
    Phase3_NetworkSplitter::run(m_ctx, m_config, m_logger);
    checkCancel();

    reportProgress(45, L"Phase 4/9 — Construction du graphe");
    Phase4_TopologyBuilder::run(m_ctx, m_config, m_logger);
    checkCancel();

    reportProgress(58, L"Phase 5/9 — Classification des nœuds");
    Phase5_SwitchClassifier::run(m_ctx, m_config, m_logger);
    checkCancel();

    reportProgress(65, L"Phase 6/9 — Extraction des blocs");
    Phase6_BlockExtractor::run(m_ctx, m_config, m_logger);
    checkCancel();

    reportProgress(75, L"Phase 7/9 — Liaisons des doubles aiguilles");
    // Nota : Phase 7 et 8 est appelée APRÈS Phase 9a_resolutionDesPointeurs
    // SwitchOrientator et DoubleSwitchDetector ont besoin des pointeurs réels(getRootBlock(), getNormalBlock())
    Phase9_RepositoryTransfer::resolve(m_ctx, m_logger);
    checkCancel();

    Phase7_DoubleSwitchDetector::run(m_ctx, m_config, m_logger);
    checkCancel();

    reportProgress(85, L"hase 8/9 — Vérification de la bonne orientation des switches");
    Phase8_SwitchOrientator::run(m_ctx, m_config, m_logger);
    checkCancel();

    reportProgress(90, L"Phase 9/9 — Transfert TopologyRepository");
    Phase9_RepositoryTransfer::transfer(m_ctx, m_logger);
    checkCancel();

    reportProgress(95, L"Affichage en cours");
    LOG_INFO(m_logger, "GeoParser COMPLETED");
    logPerformanceSummary();
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
    if (m_onProgress)
        m_onProgress(progress, label);
}

void GeoParser::logPerformanceSummary() const
{
    LOG_INFO(m_logger, "--- parser performance ---");
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
    LOG_INFO(m_logger, "--------------------------");
}

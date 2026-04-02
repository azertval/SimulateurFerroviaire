/**
 * @file  GeoParser.h
 * @brief Orchestrateur du pipeline GeoParser v2.
 *
 * Possède le @ref PipelineContext et la @ref ParserConfig.
 * Enchaîne les phases et reporte la progression via callback.
 *
 * @par Ajout d'une phase
 * Ajouter l'appel @c PhaseN::run(m_ctx, m_config, m_logger) dans
 * @ref parse() — GeoParser ne connaît pas les détails des phases (OCP).
 *
 * @note Instanciation interdite depuis l'extérieur du thread de parsing.
 *       Utiliser @ref GeoParsingTask.
 */
#pragma once

#include <functional>
#include <string>

#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"
#include "Pipeline/PipelineContext.h"

class GeoParser
{
public:
    /** Exception levée lors d'une annulation propre. */
    struct CancelledException {};

    /**
     * @brief Construit le parser avec configuration, logger et callback de progression.
     *
     * @param config      Configuration — copiée.
     * @param logger      Logger GeoParser.
     * @param onProgress  Callback (progression 0-100, label phase).
     */
    explicit GeoParser(ParserConfig config,
        Logger& logger,
        std::function<void(int, const std::wstring&)> onProgress = nullptr);

    /**
     * @brief Exécute le pipeline complet.
     *
     * @param filePath     Chemin vers le GeoJSON.
     * @param cancelToken  Token d'annulation partagé — vérifié entre chaque phase.
     *
     * @throws CancelledException  Si le token est positionné entre deux phases.
     * @throws std::runtime_error  En cas d'erreur de parsing.
     */
    void parse(const std::string& filePath,
        std::shared_ptr<std::atomic<bool>> cancelToken = nullptr);

private:

    /**
     * @brief Reporte la progression et logue un résumé de la dernière phase.
     *
     * @param progress  Valeur 0-100 envoyée au callback.
     */
    void reportProgress(int progress, const std::wstring& label);

    /**
     * @brief Logue le tableau de performance de toutes les phases exécutées.
     */
    void logPerformanceSummary() const;

    ParserConfig             m_config;
    Logger& m_logger;
    PipelineContext          m_ctx;
    std::function<void(int, const std::wstring&)> m_onProgress;

    /**
     * @brief Vérifie le cancel token et lève CancelledException si nécessaire.
     */
    void checkCancel() const;
    std::shared_ptr<std::atomic<bool>> m_cancelToken;
};
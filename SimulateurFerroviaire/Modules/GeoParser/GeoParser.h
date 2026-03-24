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

    /**
     * @brief Construit le parser avec la configuration fournie.
     *
     * @param config    Configuration — copiée (snapshot immuable pendant parse).
     * @param logger    Référence au logger GeoParser.
     * @param onProgress Callback de progression (0-100). Appelé entre les phases.
     */
    explicit GeoParser(ParserConfig config,
        Logger& logger,
        std::function<void(int)> onProgress = nullptr);

    /**
     * @brief Exécute le pipeline complet sur le fichier GeoJSON indiqué.
     *
     * Orchestre les phases 1 à 9. Chaque phase lit/écrit dans @c m_ctx.
     * En cas d'échec d'une phase, propage l'exception sans altérer
     * @ref TopologyRepository.
     *
     * @param filePath  Chemin absolu vers le fichier GeoJSON.
     *
     * @throws std::runtime_error En cas d'erreur de parsing ou de fichier.
     */
    void parse(const std::string& filePath);

private:

    /**
     * @brief Reporte la progression et logue un résumé de la dernière phase.
     *
     * @param progress  Valeur 0-100 envoyée au callback.
     */
    void reportProgress(int progress, std::string msg);

    /**
     * @brief Logue le tableau de performance de toutes les phases exécutées.
     */
    void logPerformanceSummary() const;

    ParserConfig             m_config;
    Logger& m_logger;
    PipelineContext          m_ctx;
    std::function<void(int)> m_onProgress;
};
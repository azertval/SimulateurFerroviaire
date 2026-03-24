/**
 * @file  PipelineContext.h
 * @brief Conteneur central des données inter-phases du pipeline GeoParser.
 *
 * Transporteur passé par référence à chaque phase. Chaque phase lit
 * uniquement ce dont elle a besoin et écrit uniquement son résultat.
 *
 * @par Gestion mémoire
 * Les structures intermédiaires peuvent être libérées dès qu'elles ne
 * sont plus utiles via leurs méthodes @c clear().
 * Exemple : @c rawNetwork.clear() après Phase 4.
 *
 * @par Instrumentation
 * Chaque phase appende ses @ref PhaseStats pour permettre un rapport
 * de performance en fin de pipeline.
 */
#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <numeric>

#include "RawNetwork.h"
#include "IntersectionMap.h"
#include "SplitNetwork.h"
#include "TopologyGraph.h"
#include "ClassifiedNodes.h"

 /**
  * @struct PhaseStats
  * @brief Métriques d'une phase du pipeline.
  */
struct PhaseStats
{
    std::string name;           ///< Nom de la phase.
    double      durationMs = 0; ///< Durée d'exécution en millisecondes.
    size_t      inputCount = 0; ///< Nombre d'éléments en entrée.
    size_t      outputCount = 0;///< Nombre d'éléments produits.
};

/**
 * @struct PipelineContext
 * @brief Conteneur central du pipeline GeoParser.
 *
 * Instancié par @ref GeoParser, passé par référence non-const à chaque phase.
 * Jamais copié — toujours passé par référence.
 */
struct PipelineContext
{
    // =========================================================================
    // Entrée
    // =========================================================================

    /** Chemin absolu vers le fichier GeoJSON en cours de traitement. */
    std::string filePath;

    // =========================================================================
    // Résultats intermédiaires — un champ par phase
    // =========================================================================

    /** Phase 1 — polylignes WGS84 + UTM brutes. */
    RawNetwork rawNetwork;

    /** Phase 2 — intersections géométriques + grille spatiale. */
    IntersectionData intersections;

    /** Phase 3 — segments atomiques découpés aux intersections. */
    SplitNetwork splitNetwork;

    /** Phase 4 — graphe planaire nœuds + arêtes. */
    TopologyGraph topoGraph;

    /** Phase 5 — classification topologique des nœuds. */
    ClassifiedNodes classifiedNodes;

    // BlockSet        blocks;          — ajouté en étape 6

    // =========================================================================
    // Instrumentation
    // =========================================================================

    /** Statistiques collectées par chaque phase. */
    std::vector<PhaseStats> stats;

    /**
     * @brief Démarre un chronomètre — à appeler au début d'une phase.
     *
     * @return Point de départ pour @ref endTimer.
     */
    [[nodiscard]] static std::chrono::steady_clock::time_point startTimer()
    {
        return std::chrono::steady_clock::now();
    }

    /**
     * @brief Arrête le chronomètre et enregistre les stats de la phase.
     *
     * @param start       Point de départ retourné par @ref startTimer.
     * @param name        Nom de la phase.
     * @param inputCount  Nombre d'éléments en entrée.
     * @param outputCount Nombre d'éléments produits.
     */
    void endTimer(std::chrono::steady_clock::time_point start,
        const std::string& name,
        size_t inputCount = 0,
        size_t outputCount = 0)
    {
        const auto end = std::chrono::steady_clock::now();
        stats.push_back({
            name,
            std::chrono::duration<double, std::milli>(end - start).count(),
            inputCount,
            outputCount
            });
    }
};
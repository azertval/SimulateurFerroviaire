/**
 * @file  Phase2_GeometricIntersector.h
 * @brief Phase 2 du pipeline — calcul des intersections géométriques.
 *
 * Responsabilité unique : pour chaque paire de segments du @ref RawNetwork,
 * détecter les intersections géométriques et les stocker dans
 * @ref IntersectionData.
 *
 * Utilise un **spatial grid binning** pour limiter les tests aux segments
 * candidats — complexité O(n·k) au lieu de O(n²).
 *
 * @par Ce que Phase2 ne fait PAS
 *  - Elle ne découpe pas les segments (@ref Phase3_NetworkSplitter).
 *  - Elle ne snap pas les nœuds (@ref Phase4_TopologyBuilder).
 *  - Elle ne classe pas les nœuds (@ref Phase5_SwitchClassifier).
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase2_GeometricIntersector
{
public:

    /**
     * @brief Exécute la phase 2.
     *
     * Construit la grille spatiale depuis @c ctx.rawNetwork, puis teste
     * chaque paire de segments candidats via l'algorithme de Cramer.
     * Écrit le résultat dans @c ctx.intersections.
     *
     * @param ctx     Contexte pipeline. Lit rawNetwork, écrit intersections.
     * @param config  Configuration — utilise @c intersectionEpsilon.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase2_GeometricIntersector() = delete;

private:

    /**
     * @brief Calcule la taille de cellule optimale pour la grille.
     *
     * Basée sur la longueur moyenne des segments × 2.
     * Garantit un minimum de 50 m pour éviter les cellules trop petites.
     *
     * @param ctx  Contexte pipeline contenant @c rawNetwork.
     *
     * @return Taille de cellule en mètres UTM.
     */
    static double computeCellSize(const PipelineContext& ctx);

    /**
     * @brief Construit la grille spatiale depuis le RawNetwork.
     *
     * Chaque segment est inséré dans toutes les cellules de sa bounding box.
     *
     * @param ctx       Contexte pipeline.
     * @param cellSize  Taille de cellule en mètres UTM.
     * @param logger    Référence au logger GeoParser.
     *
     * @return Grille spatiale peuplée.
     */
    static SpatialGrid buildGrid(const PipelineContext& ctx,
        double cellSize,
        Logger& logger);

    /**
     * @brief Calcule l'indice global d'un segment dans le réseau.
     *
     * Index unique = somme des (taille-1) des polylignes précédentes + pointIndex.
     *
     * @param ctx         Contexte pipeline.
     * @param polyIdx     Indice de la polyligne.
     * @param pointIdx    Indice du premier point du segment.
     *
     * @return Index global du segment.
     */
    static size_t globalSegmentIndex(const PipelineContext& ctx,
        size_t polyIdx,
        size_t pointIdx);

    /**
     * @brief Teste l'intersection entre deux segments UTM.
     *
     * Algorithme de Cramer — résout le système linéaire 2×2.
     * Retourne les paramètres t et u si l'intersection est valide.
     *
     * @param A        Premier point du segment source.
     * @param B        Second point du segment source.
     * @param C        Premier point du segment croisé.
     * @param D        Second point du segment croisé.
     * @param epsilon  Tolérance en mètres UTM.
     *
     * @return Paire (t, u) si intersection valide, @c std::nullopt sinon.
     *         t = position relative sur AB, u = position relative sur CD.
     */
    static std::optional<std::pair<double, double>>
        intersect(const CoordinateXY& A, const CoordinateXY& B,
            const CoordinateXY& C, const CoordinateXY& D,
            double epsilon);

    /**
     * @brief Vérifie si deux segments sont adjacents (partagent une extrémité).
     *
     * Les segments adjacents d'une même polyligne ne sont pas testés —
     * ils partagent un point par construction et génèreraient de faux positifs.
     *
     * @param s1  Premier segment.
     * @param s2  Second segment.
     *
     * @return @c true si les segments sont adjacents (même polyligne, indices consécutifs).
     */
    static bool areAdjacent(const SegmentId& s1, const SegmentId& s2);
};

/**
 * @file  Phase2_GeometricIntersector.cpp
 * @brief Implémentation de la phase 2 — intersections géométriques.
 *
 * @see Phase2_GeometricIntersector
 */
#include "Phase2_GeometricIntersector.h"

#include <algorithm> 
#include <cmath>
#include <unordered_set>


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void Phase2_GeometricIntersector::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    // Compte total de segments pour le log
    size_t totalSegments = 0;
    for (const auto& poly : ctx.rawNetwork.polylines)
        if (poly.pointsUTM.size() >= 2)
            totalSegments += poly.pointsUTM.size() - 1;

    LOG_INFO(logger, "Calcul des intersections — "
        + std::to_string(totalSegments) + " segments.");

    // --- Étape 1 : taille de cellule optimale ---
    const double cellSize = computeCellSize(ctx);
    ctx.intersections.cellSize = cellSize;

    LOG_DEBUG(logger, "Grille spatiale — cellSize = "
        + std::to_string(static_cast<int>(cellSize)) + " m.");

    // --- Étape 2 : construction de la grille ---
    ctx.intersections.grid = buildGrid(ctx, cellSize, logger);

    // --- Étape 3 : test des paires candidates ---
    // Pour chaque cellule, on teste toutes les paires de segments présents.
    // Un ensemble de paires déjà traitées évite les doublons.
    std::unordered_set<size_t> processedPairs;
    // ^ Clé = combinaison des deux index globaux (Cantor pairing)

    const double epsilon = config.intersectionEpsilon;

    for (const auto& [cell, segIds] : ctx.intersections.grid)
    {
        for (size_t i = 0; i < segIds.size(); ++i)
        {
            for (size_t j = i + 1; j < segIds.size(); ++j)
            {
                const SegmentId& s1 = segIds[i];
                const SegmentId& s2 = segIds[j];

                // Ignorer les segments adjacents (même polyligne, indices consécutifs)
                if (areAdjacent(s1, s2)) continue;

                // Clé canonique pour la paire — évite de tester deux fois
                const size_t idx1 = globalSegmentIndex(ctx, s1.polylineIndex, s1.pointIndex);
                const size_t idx2 = globalSegmentIndex(ctx, s2.polylineIndex, s2.pointIndex);
                const size_t pairKey = (std::min(idx1, idx2) * 1000000)
                    + std::max(idx1, idx2);
                // ^ Hachage simple — valide si totalSegments < 1 000 000
                if (processedPairs.count(pairKey)) continue;
                processedPairs.insert(pairKey);

                // Récupération des points UTM des deux segments
                const auto& poly1 = ctx.rawNetwork.polylines[s1.polylineIndex].pointsUTM;
                const auto& poly2 = ctx.rawNetwork.polylines[s2.polylineIndex].pointsUTM;

                const CoordinateXY& A = poly1[s1.pointIndex];
                const CoordinateXY& B = poly1[s1.pointIndex + 1];
                const CoordinateXY& C = poly2[s2.pointIndex];
                const CoordinateXY& D = poly2[s2.pointIndex + 1];

                // Test d'intersection géométrique
                const auto result = intersect(A, B, C, D, epsilon);
                if (!result.has_value()) continue;

                const auto [t, u] = *result;

                // Point d'intersection interpolé depuis le segment source
                const CoordinateXY pt{
                    A.x + t * (B.x - A.x),
                    A.y + t * (B.y - A.y)
                };

                // Enregistrement dans les deux sens :
                // s1 est croisé par s2 (au paramètre t)
                ctx.intersections.intersections[idx1].push_back({ pt, t, u, s2 });

                // s2 est croisé par s1 (au paramètre u)
                ctx.intersections.intersections[idx2].push_back({ pt, u, t, s1 });

                ++ctx.intersections.totalIntersections;
            }
        }
    }

    ctx.endTimer(t0, "Phase2_GeometricIntersector",
        totalSegments,
        ctx.intersections.totalIntersections);

    LOG_INFO(logger, std::to_string(ctx.intersections.totalIntersections)
        + " intersection(s) détectée(s).");
}


// =============================================================================
// Taille de cellule optimale
// =============================================================================

double Phase2_GeometricIntersector::computeCellSize(const PipelineContext& ctx)
{
    double totalLength = 0.0;
    size_t segmentCount = 0;

    for (const auto& poly : ctx.rawNetwork.polylines)
    {
        for (size_t i = 0; i + 1 < poly.pointsUTM.size(); ++i)
        {
            const auto& p1 = poly.pointsUTM[i];
            const auto& p2 = poly.pointsUTM[i + 1];
            const double dx = p2.x - p1.x;
            const double dy = p2.y - p1.y;
            totalLength += std::sqrt(dx * dx + dy * dy);
            ++segmentCount;
        }
    }

    if (segmentCount == 0) return 500.0;

    // cellSize = 2 × longueur moyenne — chaque cellule couvre ~2 segments
    const double avg = totalLength / static_cast<double>(segmentCount);
    return std::max(50.0, avg * 2.0);
    // ^ Minimum 50 m — évite les cellules trop petites sur des réseaux denses
}


// =============================================================================
// Construction de la grille
// =============================================================================

SpatialGrid Phase2_GeometricIntersector::buildGrid(const PipelineContext& ctx,
    double cellSize,
    Logger& logger)
{
    SpatialGrid grid;

    for (size_t pi = 0; pi < ctx.rawNetwork.polylines.size(); ++pi)
    {
        const auto& poly = ctx.rawNetwork.polylines[pi];
        for (size_t si = 0; si + 1 < poly.pointsUTM.size(); ++si)
        {
            const auto& p1 = poly.pointsUTM[si];
            const auto& p2 = poly.pointsUTM[si + 1];

            // Bounding box du segment → cellules traversées
            const int minCol = static_cast<int>(
                std::floor(std::min(p1.x, p2.x) / cellSize));
            const int maxCol = static_cast<int>(
                std::floor(std::max(p1.x, p2.x) / cellSize));
            const int minRow = static_cast<int>(
                std::floor(std::min(p1.y, p2.y) / cellSize));
            const int maxRow = static_cast<int>(
                std::floor(std::max(p1.y, p2.y) / cellSize));

            for (int col = minCol; col <= maxCol; ++col)
                for (int row = minRow; row <= maxRow; ++row)
                    grid[{col, row}].push_back({ pi, si });
        }
    }

    LOG_DEBUG(logger, "Grille construite — "
        + std::to_string(grid.size()) + " cellule(s) occupée(s).");

    return grid;
}


// =============================================================================
// Index global d'un segment
// =============================================================================

size_t Phase2_GeometricIntersector::globalSegmentIndex(
    const PipelineContext& ctx, size_t polyIdx, size_t pointIdx)
{
    size_t offset = 0;
    for (size_t i = 0; i < polyIdx; ++i)
    {
        const size_t n = ctx.rawNetwork.polylines[i].pointsUTM.size();
        if (n >= 2) offset += n - 1;
    }
    return offset + pointIdx;
}


// =============================================================================
// Algorithme de Cramer
// =============================================================================

std::optional<std::pair<double, double>>
Phase2_GeometricIntersector::intersect(
    const CoordinateXY& A, const CoordinateXY& B,
    const CoordinateXY& C, const CoordinateXY& D,
    double epsilon)
{
    const double dx1 = B.x - A.x;
    const double dy1 = B.y - A.y;
    const double dx2 = D.x - C.x;
    const double dy2 = D.y - C.y;

    // Déterminant — null si segments parallèles
    const double det = dx1 * (-dy2) + dx2 * dy1;

    if (std::abs(det) < epsilon * epsilon)
        return std::nullopt;   // Segments parallèles ou quasi-parallèles

    const double t = ((C.x - A.x) * (-dy2) + dx2 * (C.y - A.y)) / det;
    const double u = (dx1 * (C.y - A.y) - dy1 * (C.x - A.x)) / det;

    // Tolérance sur les bornes [0,1] — absorbe les erreurs de floating point
    // aux extrémités des segments
    const double lo = -epsilon / std::max(1.0, std::hypot(dx1, dy1));
    const double hi = 1.0 - lo;

    if (t < lo || t > hi || u < lo || u > hi)
        return std::nullopt;

    // Clamp dans [0,1] — les intersections aux extrémités exactes
    return std::make_pair(
        std::clamp(t, 0.0, 1.0),
        std::clamp(u, 0.0, 1.0));
}


// =============================================================================
// Adjacence
// =============================================================================

bool Phase2_GeometricIntersector::areAdjacent(const SegmentId& s1,
    const SegmentId& s2)
{
    if (s1.polylineIndex != s2.polylineIndex) return false;

    // Segments adjacents = indices consécutifs dans la même polyligne
    const size_t diff = (s1.pointIndex > s2.pointIndex)
        ? s1.pointIndex - s2.pointIndex
        : s2.pointIndex - s1.pointIndex;
    return diff == 1;
}

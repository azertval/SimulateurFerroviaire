/**
 * @file  Phase3_NetworkSplitter.cpp
 * @brief Implémentation de la phase 3 — découpe des segments.
 *
 * @see Phase3_NetworkSplitter
 */
#include "Phase3_NetworkSplitter.h"

#include <algorithm>
#include <cmath>


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void Phase3_NetworkSplitter::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t polyCount = ctx.rawNetwork.polylines.size();
    LOG_INFO(logger, "Découpe des segments — "
        + std::to_string(polyCount) + " polyligne(s).");

    // Diagnostic — vérifie que les intersections sont bien disponibles
    LOG_DEBUG(logger, "Carte d'intersections — "
        + std::to_string(ctx.intersections.intersections.size())
        + " segment(s) avec intersection(s) enregistré(s), "
        + "totalIntersections=" + std::to_string(ctx.intersections.totalIntersections));

    ctx.splitNetwork.segments.reserve(polyCount * 4);

    size_t globalIdx = 0;
    size_t splitCount = 0;  // Nombre de segments découpés aux intersections
    size_t subdivCount = 0;  // Nombre de segments découpés par maxSegmentLength

    for (size_t pi = 0; pi < polyCount; ++pi)
    {
        const auto& poly = ctx.rawNetwork.polylines[pi];
        const auto& ptsUTM = poly.pointsUTM;
        const auto& ptsWGS84 = poly.pointsWGS84;

        // NOTE : ne pas incrémenter globalIdx pour les polylignes dégénérées
        // Phase2::globalSegmentIndex() ajoute 0 pour n < 2 — doit rester cohérent
        if (ptsUTM.size() < 2) continue;

        for (size_t si = 0; si + 1 < ptsUTM.size(); ++si, ++globalIdx)
        {
            const CoordinateXY& A = ptsUTM[si];
            const CoordinateXY& B = ptsUTM[si + 1];
            const CoordinateLatLon& Ageo = ptsWGS84[si];
            const CoordinateLatLon& Bgeo = ptsWGS84[si + 1];

            const double segLen = std::hypot(B.x - A.x, B.y - A.y);

            const std::vector<double> cutParams = collectCutParams(
                ctx, globalIdx, segLen, config.intersectionEpsilon);

            if (cutParams.size() > 2)
            {
                ++splitCount;
                LOG_DEBUG(logger, "Segment " + std::to_string(globalIdx)
                    + " (poly=" + std::to_string(pi)
                    + " seg=" + std::to_string(si)
                    + ") — " + std::to_string(cutParams.size() - 1)
                    + " sous-segment(s).");
            }

            for (size_t ci = 0; ci + 1 < cutParams.size(); ++ci)
            {
                const double tA = cutParams[ci];
                const double tB = cutParams[ci + 1];

                const CoordinateXY     subA = interpolateUTM(A, B, tA);
                const CoordinateXY     subB = interpolateUTM(A, B, tB);
                const CoordinateLatLon subAgeo = interpolateWGS84(Ageo, Bgeo, tA);
                const CoordinateLatLon subBgeo = interpolateWGS84(Ageo, Bgeo, tB);

                const size_t beforeSubdiv = ctx.splitNetwork.segments.size();
                subdivideLong(subA, subAgeo, subB, subBgeo,
                    config.maxSegmentLength, pi,
                    ctx.splitNetwork.segments);
                subdivCount += ctx.splitNetwork.segments.size() - beforeSubdiv - 1;
            }
        }
    }

    ++globalIdx; // Aligne sur le total (optionnel — pour le rapport seulement)

    const size_t produced = ctx.splitNetwork.size();

    ctx.endTimer(t0, "Phase3_NetworkSplitter",
        globalIdx - 1,
        produced);

    LOG_INFO(logger, std::to_string(produced)
        + " segment(s) atomique(s) produit(s) "
        + "— " + std::to_string(splitCount) + " découpé(s) aux intersections"
        + ", " + std::to_string(subdivCount) + " subdivisé(s) par maxSegmentLength.");

    ctx.rawNetwork.clear();
    ctx.intersections.clear();

    LOG_DEBUG(logger, "rawNetwork et intersections libérés.");
}


// =============================================================================
// Collecte et tri des paramètres de découpe
// =============================================================================

std::vector<double> Phase3_NetworkSplitter::collectCutParams(
    const PipelineContext& ctx,
    size_t globalIdx,
    double segLen,
    double epsilon)
{
    std::vector<double> params = { 0.0, 1.0 };

    const auto it = ctx.intersections.intersections.find(globalIdx);
    if (it != ctx.intersections.intersections.end())
        for (const auto& pt : it->second)
            params.push_back(std::clamp(pt.t, 0.0, 1.0));

    std::sort(params.begin(), params.end());

    params.erase(
        std::unique(params.begin(), params.end(),
            [](double a, double b) { return std::abs(a - b) < 1e-9; }),
        params.end());

    if (segLen < 1e-6) return { 0.0, 1.0 };

    const double minDeltaT = (epsilon * 2.0) / segLen;
    std::vector<double> filtered;
    filtered.reserve(params.size());
    filtered.push_back(0.0);

    for (const double t : params)
    {
        if (t <= 0.0) continue;
        if (t >= 1.0 - 1e-9) continue;
        if (t - filtered.back() >= minDeltaT)
            filtered.push_back(t);
    }
    filtered.push_back(1.0);

    return filtered;
}


// =============================================================================
// Interpolations
// =============================================================================

CoordinateXY Phase3_NetworkSplitter::interpolateUTM(
    const CoordinateXY& A, const CoordinateXY& B, double t)
{
    return { A.x + t * (B.x - A.x),
             A.y + t * (B.y - A.y) };
}

CoordinateLatLon Phase3_NetworkSplitter::interpolateWGS84(
    const CoordinateLatLon& A, const CoordinateLatLon& B, double t)
{
    return { A.latitude + t * (B.latitude - A.latitude),
             A.longitude + t * (B.longitude - A.longitude) };
}


// =============================================================================
// Subdivision par maxSegmentLength
// =============================================================================

void Phase3_NetworkSplitter::subdivideLong(
    const CoordinateXY& A, const CoordinateLatLon& Ageo,
    const CoordinateXY& B, const CoordinateLatLon& Bgeo,
    double maxLen,
    size_t parentIdx,
    std::vector<AtomicSegment>& out)
{
    const double len = std::hypot(B.x - A.x, B.y - A.y);

    if (len <= maxLen || len < 1e-6)
    {
        AtomicSegment seg;
        seg.pointsWGS84 = { Ageo, Bgeo };
        seg.pointsUTM = { A,    B };
        seg.parentPolylineIndex = parentIdx;
        out.push_back(std::move(seg));
        return;
    }

    const int n = static_cast<int>(std::ceil(len / maxLen));

    CoordinateXY     prev = A;
    CoordinateLatLon prevGeo = Ageo;

    for (int i = 1; i <= n; ++i)
    {
        const double t = static_cast<double>(i) / static_cast<double>(n);

        const CoordinateXY     curr = interpolateUTM(A, B, t);
        const CoordinateLatLon currGeo = interpolateWGS84(Ageo, Bgeo, t);

        AtomicSegment seg;
        seg.pointsWGS84 = { prevGeo, currGeo };
        seg.pointsUTM = { prev,    curr };
        seg.parentPolylineIndex = parentIdx;
        out.push_back(std::move(seg));

        prev = curr;
        prevGeo = currGeo;
    }
}
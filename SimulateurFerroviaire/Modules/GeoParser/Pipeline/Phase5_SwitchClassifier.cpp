/**
 * @file  Phase5_SwitchClassifier.cpp
 * @brief Implémentation de la phase 5 — classification topologique.
 *
 * @see Phase5_SwitchClassifier
 */
#include "Phase5_SwitchClassifier.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void Phase5_SwitchClassifier::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t nodeCount = ctx.topoGraph.nodes.size();
    LOG_INFO(logger, "Classification des nœuds — "
        + std::to_string(nodeCount) + " nœud(s).");

    for (const auto& node : ctx.topoGraph.nodes)
    {
        const size_t deg = ctx.topoGraph.degree(node.id);
        NodeClass cls;

        switch (deg)
        {
        case 0:
            cls = NodeClass::ISOLATED;
            LOG_DEBUG(logger, "Nœud " + std::to_string(node.id)
                + " — ISOLATED (degré 0).");
            break;

        case 1:
            cls = NodeClass::TERMINUS;
            break;

        case 2:
            cls = classifyDegree2(ctx.topoGraph, ctx.splitNetwork,
                node.id, config.minSwitchAngle);
            if (cls == NodeClass::AMBIGUOUS)
                LOG_WARNING(logger, "Nœud " + std::to_string(node.id)
                    + " — AMBIGUOUS deg=2 (angle anormal).");
            break;

        case 3:
            cls = classifyDegree3(ctx.topoGraph, ctx.splitNetwork,
                node.id, config.minSwitchAngle);
            if (cls == NodeClass::AMBIGUOUS)
                LOG_WARNING(logger, "Nœud " + std::to_string(node.id)
                    + " — AMBIGUOUS deg=3 (angles trop fermés).");
            break;

        case 4:
            cls = NodeClass::CROSSING;
            LOG_DEBUG(logger, "Nœud " + std::to_string(node.id)
                + " — CROSSING (croisement plat, ignoré).");
            break;

        default:
            cls = NodeClass::AMBIGUOUS;
            LOG_WARNING(logger, "Nœud " + std::to_string(node.id)
                + " — AMBIGUOUS deg=" + std::to_string(deg)
                + " (degré inattendu).");
            break;
        }

        ctx.classifiedNodes.classify(node.id, cls);
    }

    ctx.endTimer(t0, "Phase5_SwitchClassifier",
        nodeCount,
        ctx.classifiedNodes.countSwitch);

    // Log de synthèse
    LOG_INFO(logger,
        "Classification terminée — "
        "TERMINUS=" + std::to_string(ctx.classifiedNodes.countTerminus) + " "
        "STRAIGHT=" + std::to_string(ctx.classifiedNodes.countStraight) + " "
        "SWITCH=" + std::to_string(ctx.classifiedNodes.countSwitch) + " "
        "CROSSING=" + std::to_string(ctx.classifiedNodes.countCrossing) + " "
        "ISOLATED=" + std::to_string(ctx.classifiedNodes.countIsolated) + " "
        "AMBIGUOUS=" + std::to_string(ctx.classifiedNodes.countAmbiguous));
}


// =============================================================================
// Vecteur sortant
// =============================================================================

CoordinateXY Phase5_SwitchClassifier::outVector(
    const TopologyGraph& graph,
    const SplitNetwork& split,
    size_t nodeId,
    size_t edgeIdx)
{
    const TopoEdge& edge = graph.edges[edgeIdx];
    const AtomicSegment& seg = split.segments[edge.segmentIndex];

    if (seg.pointsUTM.size() < 2)
        return { 0.0, 0.0 };   // Arête dégénérée

    const bool fromA = (edge.nodeA == nodeId);

    // Origine = extrémité adjacente au nœud
    // Direction = vers le premier point intermédiaire (tangente locale)
    if (fromA)
    {
        const CoordinateXY& origin = seg.pointsUTM.front();
        const CoordinateXY& next = seg.pointsUTM[1];
        return { next.x - origin.x, next.y - origin.y };
    }
    else
    {
        const CoordinateXY& origin = seg.pointsUTM.back();
        const CoordinateXY& next = seg.pointsUTM[seg.pointsUTM.size() - 2];
        return { next.x - origin.x, next.y - origin.y };
    }
}


// =============================================================================
// Angle entre deux vecteurs
// =============================================================================

double Phase5_SwitchClassifier::angleBetween(
    const CoordinateXY& u, const CoordinateXY& v)
{
    const double lenU = std::hypot(u.x, u.y);
    const double lenV = std::hypot(v.x, v.y);

    if (lenU < 1e-9 || lenV < 1e-9)
        return 0.0;   // Vecteur dégénéré — angle indéfini → 0°

    const double dot = u.x * v.x + u.y * v.y;
    const double cosTheta = std::clamp(dot / (lenU * lenV), -1.0, 1.0);
    //                       ↑ Clamp indispensable — évite NaN dans acos

    return std::acos(cosTheta) * (180.0 / M_PI);
}


// =============================================================================
// Classification degré 2
// =============================================================================

NodeClass Phase5_SwitchClassifier::classifyDegree2(
    const TopologyGraph& graph,
    const SplitNetwork& split,
    size_t nodeId,
    double minSwitchAngle)
{
    const auto& adj = graph.adjacency[nodeId];
    // adj contient exactement 2 indices d'arêtes (degré == 2)

    const CoordinateXY u = outVector(graph, split, nodeId, adj[0]);
    const CoordinateXY v = outVector(graph, split, nodeId, adj[1]);

    const double angle = angleBetween(u, v);

    // STRAIGHT si les deux arêtes sont quasi-opposées (angle proche de 180°)
    // Seuil : 180° - minSwitchAngle (défaut : 165°)
    if (angle >= 180.0 - minSwitchAngle)
        return NodeClass::STRAIGHT;

    return NodeClass::AMBIGUOUS;
    // Virage serré — géométrie anormale pour une voie droite
}


// =============================================================================
// Classification degré 3
// =============================================================================

NodeClass Phase5_SwitchClassifier::classifyDegree3(
    const TopologyGraph& graph,
    const SplitNetwork& split,
    size_t nodeId,
    double minSwitchAngle)
{
    const auto& adj = graph.adjacency[nodeId];
    // adj contient exactement 3 indices d'arêtes (degré == 3)

    const CoordinateXY u = outVector(graph, split, nodeId, adj[0]);
    const CoordinateXY v = outVector(graph, split, nodeId, adj[1]);
    const CoordinateXY w = outVector(graph, split, nodeId, adj[2]);

    // Teste les 3 paires — un switch réel a au moins une paire
    // avec un angle ≥ minSwitchAngle (vraie bifurcation)
    const double uvAngle = angleBetween(u, v);
    const double uwAngle = angleBetween(u, w);
    const double vwAngle = angleBetween(v, w);

    const double maxAngle = std::max({ uvAngle, uwAngle, vwAngle });

    if (maxAngle >= minSwitchAngle)
        return NodeClass::SWITCH;

    // Trois arêtes quasiment colinéaires — bruit topologique
    return NodeClass::AMBIGUOUS;
}
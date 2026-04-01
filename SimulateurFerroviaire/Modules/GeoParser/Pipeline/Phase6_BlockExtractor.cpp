/**
 * @file  Phase6_BlockExtractor.cpp
 * @brief Implémentation de la phase 6 — extraction des blocs ferroviaires.
 *
 * @par Corrections v2
 *
 * **Bug 1 — Crossover : même straight pour normal et deviation**
 * Cause : la déduplication par @c pairKey(startNode, endNode) empêchait la
 * création d'un second straight entre deux switches reliés par deux voies
 * parallèles (crossover).  De plus, @c straightByDirectedPair stockait une
 * valeur unique par clé, la seconde insertion écrasant la première.
 *
 * Correction :
 *  - @c extractStraights utilise désormais un ensemble @c usedEdges (indices
 *    d'arêtes) pour la déduplication. L'arête de départ (@c startEdgeIdx) et
 *    l'arête d'arrivée (@c prevEdge) sont marquées après chaque création de
 *    straight. Cela empêche la traversal inverse (B→A) mais autorise plusieurs
 *    straights entre la même paire de frontières.
 *  - @c straightByDirectedPair est désormais multi-valué
 *    (@c unordered_map<size_t, vector<StraightBlock*>>).
 *  - @c extractSwitches maintient un ensemble @c usedStraights par switch pour
 *    attribuer des straights distincts à chaque branche.
 *
 * **Bug 2 — Subdivision : voisins null sur les sous-blocs internes**
 * Ce bug est corrigé dans @ref Phase8_RepositoryTransfer::resolveStraight :
 * la résolution ne s'applique qu'aux endpoints dont @c neighbourId est non
 * vide, préservant ainsi les pointeurs de chaîne posés par @c registerStraight.
 *
 * @see Phase6_BlockExtractor
 */
#include "Phase6_BlockExtractor.h"

#include <unordered_set>
#include <array>
#include <cmath>


// =============================================================================
// Point d'entrée
// =============================================================================

void Phase6_BlockExtractor::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    LOG_INFO(logger, "Extraction des blocs ferroviaires.");

    // Straights en premier — construit straightByDirectedPair
    // dont extractSwitches a besoin pour résoudre les endpoints
    extractStraights(ctx, config, logger);
    extractSwitches(ctx, logger);

    ctx.endTimer(t0, "Phase6_BlockExtractor",
        ctx.topoGraph.nodes.size(),
        ctx.blocks.totalCount());

    LOG_INFO(logger,
        std::to_string(ctx.blocks.switches.size()) + " SwitchBlock(s), "
        + std::to_string(ctx.blocks.straights.size()) + " StraightBlock(s).");

    // Libération mémoire — sources plus nécessaires après Phase 6
    ctx.topoGraph.clear();
    ctx.classifiedNodes.clear();
    ctx.splitNetwork.clear();

    LOG_DEBUG(logger, "topoGraph, classifiedNodes et splitNetwork libérés.");
}


// =============================================================================
// Extraction des StraightBlocks
// =============================================================================

void Phase6_BlockExtractor::extractStraights(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    // Déduplication par arêtes — remplace l'ancienne déduplication par
    // pairKey(startNode, endNode) qui bloquait les crossovers.
    //
    // Invariant : quand un straight est créé via startEdge → ... → lastEdge,
    // les deux indices d'arêtes sont insérés dans usedEdges.
    //   • startEdge empêche qu'un autre DFS parte de la même arête.
    //   • lastEdge empêche la traversal inverse depuis le nœud d'arrivée.
    std::unordered_set<size_t> usedEdges;

    size_t straightIndex = 0;
    size_t subdivided    = 0;

    for (const auto& node : ctx.topoGraph.nodes)
    {
        if (!isFrontier(ctx, node.id)) continue;

        for (size_t startEdgeIdx : ctx.topoGraph.adjacency[node.id])
        {
            // Arête déjà consommée par un straight précédent → ignorer
            if (usedEdges.count(startEdgeIdx)) continue;

            std::vector<CoordinateXY>     ptsUTM;
            std::vector<CoordinateLatLon> ptsWGS84;

            size_t curr     = node.id;
            size_t prevEdge = SIZE_MAX;
            size_t nextEdge = startEdgeIdx;

            // Premier point = position du nœud de départ
            ptsUTM.push_back(ctx.topoGraph.nodes[curr].posUTM);
            ptsWGS84.push_back(ctx.topoGraph.nodes[curr].posWGS84);

            size_t endNode  = SIZE_MAX;

            while (true)
            {
                const TopoEdge& edge     = ctx.topoGraph.edges[nextEdge];
                const size_t    nextNode = edge.opposite(curr);

                if (nextNode == SIZE_MAX) break;

                const bool reversed = (edge.nodeA == nextNode);
                appendSegment(ptsUTM, ptsWGS84,
                    ctx.splitNetwork.segments[edge.segmentIndex],
                    reversed);

                curr     = nextNode;
                prevEdge = nextEdge;

                if (isFrontier(ctx, curr))
                {
                    endNode = curr;
                    break;
                }

                // Nœud STRAIGHT — continue sans demi-tour
                bool advanced = false;
                for (size_t eidx : ctx.topoGraph.adjacency[curr])
                {
                    if (eidx == prevEdge) continue;
                    nextEdge = eidx;
                    advanced = true;
                    break;
                }
                if (!advanced) break;
            }

            if (endNode == SIZE_MAX) continue;

            // Marque les deux arêtes extrêmes — empêche :
            //   • une nouvelle traversal depuis startEdgeIdx
            //   • la traversal inverse depuis endNode via prevEdge
            usedEdges.insert(startEdgeIdx);
            if (prevEdge != SIZE_MAX && prevEdge != startEdgeIdx)
                usedEdges.insert(prevEdge);

            const std::string baseId = "s/" + std::to_string(straightIndex);
            const BlockEndpoint epA{ node.id, "" };
            const BlockEndpoint epB{ endNode,  "" };

            const double totalLen = computeLength(ptsUTM);
            if (totalLen > config.maxSegmentLength) ++subdivided;

            registerStraight(ctx, ptsUTM, ptsWGS84,
                node.id, endNode,
                baseId, config.maxSegmentLength,
                epA, epB);

            ++straightIndex;
        }
    }

    LOG_DEBUG(logger, std::to_string(straightIndex)
        + " StraightBlock(s) créé(s) — "
        + std::to_string(subdivided) + " subdivisé(s) (maxSegmentLength="
        + std::to_string(static_cast<int>(config.maxSegmentLength)) + " m).");
}


// =============================================================================
// registerStraight — crée un ou plusieurs sous-blocs chaînés
// =============================================================================

void Phase6_BlockExtractor::registerStraight(
    PipelineContext& ctx,
    const std::vector<CoordinateXY>&     ptsUTM,
    const std::vector<CoordinateLatLon>& ptsWGS84,
    size_t nodeA,
    size_t nodeB,
    const std::string& baseId,
    double maxLen,
    const BlockEndpoint& epA,
    const BlockEndpoint& epB)
{
    const double totalLen = computeLength(ptsUTM);
    const size_t totalPts = ptsUTM.size();

    // Nombre de sous-blocs nécessaires
    const int N = (maxLen > 0.0 && totalLen > maxLen)
        ? static_cast<int>(std::ceil(totalLen / maxLen))
        : 1;

    std::vector<StraightBlock*> subPtrs;
    subPtrs.reserve(static_cast<size_t>(N));

    for (int k = 0; k < N; ++k)
    {
        // Tranche de points proportionnelle à la longueur
        const size_t startPt = (k == 0)
            ? 0
            : (static_cast<size_t>(k) * (totalPts - 1)) / static_cast<size_t>(N);
        const size_t endPt   = (k == N - 1)
            ? totalPts - 1
            : (static_cast<size_t>(k + 1) * (totalPts - 1)) / static_cast<size_t>(N);

        auto sub = std::make_unique<StraightBlock>();

        const std::string subId = (N == 1)
            ? baseId
            : baseId + "_" + std::to_string(k);

        sub->setId(subId);
        sub->setPointsUTM({
            ptsUTM.begin()   + static_cast<std::ptrdiff_t>(startPt),
            ptsUTM.begin()   + static_cast<std::ptrdiff_t>(endPt + 1)
        });
        sub->setPointsWGS84({
            ptsWGS84.begin() + static_cast<std::ptrdiff_t>(startPt),
            ptsWGS84.begin() + static_cast<std::ptrdiff_t>(endPt + 1)
        });

        subPtrs.push_back(sub.get());

        // Endpoints :
        //   Sous-bloc de tête  (k==0)   : epFirst = epA (frontierNodeId = nodeA)
        //   Sous-bloc de queue (k==N-1) : epLast  = epB (frontierNodeId = nodeB)
        //   Sous-blocs internes          : frontierNodeId = SIZE_MAX, neighbourId vide
        //     → Phase8_RepositoryTransfer::resolveStraight ne doit PAS
        //       écraser les pointeurs de chaîne pour ces entrées.
        BlockEndpoint epFirst = (k == 0)     ? epA : BlockEndpoint{ SIZE_MAX, "" };
        BlockEndpoint epLast  = (k == N - 1) ? epB : BlockEndpoint{ SIZE_MAX, "" };

        ctx.blocks.straightEndpoints.push_back({ epFirst, epLast });
        ctx.blocks.straights.push_back(std::move(sub));
    }

    // Chaînage prev/next des sous-blocs — posé ICI, avant résolution Phase 8.
    // Phase8_RepositoryTransfer::resolveStraight ne doit pas écraser ces pointeurs
    // pour les sous-blocs internes (ceux dont neighbourId est vide).
    for (int k = 0; k + 1 < N; ++k)
    {
        subPtrs[static_cast<size_t>(k)]->setNeighbourNext(
            subPtrs[static_cast<size_t>(k + 1)]);
        subPtrs[static_cast<size_t>(k + 1)]->setNeighbourPrev(
            subPtrs[static_cast<size_t>(k)]);
    }

    // -------------------------------------------------------------------------
    // Index lookup
    // -------------------------------------------------------------------------

    // straightsByNode : seuls les vrais nœuds frontières (extrémités)
    ctx.blocks.straightsByNode[nodeA].push_back(subPtrs.front());
    ctx.blocks.straightsByNode[nodeB].push_back(subPtrs.back());

    // straightByEndpointPair : clé canonique → premier sous-bloc (côté nodeA)
    // Note : pour un crossover, la seconde insertion écrase la première ici —
    // ce map n'est utilisé que pour rebuildStraightIndex() et ne sert pas à
    // la résolution des endpoints de switches (qui utilise straightByDirectedPair).
    const size_t a = std::min(nodeA, nodeB);
    const size_t b = std::max(nodeA, nodeB);
    ctx.blocks.straightByEndpointPair[(a + b) * (a + b + 1) / 2 + b] = subPtrs.front();

    // straightByDirectedPair : multi-valué — push_back pour conserver tous les
    // straights entre la même paire de frontières (cas crossover).
    //   directedKey(nodeA, nodeB) → sous-bloc adjacent à nodeA (subPtrs.front())
    //   directedKey(nodeB, nodeA) → sous-bloc adjacent à nodeB (subPtrs.back())
    ctx.blocks.straightByDirectedPair[directedKey(nodeA, nodeB)].push_back(subPtrs.front());
    ctx.blocks.straightByDirectedPair[directedKey(nodeB, nodeA)].push_back(subPtrs.back());
}


// =============================================================================
// Extraction des SwitchBlocks
// =============================================================================

void Phase6_BlockExtractor::extractSwitches(PipelineContext& ctx,
    Logger& logger)
{
    size_t switchIndex = 0;

    for (const auto& node : ctx.topoGraph.nodes)
    {
        if (ctx.classifiedNodes.getClass(node.id) != NodeClass::SWITCH)
            continue;

        auto sw = std::make_unique<SwitchBlock>();
        sw->setId("sw/" + std::to_string(switchIndex));
        sw->setJunctionUTM(node.posUTM);
        sw->setJunctionWGS84(node.posWGS84);

        SwitchBlock* rawPtr = sw.get();
        ctx.blocks.switchByNode[node.id] = rawPtr;

        std::array<BlockEndpoint, 3> endpoints{};
        const auto& adj = ctx.topoGraph.adjacency[node.id];

        // Ensemble local — empêche d'attribuer le même straight à deux branches
        // distinctes du même switch (cas crossover où deux branches aboutissent
        // au même nœud frontière et partagent la même clé directionnelle).
        std::unordered_set<StraightBlock*> usedStraights;

        for (size_t bi = 0; bi < std::min(adj.size(), size_t{ 3 }); ++bi)
        {
            const TopoEdge& edge     = ctx.topoGraph.edges[adj[bi]];
            const size_t    nextNode = edge.opposite(node.id);

            // Traverse les nœuds STRAIGHT jusqu'au prochain nœud frontière
            size_t curr     = nextNode;
            size_t prevEdge = adj[bi];

            while (!isFrontier(ctx, curr))
            {
                const auto& currAdj = ctx.topoGraph.adjacency[curr];
                bool advanced = false;
                for (size_t eidx : currAdj)
                {
                    if (eidx == prevEdge) continue;
                    prevEdge = eidx;
                    curr = ctx.topoGraph.edges[eidx].opposite(curr);
                    advanced = true;
                    break;
                }
                if (!advanced) break;
            }

            endpoints[bi].frontierNodeId = curr;

            // Lookup directionnel — donne le sous-bloc adjacent au switch.
            // straightByDirectedPair est multi-valué : deux entrées pour un
            // crossover. On sélectionne la première non encore utilisée.
            const size_t dkey = directedKey(node.id, curr);
            const auto   dit  = ctx.blocks.straightByDirectedPair.find(dkey);

            if (dit != ctx.blocks.straightByDirectedPair.end())
            {
                for (StraightBlock* candidate : dit->second)
                {
                    if (usedStraights.count(candidate)) continue;
                    endpoints[bi].neighbourId = candidate->getId();
                    usedStraights.insert(candidate);
                    break;
                }
            }
            else
            {
                // Connexion directe sw↔sw (ex. double switch avant absorption)
                // ou nœud non connecté à un straight connu.
                const auto itSw = ctx.blocks.switchByNode.find(curr);
                if (itSw != ctx.blocks.switchByNode.end())
                    endpoints[bi].neighbourId = itSw->second->getId();
            }
        }

        ctx.blocks.switchEndpoints.push_back(endpoints);
        ctx.blocks.switches.push_back(std::move(sw));
        ++switchIndex;
    }

    LOG_DEBUG(logger, std::to_string(switchIndex) + " SwitchBlock(s) créé(s).");
}


// =============================================================================
// Helpers
// =============================================================================

bool Phase6_BlockExtractor::isFrontier(const PipelineContext& ctx,
    size_t nodeId)
{
    return ctx.classifiedNodes.getClass(nodeId) != NodeClass::STRAIGHT;
}

void Phase6_BlockExtractor::appendSegment(
    std::vector<CoordinateXY>&     ptsUTM,
    std::vector<CoordinateLatLon>& ptsWGS84,
    const AtomicSegment&           seg,
    bool                           reversed)
{
    if (reversed)
    {
        for (int i = static_cast<int>(seg.pointsUTM.size()) - 2; i >= 0; --i)
        {
            ptsUTM.push_back(seg.pointsUTM[static_cast<size_t>(i)]);
            ptsWGS84.push_back(seg.pointsWGS84[static_cast<size_t>(i)]);
        }
    }
    else
    {
        for (size_t i = 1; i < seg.pointsUTM.size(); ++i)
        {
            ptsUTM.push_back(seg.pointsUTM[i]);
            ptsWGS84.push_back(seg.pointsWGS84[i]);
        }
    }
}

double Phase6_BlockExtractor::computeLength(const std::vector<CoordinateXY>& pts)
{
    double len = 0.0;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const double dx = pts[i + 1].x - pts[i].x;
        const double dy = pts[i + 1].y - pts[i].y;
        len += std::sqrt(dx * dx + dy * dy);
    }
    return len;
}

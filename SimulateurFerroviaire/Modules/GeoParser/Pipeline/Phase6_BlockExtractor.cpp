/**
 * @file  Phase6_BlockExtractor.cpp
 * @brief Implémentation de la phase 6 — extraction des blocs ferroviaires.
 *
 * @see Phase6_BlockExtractor
 */
#include "Phase6_BlockExtractor.h"

#include <unordered_set>
#include <array>


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void Phase6_BlockExtractor::run(PipelineContext& ctx,
    const ParserConfig& /*config*/,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    LOG_INFO(logger, "Extraction des blocs ferroviaires.");

    // Straights en premier — construit straightByEndpointPair
    // dont extractSwitches a besoin pour résoudre les endpoints
    extractStraights(ctx, logger);
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
    Logger& logger)
{
    std::unordered_set<size_t> processedPairs;
    size_t straightIndex = 0;

    for (const auto& node : ctx.topoGraph.nodes)
    {
        if (!isFrontier(ctx, node.id)) continue;

        for (size_t startEdgeIdx : ctx.topoGraph.adjacency[node.id])
        {
            std::vector<CoordinateXY>     ptsUTM;
            std::vector<CoordinateLatLon> ptsWGS84;

            size_t curr = node.id;
            size_t prevEdge = SIZE_MAX;
            size_t nextEdge = startEdgeIdx;

            // Premier point = position du nœud de départ
            ptsUTM.push_back(ctx.topoGraph.nodes[curr].posUTM);
            ptsWGS84.push_back(ctx.topoGraph.nodes[curr].posWGS84);

            size_t endNode = SIZE_MAX;

            while (true)
            {
                const TopoEdge& edge = ctx.topoGraph.edges[nextEdge];
                const size_t    nextNode = edge.opposite(curr);

                if (nextNode == SIZE_MAX) break;

                const bool reversed = (edge.nodeA == nextNode);
                const AtomicSegment& seg =
                    ctx.splitNetwork.segments[edge.segmentIndex];

                appendSegment(ptsUTM, ptsWGS84, seg, reversed);

                curr = nextNode;
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

            // Dédoublonnage — évite A→B et B→A
            const size_t key = pairKey(node.id, endNode);
            if (processedPairs.count(key)) continue;
            processedPairs.insert(key);

            // Création du StraightBlock
            auto st = std::make_unique<StraightBlock>();
            st->setId("s/" + std::to_string(straightIndex));
            st->setPointsUTM(ptsUTM);
            st->setPointsWGS84(ptsWGS84);

            StraightBlock* rawPtr = st.get();

            // Endpoints
            BlockEndpoint epA{ node.id, "" };
            BlockEndpoint epB{ endNode,  "" };

            // Index multi-valué : un nœud peut être connecté à plusieurs straights
            ctx.blocks.straightsByNode[node.id].push_back(rawPtr);
            ctx.blocks.straightsByNode[endNode].push_back(rawPtr);

            // Index pair canonique : O(1) sans ambiguïté pour extractSwitches
            ctx.blocks.straightByEndpointPair[key] = rawPtr;

            ctx.blocks.straightEndpoints.push_back({ epA, epB });
            ctx.blocks.straights.push_back(std::move(st));
            ++straightIndex;
        }
    }

    LOG_DEBUG(logger, std::to_string(straightIndex) + " StraightBlock(s) créé(s).");
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

        for (size_t bi = 0; bi < std::min(adj.size(), size_t{ 3 }); ++bi)
        {
            const TopoEdge& edge = ctx.topoGraph.edges[adj[bi]];
            const size_t    nextNode = edge.opposite(node.id);

            // Traverse les STRAIGHT jusqu'au prochain nœud frontière
            size_t curr = nextNode;
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

            // Résolution directe via la paire canonique (switch ↔ frontier)
            // — O(1), sans ambiguïté, sans doublon possible
            const size_t key = pairKey(node.id, curr);
            const auto   it = ctx.blocks.straightByEndpointPair.find(key);
            if (it != ctx.blocks.straightByEndpointPair.end())
            {
                endpoints[bi].neighbourId = it->second->getId();
            }
            else
            {
                // Frontier est un autre switch (connexion directe sw↔sw)
                // ou nœud non connecté à un straight connu
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
    std::vector<CoordinateXY>& ptsUTM,
    std::vector<CoordinateLatLon>& ptsWGS84,
    const AtomicSegment& seg,
    bool                           reversed)
{
    if (reversed)
    {
        for (int i = static_cast<int>(seg.pointsUTM.size()) - 2; i >= 0; --i)
        {
            ptsUTM.push_back(seg.pointsUTM[i]);
            ptsWGS84.push_back(seg.pointsWGS84[i]);
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

size_t Phase6_BlockExtractor::pairKey(size_t idA, size_t idB)
{
    const size_t a = std::min(idA, idB);
    const size_t b = std::max(idA, idB);
    return (a + b) * (a + b + 1) / 2 + b;
}
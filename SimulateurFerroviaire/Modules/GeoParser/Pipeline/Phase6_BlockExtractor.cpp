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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    // dont extractSwitches et extractCrossings ont besoin
    extractStraights(ctx, config, logger);

    // Switches — nécessite straightByDirectedPair complet
    extractSwitches(ctx, logger);

    // Crossings — nécessite straightByDirectedPair ET switchesByNode complets
    // Doit être appelé AVANT clear() — utilise topoGraph et classifiedNodes
    extractCrossings(ctx, logger);

    ctx.endTimer(t0, "Phase6_BlockExtractor",
        ctx.topoGraph.nodes.size(),
        ctx.blocks.totalCount());

    LOG_INFO(logger,
        std::to_string(ctx.blocks.switches.size()) + " SwitchBlock(s), "
        + std::to_string(ctx.blocks.straights.size()) + " StraightBlock(s), "
        + std::to_string(ctx.blocks.crossings.size()) + " CrossBlock(s).");

    // Libération mémoire — sources plus nécessaires après Phase 6
    // ATTENTION : ne pas déplacer ces clear() avant extractCrossings()
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
    const size_t totalPts = ptsUTM.size();

    // -------------------------------------------------------------------------
    // Longueurs cumulées — base du split par longueur réelle
    // -------------------------------------------------------------------------
    std::vector<double> cumLen(totalPts, 0.0);
    for (size_t i = 1; i < totalPts; ++i)
    {
        const double dx = ptsUTM[i].x - ptsUTM[i - 1].x;
        const double dy = ptsUTM[i].y - ptsUTM[i - 1].y;
        cumLen[i] = cumLen[i - 1] + std::hypot(dx, dy);
    }
    const double totalLen = cumLen.back();

    // Nombre de sous-blocs nécessaires — basé sur la longueur réelle
    const int N = (maxLen > 0.0 && totalLen > maxLen)
        ? static_cast<int>(std::ceil(totalLen / maxLen))
        : 1;

    // -------------------------------------------------------------------------
    // Indices de frontière entre sous-blocs, calculés par longueur cumulée
    // boundaries[k] = indice du premier point du sous-bloc k
    // boundaries[N] = dernier point (totalPts - 1)
    // -------------------------------------------------------------------------
    std::vector<size_t> boundaries(static_cast<size_t>(N) + 1);
    boundaries[0] = 0;
    boundaries[static_cast<size_t>(N)] = totalPts - 1;

    for (int k = 1; k < N; ++k)
    {
        const double targetLen = totalLen * static_cast<double>(k)
                               / static_cast<double>(N);

        // Premier point dont la longueur cumulée ≥ targetLen
        size_t pt = totalPts - 1;
        for (size_t i = 1; i < totalPts; ++i)
        {
            if (cumLen[i] >= targetLen - 1e-9)
            {
                pt = i;
                break;
            }
        }
        boundaries[static_cast<size_t>(k)] = pt;
    }

    std::vector<StraightBlock*> subPtrs;
    subPtrs.reserve(static_cast<size_t>(N));

    for (int k = 0; k < N; ++k)
    {
        const size_t startPt = boundaries[static_cast<size_t>(k)];
        const size_t endPt   = boundaries[static_cast<size_t>(k + 1)];

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
        ctx.blocks.switchesByNode[node.id] = rawPtr;

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
                const auto itSw = ctx.blocks.switchesByNode.find(curr);
                if (itSw != ctx.blocks.switchesByNode.end())
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
// extractCrossings — une CrossBlock par nœud NodeClass::CROSSING
// =============================================================================
void Phase6_BlockExtractor::extractCrossings(PipelineContext& ctx, Logger& logger)
{
    size_t crIndex = 0;

    for (const auto& node : ctx.topoGraph.nodes)
    {
        if (ctx.classifiedNodes.getClass(node.id) != NodeClass::CROSSING)
            continue;

        const auto& adj = ctx.topoGraph.adjacency[node.id];
        if (adj.size() != 4)
        {
            LOG_WARNING(logger, "Nœud CROSSING " + std::to_string(node.id)
                + " — degré inattendu " + std::to_string(adj.size()) + ", ignoré.");
            continue;
        }

        // -----------------------------------------------------------------
        // Étape 1 : DFS vers les 4 nœuds frontières + lookup StraightBlock*
        // -----------------------------------------------------------------
        std::array<size_t, 4>          frontierNodes{};
        std::array<StraightBlock*, 4>  branchStraights{};

        std::unordered_set<StraightBlock*> usedStraights;

        for (size_t bi = 0; bi < 4; ++bi)
        {
            const TopoEdge& firstEdge = ctx.topoGraph.edges[adj[bi]];
            size_t curr = firstEdge.opposite(node.id);
            size_t prevEdge = adj[bi];

            // Traversée des nœuds STRAIGHT jusqu'au prochain frontière
            while (!isFrontier(ctx, curr))
            {
                const auto& currAdj = ctx.topoGraph.adjacency[curr];
                for (size_t eidx : currAdj)
                {
                    if (eidx == prevEdge) continue;
                    prevEdge = eidx;
                    curr = ctx.topoGraph.edges[eidx].opposite(curr);
                    break;
                }
            }
            frontierNodes[bi] = curr;

            // Lookup directionnel — premier straight non utilisé
            const auto key = directedKey(node.id, curr);
            const auto it = ctx.blocks.straightByDirectedPair.find(key);
            StraightBlock* chosen = nullptr;
            if (it != ctx.blocks.straightByDirectedPair.end())
            {
                for (StraightBlock* st : it->second)
                {
                    if (!usedStraights.count(st))
                    {
                        chosen = st;
                        usedStraights.insert(st);
                        break;
                    }
                }
            }
            if (!chosen)
                LOG_WARNING(logger, "cr/" + std::to_string(crIndex)
                    + " branche " + std::to_string(bi) + " — straight introuvable.");
            branchStraights[bi] = chosen;
        }

        // -----------------------------------------------------------------
        // Étape 2 : Partition angulaire — trouver les 2 paires traversantes
        // Les 3 partitions possibles de {0,1,2,3} en 2 paires :
        //   P0 : {0,2} | {1,3}
        //   P1 : {0,1} | {2,3}
        //   P2 : {0,3} | {1,2}
        // -----------------------------------------------------------------

        // Vecteurs sortants depuis le nœud CROSSING
        std::array<CoordinateXY, 4> vecs;
        for (size_t i = 0; i < 4; ++i)
            vecs[i] = outVecCross(ctx.topoGraph, node.id, adj[i]);

        // Les paires de chaque partition (indices dans {0,1,2,3})
        using Pair = std::pair<size_t, size_t>;
        using PairOfPairs = std::pair<Pair, Pair>;

        const std::array<PairOfPairs, 3> partitions = {
            PairOfPairs{ Pair{0,2}, Pair{1,3} },
            PairOfPairs{ Pair{0,1}, Pair{2,3} },
            PairOfPairs{ Pair{0,3}, Pair{1,2} }
        };

        double bestScore = -1.0;
        size_t bestP = 0;

        for (size_t pi = 0; pi < 3; ++pi)
        {
            const auto& [pairAC, pairBD] = partitions[pi];
            const double score =
                angleDeg(vecs[pairAC.first], vecs[pairAC.second]) +
                angleDeg(vecs[pairBD.first], vecs[pairBD.second]);
            if (score > bestScore)
            {
                bestScore = score;
                bestP = pi;
            }
        }

        // Indices des branches dans les rôles A, B, C, D
        const auto& [pairAC, pairBD] = partitions[bestP];
        const size_t idxA = pairAC.first;
        const size_t idxC = pairAC.second;
        const size_t idxB = pairBD.first;
        const size_t idxD = pairBD.second;

        // -----------------------------------------------------------------
        // Étape 3 : Détection de variante
        // -----------------------------------------------------------------
        bool allSwitch = true;
        for (size_t bi = 0; bi < 4; ++bi)
            if (ctx.classifiedNodes.getClass(frontierNodes[bi]) != NodeClass::SWITCH)
                allSwitch = false;

        std::unique_ptr<CrossBlock> cr = allSwitch
            ? std::unique_ptr<CrossBlock>(std::make_unique<SwitchCrossBlock>())
            : std::unique_ptr<CrossBlock>(std::make_unique<StraightCrossBlock>());

        // -----------------------------------------------------------------
        // Étape 4 : Construction du CrossBlock
        // -----------------------------------------------------------------
        const std::string crId = "cr/" + std::to_string(crIndex++);
        cr->setId(crId);
        cr->setJunctionUTM(node.posUTM);
        cr->setJunctionWGS84(node.posWGS84);

        CrossBlock* rawPtr = cr.get();
        ctx.blocks.crossingsByNode[node.id] = rawPtr;

        // Endpoints (IDs des voisins — résolus en Phase 8)
        std::array<BlockEndpoint, 4> eps{};
        const auto buildEp = [&](size_t bi) -> BlockEndpoint {
            BlockEndpoint ep;
            ep.frontierNodeId = frontierNodes[bi];
            if (branchStraights[bi])
                ep.neighbourId = branchStraights[bi]->getId();
            else if (ctx.blocks.switchesByNode.count(frontierNodes[bi]))
                ep.neighbourId = ctx.blocks.switchesByNode[frontierNodes[bi]]->getId();
            return ep;
            };
        eps[0] = buildEp(idxA);
        eps[1] = buildEp(idxB);
        eps[2] = buildEp(idxC);
        eps[3] = buildEp(idxD);

        ctx.blocks.crossings.push_back(std::move(cr));
        ctx.blocks.crossingEndpoints.push_back(eps);

        LOG_DEBUG(logger, crId + (allSwitch ? " [TJD]" : " [FLAT]")
            + " partition=" + std::to_string(bestP)
            + " score=" + std::to_string(static_cast<int>(bestScore)) + "°"
            + " A=" + eps[0].neighbourId
            + " B=" + eps[1].neighbourId
            + " C=" + eps[2].neighbourId
            + " D=" + eps[3].neighbourId);
    }

    // Compteurs pour le log de synthèse
    size_t tjdCount = 0;
    for (const auto& cr : ctx.blocks.crossings)
        if (cr->isTJD()) ++tjdCount;

    LOG_INFO(logger, "Crossings extraits — "
        + std::to_string(ctx.blocks.crossings.size()) + " total ("
        + std::to_string(tjdCount) + " TJD, "
        + std::to_string(ctx.blocks.crossings.size() - tjdCount) + " FLAT).");
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

// =============================================================================
// Helper local — vecteur UTM sortant depuis un nœud via une arête
// (dupliqué depuis Phase5_SwitchClassifier pour éviter le couplage inter-phases)
// =============================================================================
CoordinateXY Phase6_BlockExtractor::outVecCross(const TopologyGraph& graph,
    size_t nodeId, size_t edgeIdx)
{
    const TopoEdge& edge = graph.edges[edgeIdx];
    const size_t   otherId = edge.opposite(nodeId);
    if (otherId == SIZE_MAX) return { 0.0, 0.0 };
    const CoordinateXY& o = graph.nodes[nodeId].posUTM;
    const CoordinateXY& t = graph.nodes[otherId].posUTM;
    return { t.x - o.x, t.y - o.y };
}

// Angle en degrés entre deux vecteurs UTM
double Phase6_BlockExtractor::angleDeg(const CoordinateXY& u, const CoordinateXY& v)
{
    const double dot = u.x * v.x + u.y * v.y;
    const double magU = std::sqrt(u.x * u.x + u.y * u.y);
    const double magV = std::sqrt(v.x * v.x + v.y * v.y);
    if (magU < 1e-9 || magV < 1e-9) return 0.0;
    const double cosA = std::clamp(dot / (magU * magV), -1.0, 1.0);
    return std::acos(cosA) * 180.0 / M_PI;
}
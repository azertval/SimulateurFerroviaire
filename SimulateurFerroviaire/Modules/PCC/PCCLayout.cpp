/**
 * @file  PCCLayout.cpp
 * @brief Implémentation du calculateur de positions logiques PCC.
 *
 * @see PCCLayout
 */
#include "PCCLayout.h"
#include "PCCSwitchNode.h"
#include "PCCCrossingNode.h"

#include <queue>
#include <algorithm>
#include <set>


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void PCCLayout::compute(PCCGraph& graph, Logger& logger)
{
    if (graph.isEmpty())
    {
        LOG_WARNING(logger, "Compute — graphe vide, aucun calcul effectué.");
        return;
    }

    LOG_INFO(logger, "Compute — début sur "
        + std::to_string(graph.nodeCount()) + " nœuds.");

    std::vector<PCCNode*> termini = findTermini(graph, logger);
    std::unordered_set<PCCNode*> visited;
    int offsetX = 0;

    for (PCCNode* terminus : termini)
    {
        if (visited.count(terminus))
            continue;

        int maxX = runBFS(graph, terminus, visited, offsetX, logger);
        offsetX = maxX + 15;
    }

    for (const auto& nodePtr : graph.getNodes())
    {
        PCCNode* node = nodePtr.get();
        if (!visited.count(node))
        {
            LOG_WARNING(logger, "Nœud non couvert : "
                + node->getSourceId() + ", BFS de secours lancé.");
            int maxX = runBFS(graph, node, visited, offsetX, logger);
            offsetX = maxX + 15;
        }
    }

    // Post-traitements dans l'ordre :
    // 1. Correction des branches convergentes (diamonds)
    // 2. Normalisation des positions des bras de crossing
    fixCollapsedBranches(graph, logger);
    fixCrossingLayout(graph, logger);

    LOG_INFO(logger, "Compute — terminé.");
}


// =============================================================================
// Détection des terminus
// =============================================================================

std::vector<PCCNode*> PCCLayout::findTermini(const PCCGraph& graph, Logger& logger)
{
    std::unordered_set<PCCNode*> nonTermini;
    for (const auto& edgePtr : graph.getEdges())
    {
        if (edgePtr->getRole() != PCCEdgeRole::STRAIGHT)
            nonTermini.insert(edgePtr->getTo());
    }

    std::vector<PCCNode*> termini;
    for (const auto& nodePtr : graph.getNodes())
    {
        PCCNode* node = nodePtr.get();
        if (!nonTermini.count(node) && node->getEdges().size() == 1)
            termini.push_back(node);
    }

    if (termini.empty())
    {
        LOG_WARNING(logger, "FindTermini — aucun terminus détecté, "
            "premier nœud utilisé comme point de départ.");
        termini.push_back(graph.getNodes().front().get());
    }
    else
    {
        LOG_DEBUG(logger, "FindTermini — "
            + std::to_string(termini.size()) + " terminus détectés.");
    }

    return termini;
}


// =============================================================================
// BFS topologique linéaire
// =============================================================================

int PCCLayout::runBFS(PCCGraph& graph,
    PCCNode* start,
    std::unordered_set<PCCNode*>& visited,
    int                           offsetX,
    Logger& logger)
{
    std::queue<BFSItem> frontier;
    std::set<std::pair<int, int>> occupied;

    visited.insert(start);
    occupied.insert({ offsetX, 0 });
    frontier.push({ start, offsetX, 0, false, nullptr });

    int processedCount = 0;
    int maxX = offsetX;

    while (!frontier.empty())
    {
        auto [node, x, y, arrivedViaDeviation, arrivedViaEdge] = frontier.front();
        frontier.pop();

        if (!node) continue;

        node->setPosition({ x, y });
        ++processedCount;
        if (x > maxX) maxX = x;

        // ---------------------------------------------------------------
        // Collecte des voisins non visités avec leur rôle et sens.
        // ---------------------------------------------------------------
        struct NeighbourInfo
        {
            PCCNode* node;
            PCCEdgeRole role;
            bool        isForward;
        };
        std::vector<NeighbourInfo> neighbours;

        for (PCCEdge* edge : node->getEdges())
        {
            const bool isForward = (edge->getFrom() == node);
            PCCNode* neighbour = isForward ? edge->getTo() : edge->getFrom();

            if (visited.count(neighbour))
                continue;

            auto it = std::find_if(neighbours.begin(), neighbours.end(),
                [&](const NeighbourInfo& n) { return n.node == neighbour; });

            if (it == neighbours.end())
                neighbours.push_back({ neighbour, edge->getRole(), isForward });
            else if (edge->getRole() == PCCEdgeRole::DEVIATION)
                it->role = PCCEdgeRole::DEVIATION;
        }

        // ---------------------------------------------------------------
        // Tri déterministe — DEVIATION en dernier dans tous les cas.
        // ---------------------------------------------------------------
        std::stable_sort(neighbours.begin(), neighbours.end(),
            [](const NeighbourInfo& a, const NeighbourInfo& b)
            {
                const bool aDev = (a.role == PCCEdgeRole::DEVIATION);
                const bool bDev = (b.role == PCCEdgeRole::DEVIATION);
                return !aDev && bDev;
            });

        // ---------------------------------------------------------------
        // swNode : switch courant — déclaré une seule fois avant la boucle.
        // ---------------------------------------------------------------
        auto* swNode = dynamic_cast<PCCSwitchNode*>(node);
        const int side = swNode ? swNode->getDeviationSide() : 1;

        for (const NeighbourInfo& neighbour : neighbours)
        {
            int  nextX = x + 1;
            int  nextY = y;
            bool nextArrivedViaDev = false;
            PCCEdge* edgeToNeighbour = nullptr;

            // Recherche de l'arête vers ce voisin (pour getExitEdgeFor)
            for (PCCEdge* edge : node->getEdges())
            {
                if ((edge->getFrom() == node && edge->getTo() == neighbour.node)
                    || (edge->getTo() == node && edge->getFrom() == neighbour.node))
                {
                    edgeToNeighbour = edge;
                    break;
                }
            }

            // -----------------------------------------------------------
            // Cas CROSSING — nœud courant est un crossing.
            // Propagation uniquement sur la voie traversante (A<->C ou B<->D).
            // -----------------------------------------------------------
            if (node->getNodeType() == PCCNodeType::CROSSING)
            {
                const auto* cr = static_cast<const PCCCrossingNode*>(node);
                PCCEdge* exitEdge = cr->getExitEdgeFor(arrivedViaEdge);

                if (!exitEdge || exitEdge->getTo() != neighbour.node)
                    continue;

                nextX = x + 1;
                nextY = y;
                nextArrivedViaDev = arrivedViaDeviation;
            }
            // -----------------------------------------------------------
            // Cas CROSSING — arête entrante (bras adjacent au crossing).
            // Traité comme STRAIGHT pendant le BFS.
            // fixCrossingLayout repositionnera le bras ensuite.
            // -----------------------------------------------------------
            else if (neighbour.role == PCCEdgeRole::CROSSING)
            {
                nextX = x + 1;
                nextY = y;
                nextArrivedViaDev = arrivedViaDeviation;
            }
            // -----------------------------------------------------------
            // Cas DEVIATION
            // -----------------------------------------------------------
            else if (neighbour.role == PCCEdgeRole::DEVIATION)
            {
                auto* swNeigh = dynamic_cast<PCCSwitchNode*>(neighbour.node);

                if (swNeigh && swNode)  // double switch : meme colonne X
                {
                    nextX = x;
                    nextY = y + side;
                    nextArrivedViaDev = true;
                }
                else  // branche deviee classique
                {
                    nextX = x + 1;
                    nextY = y + side;
                    nextArrivedViaDev = false;
                }
            }
            // -----------------------------------------------------------
            // Cas NORMAL en amont (arrivee par deviation, double switch)
            // -----------------------------------------------------------
            else if (arrivedViaDeviation
                && swNode
                && neighbour.role == PCCEdgeRole::NORMAL
                && neighbour.isForward)
            {
                nextX = x - 1;
                nextY = y;
                nextArrivedViaDev = false;
            }
            // else : STRAIGHT / ROOT / NORMAL standard -> (x+1, y) deja initialise

            // Résolution de collision
            if (occupied.count({ nextX, nextY }))
            {
                for (int delta = 1; ; ++delta)
                {
                    if (!occupied.count({ nextX, nextY + delta }))
                    {
                        nextY += delta;
                        break;
                    }
                    if (!occupied.count({ nextX, nextY - delta }))
                    {
                        nextY -= delta;
                        break;
                    }
                }
            }

            LOG_DEBUG(graph.getLogger(),
                neighbour.node->getSourceId()
                + " positionnée en [ "
                + std::to_string(nextX) + " ; "
                + std::to_string(nextY) + " ]");

            visited.insert(neighbour.node);
            occupied.insert({ nextX, nextY });
            frontier.push({ neighbour.node, nextX, nextY,
                            nextArrivedViaDev, edgeToNeighbour });
        }
    }

    LOG_DEBUG(graph.getLogger(),
        std::to_string(processedCount)
        + " noeuds positionnés depuis " + start->getSourceId()
        + " (offsetX=" + std::to_string(offsetX) + ").");

    return maxX;
}


// =============================================================================
// Post-traitement — correction des branches convergentes de longueurs inegales
// =============================================================================

void PCCLayout::fixCollapsedBranches(PCCGraph& graph, Logger& logger)
{
    bool changed = true;
    int  pass = 0;

    while (changed)
    {
        changed = false;
        ++pass;

        for (const auto& nodePtr : graph.getNodes())
        {
            if (nodePtr->getNodeType() != PCCNodeType::SWITCH) continue;
            auto* sw = static_cast<PCCSwitchNode*>(nodePtr.get());

            const int swX = sw->getPosition().x;
            const int swY = sw->getPosition().y;

            bool collapsed = false;

            const PCCEdge* devEdge = sw->getDeviationEdge();
            if (devEdge && devEdge->getTo())
            {
                const PCCNode* tgt = devEdge->getTo();
                if (tgt->getPosition().x == swX
                    && tgt->getPosition().y != swY)
                    collapsed = true;
            }

            if (!collapsed)
            {
                const PCCEdge* normEdge = sw->getNormalEdge();
                if (normEdge && normEdge->getTo())
                {
                    const PCCNode* tgt = normEdge->getTo();
                    if (tgt->getPosition().x == swX
                        && tgt->getPosition().y != swY)
                        collapsed = true;
                }
            }

            if (!collapsed) continue;

            std::unordered_set<PCCNode*> toShift;
            toShift.insert(sw);

            // Phase 1 : backbone aval (x > currX) + partenaires double switch
            const PCCEdge* rootEdge = sw->getRootEdge();
            if (rootEdge && rootEdge->getTo())
            {
                PCCNode* rootTarget = rootEdge->getTo();
                toShift.insert(rootTarget);

                std::queue<PCCNode*> q;
                q.push(rootTarget);

                while (!q.empty())
                {
                    PCCNode* curr = q.front();
                    q.pop();

                    const int  currX = curr->getPosition().x;
                    const bool currIsSwitch = (curr->getNodeType() == PCCNodeType::SWITCH);

                    for (PCCEdge* edge : curr->getEdges())
                    {
                        PCCNode* nb = (edge->getFrom() == curr)
                            ? edge->getTo() : edge->getFrom();
                        if (!nb || toShift.count(nb)) continue;

                        const int  nbX = nb->getPosition().x;
                        const bool nbIsSwitch = (nb->getNodeType() == PCCNodeType::SWITCH);

                        const bool isDownstream = (nbX > currX);
                        const bool isDoubleSwitchPartner =
                            (nbX == currX) && currIsSwitch && nbIsSwitch;

                        if (isDownstream || isDoubleSwitchPartner)
                        {
                            toShift.insert(nb);
                            q.push(nb);
                        }
                    }
                }
            }

            // Phase 2 : voies deviees des switches aval (x > swX)
            bool devPhaseChanged = true;
            while (devPhaseChanged)
            {
                devPhaseChanged = false;
                const std::vector<PCCNode*> snapshot(toShift.begin(), toShift.end());

                for (PCCNode* candidate : snapshot)
                {
                    if (candidate->getNodeType() != PCCNodeType::SWITCH) continue;

                    for (PCCEdge* edge : candidate->getEdges())
                    {
                        PCCNode* nb = (edge->getFrom() == candidate)
                            ? edge->getTo() : edge->getFrom();
                        if (!nb || toShift.count(nb)) continue;
                        if (nb->getNodeType() == PCCNodeType::SWITCH) continue;
                        if (nb->getPosition().x <= swX) continue;

                        std::queue<PCCNode*> devQ;
                        toShift.insert(nb);
                        devQ.push(nb);
                        devPhaseChanged = true;

                        while (!devQ.empty())
                        {
                            PCCNode* curr = devQ.front();
                            devQ.pop();

                            for (PCCEdge* e : curr->getEdges())
                            {
                                PCCNode* next = (e->getFrom() == curr)
                                    ? e->getTo() : e->getFrom();
                                if (!next || toShift.count(next)) continue;
                                if (next->getNodeType() == PCCNodeType::SWITCH) continue;
                                if (next->getPosition().x > swX)
                                {
                                    toShift.insert(next);
                                    devQ.push(next);
                                }
                            }
                        }
                    }
                }
            }

            // Décalage de +1 sur le sous-graphe complet
            for (PCCNode* shiftNode : toShift)
            {
                auto pos = shiftNode->getPosition();
                pos.x += 1;
                shiftNode->setPosition(pos);
            }

            LOG_DEBUG(logger,
                "fixCollapsedBranches pass=" + std::to_string(pass)
                + " : " + sw->getSourceId()
                + " + " + std::to_string(toShift.size() - 1)
                + " noeuds aval decales +1"
                + " (swX: " + std::to_string(swX)
                + " -> " + std::to_string(swX + 1) + ")");

            changed = true;
            break;
        }
    }

    if (pass > 1)
        LOG_DEBUG(logger,
            "fixCollapsedBranches termine — "
            + std::to_string(pass - 1) + " passe(s).");
}


// =============================================================================
// Post-traitement — normalisation des positions des bras de CrossBlock
//
// Spec StraightCrossBlock :
//   Slot A : [crX-1, crY  ]   Slot D : [crX+1, crY  ]
//   Slot B : [crX-1, crY+voie2Offset]   Slot C : [crX+1, crY+voie2Offset]
//
//   Le crossing lui-meme reste a [crX, crY] — centre pivot, invisible.
//   voie2Offset = ±1 selon le cote naturel du BFS (déterminé depuis les bras).
// =============================================================================

void PCCLayout::fixCrossingLayout(PCCGraph& graph, Logger& logger)
{
    for (const auto& nodePtr : graph.getNodes())
    {
        if (nodePtr->getNodeType() != PCCNodeType::CROSSING) continue;

        auto* cr = static_cast<PCCCrossingNode*>(nodePtr.get());
        const int crX = cr->getPosition().x;
        const int crY = cr->getPosition().y;

        // Collecte les 4 voisins depuis les slots edgeA/B/C/D
        PCCNode* nbs[4] = {
            cr->getEdgeA() ? cr->getEdgeA()->getTo() : nullptr,
            cr->getEdgeB() ? cr->getEdgeB()->getTo() : nullptr,
            cr->getEdgeC() ? cr->getEdgeC()->getTo() : nullptr,
            cr->getEdgeD() ? cr->getEdgeD()->getTo() : nullptr
        };

        bool allValid = true;
        for (PCCNode* nb : nbs)
        {
            if (!nb) { allValid = false; break; }
        }
        if (!allValid)
        {
            LOG_WARNING(logger, cr->getSourceId()
                + " — fixCrossingLayout : slot(s) null, ignore.");
            continue;
        }

        // Separation gauche / droite selon X BFS du bras
        std::vector<PCCNode*> leftSide, rightSide;
        for (PCCNode* nb : nbs)
        {
            if (nb->getPosition().x <= crX) leftSide.push_back(nb);
            else                            rightSide.push_back(nb);
        }

        // Tri par Y dans chaque groupe : haut (Y min) en premier
        auto byY = [](const PCCNode* a, const PCCNode* b)
            {
                return a->getPosition().y < b->getPosition().y;
            };
        std::sort(leftSide.begin(), leftSide.end(), byY);
        std::sort(rightSide.begin(), rightSide.end(), byY);

        // Déterminer voie2Offset depuis le Y naturel des bras inférieurs
        // voie1 = crY (bras du dessus), voie2 = crY + offset (bras du dessous)
        int voie2Offset = 1;  // valeur par défaut
        if (leftSide.size() >= 2)
            voie2Offset = leftSide[1]->getPosition().y - crY;
        else if (rightSide.size() >= 2)
            voie2Offset = rightSide[1]->getPosition().y - crY;

        // Si offset nul (tous au même Y), utiliser -1 par défaut
        if (voie2Offset == 0) voie2Offset = -1;

        // Assignation : A[crX-1, crY]  D[crX+1, crY]
        //               B[crX-1, crY+voie2Offset]  C[crX+1, crY+voie2Offset]
        if (leftSide.size() >= 1) leftSide[0]->setPosition({ crX - 1, crY }); // A
        if (leftSide.size() >= 2) leftSide[1]->setPosition({ crX - 1, crY + voie2Offset }); // B
        if (rightSide.size() >= 1) rightSide[0]->setPosition({ crX + 1, crY }); // D
        if (rightSide.size() >= 2) rightSide[1]->setPosition({ crX + 1, crY + voie2Offset }); // C

        LOG_DEBUG(logger, cr->getSourceId()
            + " fixCrossing cr=[" + std::to_string(crX) + "," + std::to_string(crY) + "]"
            + " voie2Offset=" + std::to_string(voie2Offset)
            + " A=" + (leftSide.size() >= 1 ? leftSide[0]->getSourceId() : "null")
            + "[" + std::to_string(crX - 1) + "," + std::to_string(crY) + "]"
            + " B=" + (leftSide.size() >= 2 ? leftSide[1]->getSourceId() : "null")
            + "[" + std::to_string(crX - 1) + "," + std::to_string(crY + voie2Offset) + "]"
            + " D=" + (rightSide.size() >= 1 ? rightSide[0]->getSourceId() : "null")
            + "[" + std::to_string(crX + 1) + "," + std::to_string(crY) + "]"
            + " C=" + (rightSide.size() >= 2 ? rightSide[1]->getSourceId() : "null")
            + "[" + std::to_string(crX + 1) + "," + std::to_string(crY + voie2Offset) + "]");
    }
}
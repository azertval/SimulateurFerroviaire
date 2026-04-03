/**
 * @file  PCCLayout.cpp
 * @brief Implémentation du calculateur de positions logiques PCC.
 *
 * @see PCCLayout
 */
#include "PCCLayout.h"
#include "PCCSwitchNode.h"

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
        offsetX = maxX + 2;
    }

    for (const auto& nodePtr : graph.getNodes())
    {
        PCCNode* node = nodePtr.get();
        if (!visited.count(node))
        {
            LOG_WARNING(logger, "Nœud non couvert : "
                + node->getSourceId() + ", BFS de secours lancé.");
            int maxX = runBFS(graph, node, visited, offsetX, logger);
            offsetX = maxX + 2;
        }
    }

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
    frontier.push({ start, offsetX, 0, false });

    int processedCount = 0;
    int maxX = offsetX;

    while (!frontier.empty())
    {
        auto [node, x, y, arrivedViaDeviation] = frontier.front();
        frontier.pop();

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
        // Tri déterministe.
        //
        // Cas standard (arrivedViaDeviation == false) :
        //   ROOT / NORMAL / STRAIGHT d'abord — la continuation backbone
        //   occupe (x+1, y) avant les branches déviées.
        //
        // Cas arrivée par déviation (arrivedViaDeviation == true) :
        //   ROOT d'abord — il est la continuation de la route déviée
        //   et doit occuper (x+1, y) avant que NORMAL (devenu branche
        //   secondaire) ne soit planifié.
        //   Le tri non-DEVIATION en premier reste valide dans les deux cas.
        // ---------------------------------------------------------------
        std::stable_sort(neighbours.begin(), neighbours.end(),
            [](const NeighbourInfo& a, const NeighbourInfo& b)
            {
                const bool aDev = (a.role == PCCEdgeRole::DEVIATION);
                const bool bDev = (b.role == PCCEdgeRole::DEVIATION);
                return !aDev && bDev;
            });

        // ---------------------------------------------------------------
        // Calcul des positions — règle linéaire strictement topologique.
        //
        //  Cas standard (arrivedViaDeviation == false)
        //  ─────────────────────────────────────────────────────────────
        //  STRAIGHT / ROOT / NORMAL
        //      → (x+1, y)            continuité de ligne, Y constant.
        //
        //  DEVIATION → SwitchNode    [aiguille double]
        //      → (x, y ± côté)       même colonne X, Y décalé.
        //      arrivedViaDeviation=true transmis au switch partenaire.
        //
        //  DEVIATION → nœud ordinaire  [branche déviée classique]
        //      → (x+1, y ± côté)     colonne suivante, Y décalé.
        //
        //  Cas arrivée par déviation (arrivedViaDeviation == true)
        //  ─────────────────────────────────────────────────────────────
        //  ROOT (forward)
        //      → (x+1, y)            ROOT est la continuation de la route
        //                            déviée — Y constant, même logique que
        //                            STRAIGHT en cas standard.
        //
        //  NORMAL (forward)
        //      → (x-1, y)            NORMAL est en AMONT dans ce cas.
        //                            La double aiguille est traversée de côté :
        //                            ROOT poursuit vers l'aval, NORMAL recule.
        //
        //  DEVIATION → SwitchNode    [aiguille double, symétrique]
        //      → (x, y ± côté)       idem cas standard.
        //
        //  DEVIATION → nœud ordinaire
        //      → (x+1, y ± côté)     idem cas standard.
        //
        //  Le côté (±1) provient exclusivement de
        //  PCCSwitchNode::getDeviationSide(), calculé une seule fois par
        //  PCCGraphBuilder::computeDeviationSides à partir des lat/lon.
        //  C'est la seule donnée GPS tolérée dans ce calcul.
        // ---------------------------------------------------------------
        auto* swNode = dynamic_cast<PCCSwitchNode*>(node);
        const int side = swNode ? swNode->getDeviationSide() : 1;

        for (const NeighbourInfo& neighbour : neighbours)
        {
            int  nextX = x + 1;
            int  nextY = y;
            bool nextArrivedViaDev = false;

            if (neighbour.role == PCCEdgeRole::DEVIATION)
            {
                auto* swNeigh = dynamic_cast<PCCSwitchNode*>(neighbour.node);
                auto* swNode = dynamic_cast<PCCSwitchNode*>(node);  // nœud courant
                if (swNeigh && swNode)  // double switch : switch → switch direct
                {
                    nextX = x;
                    nextY = y + side;
                    nextArrivedViaDev = true;
                }
                else  // straight→switch ou switch→straight via déviation
                {
                    nextX = x + 1;
                    nextY = y + side;
                    nextArrivedViaDev = false;
                }
            }
            else if (arrivedViaDeviation
                && swNode
                && neighbour.role == PCCEdgeRole::NORMAL
                && neighbour.isForward)
            {
                // Arrivée par déviation (double aiguille) — NORMAL est en AMONT.
                //
                // Quand le BFS atteint sw/B via la déviation de sw/A :
                //   • ROOT de sw/B pointe vers l'aval (suite du réseau) → x+1, y
                //   • NORMAL de sw/B pointe vers l'amont (dead end / sens inverse) → x-1, y
                //
                // On ne décale PAS Y : NORMAL reste sur le même rang que sw/B.
                //
                // Exemple : sw/2[32,1] atteint depuis sw/4[32,0].
                //   ROOT  s/6 → [33, 1]  (aval, standard)   ✓
                //   NORMAL s/4 → [31, 1] (amont, x-1)       ✓
                nextX = x - 1;
                nextY = y;
                nextArrivedViaDev = false;
            }
            // else : STRAIGHT / ROOT / NORMAL standard → (x+1, y)

            // Résolution de collision — topologies exceptionnelles uniquement.
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

            LOG_DEBUG(graph.getLogger(), neighbour.node->getSourceId() + " positionnée en [ " + std::to_string(nextX) + " ; " + std::to_string(nextY) + " ]");

            visited.insert(neighbour.node);
            occupied.insert({ nextX, nextY });
            frontier.push({ neighbour.node, nextX, nextY, nextArrivedViaDev });
        }
    }

    LOG_DEBUG(graph.getLogger(), std::to_string(processedCount)
        + " nœuds positionnés depuis " + start->getSourceId()
        + " (offsetX=" + std::to_string(offsetX) + ").");

    return maxX;
}
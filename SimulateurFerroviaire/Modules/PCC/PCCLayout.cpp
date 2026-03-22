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
#include <unordered_map>


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
            continue;  // Terminus déjà couvert par un BFS précédent

        int maxX = runBFS(graph, terminus, visited, offsetX, logger);
        offsetX = maxX + 2;
        // ^ Le prochain composant commence 2 unités après le X max du BFS précédent
    }

    // Nœuds non couverts — composante sans terminus détecté
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
    // Collecte les nœuds cibles d'une arête switch (ROOT / NORMAL / DEVIATION)
    // Ces nœuds ne peuvent pas être des terminus
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
        // Un terminus a exactement 1 arête adjacente et n'est cible d'aucun switch
        if (!nonTermini.count(node) && node->getEdges().size() == 1)
            termini.push_back(node);
    }

    if (termini.empty())
    {
        // Réseau circulaire ou entièrement bidirectionnel — point de départ arbitraire
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
// BFS
// =============================================================================

int PCCLayout::runBFS(PCCGraph& graph,
    PCCNode* start,
    std::unordered_set<PCCNode*>& visited,
    int                           offsetX,
    Logger& logger)
{
    std::queue<BFSItem> frontier;

    // Positions occupées — permet de détecter et résoudre les collisions.
    // Clé = paire (x, y). Marquée à l'enqueue (pas au dequeue) pour que
    // deux voisins du même nœud ne reçoivent pas la même position.
    std::set<std::pair<int, int>> occupied;

    visited.insert(start);
    occupied.insert({ offsetX, 0 });
    frontier.push({ start, offsetX, 0 });

    int processedCount = 0;
    int maxX = offsetX;

    while (!frontier.empty())
    {
        auto [node, x, y] = frontier.front();
        frontier.pop();

        node->setPosition({ x, y });
        ++processedCount;
        if (x > maxX) maxX = x;

        // -----------------------------------------------------------------
        // Passe 1 : identifier les voisins non visités et déterminer si
        //           AU MOINS UNE arête forward DEVIATION pointe vers eux.
        //
        // Nécessaire pour les doubles aiguilles (sw/A ↔ sw/B) :
        //   sw/B→sw/A DEVIATION est ajoutée à sw/A en premier (backward),
        //   sw/A→sw/B DEVIATION est ajoutée après (forward).
        //   Sans ce scan, l'arête backward est parcourue en premier,
        //   sw/B est visité à y courant, et l'arête forward arrive trop tard.
        // -----------------------------------------------------------------
        std::unordered_map<PCCNode*, bool> neighbourDevMap;

        for (PCCEdge* edge : node->getEdges())
        {
            const bool isForward = (edge->getFrom() == node);
            PCCNode* neighbour = isForward ? edge->getTo() : edge->getFrom();

            if (visited.count(neighbour))
                continue;

            if (isForward && edge->getRole() == PCCEdgeRole::DEVIATION)
                neighbourDevMap[neighbour] = true;
            else
                neighbourDevMap.emplace(neighbour, false);
        }

        // -----------------------------------------------------------------
        // Passe 2 : tri déterministe — non-déviation d'abord.
        //
        // Les branches ROOT / NORMAL / STRAIGHT sont placées avant les
        // branches DEVIATION. Cela garantit que la continuation du backbone
        // reçoit la position "naturelle" (même Y) tandis que les branches
        // déviées sont décalées. En cas de collision, c'est la branche
        // secondaire qui est déplacée, pas la continuation principale.
        // -----------------------------------------------------------------
        std::vector<std::pair<PCCNode*, bool>> sortedNeighbours(
            neighbourDevMap.begin(), neighbourDevMap.end());
        std::sort(sortedNeighbours.begin(), sortedNeighbours.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        // -----------------------------------------------------------------
        // Passe 3 : calcul du Y et résolution des collisions.
        // -----------------------------------------------------------------
        for (auto& [neighbour, isDeviation] : sortedNeighbours)
        {
            const int nextX = x + 1;
            int nextY = y;

            if (isDeviation)
            {
                // Direction de la déviation déterminée par la géographie.
                // PCCGraphBuilder::computeDeviationSides a calculé le côté
                // pour chaque switch à partir des coordonnées GPS :
                //   +1 = déviation au nord (vers le haut)
                //   -1 = déviation au sud  (vers le bas)
                auto* sw = dynamic_cast<PCCSwitchNode*>(node);
                const int side = sw ? sw->getDeviationSide() : 1;
                nextY = y + side;
            }

            // Résolution de collision — si la position est déjà prise,
            // chercher la position libre la plus proche en alternant +/-.
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

            visited.insert(neighbour);
            occupied.insert({ nextX, nextY });
            frontier.push({ neighbour, nextX, nextY });
        }
    }

    LOG_DEBUG(graph.getLogger(), std::to_string(processedCount)
        + " nœuds positionnés depuis " + start->getSourceId()
        + " (offsetX=" + std::to_string(offsetX) + ").");

    return maxX;
}
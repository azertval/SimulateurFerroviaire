/**
 * @file  PCCLayout.cpp
 * @brief Implémentation du calculateur de positions logiques PCC.
 *
 * @see PCCLayout
 */
#include "PCCLayout.h"

#include <queue>
#include <algorithm>


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

        runBFS(graph, terminus, visited, offsetX, logger);
        offsetX += 10;
        // ^ Séparation entre composantes déconnectées — espacées de 10 unités X
        //   pour éviter les superpositions dans TCORenderer
    }

    // Nœuds non couverts — composante sans terminus détecté
    for (const auto& nodePtr : graph.getNodes())
    {
        PCCNode* node = nodePtr.get();
        if (!visited.count(node))
        {
            LOG_WARNING(logger, "Nœud non couvert : "
                + node->getSourceId() + ", BFS de secours lancé.");
            runBFS(graph, node, visited, offsetX, logger);
            offsetX += 10;
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

void PCCLayout::runBFS(PCCGraph& graph,
    PCCNode* start,
    std::unordered_set<PCCNode*>& visited,
    int                           offsetX,
    Logger& logger)
{
    std::queue<BFSItem> frontier;

    visited.insert(start);
    frontier.push({ start, offsetX, 0 });
    // ^ Nœud de départ à x=offsetX (séparation entre composantes), y=0 (backbone)

    int processedCount = 0;

    while (!frontier.empty())
    {
        // Structured binding C++17 — déstructure BFSItem en trois variables
        auto [node, x, y] = frontier.front();
        frontier.pop();

        node->setPosition({ x, y });
        ++processedCount;

        for (PCCEdge* edge : node->getEdges())
        {
            PCCNode* neighbour = edge->getTo();

            if (visited.count(neighbour))
                continue;  // Nœud déjà visité — évite les cycles

            // Calcul du Y selon le rôle de l'arête empruntée
            int nextY = y;
            if (edge->getRole() == PCCEdgeRole::DEVIATION)
                nextY = y + 1;
            // NORMAL, ROOT, STRAIGHT → y inchangé (continuité du rang courant)

            visited.insert(neighbour);
            frontier.push({ neighbour, x + 1, nextY });
            // ^ x grandit de 1 à chaque niveau BFS
        }
    }

    LOG_DEBUG(logger, "RunBFS — "
        + std::to_string(processedCount)
        + " nœuds positionnés depuis " + start->getSourceId()
        + " (offsetX=" + std::to_string(offsetX) + ").");
}
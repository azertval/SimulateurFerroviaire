/**
 * @file  PCCGraphBuilder.cpp
 * @brief Implémentation du constructeur du graphe PCC.
 *
 * @see PCCGraphBuilder
 */
#include "PCCGraphBuilder.h"

#include "Engine/Core/Topology/TopologyRepository.h"
#include "Engine/Core/Topology/TopologyData.h"

#include <unordered_set>


 // =============================================================================
 // Point d'entrée
 // =============================================================================

void PCCGraphBuilder::build(PCCGraph& graph, Logger& logger)
{
    // Remise à zéro — tous les raw pointers précédemment distribués sont invalidés
    graph.clear();

    const TopologyData& topo = TopologyRepository::instance().data();

    if (topo.straights.empty() && topo.switches.empty())
    {
        LOG_WARNING(logger, "TopologyRepository vide, graphe non construit.");
        return;
    }

    LOG_INFO(logger, "PCCGraphBuilder::build — début : "
        + std::to_string(topo.straights.size()) + " straights, "
        + std::to_string(topo.switches.size()) + " switches.");

    // Passe 1 : tous les nœuds sont créés et indexés
    //   L'index est complet ici — buildEdges peut résoudre tous les IDs
    buildNodes(graph, topo, logger);
   
    // Passe 2 : toutes les arêtes sont créées et câblées
    buildEdges(graph, topo, logger);
   

    LOG_INFO(logger, "PCCGraphBuilder::build — terminé : "
        + std::to_string(graph.nodeCount()) + " nœuds, "
        + std::to_string(graph.edgeCount()) + " arêtes.");
}


// =============================================================================
// Passe 1 — Nœuds
// =============================================================================

void PCCGraphBuilder::buildNodes(PCCGraph& graph, const TopologyData& topo, Logger& logger)
{
    // Parcours des straights — const auto& pour éviter la copie du unique_ptr
    for (const auto& st : topo.straights)
        graph.addStraightNode(st.get());
    // ^ st est un const unique_ptr<StraightBlock>&
    //   st.get() retourne StraightBlock* non-propriétaire

    // Parcours des switches
    for (const auto& sw : topo.switches)
        graph.addSwitchNode(sw.get());

    LOG_DEBUG(logger, std::to_string(graph.nodeCount()) + " nœuds indexés.");
}


// =============================================================================
// Passe 2 — Arêtes
// =============================================================================

void PCCGraphBuilder::buildEdges(PCCGraph& graph, const TopologyData& topo, Logger& logger)
{
    int edgeCount = 0;

    // --- Arêtes depuis les SwitchBlocks ---
    // Un switch orienté expose trois branches par ID.
    // On crée une arête pour chaque branche résolue dans l'index.
    for (const auto& sw : topo.switches)
    {
        

        if (!sw->isOriented())
        {
            LOG_WARNING(logger, "Switch non orienté ignoré : " + sw->getId());
            continue;
        }

        PCCNode* swNode = graph.findNode(sw->getId());
        if (!swNode) continue;  // Ne devrait pas arriver après buildNodes

        auto resolveEdge = [&](const std::optional<std::string>& branchId, PCCEdgeRole role)
            {
                if (!branchId.has_value()) return;
                PCCNode* target = graph.findNode(*branchId);
                if (!target)
                {
                    LOG_WARNING(logger, "Branche introuvable : "
                        + *branchId + " (depuis " + sw->getId() + ")");
                    return;
                }
                graph.addEdge(swNode, target, role);
                ++edgeCount;
            };

        resolveEdge(sw->getRootBranchId(), PCCEdgeRole::ROOT);
        resolveEdge(sw->getNormalBranchId(), PCCEdgeRole::NORMAL);
        resolveEdge(sw->getDeviationBranchId(), PCCEdgeRole::DEVIATION);
    }

    // --- Arêtes STRAIGHT entre StraightBlocks adjacents ---
    // Les connexions straight-to-straight (segments découpés, ex. "s/0_c1" ↔ "s/0_c2")
    // sont gérées ici. Les connexions straight↔switch sont déjà couvertes
    // côté switch (ROOT/NORMAL/DEVIATION) — on les ignore ici pour éviter
    // les doublons.
    std::unordered_set<std::string> processedEdges;
    // ^ Mémorise les paires déjà traitées pour éviter a→b ET b→a

    for (const auto& st : topo.straights)
    {
        PCCNode* fromNode = graph.findNode(st->getId());
        if (!fromNode) continue;

        for (const auto& neighbourId : st->getNeighbourIds())
        {
            PCCNode* toNode = graph.findNode(neighbourId);
            if (!toNode) continue;

            // Ignorer les connexions vers un switch — déjà traitées côté switch
            if (toNode->getNodeType() == PCCNodeType::SWITCH)
                continue;

            // Dédoublonnage via clé canonique
            const std::string key = makeEdgeKey(st->getId(), neighbourId);
            if (processedEdges.count(key))
                continue;  // Arête déjà créée dans l'autre sens

            processedEdges.insert(key);
            graph.addEdge(fromNode, toNode, PCCEdgeRole::STRAIGHT);
            ++edgeCount;
        }
    }
    
    LOG_DEBUG(logger, std::to_string(edgeCount) + " arêtes créées.");
}


// =============================================================================
// Helper
// =============================================================================

std::string PCCGraphBuilder::makeEdgeKey(const std::string& idA,
    const std::string& idB)
{
    // Clé canonique — indépendante de l'ordre des paramètres
    // "s/0|sw/3" == makeEdgeKey("sw/3", "s/0") == makeEdgeKey("s/0", "sw/3")
    return (idA < idB) ? (idA + "|" + idB) : (idB + "|" + idA);
}
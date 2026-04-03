/**
 * @file  PCCGraphBuilder.cpp
 * @brief Implémentation du constructeur du graphe PCC.
 *
 * @par Correction v2 — chaînes de sous-blocs (getNeighbours vs getNeighbourIds)
 * buildEdges() utilise StraightBlock::getNeighbours() (pointeurs résolus par
 * Phase9_RepositoryTransfer) et non getNeighbourIds() qui est vide pour les
 * sous-blocs de subdivision.
 *
 * @par Modification v3 — computeDeviationSides sur UTM (Famille G3)
 * computeDeviationSides() compare désormais getTipOnDeviationUTM()->y avec
 * getJunctionUTM().y (axe Y UTM = nord) au lieu des latitudes WGS84.
 * Supprime la dernière dépendance WGS84 dans le module PCC.
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

    // Passe 1 : tous les nœuds sont créés et indexés.
    buildNodes(graph, topo, logger);

    // Passe 2 : toutes les arêtes sont créées et câblées.
    buildEdges(graph, topo, logger);

    // Passe 3 : calcul du côté géographique de chaque déviation (UTM).
    computeDeviationSides(graph, topo, logger);

    LOG_INFO(logger, "PCCGraphBuilder::build — terminé : "
        + std::to_string(graph.nodeCount()) + " nœuds, "
        + std::to_string(graph.edgeCount()) + " arêtes.");
}


// =============================================================================
// Passe 1 — Nœuds
// =============================================================================

void PCCGraphBuilder::buildNodes(PCCGraph& graph,
    const TopologyData& topo,
    Logger& logger)
{
    for (const auto& st : topo.straights)
        graph.addStraightNode(st.get());

    for (const auto& sw : topo.switches)
        graph.addSwitchNode(sw.get());

    LOG_DEBUG(logger, std::to_string(graph.nodeCount()) + " nœuds indexés.");
}


// =============================================================================
// Passe 2 — Arêtes
// =============================================================================

void PCCGraphBuilder::buildEdges(PCCGraph& graph,
    const TopologyData& topo,
    Logger& logger)
{
    int edgeCount = 0;

    // -------------------------------------------------------------------------
    // Dédoublonnage des arêtes orientées switch→straight.
    // Un crossover produit deux arêtes depuis deux switches différents vers le
    // même straight — sans dédoublonnage, le straight recevrait deux arêtes
    // entrantes et son BFS serait brisé.
    // -------------------------------------------------------------------------
    std::unordered_set<std::string> processedSwitchEdges;

    for (const auto& swPtr : topo.switches)
    {
        SwitchBlock* source = swPtr.get();
        if (!source->isOriented()) continue;

        PCCNode* swNode = graph.findNode(source->getId());
        if (!swNode) continue;

        auto addSwitchEdge = [&](ShuntingElement* branch, PCCEdgeRole role)
            {
                if (!branch) return;

                PCCNode* branchNode = graph.findNode(branch->getId());
                if (!branchNode) return;

                // Double liaison sw↔sw : les deux arêtes dirigées sont nécessaires
                // (sw/A→sw/B ET sw/B→sw/A) pour que chaque switch ait son deviationEdge.
                // → clé dirigée pour ne pas dédoublonner ces deux arêtes distinctes.
                //
                // Crossover sw→straight←sw : une seule arête par switch suffit,
                // deux switches différents pointant vers le même straight auraient
                // des clés canoniques différentes de toute façon.
                // → clé canonique pour les cas standard (évite les doublons réels).
                const bool isSwitchToSwitch =
                    (branchNode->getNodeType() == PCCNodeType::SWITCH);

                const std::string key = isSwitchToSwitch
                    ? (source->getId() + ">" + branch->getId())    // dirigée
                    : makeEdgeKey(source->getId(), branch->getId()); // canonique

                if (processedSwitchEdges.count(key)) return;
                processedSwitchEdges.insert(key);

                graph.addEdge(swNode, branchNode, role);
                ++edgeCount;
            };

        addSwitchEdge(source->getRootBlock(), PCCEdgeRole::ROOT);
        addSwitchEdge(source->getNormalBlock(), PCCEdgeRole::NORMAL);
        addSwitchEdge(source->getDeviationBlock(), PCCEdgeRole::DEVIATION);
    }

    // -------------------------------------------------------------------------
    // Arêtes STRAIGHT entre sous-blocs adjacents (chaînes de subdivision).
    // Depuis Phase6_BlockExtractor v2, getNeighbourIds() est vide pour les
    // sous-blocs internes — on utilise getNeighbours() (pointeurs résolus).
    // Les connexions straight↔switch sont ignorées ici (créées depuis le switch).
    // -------------------------------------------------------------------------
    std::unordered_set<std::string> processedChainEdges;

    for (const auto& st : topo.straights)
    {
        PCCNode* fromNode = graph.findNode(st->getId());
        if (!fromNode) continue;

        const auto& neighbours = st->getNeighbours();

        auto addChainEdge = [&](ShuntingElement* neighbour)
            {
                if (!neighbour) return;

                PCCNode* toNode = graph.findNode(neighbour->getId());
                if (!toNode) return;

                // Connexion straight↔switch — déjà créée depuis le switch
                if (toNode->getNodeType() == PCCNodeType::SWITCH) return;

                const std::string key = makeEdgeKey(st->getId(), neighbour->getId());
                if (processedChainEdges.count(key)) return;
                processedChainEdges.insert(key);

                graph.addEdge(fromNode, toNode, PCCEdgeRole::STRAIGHT);
                ++edgeCount;
            };

        addChainEdge(neighbours.prev);
        addChainEdge(neighbours.next);
    }

    tagCrossovers(graph, topo, logger);

    LOG_DEBUG(graph.getLogger(), std::to_string(edgeCount) + " arêtes créées.");
}


// =============================================================================
// Passe 3 — Côté géographique des déviations (Famille G3 — UTM)
// =============================================================================

void PCCGraphBuilder::computeDeviationSides(PCCGraph& graph,
    const TopologyData& topo,
    Logger& logger)
{
    for (const auto& nodePtr : graph.getNodes())
    {
        if (nodePtr->getNodeType() != PCCNodeType::SWITCH) continue;

        auto* sw = static_cast<PCCSwitchNode*>(nodePtr.get());

        const SwitchBlock* source = sw->getSwitchSource();
        if (!source->isOriented()) continue;

        // Tip déviation UTM requis — absent si la branche est manquante
        const auto& tipUTM = source->getTipOnDeviationUTM();
        if (!tipUTM.has_value()) continue;

        // Comparaison sur l'axe Y UTM (y croissant = nord = side +1).
        // Sémantiquement identique à la comparaison de latitudes WGS84,
        // mais sans ambiguïté degré/radian et sans dépendance au WGS84.
        const CoordinateXY& junc = source->getJunctionUTM();
        const int side = (tipUTM->y > junc.y) ? 1 : -1;
        sw->setDeviationSide(side);

        LOG_DEBUG(logger, sw->getSourceId() + " deviationSide="
            + std::to_string(side)
            + " (Δy UTM=" + std::to_string(tipUTM->y - junc.y) + " m)");
    }
}


// =============================================================================
// Helper — clé canonique de paire
// =============================================================================

std::string PCCGraphBuilder::makeEdgeKey(const std::string& idA,
    const std::string& idB)
{
    return (idA < idB) ? (idA + "|" + idB) : (idB + "|" + idA);
}


// =============================================================================
// Tag crossovers
// =============================================================================

void PCCGraphBuilder::tagCrossovers(PCCGraph& graph,
    const TopologyData& topo,
    Logger& logger)
{
    // Pour chaque paire de switches, vérifie s'ils partagent exactement
    // les mêmes deux straights en NORMAL et DEVIATION.
    // Si oui → les deux straights sont des nœuds crossover.
    for (size_t i = 0; i < topo.switches.size(); ++i)
    {
        for (size_t j = i + 1; j < topo.switches.size(); ++j)
        {
            const auto& swA = topo.switches[i];
            const auto& swB = topo.switches[j];

            if (!swA->getNormalBranchId().has_value())    continue;
            if (!swA->getDeviationBranchId().has_value()) continue;
            if (!swB->getNormalBranchId().has_value())    continue;
            if (!swB->getDeviationBranchId().has_value()) continue;

            const std::string& aN = *swA->getNormalBranchId();
            const std::string& aD = *swA->getDeviationBranchId();
            const std::string& bN = *swB->getNormalBranchId();
            const std::string& bD = *swB->getDeviationBranchId();

            // Crossover : les deux switches partagent exactement les mêmes
            // deux straights (dans n'importe quel ordre)
            const bool sameSet = (aN == bN && aD == bD)
                || (aN == bD && aD == bN);
            if (!sameSet) continue;

            PCCNode* nodeN = graph.findNode(aN);
            PCCNode* nodeD = graph.findNode(aD);
            if (!nodeN || !nodeD) continue;
            if (nodeN->getNodeType() != PCCNodeType::STRAIGHT) continue;
            if (nodeD->getNodeType() != PCCNodeType::STRAIGHT) continue;

            // Marque les deux straights comme nœuds crossover
            nodeN->setCrossover(true);
            nodeD->setCrossover(true);

            LOG_DEBUG(logger, "Crossover détecté : "
                + swA->getId() + " ↔ " + swB->getId()
                + " via " + aN + " + " + aD);
        }
    }
}
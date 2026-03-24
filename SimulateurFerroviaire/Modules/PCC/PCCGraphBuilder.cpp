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

    // Passe 3 : calcul du côté géographique de chaque déviation
    computeDeviationSides(graph, topo, logger);

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

    // Ensemble des paires (from, to) déjà créées — évite les doublons crossover
    // Un crossover crée sw/0→s/1 ET sw/1→s/1 : sans dédoublonnage, s/1 reçoit
    // deux arêtes entrantes de deux switches différents, ce qui brise le BFS.
    std::unordered_set<std::string> processedPairs;

    auto addUniqueEdge = [&](PCCNode* from, PCCNode* to, PCCEdgeRole role) -> bool
        {
            if (!from || !to) return false;
            const std::string key = from->getSourceId() + ">" + to->getSourceId();
            if (processedPairs.count(key)) return false;
            processedPairs.insert(key);
            graph.addEdge(from, to, role);
            ++edgeCount;
            return true;
        };

    // -------------------------------------------------------------------------
    // Arêtes depuis les SwitchBlocks
    // -------------------------------------------------------------------------
    for (const auto& sw : topo.switches)
    {
        if (!sw->isOriented())
        {
            LOG_WARNING(graph.getLogger(), "Switch non orienté ignoré : " + sw->getId());
            continue;
        }

        PCCNode* swNode = graph.findNode(sw->getId());
        if (!swNode) continue;

        auto resolveEdge = [&](const std::optional<std::string>& branchId, PCCEdgeRole role)
            {
                if (!branchId.has_value()) return;
                PCCNode* target = graph.findNode(*branchId);
                if (!target)
                {
                    LOG_WARNING(graph.getLogger(), "Branche introuvable : "
                        + *branchId + " (depuis " + sw->getId() + ")");
                    return;
                }
                addUniqueEdge(swNode, target, role);
            };

        resolveEdge(sw->getRootBranchId(), PCCEdgeRole::ROOT);
        resolveEdge(sw->getNormalBranchId(), PCCEdgeRole::NORMAL);
        resolveEdge(sw->getDeviationBranchId(), PCCEdgeRole::DEVIATION);

        // ---------------------------------------------------------------------
        // Double switch — arête directe switch ↔ switch
        // Après absorption de s/5, sw/2.deviation == "sw/4" et sw/4.deviation == "sw/2".
        // Il n'existe plus de StraightBlock entre eux — l'arête doit être créée
        // directement entre les deux nœuds switch en utilisant les Coordinates absorbées.
        // ---------------------------------------------------------------------
        if (sw->isDouble())
        {
            // Côté normal
            if (sw->getDoubleOnNormal().has_value())
            {
                PCCNode* partner = graph.findNode(*sw->getNormalBranchId());
                if (partner)
                {
                    addUniqueEdge(swNode, partner, PCCEdgeRole::NORMAL);
                    LOG_DEBUG(graph.getLogger(), "Double switch normal : "
                        + sw->getId() + " → " + partner->getSourceId());
                }
            }

            // Côté deviation
            if (sw->getDoubleOnDeviation().has_value())
            {
                PCCNode* partner = graph.findNode(*sw->getDeviationBranchId());
                if (partner)
                {
                    addUniqueEdge(swNode, partner, PCCEdgeRole::DEVIATION);
                    LOG_DEBUG(graph.getLogger(), "Double switch deviation : "
                        + sw->getId() + " → " + partner->getSourceId());
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Arêtes STRAIGHT entre StraightBlocks adjacents
    // -------------------------------------------------------------------------
    std::unordered_set<std::string> processedEdges;

    for (const auto& st : topo.straights)
    {
        PCCNode* fromNode = graph.findNode(st->getId());
        if (!fromNode) continue;

        for (const auto& neighbourId : st->getNeighbourIds())
        {
            PCCNode* toNode = graph.findNode(neighbourId);
            if (!toNode) continue;

            // Les connexions straight↔switch sont déjà couvertes côté switch
            if (toNode->getNodeType() == PCCNodeType::SWITCH)
                continue;

            const std::string key = makeEdgeKey(st->getId(), neighbourId);
            if (processedEdges.count(key)) continue;
            processedEdges.insert(key);

            graph.addEdge(fromNode, toNode, PCCEdgeRole::STRAIGHT);
            ++edgeCount;
        }
    }

    tagCrossovers(graph, topo, logger);

    LOG_DEBUG(graph.getLogger(), std::to_string(edgeCount) + " arêtes créées.");
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

// Nouvelle méthode statique
void PCCGraphBuilder::tagCrossovers(PCCGraph& graph,
    const TopologyData& topo,
    Logger& logger)
{
    // Pour chaque paire de switches, vérifier s'ils partagent
    // exactement les mêmes deux straights en NORMAL et DEVIATION.
    // Si oui → les deux straights sont des nœuds crossover.
    for (size_t i = 0; i < topo.switches.size(); ++i)
    {
        for (size_t j = i + 1; j < topo.switches.size(); ++j)
        {
            const auto& swA = topo.switches[i];
            const auto& swB = topo.switches[j];

            if (!swA->getNormalBranchId().has_value())  continue;
            if (!swA->getDeviationBranchId().has_value()) continue;
            if (!swB->getNormalBranchId().has_value())  continue;
            if (!swB->getDeviationBranchId().has_value()) continue;

            const std::string& aN = *swA->getNormalBranchId();
            const std::string& aD = *swA->getDeviationBranchId();
            const std::string& bN = *swB->getNormalBranchId();
            const std::string& bD = *swB->getDeviationBranchId();

            // Crossover : les deux switches partagent exactement
            // les mêmes deux straights (dans n'importe quel ordre)
            const bool sameSet = (aN == bN && aD == bD)
                || (aN == bD && aD == bN);
            if (!sameSet) continue;

            // Vérifier que ce sont bien des straights (pas des switches)
            PCCNode* nodeN = graph.findNode(aN);
            PCCNode* nodeD = graph.findNode(aD);
            if (!nodeN || !nodeD) continue;
            if (nodeN->getNodeType() != PCCNodeType::STRAIGHT) continue;
            if (nodeD->getNodeType() != PCCNodeType::STRAIGHT) continue;

            nodeN->setCrossover(true);
            nodeD->setCrossover(true);
            LOG_DEBUG(logger, "Crossover détecté : "
                + swA->getId() + "↔" + swB->getId()
                + " via " + aN + " + " + aD);
        }
    }
}


// =============================================================================
// Passe 3 — Côté géographique des déviations
// =============================================================================

void PCCGraphBuilder::computeDeviationSides(PCCGraph& graph,
    const TopologyData& topo,
    Logger& logger)
{
    for (const auto& nodePtr : graph.getNodes())
    {
        auto* sw = dynamic_cast<PCCSwitchNode*>(nodePtr.get());
        if (!sw) continue;

        const SwitchBlock* source = sw->getSwitchSource();
        if (!source) continue;

        // Tip CDC de la branche déviation — calculé par SwitchOrientator (Phase 6).
        // Absent si le switch n'est pas orienté.
        const auto& tipOpt = source->getTipOnDeviation();
        if (!tipOpt.has_value()) continue;

        // Comparaison latitude : tip déviation vs jonction du switch
        const double swLat = source->getJunctionCoordinate().latitude;
        const double devLat = tipOpt->latitude;

        // Déviation au nord → +1 (y positif, vers le haut sur l'écran)
        // Déviation au sud  → -1 (y négatif, vers le bas)
        const int side = (devLat > swLat) ? 1 : -1;
        sw->setDeviationSide(side);

        LOG_DEBUG(logger, sw->getSourceId() + " deviationSide="
            + std::to_string(side)
            + " (Δlat=" + std::to_string(devLat - swLat) + ")");
    }
}
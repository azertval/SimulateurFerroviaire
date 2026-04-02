/**
 * @file  PCCGraphBuilder.cpp
 * @brief Implémentation du constructeur du graphe PCC.
 *
 * @par Correction v2 — chaînes de sous-blocs non câblées
 * La refonte du parser (Phase6_BlockExtractor v2) a remplacé le système
 * d'identifiants (@c m_neighbourIds / @c addNeighbourId) par des pointeurs
 * directs (@c m_neighbours / @c setNeighbourPrev / @c setNeighbourNext).
 *
 * L'ancienne implémentation de @c buildEdges utilisait
 * @c StraightBlock::getNeighbourIds() pour créer les arêtes STRAIGHT entre
 * blocs adjacents. Cette liste est désormais vide pour tous les sous-blocs
 * produits par subdivision (@c maxSegmentLength), ce qui rompait les chaînes :
 * @code
 *   s/0_0  [sw/0]───[s/0_0]   [s/0_1]   [s/0_2]  …  [s/0_11]───[sw/4]
 *                     ←── chaîne absente du graphe ───→
 * @endcode
 *
 * Correction : @c buildEdges utilise désormais @c StraightBlock::getNeighbours()
 * (les pointeurs résolus par @ref Phase8_RepositoryTransfer) pour créer les
 * arêtes STRAIGHT. Cette approche couvre les deux cas :
 *  - Connexions internes de la chaîne (sous-blocs de subdivision).
 *  - Connexions externe switch↔straight (ignorées ici — créées depuis le switch).
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
    //   L'index est complet ici — buildEdges peut résoudre tous les IDs.
    buildNodes(graph, topo, logger);

    // Passe 2 : toutes les arêtes sont créées et câblées.
    buildEdges(graph, topo, logger);

    // Passe 3 : calcul du côté géographique de chaque déviation.
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
    // Parcours des straights — const auto& évite la copie du unique_ptr
    for (const auto& st : topo.straights)
        graph.addStraightNode(st.get());

    // Parcours des switches
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
    //
    // Un crossover produit deux arêtes depuis deux switches différents vers le
    // même straight (ex. sw/0→s/1 ET sw/1→s/1). Sans dédoublonnage, s/1
    // recevrait deux arêtes entrantes et son BFS serait brisé.
    // -------------------------------------------------------------------------
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
    // Arêtes depuis les SwitchBlocks (ROOT / NORMAL / DEVIATION)
    // -------------------------------------------------------------------------
    for (const auto& sw : topo.switches)
    {
        if (!sw->isOriented())
        {
            LOG_WARNING(graph.getLogger(),
                "Switch non orienté ignoré : " + sw->getId());
            continue;
        }

        PCCNode* swNode = graph.findNode(sw->getId());
        if (!swNode) continue;

        auto resolveEdge = [&](const std::optional<std::string>& branchId,
            PCCEdgeRole role)
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
        // Double switch — arête directe switch ↔ switch.
        // Après absorption du segment de liaison, la déviation pointe vers
        // l'autre switch (ex. sw/2.deviation == "sw/4").
        // Il n'existe plus de StraightBlock entre eux — l'arête est créée
        // directement entre les deux nœuds switch.
        // ---------------------------------------------------------------------
        if (sw->isDouble())
        {
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
    // Arêtes STRAIGHT entre StraightBlocks adjacents.
    //
    // IMPORTANT — utilise getNeighbours() (pointeurs résolus par
    // Phase8_RepositoryTransfer) et NON getNeighbourIds() (IDs textuels).
    //
    // Depuis Phase6_BlockExtractor v2, les connexions entre sous-blocs de
    // subdivision (s/0_0 → s/0_1 → … → s/0_11) ne sont enregistrées que
    // dans m_neighbours — m_neighbourIds reste vide pour ces blocs internes.
    // Utiliser getNeighbourIds() laisserait tous les sous-blocs isolés dans
    // le graphe PCC.
    //
    // Les connexions straight↔switch (prev ou next == SwitchBlock*) sont
    // ignorées ici — elles sont déjà couvertes par les arêtes ROOT/NORMAL/
    // DEVIATION créées depuis le switch dans la boucle ci-dessus.
    // -------------------------------------------------------------------------
    std::unordered_set<std::string> processedChainEdges;

    for (const auto& st : topo.straights)
    {
        PCCNode* fromNode = graph.findNode(st->getId());
        if (!fromNode) continue;

        const auto& neighbours = st->getNeighbours();

        // Lambda local — évite la duplication du code pour prev et next
        auto addChainEdge = [&](ShuntingElement* neighbour)
            {
                if (!neighbour) return;

                PCCNode* toNode = graph.findNode(neighbour->getId());
                if (!toNode) return;

                // Connexion straight↔switch — déjà créée depuis le switch
                if (toNode->getNodeType() == PCCNodeType::SWITCH) return;

                // Clé canonique — évite les doublons A→B / B→A
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
// Helper — clé canonique de paire
// =============================================================================

std::string PCCGraphBuilder::makeEdgeKey(const std::string& idA,
    const std::string& idB)
{
    // Clé indépendante de l'ordre — "s/0|sw/3" == makeEdgeKey("sw/3", "s/0")
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

            // Vérifie que ce sont bien des straights (pas des switches)
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

        // Tip CDC de la branche déviation — calculé par SwitchOrientator.
        // Absent si le switch n'est pas orienté.
        const auto& tipOpt = source->getTipOnDeviation();
        if (!tipOpt.has_value()) continue;

        // Comparaison latitude : tip déviation vs jonction du switch.
        // Déviation au nord → +1 (y positif, vers le haut sur l'écran).
        // Déviation au sud  → -1 (y négatif, vers le bas).
        const double swLat = source->getJunctionWGS84().latitude;
        const double devLat = tipOpt->latitude;

        const int side = (devLat > swLat) ? 1 : -1;
        sw->setDeviationSide(side);

        LOG_DEBUG(logger, sw->getSourceId() + " deviationSide="
            + std::to_string(side)
            + " (Δlat=" + std::to_string(devLat - swLat) + ")");
    }
}
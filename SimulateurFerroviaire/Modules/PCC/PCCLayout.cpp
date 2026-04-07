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

    // Post-traitement : correction des branches convergentes de longueurs inégales.
    // Doit être appelé après le BFS complet — positions finales requises.
    fixCollapsedBranches(graph, logger);

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
        //  CROSSING
        //      → (x+1, y)            continuité de ligne, voie traversante uniquement.
        //
        //  Le côté (±1) provient exclusivement de
        //  PCCSwitchNode::getDeviationSide(), calculé une seule fois par
        //  PCCGraphBuilder::computeDeviationSides à partir des lat/lon.
        //  C'est la seule donnée GPS tolérée dans ce calcul.
        // ---------------------------------------------------------------

        // swNode : switch courant pour les calculs de côté et d'arrivée par déviation.
        // Déclaré UNE SEULE FOIS ici — ne pas re-déclarer dans les branches ci-dessous.
        auto* swNode = dynamic_cast<PCCSwitchNode*>(node);
        const int side = swNode ? swNode->getDeviationSide() : 1;

        for (const NeighbourInfo& neighbour : neighbours)
        {
            int  nextX = x + 1;
            int  nextY = y;
            bool nextArrivedViaDev = false;
            PCCEdge* edgeToNeighbour = nullptr;

            // Trouver l'arête vers ce voisin (pour getExitEdgeFor sur crossing)
            for (PCCEdge* edge : node->getEdges())
            {
                if ((edge->getFrom() == node && edge->getTo() == neighbour.node)
                    || (edge->getTo() == node && edge->getFrom() == neighbour.node))
                {
                    edgeToNeighbour = edge;
                    break;
                }
            }

            // ---------------------------------------------------------------
            // Cas CROSSING — nœud courant est un crossing :
            // propager uniquement sur la voie traversante (A↔C ou B↔D).
            // ---------------------------------------------------------------
            if (node->getNodeType() == PCCNodeType::CROSSING)
            {
                const auto* cr = static_cast<const PCCCrossingNode*>(node);
                PCCEdge* exitEdge = cr->getExitEdgeFor(arrivedViaEdge);

                // N'enqueuer que le voisin sur la bonne voie traversante
                if (!exitEdge || exitEdge->getTo() != neighbour.node)
                    continue;

                // Continuité de la voie — même Y, avancement +1
                nextX = x + 1;
                nextY = y;
                nextArrivedViaDev = arrivedViaDeviation;
            }
            // ---------------------------------------------------------------
            // Cas CROSSING comme rôle d'arête (voisin est un CrossBlock) :
            // se comporte comme un STRAIGHT — même Y, avancement +1.
            // ---------------------------------------------------------------
            else if (neighbour.role == PCCEdgeRole::CROSSING)
            {
                nextX = x + 1;
                nextY = y;
                nextArrivedViaDev = arrivedViaDeviation;
            }
            // ---------------------------------------------------------------
            // Cas DEVIATION
            // ---------------------------------------------------------------
            else if (neighbour.role == PCCEdgeRole::DEVIATION)
            {
                // Utilise dynamic_cast sur le VOISIN uniquement —
                // swNode (nœud courant) est déjà calculé avant la boucle.
                auto* swNeigh = dynamic_cast<PCCSwitchNode*>(neighbour.node);

                if (swNeigh && swNode)  // double switch : switch → switch direct
                {
                    nextX = x;
                    nextY = y + side;
                    nextArrivedViaDev = true;
                }
                else  // branche déviée classique (switch → straight ou straight → switch)
                {
                    nextX = x + 1;
                    nextY = y + side;
                    nextArrivedViaDev = false;
                }
            }
            // ---------------------------------------------------------------
            // Cas arrivée par déviation — NORMAL est en AMONT
            // ---------------------------------------------------------------
            else if (arrivedViaDeviation
                && swNode
                && neighbour.role == PCCEdgeRole::NORMAL
                && neighbour.isForward)
            {
                // Quand le BFS atteint sw/B via la déviation de sw/A :
                //   • ROOT de sw/B pointe vers l'aval  → x+1, y
                //   • NORMAL de sw/B pointe vers l'amont → x-1, y
                nextX = x - 1;
                nextY = y;
                nextArrivedViaDev = false;
            }
            // else : STRAIGHT / ROOT / NORMAL standard → (x+1, y) — déjà initialisé

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
        + " nœuds positionnés depuis " + start->getSourceId()
        + " (offsetX=" + std::to_string(offsetX) + ").");

    return maxX;
}


// =============================================================================
// Post-traitement — correction des branches convergentes de longueurs inégales
// =============================================================================

void PCCLayout::fixCollapsedBranches(PCCGraph& graph, Logger& logger)
{
    // Un switch sw/B a une branche "de longueur nulle" quand sa cible via
    // DEVIATION (ou NORMAL) se trouve au même X que lui-même mais à un Y
    // différent : devBorderX == center.x → aucune extension horizontale.
    //
    // Cause : les deux chemins (normal et dévié) depuis sw/A vers sw/B ont
    // des longueurs différentes, et le BFS les place au même X final.
    //
    // Solution : décaler sw/B + tout son sous-graphe aval (via ROOT)
    // de +1 jusqu'à ce qu'aucun switch ne soit dans cet état.
    // On itère en cas de corrections en chaîne (plusieurs diamonds).

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

            // Détection : une branche (DEVIATION ou NORMAL) atterrit au même X
            // que sw mais à un Y différent → longueur nulle dans le renderer.
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

            // ---------------------------------------------------------------
            // Collecte du sous-graphe aval : sw + tout ce qui est accessible
            // via son arête ROOT en avançant vers les X croissants.
            // ---------------------------------------------------------------
            std::unordered_set<PCCNode*> toShift;
            toShift.insert(sw);

            // ---------------------------------------------------------------
            // Phase 1 : backbone aval (x > currX) + partenaires double switch
            // ---------------------------------------------------------------
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
                    const bool currIsSwitch =
                        (curr->getNodeType() == PCCNodeType::SWITCH);

                    for (PCCEdge* edge : curr->getEdges())
                    {
                        PCCNode* nb = (edge->getFrom() == curr)
                            ? edge->getTo() : edge->getFrom();
                        if (!nb || toShift.count(nb)) continue;

                        const int  nbX = nb->getPosition().x;
                        const bool nbIsSwitch =
                            (nb->getNodeType() == PCCNodeType::SWITCH);

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

            // ---------------------------------------------------------------
            // Phase 2 : voies déviées des switches aval (x > swX)
            // ---------------------------------------------------------------
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
                + " nœuds aval décalés +1"
                + " (swX: " + std::to_string(swX)
                + " → " + std::to_string(swX + 1) + ")");

            changed = true;
            break; // Redémarre le scan — une correction peut en révéler d'autres
        }
    }

    if (pass > 1)
        LOG_DEBUG(logger,
            "fixCollapsedBranches terminé — "
            + std::to_string(pass - 1) + " passe(s).");
}
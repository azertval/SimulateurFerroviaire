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
#include <array>
#include <set>
#include <map>


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

    // BFS de secours pour les nœuds non couverts (composantes déconnectées
    // sans terminus détectable).
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

    // Post-traitements dans l'ordre (voir @par Post-traitements dans PCCLayout.h) :
    //   1. fixCollapsedBranches — diamonds (doit précéder le spacing crossing)
    //   2. fixCrossingSpacing   — réserve crX±1 (doit précéder le layout crossing)
    //   3. fixCrossingLayout    — positionne les bras selon le type
    //   4. resolveCollisions    — cas résiduels non couverts par les étapes 1-3
    fixCollapsedBranches(graph, logger);
    fixCrossingSpacing(graph, logger);
    fixCrossingLayout(graph, logger);
    resolveCollisions(graph, logger);

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

        auto* swNode = dynamic_cast<PCCSwitchNode*>(node);
        const int side = swNode ? swNode->getDeviationSide() : 1;

        for (const NeighbourInfo& neighbour : neighbours)
        {
            int  nextX = x + 1;
            int  nextY = y;
            bool nextArrivedViaDev = false;
            PCCEdge* edgeToNeighbour = nullptr;

            // Recherche de l'arête vers ce voisin.
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
            // Nœud courant = CROSSING : propagation uniquement sur la voie
            // traversante (A↔C ou B↔D). Les autres arêtes sont ignorées.
            //
            // Note (TJD) : pas besoin de propagation diagonale ici — les
            // 4 corners d'un TJD sont déjà reliés deux à deux par des
            // arêtes NORMAL/DEVIATION directes (m_doubleOnNormal/Deviation
            // post-absorption Phase 7), donc tous atteints naturellement
            // par le BFS via les switches partenaires.
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
            // Arête CROSSING entrante (bras adjacent au crossing) :
            // traité comme STRAIGHT pendant le BFS. fixCrossingLayout
            // repositionnera le bras après.
            // -----------------------------------------------------------
            else if (neighbour.role == PCCEdgeRole::CROSSING)
            {
                nextX = x + 1;
                nextY = y;
                nextArrivedViaDev = arrivedViaDeviation;
            }
            // -----------------------------------------------------------
            // Arête DEVIATION
            // -----------------------------------------------------------
            else if (neighbour.role == PCCEdgeRole::DEVIATION)
            {
                auto* swNeigh = dynamic_cast<PCCSwitchNode*>(neighbour.node);

                if (swNeigh && swNode)  // double switch : même colonne X
                {
                    nextX = x;
                    nextY = y + side;
                    nextArrivedViaDev = true;
                }
                else if (swNeigh && !swNode)
                {
                    // Cas particulier : on arrive SUR un switch (swNeigh) depuis
                    // un straight (swNode == null), via l'arête DEVIATION du
                    // switch. Le straight est donc topologiquement la BRANCHE
                    // déviée du switch — on est physiquement à la tip de la
                    // déviation.
                    //
                    // Règle générale : la position logique d'un switch est
                    // toujours sur l'axe de sa branche NORMALE (même Y que
                    // normal et root). La déviation est décalée de ±side en Y
                    // par rapport à cet axe.
                    //
                    // Venant de la tip de la déviation (à Y_dev = y), pour
                    // atteindre la jonction du switch sur l'axe normal, on
                    // annule le décalage de la déviation :
                    //   Y_jonction = Y_dev - side_target
                    nextX = x + 1;
                    nextY = y - swNeigh->getDeviationSide();
                    nextArrivedViaDev = false;
                }
                else  // branche déviée classique (depuis un switch vers un straight)
                {
                    nextX = x + 1;
                    nextY = y + side;
                    nextArrivedViaDev = false;
                }
            }
            // -----------------------------------------------------------
            // Arête NORMAL en amont (arrivée par déviation, double switch)
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
            // else : STRAIGHT / ROOT / NORMAL standard → (x+1, y) déjà initialisé.

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
// Post-traitement — correction des branches convergentes de longueurs inégales
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

            // Skip les switches qui sont bras d'un crossing (Flat ou TJD).
            // Leur position finale est gérée par fixCrossingLayout — détecter
            // un "collapsed" ici provoque des shifts qui entrent en conflit
            // avec le crossing (ex. corner TJD qui finirait sur la position
            // du crossing lui-même).
            bool isCrossingArm = false;
            for (const PCCEdge* e : sw->getEdges())
            {
                if (e->getRole() == PCCEdgeRole::CROSSING)
                {
                    isCrossingArm = true;
                    break;
                }
            }
            if (isCrossingArm) continue;

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

            // Phase 1 : backbone aval (x > swX) + partenaires double switch.
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

            // Phase 2 : voies déviées des switches aval (x > swX).
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
                + " noeuds aval décalés +1"
                + " (swX: " + std::to_string(swX)
                + " -> " + std::to_string(swX + 1) + ")");

            changed = true;
            break;
        }
    }

    if (pass > 1)
        LOG_DEBUG(logger,
            "fixCollapsedBranches terminé — "
            + std::to_string(pass - 1) + " passe(s).");
}


// =============================================================================
// Post-traitement — fixCrossingSpacing
// =============================================================================

void PCCLayout::fixCrossingSpacing(PCCGraph& graph, Logger& logger)
{
    bool changed = true;
    int  pass = 0;
    const int maxPass = graph.nodeCount() * 2;  // cap anti-boucle infinie

    while (changed && pass < maxPass)
    {
        changed = false;
        ++pass;

        // Ensemble global de tous les bras légitimes de tous les crossings.
        // Un bras légitime ne doit pas être chassé par un autre crossing
        // même s'il occupe sa colonne réservée.
        std::unordered_set<const PCCNode*> allCrossingArms;
        for (const auto& nodePtr : graph.getNodes())
        {
            if (nodePtr->getNodeType() != PCCNodeType::CROSSING) continue;
            const auto* cr = static_cast<const PCCCrossingNode*>(nodePtr.get());
            for (const PCCEdge* e : { cr->getEdgeA(), cr->getEdgeB(),
                                      cr->getEdgeC(), cr->getEdgeD() })
            {
                if (e && e->getTo())
                    allCrossingArms.insert(e->getTo());
            }
        }

        for (const auto& nodePtr : graph.getNodes())
        {
            if (nodePtr->getNodeType() != PCCNodeType::CROSSING) continue;

            auto* cr = static_cast<PCCCrossingNode*>(nodePtr.get());
            const int crX = cr->getPosition().x;

            // Skip TJD : les corners d'un TJD sont déjà placés à leurs Y BFS
            // naturels via les arêtes NORMAL/DEVIATION inter-corners (post-
            // absorption Phase 7). fixTJDCrossingLayout les forcera ensuite
            // à crX±1. Activer le spacing ici déclenche des boucles
            // mutuelles entre TJD et crossings adjacents.
            const CrossBlock* source = cr->getCrossingSource();
            if (source && source->isTJD())
                continue;

            // -----------------------------------------------------------
            // Recherche d'un intrus dans crX±1.
            // Un intrus est un nœud non-crossing, non-bras-légitime,
            // qui occupe une colonne réservée du crossing courant.
            // -----------------------------------------------------------
            for (const auto& otherPtr : graph.getNodes())
            {
                PCCNode* other = otherPtr.get();
                const int otherX = other->getPosition().x;

                if (allCrossingArms.count(other))                   continue;
                if (other->getNodeType() == PCCNodeType::CROSSING)  continue;
                if (otherX != crX - 1 && otherX != crX + 1)        continue;

                // Direction du décalage : s'éloigner du crossing.
                const int shiftDir = (otherX < crX) ? -1 : +1;

                // -----------------------------------------------------------
                // BFS : collecte tout le sous-graphe non-crossing du côté
                // concerné. Arrêt aux bras légitimes d'un crossing adjacent.
                // -----------------------------------------------------------
                std::unordered_set<PCCNode*> toShift;
                std::queue<PCCNode*>         q;
                toShift.insert(other);
                q.push(other);

                while (!q.empty())
                {
                    PCCNode* curr = q.front();
                    q.pop();

                    for (PCCEdge* ae : curr->getEdges())
                    {
                        PCCNode* nb = (ae->getFrom() == curr)
                            ? ae->getTo() : ae->getFrom();
                        if (!nb || toShift.count(nb))               continue;
                        if (nb->getNodeType() == PCCNodeType::CROSSING) continue;
                        if (allCrossingArms.count(nb))              continue;

                        const int nbX = nb->getPosition().x;
                        const bool inShiftZone = (shiftDir > 0)
                            ? (nbX >= otherX)
                            : (nbX <= otherX);

                        if (inShiftZone)
                        {
                            toShift.insert(nb);
                            q.push(nb);
                        }
                    }
                }

                for (PCCNode* n : toShift)
                {
                    auto pos = n->getPosition();
                    pos.x += shiftDir;
                    n->setPosition(pos);
                }

                LOG_DEBUG(logger,
                    "fixCrossingSpacing pass=" + std::to_string(pass)
                    + " : intrus " + other->getSourceId()
                    + " en X=" + std::to_string(otherX)
                    + " (col réservée crossing " + cr->getSourceId() + ")"
                    + " → décalage " + std::to_string(shiftDir)
                    + " sur " + std::to_string(toShift.size()) + " nœud(s)");

                changed = true;
                break;
            }

            if (changed) break;
        }
    }

    if (pass >= maxPass)
        LOG_WARNING(logger,
            "fixCrossingSpacing — cap de " + std::to_string(maxPass)
            + " passes atteint, possible conflit entre crossings adjacents.");

    if (pass > 1)
        LOG_DEBUG(logger,
            "fixCrossingSpacing terminé — "
            + std::to_string(pass - 1) + " passe(s).");
}


// =============================================================================
// Post-traitement — fixCrossingLayout
// =============================================================================

void PCCLayout::fixCrossingLayout(PCCGraph& graph, Logger& logger)
{
    for (const auto& nodePtr : graph.getNodes())
    {
        if (nodePtr->getNodeType() != PCCNodeType::CROSSING) continue;

        auto* cr = static_cast<PCCCrossingNode*>(nodePtr.get());

        // Validation : les 4 slots doivent être valides avant toute modification.
        const bool allValid =
            cr->getEdgeA() && cr->getEdgeA()->getTo() &&
            cr->getEdgeB() && cr->getEdgeB()->getTo() &&
            cr->getEdgeC() && cr->getEdgeC()->getTo() &&
            cr->getEdgeD() && cr->getEdgeD()->getTo();

        if (!allValid)
        {
            LOG_WARNING(logger, cr->getSourceId()
                + " — fixCrossingLayout : slot(s) null, ignoré.");
            continue;
        }

        const CrossBlock* source = cr->getCrossingSource();

        if (source && source->isTJD())
            fixTJDCrossingLayout(cr, logger);
        else
            fixFlatCrossingLayout(cr, logger);
    }
}


// =============================================================================
// Crossing plat — repositionnement des bras (StraightCrossBlock)
// =============================================================================

void PCCLayout::fixFlatCrossingLayout(PCCCrossingNode* cr, Logger& logger)
{
    const int crX = cr->getPosition().x;
    const int crY = cr->getPosition().y;

    const PCCEdge* slots[4] = {
        cr->getEdgeA(), cr->getEdgeB(),
        cr->getEdgeC(), cr->getEdgeD()
    };

    // -------------------------------------------------------------------------
    // Étape 1 : placement de chaque bras à crX-1 ou crX+1 selon son voisin
    // non-crossing. Le Y BFS du bras est conservé.
    // -------------------------------------------------------------------------
    for (const PCCEdge* e : slots)
    {
        if (!e || !e->getTo()) continue;
        PCCNode* arm = e->getTo();

        // Trouver le voisin non-crossing du bras (côté "extérieur").
        PCCNode* neighbour = nullptr;
        for (const PCCEdge* ae : arm->getEdges())
        {
            PCCNode* nb = (ae->getFrom() == arm)
                ? ae->getTo() : ae->getFrom();
            if (nb && nb->getNodeType() != PCCNodeType::CROSSING)
            {
                neighbour = nb;
                break;
            }
        }

        if (!neighbour) continue;

        const int nbX = neighbour->getPosition().x;
        const int bfsY = arm->getPosition().y;
        const int armX = (nbX < crX) ? crX - 1 : crX + 1;

        arm->setPosition({ armX, bfsY });
    }

    // -------------------------------------------------------------------------
    // Étape 2 : miroir Y si les deux bras déviés (Y ≠ crY) se trouvent du
    // même côté vertical après le placement. Le bras gauche conserve son Y
    // BFS (référence géographique) ; le bras droit est mis en miroir par
    // rapport à crY pour former un X correct.
    // -------------------------------------------------------------------------
    PCCNode* devRight = nullptr;  // bras dévié à crX+1
    PCCNode* devLeft = nullptr;  // bras dévié à crX-1

    for (const PCCEdge* e : slots)
    {
        if (!e || !e->getTo()) continue;
        PCCNode* arm = e->getTo();
        if (arm->getPosition().y == crY) continue;  // bras normal → skip

        if (arm->getPosition().x > crX) devRight = arm;
        else                            devLeft = arm;
    }

    if (devRight && devLeft)
    {
        const int rightY = devRight->getPosition().y;
        const int leftY = devLeft->getPosition().y;

        const bool rightAbove = (rightY > crY);
        const bool leftAbove = (leftY > crY);

        if (rightAbove == leftAbove)
        {
            // Même côté → miroir de devRight pour former le X.
            const int mirrorY = 2 * crY - leftY;
            devRight->setPosition({ devRight->getPosition().x, mirrorY });

            LOG_DEBUG(logger, cr->getSourceId()
                + " fixFlatCrossing mirrorY : "
                + devLeft->getSourceId() + " Y=" + std::to_string(leftY)
                + " → " + devRight->getSourceId() + " Y=" + std::to_string(mirrorY));

            // Seul le bras devRight (slot direct du crossing) est repositionné.
            // Les straights intermédiaires conservent leur Y BFS naturel —
            // drawStraightBlock (isCrossingArm=true) s'étend vers ses deux
            // voisins quelle que soit la différence de Y.
        }
        else
        {
            LOG_DEBUG(logger, cr->getSourceId()
                + " fixFlatCrossing no mirror needed : "
                + devRight->getSourceId() + " Y=" + std::to_string(rightY)
                + " / " + devLeft->getSourceId() + " Y=" + std::to_string(leftY));
        }
    }

    // Log des positions finales.
    auto posStr = [](const PCCEdge* e) -> std::string {
        if (!e || !e->getTo()) return "null";
        const PCCNode* n = e->getTo();
        return n->getSourceId()
            + "[" + std::to_string(n->getPosition().x)
            + "," + std::to_string(n->getPosition().y) + "]";
        };

    LOG_DEBUG(logger, cr->getSourceId()
        + " fixFlatCrossing cr=[" + std::to_string(crX) + "," + std::to_string(crY) + "]"
        + " A=" + posStr(cr->getEdgeA())
        + " B=" + posStr(cr->getEdgeB())
        + " C=" + posStr(cr->getEdgeC())
        + " D=" + posStr(cr->getEdgeD()));
}


// =============================================================================
// Crossing TJD — placement compact (SwitchCrossBlock)
//
// Spec : à la différence du Flat (qui occupe 3 cellules crX-1/crX/crX+1 avec
// les 4 bras à crX±1), le TJD est COMPACT : les 4 corners et le crossing
// sont **dans la même cellule X=crX**, distinguées par leur Y de voie.
//
//   X=crX-1     X=crX      X=crX+1
//                 │
//   ───── sw/16(A) ─── sw/7(C) ─────         Y=Y_AC (voie 1)
//                 ✕                    ← cr/1 logique dans la cellule
//   ───── sw/15(B) ─── sw/8(D) ─────         Y=Y_BD (voie 2)
//                 │
//
// Note : sw/16 et sw/7 partagent la même cellule (X=crX, Y=Y_AC), comme
// dans un double-switch standard. Le TJD est topologiquement deux paires
// de doubles switches superposées et croisées.
// =============================================================================

void PCCLayout::fixTJDCrossingLayout(PCCCrossingNode* cr, Logger& logger)
{
    const int crX = cr->getPosition().x;
    const int crY = cr->getPosition().y;

    PCCNode* nodeA = cr->getEdgeA() ? cr->getEdgeA()->getTo() : nullptr;
    PCCNode* nodeB = cr->getEdgeB() ? cr->getEdgeB()->getTo() : nullptr;
    PCCNode* nodeC = cr->getEdgeC() ? cr->getEdgeC()->getTo() : nullptr;
    PCCNode* nodeD = cr->getEdgeD() ? cr->getEdgeD()->getTo() : nullptr;

    if (!nodeA || !nodeB || !nodeC || !nodeD)
    {
        LOG_WARNING(logger, cr->getSourceId()
            + " — fixTJDCrossing : un slot A/B/C/D est nullptr, ignoré.");
        return;
    }

    // -------------------------------------------------------------------------
    // Détermination des Y des deux voies. On prend les Y BFS des corners
    // gauches (X minimum) qui ont été placés en premier par le BFS.
    // -------------------------------------------------------------------------
    auto pickRefY = [&](PCCNode* a, PCCNode* b) -> int
        {
            const int xa = a->getPosition().x;
            const int xb = b->getPosition().x;
            if (xa < xb) return a->getPosition().y;
            if (xb < xa) return b->getPosition().y;
            return a->getPosition().y;
        };

    const int yAC = pickRefY(nodeA, nodeC);  // voie 1
    int       yBD = pickRefY(nodeB, nodeD);  // voie 2

    if (yBD == yAC)
    {
        yBD = yAC + 1;
        LOG_DEBUG(logger, cr->getSourceId()
            + " fixTJDCrossing : Y_AC == Y_BD, voie 2 décalée à Y="
            + std::to_string(yBD));
    }

    // -------------------------------------------------------------------------
    // Place les 4 corners dans la cellule X=crX.
    // Note : on ne touche PAS aux straights externes. Le BFS les a déjà
    // posés à leur Y de voie naturel. Les "collisions" entre corners et
    // crossing dans la même cellule sont gérées par resolveCollisions
    // (qui sait qu'un TJD partage sa cellule avec ses 4 corners par design).
    // -------------------------------------------------------------------------
    nodeA->setPosition({ crX, yAC });
    nodeC->setPosition({ crX, yAC });
    nodeB->setPosition({ crX, yBD });
    nodeD->setPosition({ crX, yBD });

    auto posStr = [](const PCCEdge* e) -> std::string {
        if (!e || !e->getTo()) return "null";
        const PCCNode* n = e->getTo();
        return n->getSourceId()
            + "[" + std::to_string(n->getPosition().x)
            + "," + std::to_string(n->getPosition().y) + "]";
        };

    LOG_DEBUG(logger, cr->getSourceId()
        + " fixTJDCrossing cr=[" + std::to_string(crX) + "," + std::to_string(crY) + "]"
        + " yAC=" + std::to_string(yAC)
        + " yBD=" + std::to_string(yBD)
        + " A=" + posStr(cr->getEdgeA())
        + " B=" + posStr(cr->getEdgeB())
        + " C=" + posStr(cr->getEdgeC())
        + " D=" + posStr(cr->getEdgeD()));
}


// =============================================================================
// Post-traitement — résolution des collisions résiduelles
// =============================================================================

void PCCLayout::resolveCollisions(PCCGraph& graph, Logger& logger)
{
    // Nœuds figés : CrossingNode eux-mêmes + bras de tous les crossings.
    // Leurs positions sont fixées par fixCrossingLayout() et ne doivent
    // pas être perturbées.
    std::unordered_set<const PCCNode*> frozen;

    // Ensemble des "cellules TJD" : pour chaque TJD, la cellule (X=crX) est
    // partagée par le crossing et ses 4 corners par design — pas une vraie
    // collision. On enregistre l'identifiant du TJD (raw pointer) comme
    // « famille » pour chaque corner et le crossing lui-même.
    std::unordered_map<const PCCNode*, const PCCNode*> tjdFamily;

    for (const auto& nodePtr : graph.getNodes())
    {
        if (nodePtr->getNodeType() != PCCNodeType::CROSSING) continue;
        const auto* cr = static_cast<const PCCCrossingNode*>(nodePtr.get());
        frozen.insert(nodePtr.get());

        const CrossBlock* source = cr->getCrossingSource();
        const bool isTJD = (source && source->isTJD());

        for (const PCCEdge* e : { cr->getEdgeA(), cr->getEdgeB(),
                                  cr->getEdgeC(), cr->getEdgeD() })
        {
            if (e && e->getTo())
            {
                frozen.insert(e->getTo());
                if (isTJD) tjdFamily[e->getTo()] = nodePtr.get();
            }
        }
        if (isTJD) tjdFamily[nodePtr.get()] = nodePtr.get();
    }

    // Helper : 2 nœuds appartiennent au même TJD (cr+corners partagent leur cellule).
    auto sameTJDFamily = [&](const PCCNode* a, const PCCNode* b) -> bool
        {
            const auto itA = tjdFamily.find(a);
            const auto itB = tjdFamily.find(b);
            return itA != tjdFamily.end()
                && itB != tjdFamily.end()
                && itA->second == itB->second;
        };

    bool changed = true;
    while (changed)
    {
        changed = false;

        std::map<std::pair<int, int>, PCCNode*> posMap;

        for (const auto& nodePtr : graph.getNodes())
        {
            PCCNode* node = nodePtr.get();
            const auto pos = std::make_pair(node->getPosition().x, node->getPosition().y);

            if (posMap.count(pos))
            {
                PCCNode* existing = posMap[pos];
                const int x = node->getPosition().x;
                const int y = node->getPosition().y;

                // --- Cas TJD : cr + 4 corners partagent la cellule par design ---
                if (sameTJDFamily(existing, node))
                {
                    // Pas une vraie collision — on ignore.
                    posMap[pos] = node;  // dernier vu pour le mapping
                    continue;
                }

                const bool existingFrozen = frozen.count(existing) > 0;
                const bool nodeFrozen = frozen.count(node) > 0;

                if (existingFrozen && nodeFrozen)
                {
                    LOG_WARNING(logger, node->getSourceId()
                        + " resolveCollisions: collision non résolvable entre deux nœuds "
                        "figés en [" + std::to_string(x) + "," + std::to_string(y) + "]");
                    posMap[pos] = node;  // avance quand même pour éviter la boucle
                    continue;
                }

                PCCNode* toMove = nodeFrozen ? existing : node;

                // --- Cas TJD : si un nœud non-figé est dans la cellule d'un TJD,
                // il faut le pousser EN X hors de la cellule (pas en Y), pour
                // sortir de la cellule TJD compacte. ---
                const bool inTJDCell = tjdFamily.count(nodeFrozen ? node : existing) > 0;

                if (inTJDCell && !frozen.count(toMove))
                {
                    // Le nœud figé est un membre d'un TJD. Le nœud non-figé doit
                    // sortir en X. Direction = vers l'extérieur du graphe (signe
                    // qui éloigne de la position du to-move actuelle vs la
                    // position du membre TJD — par défaut +1 vers la droite).
                    const PCCNode* tjdMember = nodeFrozen ? node : existing;
                    const int tjdX = tjdMember->getPosition().x;

                    // On essaye d'abord X+1 puis X-1 et on prend le 1er libre.
                    bool moved = false;
                    for (int xDelta : {1, -1})
                    {
                        const int newX = x + xDelta;
                        if (!posMap.count({ newX, y }))
                        {
                            toMove->setPosition({ newX, y });
                            LOG_DEBUG(logger, toMove->getSourceId()
                                + " resolveCollisions: collision avec TJD-cell de "
                                + tjdMember->getSourceId()
                                + " en [" + std::to_string(x) + "," + std::to_string(y) + "]"
                                + " → X=" + std::to_string(newX));
                            moved = true;
                            changed = true;
                            break;
                        }
                        (void)tjdX;
                    }
                    if (moved) break;  // restart map
                    // Sinon : fallback sur la résolution Y standard ci-dessous.
                }

                for (int delta = 1; ; ++delta)
                {
                    const auto above = std::make_pair(x, y + delta);
                    const auto below = std::make_pair(x, y - delta);

                    if (!posMap.count(above))
                    {
                        toMove->setPosition({ x, y + delta });
                        LOG_DEBUG(logger, toMove->getSourceId()
                            + " resolveCollisions: collision avec "
                            + (toMove == node ? existing : node)->getSourceId()
                            + " en [" + std::to_string(x) + "," + std::to_string(y) + "]"
                            + " → Y=" + std::to_string(y + delta));
                        changed = true;
                        break;
                    }
                    if (!posMap.count(below))
                    {
                        toMove->setPosition({ x, y - delta });
                        LOG_DEBUG(logger, toMove->getSourceId()
                            + " resolveCollisions: collision avec "
                            + (toMove == node ? existing : node)->getSourceId()
                            + " en [" + std::to_string(x) + "," + std::to_string(y) + "]"
                            + " → Y=" + std::to_string(y - delta));
                        changed = true;
                        break;
                    }
                }

                break;  // Restart map pour reprise propre.
            }
            else
            {
                posMap[pos] = node;
            }
        }
    }
}
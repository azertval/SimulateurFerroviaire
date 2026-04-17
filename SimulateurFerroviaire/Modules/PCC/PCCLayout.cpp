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
	// 3. Résolution des collisions restantes (cas complexes non traités par les deux étapes précédentes)
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
// Post-traitement — correction du layout des bras de CrossBlock
// =============================================================================

void PCCLayout::fixCrossingLayout(PCCGraph& graph, Logger& logger)
{
    for (const auto& nodePtr : graph.getNodes())
    {
        if (nodePtr->getNodeType() != PCCNodeType::CROSSING) continue;

        auto* cr = static_cast<PCCCrossingNode*>(nodePtr.get());
        const int crX = cr->getPosition().x;
        const int crY = cr->getPosition().y;

        const PCCEdge* slots[4] = {
            cr->getEdgeA(), cr->getEdgeB(),
            cr->getEdgeC(), cr->getEdgeD()
        };

        // -----------------------------------------------------------------------
        // Étape 1 : normalise le X des bras à crX±1
        // -----------------------------------------------------------------------
        for (const PCCEdge* e : slots)
        {
            if (!e || !e->getTo()) continue;
            PCCNode* arm = e->getTo();

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

            if (!neighbour) { /* warning déjà là */ continue; }

            const int nbX = neighbour->getPosition().x;
            const int bfsY = arm->getPosition().y;
            const int armX = (nbX < crX) ? crX - 1 : crX + 1;

            // [GARDE 1] — si le bras est déjà au bon X, ne pas le bouger inutilement
            if (arm->getPosition().x == armX)
                continue;

            // [GARDE 2] — si armX coïncide avec nbX ET nbY, le bras atterrirait
            // exactement sur son voisin : décaler la chaîne amont d'un pas
            if (armX == nbX && bfsY == neighbour->getPosition().y)
            {
                const int shiftDir = (nbX < crX) ? -1 : +1;

                // BFS de la chaîne non-crossing depuis neighbour, côté éloigné du crossing
                std::unordered_set<PCCNode*> toShift;
                std::queue<PCCNode*> q;
                toShift.insert(neighbour);
                q.push(neighbour);

                while (!q.empty())
                {
                    PCCNode* curr = q.front(); q.pop();
                    for (PCCEdge* ae : curr->getEdges())
                    {
                        PCCNode* nb2 = (ae->getFrom() == curr) ? ae->getTo() : ae->getFrom();
                        if (!nb2 || toShift.count(nb2)) continue;
                        if (nb2->getNodeType() == PCCNodeType::CROSSING) continue;
                        // Propager uniquement vers le côté éloigné du crossing
                        if (shiftDir < 0 && nb2->getPosition().x <= neighbour->getPosition().x)
                        {
                            toShift.insert(nb2); q.push(nb2);
                        }
                        else if (shiftDir > 0 && nb2->getPosition().x >= neighbour->getPosition().x)
                        {
                            toShift.insert(nb2); q.push(nb2);
                        }
                    }
                }

                for (PCCNode* n : toShift)
                {
                    auto p = n->getPosition();
                    p.x += shiftDir;
                    n->setPosition(p);
                }

                LOG_DEBUG(logger, arm->getSourceId()
                    + " fixCrossing: chaîne amont décalée de " + std::to_string(shiftDir)
                    + " (" + std::to_string(toShift.size()) + " nœuds) pour éviter collision sur bras");
            }

            arm->setPosition({ armX, bfsY });
        }

        // -----------------------------------------------------------------------
        // Étape 2 : miroir Y si les deux bras de déviation sont du même côté
        // -----------------------------------------------------------------------
        PCCNode* devRight = nullptr;  // bras dévié à crX+1
        PCCNode* devLeft = nullptr;  // bras dévié à crX-1

        for (const PCCEdge* e : slots)
        {
            if (!e || !e->getTo()) continue;
            PCCNode* arm = e->getTo();
            if (arm->getPosition().y == crY) continue;  // bras normal, skip

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
                // devLeft conserve son Y BFS (déterminé par son switch adjacent).
                const int mirrorY = 2 * crY - leftY;
                devRight->setPosition({ devRight->getPosition().x, mirrorY });

                LOG_DEBUG(logger, cr->getSourceId()
                    + " fixCrossing mirrorY (same side): "
                    + devLeft->getSourceId() + " Y=" + std::to_string(leftY)
                    + " → " + devRight->getSourceId() + " Y=" + std::to_string(mirrorY));

                // Pas de propagation aux intermédiaires : seul le bras devRight
                // (slot direct du crossing) est repositionné. Les straights entre
                // devRight et son switch adjacent conservent leur Y BFS naturel —
                // drawStraightBlock (isCrossingArm=true) s'étend vers ses deux
                // voisins (switch et crossing) quelle que soit la différence de Y.
            }
            else
            {
                // Côtés opposés → X naturel, pas de miroir
                LOG_DEBUG(logger, cr->getSourceId()
                    + " fixCrossing no mirror needed: "
                    + devRight->getSourceId() + " Y=" + std::to_string(rightY)
                    + " / " + devLeft->getSourceId() + " Y=" + std::to_string(leftY));
            }
        }

        // Log des positions finales
        auto pos = [](const PCCEdge* e) -> std::string {
            if (!e || !e->getTo()) return "null";
            const PCCNode* n = e->getTo();
            return n->getSourceId()
                + "[" + std::to_string(n->getPosition().x)
                + "," + std::to_string(n->getPosition().y) + "]";
            };

        LOG_DEBUG(logger, cr->getSourceId()
            + " fixCrossing cr=[" + std::to_string(crX) + "," + std::to_string(crY) + "]"
            + " A=" + pos(cr->getEdgeA())
            + " B=" + pos(cr->getEdgeB())
            + " C=" + pos(cr->getEdgeC())
            + " D=" + pos(cr->getEdgeD()));
    }
}

void PCCLayout::resolveCollisions(PCCGraph& graph, Logger& logger)
{
    bool changed = true;
    while (changed)
    {
        changed = false;

        // Construire la map position → nœud
        std::map<std::pair<int, int>, PCCNode*> posMap;

        for (const auto& nodePtr : graph.getNodes())
        {
            PCCNode* node = nodePtr.get();
            const auto pos = std::make_pair(node->getPosition().x, node->getPosition().y);

            if (posMap.count(pos))
            {
                // Collision : déplacer le nœud courant d'un delta en Y
                // Priorité : garder le nœud déjà dans la map, déplacer le newcomer.
                // On cherche le premier Y libre autour de la position.
                PCCNode* existing = posMap[pos];
                const int x = node->getPosition().x;
                const int y = node->getPosition().y;

                for (int delta = 1; ; ++delta)
                {
                    const auto above = std::make_pair(x, y + delta);
                    const auto below = std::make_pair(x, y - delta);

                    if (!posMap.count(above))
                    {
                        node->setPosition({ x, y + delta });
                        LOG_DEBUG(logger, node->getSourceId()
                            + " resolveCollisions: collision avec " + existing->getSourceId()
                            + " en [" + std::to_string(x) + "," + std::to_string(y) + "]"
                            + " → Y=" + std::to_string(y + delta));
                        changed = true;
                        break;
                    }
                    if (!posMap.count(below))
                    {
                        node->setPosition({ x, y - delta });
                        LOG_DEBUG(logger, node->getSourceId()
                            + " resolveCollisions: collision avec " + existing->getSourceId()
                            + " en [" + std::to_string(x) + "," + std::to_string(y) + "]"
                            + " → Y=" + std::to_string(y - delta));
                        changed = true;
                        break;
                    }
                }

                // Restart map pour reprise propre
                break;
            }
            else
            {
                posMap[pos] = node;
            }
        }
    }
}

// =============================================================================
// Post-traitement — réserve les colonnes crX±1 aux bras de crossing
//
// Problème : après le BFS, des nœuds non-bras (switches, straights) peuvent
// atterrir en crX±1. Le bras occupe la même colonne que son voisin → aucun
// espace visuel pour dessiner le bras, d'où le trou au rendu.
//
// Fix : itérer jusqu'à stabilité. À chaque passe, tout nœud qui n'est PAS
// un bras de crossing mais se trouve en crX±1 (colonne réservée) est décalé
// d'un pas vers l'extérieur avec toute sa chaîne connexe.
// =============================================================================

void PCCLayout::fixCrossingSpacing(PCCGraph& graph, Logger& logger)
{
    bool changed = true;
    int  pass = 0;

    while (changed)
    {
        changed = false;
        ++pass;

        for (const auto& nodePtr : graph.getNodes())
        {
            if (nodePtr->getNodeType() != PCCNodeType::CROSSING) continue;

            auto* cr = static_cast<PCCCrossingNode*>(nodePtr.get());
            const int crX = cr->getPosition().x;

            // ---------------------------------------------------------------
            // Collecte des bras légitimes (slots A/B/C/D du crossing courant)
            // ---------------------------------------------------------------
            std::unordered_set<const PCCNode*> arms;
            for (const PCCEdge* e : { cr->getEdgeA(), cr->getEdgeB(),
                                      cr->getEdgeC(), cr->getEdgeD() })
            {
                if (e && e->getTo())
                    arms.insert(e->getTo());
            }

            // ---------------------------------------------------------------
            // Recherche d'un intrus dans crX±1
            // ---------------------------------------------------------------
            for (const auto& otherPtr : graph.getNodes())
            {
                PCCNode* other = otherPtr.get();
                const int otherX = other->getPosition().x;

                // Ignorer : bras légitimes, autres crossings, colonnes non réservées
                if (arms.count(other))                              continue;
                if (other->getNodeType() == PCCNodeType::CROSSING)  continue;
                if (otherX != crX - 1 && otherX != crX + 1)        continue;

                // Direction du décalage : s'éloigner du crossing
                const int shiftDir = (otherX < crX) ? -1 : +1;

                // -----------------------------------------------------------
                // BFS : collecte tout le sous-graphe non-crossing du côté
                // concerné (X >= otherX si shiftDir > 0, X <= otherX sinon)
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

                // -----------------------------------------------------------
                // Application du décalage
                // -----------------------------------------------------------
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
                break; // Redémarre proprement (map invalidée)
            }

            if (changed) break;
        }
    }

    if (pass > 1)
        LOG_DEBUG(logger,
            "fixCrossingSpacing terminé — "
            + std::to_string(pass - 1) + " passe(s).");
}
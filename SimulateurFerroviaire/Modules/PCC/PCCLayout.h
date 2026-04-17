/**
 * @file  PCCLayout.h
 * @brief Calcul des positions logiques X/Y du graphe PCC par parcours BFS.
 *
 * Classe utilitaire statique sans état. Assigne à chaque @ref PCCNode
 * une @ref PCCPosition indépendante des coordonnées GPS, utilisable
 * directement par @ref TCORenderer pour le dessin GDI.
 *
 * @par Algorithme — règle linéaire topologique
 *  -# Détection des terminus (nœuds de départ gauche).
 *  -# BFS multi-sources depuis chaque terminus non visité.
 *  -# Règle de positionnement selon le rôle de l'arête :
 *     | Rôle                                         | ΔX | ΔY              |
 *     |----------------------------------------------|----|----------------|
 *     | STRAIGHT / ROOT standard                     | +1 | 0 (Y constant) |
 *     | NORMAL standard                              | +1 | 0 (Y constant) |
 *     | NORMAL → SwitchNode (aiguille double)        |  0 | ±côté géo      |
 *     | ROOT forward + arrivée par déviation         | -1 | 0 (upstream)   |
 *     | DEVIATION → nœud ordinaire                   | +1 | ±côté géo      |
 *     | DEVIATION → SwitchNode (aiguille double)     |  0 | ±côté géo      |
 *
 * @par Détection "arrivée par déviation"
 *  Le flag @c arrivedViaDeviation est stocké dans @ref BFSItem et propagé
 *  à chaque nœud enqueué. Il est levé dès qu'un voisin est atteint via
 *  une arête DEVIATION. Quand un switch est dépilé avec ce flag à true,
 *  sa voie ROOT est traitée comme upstream (x-1) plutôt que downstream (x+1).
 *
 * @par Côté géographique
 *  Le côté (±1) est calculé une seule fois par
 *  PCCGraphBuilder::computeDeviationSides à partir des coordonnées UTM
 *  et stocké dans @ref PCCSwitchNode::getDeviationSide().
 *  C'est la seule donnée GPS tolérée dans ce calcul.
 *
 * @par Post-traitements (dans l'ordre d'application)
 *  -# @ref fixCollapsedBranches — corrige les branches convergentes de
 *     longueurs inégales (diamonds) en décalant le sous-graphe aval.
 *  -# @ref fixCrossingSpacing — réserve les colonnes crX±1 aux bras de
 *     crossing en repoussant tout intrus vers l'extérieur.
 *  -# @ref fixCrossingLayout — repositionne les bras à crX±1 selon le type
 *     de crossing (flat ou TJD) ; applique le miroir Y si nécessaire.
 *  -# @ref resolveCollisions — résout les collisions résiduelles (deux nœuds
 *     au même [X, Y]) en décalant verticalement le non-figé.
 *
 * @par Graphes déconnectés
 *  Un BFS est lancé depuis chaque terminus de chaque composante.
 *  Un décalage X est appliqué entre composantes pour éviter les superpositions.
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include <unordered_set>
#include <vector>

#include "PCCGraph.h"
#include "Engine/Core/Logger/Logger.h"

class PCCCrossingNode;

 /**
  * @class PCCLayout
  * @brief Calculateur de positions logiques X/Y du @ref PCCGraph.
  */
class PCCLayout
{
public:

    /**
     * @brief Calcule et assigne les positions logiques à tous les nœuds.
     *
     * Enchaîne : findTermini → runBFS (multi-sources) → fixCollapsedBranches
     * → fixCrossingSpacing → fixCrossingLayout → resolveCollisions.
     *
     * @param graph   Graphe dont les positions seront calculées. Modifié en place.
     * @param logger  Référence au logger HMI fourni par @ref PCCPanel.
     */
    static void compute(PCCGraph& graph, Logger& logger);

    /** @brief Interdit l'instanciation — classe utilitaire statique. */
    PCCLayout() = delete;

private:

    /**
     * @brief Contexte BFS d'un nœud en file d'attente.
     */
    struct BFSItem
    {
        PCCNode* node;                ///< Nœud à traiter.
        int      x;                   ///< Profondeur BFS (position horizontale).
        int      y;                   ///< Rang vertical (0 = backbone, ±n = branches).
        bool     arrivedViaDeviation; ///< Vrai si atteint via une arête DEVIATION.
        PCCEdge* arrivedViaEdge;      ///< Arête d'arrivée au nœud.
    };

    /**
     * @brief Détecte les nœuds terminus (points de départ du schéma).
     *
     * @param graph   Graphe à analyser.
     * @param logger  Référence au logger HMI.
     * @return Liste des nœuds terminus. Jamais vide si le graphe est non vide.
     */
    static std::vector<PCCNode*> findTermini(const PCCGraph& graph, Logger& logger);

    /**
     * @brief Lance un BFS linéaire depuis @p start et assigne les positions X/Y.
     *
     * @param graph    Graphe dont les positions sont calculées.
     * @param start    Nœud de départ du BFS.
     * @param visited  Ensemble des nœuds déjà traités. Modifié en place.
     * @param offsetX  Décalage X de la composante.
     * @param logger   Référence au logger HMI.
     * @return Le X maximal atteint.
     */
    static int runBFS(PCCGraph& graph,
                      PCCNode* start,
                      std::unordered_set<PCCNode*>& visited,
                      int                           offsetX,
                      Logger& logger);

    /**
     * @brief Corrige les branches convergentes de longueurs inégales (post-BFS).
     *
     * Détecte tout switch dont une branche DEVIATION ou NORMAL atterrit au
     * même X que lui-même mais à un Y différent (branche visuellement nulle).
     * Décale ce switch et tout son sous-graphe aval (via ROOT) de +1 cellule.
     * Itère jusqu'à convergence pour traiter les corrections en chaîne.
     *
     * @par Exemple
     * @code
     * sw/A[32,0] ─── sw/3[33,0] ─── s/5[34,0] ─── sw/B[35,0]   (1 bloc normal)
     *     \                                               /
     *      s/10[33,-1] ─── s/21[34,-1] ─── s/6[35,-1]   (3 blocs déviation)
     * @endcode
     * → s/6 et sw/B sont au même X=35 → sw/B décalé à [36,0], aval suit.
     *
     * @param graph   Graphe à corriger. Modifié en place.
     * @param logger  Référence au logger HMI.
     */
    static void fixCollapsedBranches(PCCGraph& graph, Logger& logger);

    /**
     * @brief Réserve les colonnes crX±1 aux bras de crossing (post-BFS).
     *
     * Après le BFS, des nœuds non-bras peuvent atterrir en crX±1, supprimant
     * l'espace visuel nécessaire au dessin du bras. Cette fonction pousse tout
     * intrus vers l'extérieur avec sa chaîne connexe, jusqu'à stabilité.
     *
     * @note Sans effet sur les TJD (pas de colonnes réservées pour ce type).
     *
     * @param graph   Graphe à corriger. Modifié en place.
     * @param logger  Référence au logger HMI.
     */
    static void fixCrossingSpacing(PCCGraph& graph, Logger& logger);

    /**
     * @brief Repositionne les bras de chaque crossing, puis route selon le type.
     *
     * Pour chaque CrossingNode, délègue à fixFlatCrossingLayout() ou
     * fixTJDCrossingLayout() selon que la source est un StraightCrossBlock
     * ou un SwitchCrossBlock.
     *
     * @param graph   Graphe à corriger. Modifié en place.
     * @param logger  Référence au logger HMI.
     */
    static void fixCrossingLayout(PCCGraph& graph, Logger& logger);

    /**
     * @brief Repositionne les bras d'un croisement plat (StraightCrossBlock).
     *
     * Étape 1 — Placement : chaque bras est repositionné à crX-1 ou crX+1
     * selon le côté de son voisin non-crossing. Le Y BFS du bras est conservé.
     *
     * Étape 2 — Miroir Y : si les deux bras déviés (Y ≠ crY) se retrouvent
     * du même côté vertical après le placement, le bras droit est mis en miroir
     * par rapport à crY pour former un X correct. Le bras gauche conserve son Y
     * BFS (référence géographique).
     *
     * @par Topologie cible
     * @code
     *   A [crX-1, Y_A]  ───╲───╱──  C [crX+1, Y_C]   (voie 1)
     *                       ╲ ╱
     *                       ╱ ╲
     *   B [crX-1, Y_B]  ───╱───╲──  D [crX+1, Y_D]   (voie 2)
     * @endcode
     *
     * @param cr      Nœud crossing (StraightCrossBlock). Modifié en place.
     * @param logger  Référence au logger HMI.
     */
    static void fixFlatCrossingLayout(PCCCrossingNode* cr, Logger& logger);

    /**
     * @brief Repositionne les bras d'un croisement TJD (SwitchCrossBlock).
     *
     * Les 4 bras sont des SwitchBlock pouvant avoir le même X BFS (double
     * switch). La séparation gauche/droite se fait via le voisin extérieur
     * (non-crossing) de chaque bras, pas via la position du bras lui-même.
     *
     * Après tri par Y croissant, les bras gauches sont placés à crX et les
     * bras droits à crX+1. Les Y droits sont croisés (C prend crYB, D prend
     * crYA) pour former les diagonales du ✕.
     *
     * @par Topologie cible
     * @code
     *                            ──── D[crX+1, crY+1]
     *                          /
     *   A[crX, crY]  ──────────────── C[crX+1, crY]
     *                       ╱
     * B[crX, crY-1]────────
     * @endcode
     *
     * @param cr      Nœud crossing (SwitchCrossBlock). Modifié en place.
     * @param logger  Référence au logger HMI.
     */
    static void fixTJDCrossingLayout(PCCCrossingNode* cr, Logger& logger);

    /**
     * @brief Résout les collisions résiduelles (deux nœuds au même [X, Y]).
     *
     * Itère jusqu'à stabilité. À chaque collision, déplace le nœud non-figé
     * (non-crossing, non-bras) vers le Y libre le plus proche (±delta croissant).
     * Les CrossingNode et leurs bras sont toujours figés — leurs positions sont
     * fixées par fixCrossingLayout() et ne doivent pas être perturbées.
     *
     * @param graph   Graphe à corriger. Modifié en place.
     * @param logger  Référence au logger HMI.
     */
    static void resolveCollisions(PCCGraph& graph, Logger& logger);
};

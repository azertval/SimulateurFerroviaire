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
 *     | Rôle                                        | ΔX | ΔY             |
 *     |---------------------------------------------|----|----------------|
 *     | STRAIGHT / ROOT standard                     | +1 | 0 (Y constant) |
 *     | NORMAL standard                              | +1 | 0 (Y constant) |
 *     | NORMAL → SwitchNode (aiguille double)        |  0 | ±côté géo      |
 *     | ROOT forward + arrivée par déviation         | -1 | 0 (upstream)   |
 *     | DEVIATION → nœud ordinaire                   | +1 | ±côté géo      |
 *     | DEVIATION → SwitchNode  (aiguille double)    |  0 | ±côté géo      |
 *
 * @par Détection "arrivée par déviation"
 *  Le flag @c arrivedViaDeviation est stocké dans @ref BFSItem et propagé
 *  à chaque nœud enqueué. Il est levé dès qu'un voisin est atteint via
 *  une arête DEVIATION. Quand un switch est dépilé avec ce flag à true,
 *  sa voie ROOT est traitée comme upstream (x-1) plutôt que downstream
 *  (x+1).
 *
 * @par Côté géographique
 *  Le côté (±1) est calculé **une seule fois** par
 *  PCCGraphBuilder::computeDeviationSides à partir des coordonnées UTM
 *  et stocké dans @ref PCCSwitchNode::getDeviationSide().
 *  C'est la **seule** donnée GPS tolérée dans ce calcul.
 *
 * @par Post-traitement — branches convergentes de longueurs inégales
 *  Après le BFS, @ref fixCollapsedBranches détecte tout switch dont
 *  une branche (DEVIATION ou NORMAL) atterrit au même X que lui-même
 *  (longueur visuelle nulle). Il décale ce switch et tout son sous-graphe
 *  aval (via ROOT) de +1 cellule, jusqu'à ce qu'aucun cas subsiste.
 *  La logique de @ref TCORenderer::drawStraightBlock (extension au mi-chemin
 *  vers les voisins de même Y) et de @ref TCORenderer::drawSwitchBlock
 *  gèrent alors le rendu automatiquement, sans modification du renderer.
 *
 * @par Graphes déconnectés
 *  Un BFS est lancé depuis chaque terminus de chaque composante.
 *  Un décalage X est appliqué entre composantes pour éviter les
 *  superpositions.
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include <unordered_set>
#include <vector>

#include "PCCGraph.h"
#include "Engine/Core/Logger/Logger.h"

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
     * Enchaîne : findTermini → runBFS (multi-sources) → fixCollapsedBranches.
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
        PCCEdge* arrivedViaEdge;       ///< arête d'arrivée au nœud
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
     * → s/6 et sw/B sont au même X=35 → sw/B décalé à [36,0], s/4_0 etc. suivent.
     *
     * @param graph   Graphe à corriger. Modifié en place.
     * @param logger  Référence au logger HMI.
     */
    static void fixCollapsedBranches(PCCGraph& graph, Logger& logger);
};
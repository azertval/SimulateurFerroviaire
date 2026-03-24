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
 *     | STRAIGHT / NORMAL / ROOT standard            | +1 | 0 (Y constant) |
 *     | ROOT forward + arrivée par déviation         | -1 | 0 (upstream)   |
 *     | DEVIATION → nœud ordinaire                   | +1 | ±côté géo      |
 *     | DEVIATION → SwitchNode  (aiguille double)    |  0 | ±côté géo      |
 *
 * @par Détection "arrivée par déviation"
 *  Le flag @c arrivedViaDeviation est stocké dans @ref BFSItem et propagé
 *  à chaque nœud enqueué. Il est levé dès qu'un voisin est atteint via
 *  une arête DEVIATION. Quand un switch est dépilé avec ce flag à true,
 *  sa voie ROOT est traitée comme upstream (x-1) plutôt que downstream
 *  (x+1). Ce mécanisme couvre l'aiguille simple et la double aiguille
 *  sans distinction de cas.
 *
 * @par Côté géographique
 *  Le côté (±1) est calculé **une seule fois** par
 *  PCCGraphBuilder::computeDeviationSides à partir des lat/lon
 *  et stocké dans @ref PCCSwitchNode::getDeviationSide().
 *  C'est la **seule** donnée GPS utilisée pendant le layout.
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
     * @param graph   Graphe dont les positions seront calculées. Modifié en place.
     * @param logger  Référence au logger HMI fourni par @ref PCCPanel.
     */
    static void compute(PCCGraph& graph, Logger& logger);

    /** @brief Interdit l'instanciation — classe utilitaire statique. */
    PCCLayout() = delete;

private:

    /**
     * @brief Contexte BFS d'un nœud en file d'attente.
     *
     * Le champ @c arrivedViaDeviation est le mécanisme central du layout :
     * il indique si le nœud a été enqueué via une arête DEVIATION.
     * Quand un @ref PCCSwitchNode est dépilé avec ce flag à true,
     * sa voie ROOT (forward) est positionnée à x-1 (amont) plutôt
     * qu'à x+1 (aval).
     */
    struct BFSItem
    {
        PCCNode* node;               ///< Nœud à traiter.
        int      x;                  ///< Profondeur BFS (position horizontale).
        int      y;                  ///< Rang vertical (0 = backbone, ±n = branches).
        bool     arrivedViaDeviation;///< Vrai si atteint via une arête DEVIATION.
    };

    /**
     * @brief Détecte les nœuds terminus (points de départ du schéma).
     *
     * Un terminus est absent des cibles d'arêtes switch (ROOT/NORMAL/DEVIATION)
     * et ne possède qu'un seul voisin. Si aucun terminus n'est trouvé
     * (graphe circulaire), retourne le premier nœud de la collection.
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
     * @param offsetX  Décalage X de la composante (séparation entre composantes).
     * @param logger   Référence au logger HMI.
     * @return Le X maximal atteint — utilisé pour espacer les composantes.
     */
    static int runBFS(PCCGraph& graph,
        PCCNode* start,
        std::unordered_set<PCCNode*>& visited,
        int                           offsetX,
        Logger& logger);
};
/**
 * @file  PCCLayout.h
 * @brief Calcul des positions logiques X/Y du graphe PCC par parcours BFS.
 *
 * Classe utilitaire statique sans état. Assigne à chaque @ref PCCNode
 * une @ref PCCPosition indépendante des coordonnées GPS, utilisable
 * directement par @ref TCORenderer pour le dessin GDI.
 *
 * @par Algorithme
 *  -# Détection des terminus (nœuds de départ gauche).
 *  -# BFS multi-sources depuis chaque terminus non visité.
 *  -# X = profondeur BFS (distance minimale depuis le terminus).
 *  -# Y = rang vertical — 0 backbone, +1 par branche déviation empruntée.
 *
 * @par Graphes déconnectés
 * Si le réseau contient plusieurs composantes non connectées, un BFS
 * est lancé depuis chaque terminus de chaque composante. Un décalage X
 * est appliqué entre composantes pour éviter les superpositions.
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
     * Appelle successivement @ref findTermini puis @ref runBFS pour chaque
     * composante. Les nœuds non couverts reçoivent un BFS de secours avec
     * un avertissement dans le logger.
     *
     * @param graph   Graphe dont les positions seront calculées. Modifié en place.
     * @param logger  Référence au logger HMI fourni par @ref PCCPanel.
     */
    static void compute(PCCGraph& graph, Logger& logger);

    /** @brief Interdit l'instanciation — classe utilitaire statique. */
    PCCLayout() = delete;

private:

    /**
     * @brief Structure interne portant un nœud et son contexte de position BFS.
     *
     * Utilisée comme élément de la file BFS pour propager x et y
     * sans modifier le nœud avant de le dépiler.
     */
    struct BFSItem
    {
        PCCNode* node;  ///< Nœud à traiter.
        int      x;     ///< Profondeur BFS (position horizontale).
        int      y;     ///< Rang vertical (0 = backbone, +1 = déviation).
    };

    /**
     * @brief Détecte les nœuds terminus (points de départ du schéma).
     *
     * Un terminus est un nœud absent de l'ensemble des cibles d'arêtes
     * switch (ROOT/NORMAL/DEVIATION) et possédant un seul voisin.
     * Si aucun terminus n'est trouvé (graphe circulaire), retourne le
     * premier nœud de la collection.
     *
     * @param graph   Graphe à analyser.
     * @param logger  Référence au logger HMI.
     *
     * @return Liste des nœuds terminus. Jamais vide si le graphe est non vide.
     */
    static std::vector<PCCNode*> findTermini(const PCCGraph& graph, Logger& logger);

    /**
     * @brief Lance un BFS depuis @p start et assigne les positions X/Y.
     *
     * Le BFS s'arrête lorsque tous les nœuds atteignables depuis @p start
     * ont été visités. Les nœuds déjà présents dans @p visited sont ignorés
     * (support multi-composantes).
     *
     * @param graph           Graphe dont les positions sont calculées.
     * @param start           Nœud de départ du BFS.
     * @param visited         Ensemble des nœuds déjà traités. Modifié en place.
     * @param offsetX         Décalage X appliqué à toute la composante
     *                        (séparation entre composantes déconnectées).
     * @param logger          Référence au logger HMI.
     *
     * @return Le X maximal atteint par ce BFS — utilisé par compute()
     *         pour espacer les composantes déconnectées.
     */
    static int runBFS(PCCGraph& graph,
        PCCNode* start,
        std::unordered_set<PCCNode*>& visited,
        int                             offsetX,
        Logger& logger);
};
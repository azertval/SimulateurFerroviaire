/**
 * @file  BlockSet.h
 * @brief Structures de données produites par @ref Phase6_BlockExtractor.
 *
 * Conteneur des blocs ferroviaires non-orientés.
 * Possède les blocs via @c std::unique_ptr — propriété exclusive,
 * destruction automatique.
 */
#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Modules/Elements/ShuntingElements/StraightBlock.h"
#include "Modules/Elements/ShuntingElements/SwitchBlock.h"

 /**
  * @struct BlockEndpoint
  * @brief Extrémité d'un bloc — nœud frontière + ID du bloc voisin.
  *
  * Données intermédiaires construites en Phase 6, résolues en Phase 9.
  * Le champ @c neighbourId est vide jusqu'à la résolution.
  */
struct BlockEndpoint
{
    size_t      frontierNodeId = 0;  ///< ID du nœud topologique frontière.
    std::string neighbourId;         ///< ID du bloc voisin — résolu en Phase 9.
};

/**
 * @struct BlockSet
 * @brief Conteneur propriétaire des StraightBlock et SwitchBlock.
 *
 * Produit en Phase 6, enrichi par Phase 7 (orientation) et Phase 8
 * (double switch), transféré vers @ref TopologyRepository en Phase 9.
 *
 * @par Ownership
 * Les blocs sont possédés via @c unique_ptr — copiabilité interdite,
 * déplacement autorisé. Les index @c straightByNode / @c switchByNode
 * contiennent des raw pointers non-propriétaires.
 */
struct BlockSet
{
    /** Voies droites — propriétaire exclusif. */
    std::vector<std::unique_ptr<StraightBlock>> straights;

    /** Aiguillages — propriétaire exclusif. */
    std::vector<std::unique_ptr<SwitchBlock>> switches;

    /**
     * Index nodeId → StraightBlock* (non-propriétaire).
     * Clé = ID du nœud frontière A ou B du bloc.
     * Construit en Phase 6, utilisé en Phase 9 pour résoudre les voisins.
     */
    std::unordered_map<size_t, std::vector<StraightBlock*>> straightsByNode;

    /**
     * Index nodeId → SwitchBlock* (non-propriétaire).
     * Clé = ID du nœud SWITCH correspondant au bloc.
     */
    std::unordered_map<size_t, SwitchBlock*> switchByNode;

    /**
     * Index pairKey(nodeA, nodeB) → liste de StraightBlock* (non-propriétaires).
     * Clé canonique = Cantor(min(idA,idB), max(idA,idB)).
     *
     * La liste contient en général un seul élément.
     * Elle en contient plusieurs uniquement dans le cas d'un crossover :
     * deux straights distincts reliant exactement les mêmes nœuds frontières.
     *
     * La désambiguïsation est géométrique dans @ref Phase6_BlockExtractor::extractSwitches :
     * on sélectionne le straight dont la géométrie passe le plus près du tip
     * de la branche (premier nœud STRAIGHT immédiatement après le switch).
     */
    std::unordered_map<size_t, std::vector<StraightBlock*>> straightByEndpointPair;

    /**
     * Endpoints des StraightBlocks — deux par bloc (prev et next).
     * Index parallèle à @c straights : endpoints[i] = {prevEndpoint, nextEndpoint}
     */
    std::vector<std::pair<BlockEndpoint, BlockEndpoint>> straightEndpoints;

    /**
     * Endpoints des SwitchBlocks — trois par bloc (une par branche).
     * Index parallèle à @c switches.
     */
    std::vector<std::array<BlockEndpoint, 3>> switchEndpoints;

    /** @brief Vide le conteneur — libère la mémoire après Phase 9. */
    void clear()
    {
        straights.clear();               straights.shrink_to_fit();
        switches.clear();                switches.shrink_to_fit();
        straightsByNode.clear();
        switchByNode.clear();
        straightByEndpointPair.clear();
        straightEndpoints.clear();
        switchEndpoints.clear();
    }

    /** @return Nombre total de blocs. */
    [[nodiscard]] size_t totalCount() const
    {
        return straights.size() + switches.size();
    }

    /**
     * @brief Reconstruit les index straightsByNode et straightByEndpointPair.
     *
     * Appelé par @ref Phase7_SwitchProcessor::absorbLinkSegment() après
     * suppression d'un segment de liaison pour invalider les entrées obsolètes.
     */
    void rebuildStraightIndex()
    {
        straightsByNode.clear();
        straightByEndpointPair.clear();

        for (size_t i = 0; i < straights.size(); ++i)
        {
            if (i >= straightEndpoints.size()) break;
            StraightBlock* st = straights[i].get();
            const size_t   nA = straightEndpoints[i].first.frontierNodeId;
            const size_t   nB = straightEndpoints[i].second.frontierNodeId;

            straightsByNode[nA].push_back(st);
            straightsByNode[nB].push_back(st);

            // Cantor pairing canonique
            const size_t a = std::min(nA, nB);
            const size_t b = std::max(nA, nB);
            const size_t key = (a + b) * (a + b + 1) / 2 + b;
            straightByEndpointPair[key].push_back(st);
        }
    }
};
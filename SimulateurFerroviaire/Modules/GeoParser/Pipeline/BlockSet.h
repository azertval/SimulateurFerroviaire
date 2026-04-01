/**
 * @file  BlockSet.h
 * @brief Structures de données produites par @ref Phase6_BlockExtractor.
 *
 * @par straightByDirectedPair — multi-valué depuis la v2
 * La clé directionnelle (from * 1'000'000 + to) peut désormais pointer
 * vers plusieurs StraightBlock* lorsque deux straights relient les mêmes
 * nœuds frontières (cas crossover / voie double).
 * Avant la correction, la seconde insertion écrasait la première, ce qui
 * faisait apparaître le même straight en normal ET deviation d'un switch.
 */
#pragma once

#include <array>
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
 * @c frontierNodeId vaut @c SIZE_MAX pour les endpoints internes des
 * sous-blocs produits par subdivision (@c maxSegmentLength) : il n'existe
 * aucun nœud topologique réel à ces positions intermédiaires.
 * @ref Phase8_RepositoryTransfer::resolveStraight teste cette valeur pour
 * éviter d'écraser les pointeurs de chaîne posés par
 * @ref Phase6_BlockExtractor::registerStraight.
 */
struct BlockEndpoint
{
    size_t      frontierNodeId = 0;
    std::string neighbourId;
};

/**
 * @struct BlockSet
 * @brief Conteneur propriétaire des StraightBlock et SwitchBlock.
 *
 * Construit exclusivement par @ref Phase6_BlockExtractor.
 * Transféré vers @ref TopologyRepository par @ref Phase8_RepositoryTransfer.
 */
struct BlockSet
{
    // =========================================================================
    // Blocs — ownership exclusif
    // =========================================================================

    std::vector<std::unique_ptr<StraightBlock>> straights;
    std::vector<std::unique_ptr<SwitchBlock>>   switches;

    // =========================================================================
    // Index de lookup
    // =========================================================================

    /**
     * nodeId → liste de StraightBlock* adjacents.
     * Multi-valué : un nœud SWITCH est adjacent à plusieurs straights.
     */
    std::unordered_map<size_t, std::vector<StraightBlock*>> straightsByNode;

    /** nodeId → SwitchBlock* correspondant. */
    std::unordered_map<size_t, SwitchBlock*> switchByNode;

    /**
     * Cantor(min(A,B), max(A,B)) → premier StraightBlock* depuis A.
     * Utilisé par @c rebuildStraightIndex(). Pour la résolution des endpoints
     * de switches, préférer @c straightByDirectedPair.
     */
    std::unordered_map<size_t, StraightBlock*> straightByEndpointPair;

    /**
     * Clé directionnelle : (from * 1'000'000 + to) → liste de StraightBlock*
     * adjacents au nœud @c from vers le nœud @c to.
     *
     * @par Multi-valué — indispensable pour les crossovers
     * Quand deux straights relient les mêmes deux nœuds frontières (voie double
     * ou crossover), les deux entrées sont conservées dans le vecteur.
     * @ref Phase6_BlockExtractor::extractSwitches consomme les entrées dans
     * l'ordre via un ensemble @c usedStraights par switch pour éviter
     * d'attribuer le même straight à deux branches distinctes.
     *
     * @par Sens de traversal
     *  - directedKey(switchNode, frontierNode) → sous-bloc adjacent au switch
     *  - directedKey(frontierNode, switchNode) → sous-bloc adjacent au frontier
     *
     * Valide pour < 1 000 000 nœuds (réseau typique ~420 nœuds).
     */
    std::unordered_map<size_t, std::vector<StraightBlock*>> straightByDirectedPair;

    // =========================================================================
    // Endpoints — index parallèles aux vecteurs straights / switches
    // =========================================================================

    /**
     * Index parallèle à @c straights.
     * Pour un sous-bloc interne (subdivision), @c frontierNodeId == SIZE_MAX
     * et @c neighbourId est vide — @ref Phase8_RepositoryTransfer ne doit
     * PAS écraser les pointeurs de chaîne pour ces entrées.
     */
    std::vector<std::pair<BlockEndpoint, BlockEndpoint>> straightEndpoints;

    /** Index parallèle à @c switches — 3 endpoints par switch. */
    std::vector<std::array<BlockEndpoint, 3>>            switchEndpoints;

    // =========================================================================
    // Utilitaires
    // =========================================================================

    void clear()
    {
        straights.clear();               straights.shrink_to_fit();
        switches.clear();                switches.shrink_to_fit();
        straightsByNode.clear();
        switchByNode.clear();
        straightByEndpointPair.clear();
        straightByDirectedPair.clear();
        straightEndpoints.clear();
        switchEndpoints.clear();
    }

    [[nodiscard]] size_t totalCount() const
    {
        return straights.size() + switches.size();
    }

    /**
     * @brief Reconstruit tous les index straight depuis @c straights et
     *        @c straightEndpoints (après absorption Phase7).
     *
     * Recrée @c straightsByNode, @c straightByEndpointPair et
     * @c straightByDirectedPair en parcourant les endpoints enregistrés.
     * Ignore les sous-blocs internes (frontierNodeId == SIZE_MAX).
     */
    void rebuildStraightIndex()
    {
        straightsByNode.clear();
        straightByEndpointPair.clear();
        straightByDirectedPair.clear();

        for (size_t i = 0; i < straights.size(); ++i)
        {
            if (i >= straightEndpoints.size()) break;
            StraightBlock* st = straights[i].get();
            const size_t   nA = straightEndpoints[i].first.frontierNodeId;
            const size_t   nB = straightEndpoints[i].second.frontierNodeId;

            // Saute les sous-blocs internes — pas de nœud frontière réel
            if (nA == SIZE_MAX || nB == SIZE_MAX) continue;

            straightsByNode[nA].push_back(st);
            straightsByNode[nB].push_back(st);

            const size_t a = std::min(nA, nB);
            const size_t b = std::max(nA, nB);
            straightByEndpointPair[(a + b) * (a + b + 1) / 2 + b] = st;

            // Multi-valué : push_back pour conserver les deux entrées crossover
            straightByDirectedPair[nA * 1'000'000ULL + nB].push_back(st);
            straightByDirectedPair[nB * 1'000'000ULL + nA].push_back(st);
        }
    }
};

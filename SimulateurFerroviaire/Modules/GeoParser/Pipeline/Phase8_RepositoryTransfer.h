/**
 * @file  Phase8_RepositoryTransfer.h
 * @brief Phase 9 du pipeline — résolution des pointeurs et transfert vers TopologyRepository.
 *
 * Scindée en deux méthodes publiques pour respecter la contrainte d'ordre
 * avec @ref Phase8_SwitchOrientator :
 *
 * @par Ordre d'appel obligatoire
 * @code
 * Phase8_RepositoryTransfer::resolve(ctx, logger);    // 9a
 * Phase8_SwitchOrientator::run(ctx, config, logger);  // 7 — nécessite pointeurs
 * Phase8_RepositoryTransfer::transfer(ctx, logger);   // 9b — transfert final
 * @endcode
 *
 * @par 9a — resolve()
 * Construit l'index ID → bloc* et résout tous les pointeurs inter-blocs
 * (prev/next des StraightBlocks, root/normal/deviation des SwitchBlocks).
 *
 * @par 9b — transfer()
 * Vide @ref TopologyRepository, transfère les blocs via @c std::move,
 * appelle @c buildIndex() et libère @c ctx.blocks.
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Logger/Logger.h"

class Phase8_RepositoryTransfer
{
public:

    /**
     * @brief 8a — Résout les pointeurs inter-blocs.
     *
     * Enchaîne deux passes :
     *
     * @par Passe 1 — Renseignement des neighbourId des StraightBlocks
     * Pour chaque endpoint d'un @ref StraightBlock, recherche le bloc voisin
     * dans l'ordre de priorité suivant :
     *  -# @c switchByNode   — voisin SwitchBlock
     *  -# @c crossingByNode — voisin CrossBlock
     *  -# @c straightsByNode — voisin StraightBlock adjacent
     *
     * Les sous-blocs internes (@c frontierNodeId == @c SIZE_MAX) produisent
     * un @c neighbourId vide — la Passe 2 ne les modifiera pas, préservant
     * la chaîne prev/next posée par @ref Phase6_BlockExtractor::registerStraight.
     *
     * @par Passe 2 — Résolution des pointeurs depuis l'index ID → bloc*
     * Construit l'index via @ref buildBlockIndex (couvrant straights, switches
     * et crossings), puis appelle @ref resolveStraight, @ref resolveSwitch et
     * @ref resolveCrossing pour chaque bloc correspondant.
     *
     * @param ctx     Contexte pipeline. Modifie les pointeurs dans @c ctx.blocks.
     * @param logger  Référence au logger GeoParser.
     */
    static void resolve(PipelineContext& ctx, Logger& logger);

    /**
     * @brief 9b — Transfère les blocs vers @ref TopologyRepository.
     *
     * Vide @ref TopologyRepository via @c clear(), transfère les
     * @c unique_ptr de straights, switches et crossings via @c std::move
     * (O(1), adresses stables), appelle @c buildIndex() et libère
     * @c ctx.blocks.
     *
     * @par Précondition
     * @ref resolve() et @ref Phase7_SwitchProcessor::run() doivent avoir été
     * appelés avant ce transfert. Les pointeurs non-propriétaires distribués
     * pendant la Passe 2 restent valides après le move.
     *
     * @param ctx     Contexte pipeline. Transfère @c ctx.blocks → @ref TopologyRepository.
     * @param logger  Référence au logger GeoParser.
     */
    static void transfer(PipelineContext& ctx, Logger& logger);

    Phase8_RepositoryTransfer() = delete;

private:

    /**
     * @brief Construit l'index ID → @ref ShuntingElement* depuis @c ctx.blocks.
     *
     * Couvre les trois catégories de blocs : @ref StraightBlock,
     * @ref SwitchBlock et @ref CrossBlock. Utilisé en Passe 2 de @ref resolve()
     * pour permettre la résolution des pointeurs inter-blocs en O(1).
     *
     * @param blocks  Ensemble des blocs produits par @ref Phase6_BlockExtractor.
     *
     * @return Map ID → @ref ShuntingElement* (non-propriétaire).
     *         Valide tant que @c blocks est en vie et non modifié.
     */
    static std::unordered_map<std::string, ShuntingElement*>
        buildBlockIndex(const BlockSet& blocks);

    /**
     * @brief Résout les pointeurs prev/next d'un StraightBlock.
     *
     * @param st       StraightBlock à résoudre.
     * @param epPrev   Endpoint côté prev.
     * @param epNext   Endpoint côté next.
     * @param index    Index ID → ShuntingElement*.
     * @param logger   Référence au logger.
     */
    static void resolveStraight(
        StraightBlock& st,
        const BlockEndpoint& epPrev,
        const BlockEndpoint& epNext,
        const std::unordered_map<std::string, ShuntingElement*>& index,
        Logger& logger);

    /**
     * @brief Résout les pointeurs root/normal/deviation d'un SwitchBlock.
     *
     * @param sw       SwitchBlock à résoudre.
     * @param eps      Tableau des 3 endpoints (root, normal, deviation).
     * @param index    Index ID → ShuntingElement*.
     * @param logger   Référence au logger.
     */
    static void resolveSwitch(
        SwitchBlock& sw,
        const std::array<BlockEndpoint, 3>& eps,
        const std::unordered_map<std::string, ShuntingElement*>& index,
        Logger& logger);

    /**
     * @brief Résout les pointeurs de branches A/B/C/D d'un CrossBlock.
     *
     * Pour chaque endpoint, recherche l'ID voisin dans l'index commun
     * (StraightBlock* + SwitchBlock* + CrossBlock*) et assigne le pointeur
     * non-propriétaire correspondant via setBranchAPointer … setBranchDPointer.
     *
     * Un endpoint avec neighbourId vide produit un pointeur nullptr sans warning
     * (cas terminus — ne doit pas survenir sur un nœud CROSSING bien formé).
     * Un ID introuvable dans l'index produit un WARNING et un pointeur nullptr.
     *
     * @param cr      CrossBlock à résoudre. Modifié en place.
     * @param eps     Tableau de 4 endpoints dans l'ordre A, B, C, D.
     * @param index   Index ID → ShuntingElement* couvrant straights,
     *                switches et crossings (construit par buildBlockIndex()).
     * @param logger  Référence au logger GeoParser.
     */
    static void resolveCrossing(CrossBlock& cr,
        const std::array<BlockEndpoint, 4>& eps,
        const std::unordered_map<std::string, ShuntingElement*>& index,
        Logger& logger);
};
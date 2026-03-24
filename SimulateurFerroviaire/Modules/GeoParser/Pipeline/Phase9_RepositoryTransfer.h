/**
 * @file  Phase9_RepositoryTransfer.h
 * @brief Phase 9 du pipeline — résolution des pointeurs et transfert vers TopologyRepository.
 *
 * Scindée en deux méthodes publiques pour respecter la contrainte d'ordre
 * avec @ref Phase7_SwitchOrientator :
 *
 * @par Ordre d'appel obligatoire
 * @code
 * Phase9_RepositoryTransfer::resolve(ctx, logger);    // 9a
 * Phase7_SwitchOrientator::run(ctx, config, logger);  // 7 — nécessite pointeurs
 * Phase9_RepositoryTransfer::transfer(ctx, logger);   // 9b — transfert final
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

class Phase9_RepositoryTransfer
{
public:

    /**
     * @brief 9a — Résout les pointeurs inter-blocs.
     *
     * Construit un index ID → bloc*, puis résout @c prev/@c next des
     * @ref StraightBlock et @c root/@c normal/@c deviation des
     * @ref SwitchBlock depuis les @ref BlockEndpoint enregistrés en Phase 6.
     *
     * @par Précondition
     * @c ctx.blocks doit être peuplé (Phase 6) et les doubles aiguilles
     * absorbées (Phase 8). Les @c BlockEndpoint::neighbourId doivent être
     * renseignés (remplis automatiquement par Phase 6 via les nœuds frontières).
     *
     * @param ctx     Contexte pipeline. Modifie les pointeurs dans ctx.blocks.
     * @param logger  Référence au logger GeoParser.
     */
    static void resolve(PipelineContext& ctx, Logger& logger);

    /**
     * @brief 9b — Transfère les blocs vers TopologyRepository.
     *
     * @par Précondition
     * @c resolve() et @ref Phase7_SwitchOrientator::run() doivent avoir été
     * appelés avant ce transfert.
     *
     * Vide @ref TopologyRepository via @c clear(), transfère les
     * @c unique_ptr via @c std::move, appelle @c buildIndex() et libère
     * @c ctx.blocks.
     *
     * @param ctx     Contexte pipeline. Transfère ctx.blocks → TopologyRepository.
     * @param logger  Référence au logger GeoParser.
     */
    static void transfer(PipelineContext& ctx, Logger& logger);

    Phase9_RepositoryTransfer() = delete;

private:

    /**
     * @brief Construit l'index ID → ShuntingElement* depuis ctx.blocks.
     *
     * Utilisé en Passe 1 de @c resolve() pour permettre la résolution
     * des pointeurs en Passe 2.
     *
     * @param blocks  Ensemble des blocs.
     *
     * @return Map ID → ShuntingElement* (non-propriétaire).
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
};
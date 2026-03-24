/**
 * @file  Phase9_RepositoryTransfer.cpp
 * @brief Implémentation de la phase 9 — résolution + transfert.
 *
 * @see Phase9_RepositoryTransfer
 */
#include "Phase9_RepositoryTransfer.h"

#include "Engine/Core/Topology/TopologyRepository.h"
#include "Engine/Core/Topology/TopologyData.h"
#include "Modules/InteractiveElements/ShuntingElements/StraightBlock.h"
#include "Modules/InteractiveElements/ShuntingElements/SwitchBlock.h"

#include <unordered_map>


 // =============================================================================
 // 9a — Résolution des pointeurs
 // =============================================================================

void Phase9_RepositoryTransfer::resolve(PipelineContext& ctx,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    LOG_INFO(logger, "Résolution des pointeurs inter-blocs — "
        + std::to_string(ctx.blocks.straights.size()) + " straight(s), "
        + std::to_string(ctx.blocks.switches.size()) + " switch(es).");

    // -------------------------------------------------------------------------
    // Passe 1 — Renseignement des neighbourId depuis les nœuds frontières
    // -------------------------------------------------------------------------
    // Les BlockEndpoint stockent un frontierNodeId (nœud topologique).
    // On les convertit en ID de bloc via les index de BlockSet.
    // -------------------------------------------------------------------------

    // Pour chaque StraightBlock : résout les neighbourId des endpoints
    for (size_t i = 0; i < ctx.blocks.straights.size(); ++i)
    {
        if (i >= ctx.blocks.straightEndpoints.size()) break;

        auto& [epA, epB] = ctx.blocks.straightEndpoints[i];

        // Cherche le switch voisin du côté epA
        auto itSwA = ctx.blocks.switchByNode.find(epA.frontierNodeId);
        if (itSwA != ctx.blocks.switchByNode.end())
            epA.neighbourId = itSwA->second->getId();
        else
        {
            // Cherche un straight voisin (TERMINUS ou autre straight)
            auto itStA = ctx.blocks.straightByNode.find(epA.frontierNodeId);
            if (itStA != ctx.blocks.straightByNode.end()
                && itStA->second != ctx.blocks.straights[i].get())
                epA.neighbourId = itStA->second->getId();
        }

        // Côté epB
        auto itSwB = ctx.blocks.switchByNode.find(epB.frontierNodeId);
        if (itSwB != ctx.blocks.switchByNode.end())
            epB.neighbourId = itSwB->second->getId();
        else
        {
            auto itStB = ctx.blocks.straightByNode.find(epB.frontierNodeId);
            if (itStB != ctx.blocks.straightByNode.end()
                && itStB->second != ctx.blocks.straights[i].get())
                epB.neighbourId = itStB->second->getId();
        }
    }

    // Pour chaque SwitchBlock : résout les neighbourId des 3 branches
    for (size_t i = 0; i < ctx.blocks.switches.size(); ++i)
    {
        if (i >= ctx.blocks.switchEndpoints.size()) break;

        auto& eps = ctx.blocks.switchEndpoints[i];
        for (auto& ep : eps)
        {
            // Cherche straight d'abord, puis switch
            auto itSt = ctx.blocks.straightByNode.find(ep.frontierNodeId);
            if (itSt != ctx.blocks.straightByNode.end())
            {
                ep.neighbourId = itSt->second->getId();
                continue;
            }
            auto itSw = ctx.blocks.switchByNode.find(ep.frontierNodeId);
            if (itSw != ctx.blocks.switchByNode.end())
                ep.neighbourId = itSw->second->getId();
        }
    }

    // -------------------------------------------------------------------------
    // Passe 2 — Résolution des pointeurs depuis l'index ID → bloc*
    // -------------------------------------------------------------------------
    const auto index = buildBlockIndex(ctx.blocks);

    for (size_t i = 0; i < ctx.blocks.straights.size(); ++i)
    {
        if (i >= ctx.blocks.straightEndpoints.size()) break;
        const auto& [epA, epB] = ctx.blocks.straightEndpoints[i];
        resolveStraight(*ctx.blocks.straights[i], epA, epB, index, logger);
    }

    for (size_t i = 0; i < ctx.blocks.switches.size(); ++i)
    {
        if (i >= ctx.blocks.switchEndpoints.size()) break;
        resolveSwitch(*ctx.blocks.switches[i],
            ctx.blocks.switchEndpoints[i],
            index, logger);
    }

    ctx.endTimer(t0, "Phase9_resolve",
        ctx.blocks.totalCount(), 0);

    LOG_INFO(logger, "Pointeurs résolus.");
}


// =============================================================================
// 9b — Transfert vers TopologyRepository
// =============================================================================

void Phase9_RepositoryTransfer::transfer(PipelineContext& ctx,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t stCount = ctx.blocks.straights.size();
    const size_t swCount = ctx.blocks.switches.size();

    LOG_INFO(logger, "Transfert vers TopologyRepository — "
        + std::to_string(swCount) + " SwitchBlock(s), "
        + std::to_string(stCount) + " StraightBlock(s).");

    TopologyData& data = TopologyRepository::instance().data();

    // Vide l'ancien contenu — obligatoire avant chaque nouveau parsing
    data.clear();

    // Transfert O(1) via std::move — les adresses des blocs sont inchangées
    data.straights = std::move(ctx.blocks.straights);
    data.switches = std::move(ctx.blocks.switches);
    // Après le move : ctx.blocks.straights et ctx.blocks.switches sont vides
    // Les StraightBlock* / SwitchBlock* non-propriétaires restent valides

    // Construction des index de lookup O(1) — APRÈS le move (adresses finales)
    data.buildIndex();

    ctx.endTimer(t0, "Phase9_transfer", stCount + swCount, 0);

    // Libération des structures intermédiaires de BlockSet
    ctx.blocks.clear();

    LOG_INFO(logger, "=== Transfert terminé — "
        + std::to_string(data.switches.size()) + " SwitchBlock(s), "
        + std::to_string(data.straights.size()) + " StraightBlock(s) ===");
}


// =============================================================================
// Helpers
// =============================================================================

std::unordered_map<std::string, ShuntingElement*>
Phase9_RepositoryTransfer::buildBlockIndex(const BlockSet& blocks)
{
    std::unordered_map<std::string, ShuntingElement*> index;
    index.reserve(blocks.totalCount());

    for (const auto& st : blocks.straights)
        index[st->getId()] = st.get();

    for (const auto& sw : blocks.switches)
        index[sw->getId()] = sw.get();

    return index;
}

void Phase9_RepositoryTransfer::resolveStraight(
    StraightBlock& st,
    const BlockEndpoint& epPrev,
    const BlockEndpoint& epNext,
    const std::unordered_map<std::string, ShuntingElement*>& index,
    Logger& logger)
{
    auto resolve = [&](const BlockEndpoint& ep,
        const std::string& side) -> ShuntingElement*
        {
            if (ep.neighbourId.empty()) return nullptr;
            const auto it = index.find(ep.neighbourId);
            if (it == index.end())
            {
                LOG_WARNING(logger, st.getId() + " — voisin " + side
                    + " introuvable : " + ep.neighbourId);
                return nullptr;
            }
            return it->second;
        };

    st.setNeighbourPrev(resolve(epPrev, "prev"));
    st.setNeighbourNext(resolve(epNext, "next"));
}

void Phase9_RepositoryTransfer::resolveSwitch(
    SwitchBlock& sw,
    const std::array<BlockEndpoint, 3>& eps,
    const std::unordered_map<std::string, ShuntingElement*>& index,
    Logger& logger)
{
    auto resolve = [&](const BlockEndpoint& ep,
        const std::string& branch) -> ShuntingElement*
        {
            if (ep.neighbourId.empty()) return nullptr;
            const auto it = index.find(ep.neighbourId);
            if (it == index.end())
            {
                LOG_WARNING(logger, sw.getId() + " — branche " + branch
                    + " introuvable : " + ep.neighbourId);
                return nullptr;
            }
            return it->second;
        };

    sw.setRootPointer(resolve(eps[0], "root"));
    sw.setNormalPointer(resolve(eps[1], "normal"));
    sw.setDeviationPointer(resolve(eps[2], "deviation"));
}
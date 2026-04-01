/**
 * @file  Phase8_RepositoryTransfer.cpp
 * @brief Implémentation de la phase 9 — résolution + transfert.
 *
 * @see Phase8_RepositoryTransfer
 */
#include "Phase8_RepositoryTransfer.h"

#include "Engine/Core/Topology/TopologyRepository.h"
#include "Engine/Core/Topology/TopologyData.h"
#include "Modules/Elements/ShuntingElements/StraightBlock.h"
#include "Modules/Elements/ShuntingElements/SwitchBlock.h"

#include <unordered_map>


 // =============================================================================
 // 9a — Résolution des pointeurs
 // =============================================================================

void Phase8_RepositoryTransfer::resolve(PipelineContext& ctx,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    LOG_INFO(logger, "Résolution des pointeurs inter-blocs — "
        + std::to_string(ctx.blocks.straights.size()) + " straight(s), "
        + std::to_string(ctx.blocks.switches.size()) + " switch(es).");

    // -------------------------------------------------------------------------
    // Passe 1 — Renseignement des neighbourId des StraightBlocks
    // -------------------------------------------------------------------------
    // Les SwitchBlocks ont déjà leurs neighbourId renseignés par
    // Phase6_BlockExtractor::extractSwitches() — on ne les traite pas ici.
    // -------------------------------------------------------------------------

    for (size_t i = 0; i < ctx.blocks.straights.size(); ++i)
    {
        if (i >= ctx.blocks.straightEndpoints.size()) break;

        auto& [epA, epB] = ctx.blocks.straightEndpoints[i];

        // Côté epA — cherche switch en priorité, puis straight
        {
            auto itSw = ctx.blocks.switchByNode.find(epA.frontierNodeId);
            if (itSw != ctx.blocks.switchByNode.end())
            {
                epA.neighbourId = itSw->second->getId();
            }
            else
            {
                // Terminus ou straight adjacent — lookup dans straightsByNode
                const auto itSt = ctx.blocks.straightsByNode.find(epA.frontierNodeId);
                if (itSt != ctx.blocks.straightsByNode.end())
                {
                    for (StraightBlock* candidate : itSt->second)
                    {
                        if (candidate != ctx.blocks.straights[i].get())
                        {
                            epA.neighbourId = candidate->getId();
                            break;
                        }
                    }
                }
            }
        }

        // Côté epB
        {
            auto itSw = ctx.blocks.switchByNode.find(epB.frontierNodeId);
            if (itSw != ctx.blocks.switchByNode.end())
            {
                epB.neighbourId = itSw->second->getId();
            }
            else
            {
                const auto itSt = ctx.blocks.straightsByNode.find(epB.frontierNodeId);
                if (itSt != ctx.blocks.straightsByNode.end())
                {
                    for (StraightBlock* candidate : itSt->second)
                    {
                        if (candidate != ctx.blocks.straights[i].get())
                        {
                            epB.neighbourId = candidate->getId();
                            break;
                        }
                    }
                }
            }
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

    ctx.endTimer(t0, "Phase8_resolve", ctx.blocks.totalCount(), 0);

    LOG_INFO(logger, "Pointeurs résolus.");
}


// =============================================================================
// 9b — Transfert vers TopologyRepository
// =============================================================================

void Phase8_RepositoryTransfer::transfer(PipelineContext& ctx,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t stCount = ctx.blocks.straights.size();
    const size_t swCount = ctx.blocks.switches.size();

    LOG_INFO(logger, "Transfert vers TopologyRepository — "
        + std::to_string(swCount) + " SwitchBlock(s), "
        + std::to_string(stCount) + " StraightBlock(s).");

    for (const auto& st : ctx.blocks.straights)
        LOG_DEBUG(logger, st->toString());
    for (const auto& sw : ctx.blocks.switches)
        LOG_DEBUG(logger, sw->toString());

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

    ctx.endTimer(t0, "Phase8_transfer", stCount + swCount, 0);

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
Phase8_RepositoryTransfer::buildBlockIndex(const BlockSet& blocks)
{
    std::unordered_map<std::string, ShuntingElement*> index;
    index.reserve(blocks.totalCount());

    for (const auto& st : blocks.straights)
        index[st->getId()] = st.get();

    for (const auto& sw : blocks.switches)
        index[sw->getId()] = sw.get();

    return index;
}

void Phase8_RepositoryTransfer::resolveStraight(
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

void Phase8_RepositoryTransfer::resolveSwitch(
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

    ShuntingElement* root = resolve(eps[0], "root");
    ShuntingElement* normal = resolve(eps[1], "normal");
    ShuntingElement* deviation = resolve(eps[2], "deviation");

    sw.setRootPointer(root);
    sw.setNormalPointer(normal);
    sw.setDeviationPointer(deviation);

    // Renseigne aussi les IDs — compatibilité avec orient() et isOriented()
    if (root)      sw.setRootBranchId(eps[0].neighbourId);
    if (normal)    sw.setNormalBranchId(eps[1].neighbourId);
    if (deviation) sw.setDeviationBranchId(eps[2].neighbourId);
}
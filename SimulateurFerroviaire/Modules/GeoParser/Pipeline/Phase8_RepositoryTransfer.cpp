/**
 * @file  Phase8_RepositoryTransfer.cpp
 * @brief Implémentation de la phase 9 — résolution + transfert.
 *
 * @par Correction v2 — resolveStraight ne doit pas écraser la chaîne
 * Les sous-blocs internes produits par la subdivision (@c maxSegmentLength)
 * ont des @ref BlockEndpoint avec @c frontierNodeId == SIZE_MAX et
 * @c neighbourId vide.  L'ancien code appelait systématiquement
 * @c setNeighbourPrev(nullptr) et @c setNeighbourNext(nullptr) pour ces
 * entrées, écrasant la chaîne prev/next posée par
 * @ref Phase6_BlockExtractor::registerStraight.
 *
 * Correction : @c resolveStraight ne résout (et ne surécrit) que les
 * endpoints dont @c neighbourId est non vide.  Les pointeurs internes
 * de la chaîne sont ainsi préservés.
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
// 8a — Résolution des pointeurs
// =============================================================================

void Phase8_RepositoryTransfer::resolve(PipelineContext& ctx,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    LOG_INFO(logger, "Résolution des pointeurs inter-blocs — "
        + std::to_string(ctx.blocks.straights.size()) + " straight(s), "
        + std::to_string(ctx.blocks.switches.size()) + " switch(es), "
        + std::to_string(ctx.blocks.crossings.size()) + " crossing(s).");

    // -------------------------------------------------------------------------
    // Passe 1 — Renseignement des neighbourId des StraightBlocks
    // -------------------------------------------------------------------------
    // Les SwitchBlocks ont déjà leurs neighbourId renseignés par
    // Phase6_BlockExtractor::extractSwitches() — on ne les traite pas ici.
    // 
    // Les CrossBlocks ont déjà leurs neighbourId renseignés par
    // Phase6_BlockExtractor::extractCrossBlocks() — on ne les traite pas ici.
    //
    // Pour les sous-blocs internes (frontierNodeId == SIZE_MAX), les deux
    // lookups échoueront naturellement et neighbourId restera vide.
    // Passe 2 ne les modifiera donc pas — la chaîne est préservée.
    // -------------------------------------------------------------------------

    for (size_t i = 0; i < ctx.blocks.straights.size(); ++i)
    {
        if (i >= ctx.blocks.straightEndpoints.size()) break;

        auto& [epA, epB] = ctx.blocks.straightEndpoints[i];

        auto fillEndpoint = [&](BlockEndpoint& ep)
            {
                // Switch prioritaire
                auto itSw = ctx.blocks.switchesByNode.find(ep.frontierNodeId);
                if (itSw != ctx.blocks.switchesByNode.end())
                {
                    ep.neighbourId = itSw->second->getId();
                    return;
                }
                // Crossing
                auto itCr = ctx.blocks.crossingsByNode.find(ep.frontierNodeId);
                if (itCr != ctx.blocks.crossingsByNode.end())
                {
                    ep.neighbourId = itCr->second->getId();
                    return;
                }
                // Straight adjacent
                const auto itSt = ctx.blocks.straightsByNode.find(ep.frontierNodeId);
                if (itSt != ctx.blocks.straightsByNode.end())
                {
                    for (StraightBlock* candidate : itSt->second)
                    {
                        if (candidate != ctx.blocks.straights[i].get())
                        {
                            ep.neighbourId = candidate->getId();
                            break;
                        }
                    }
                }
            };

        fillEndpoint(epA);
        fillEndpoint(epB);
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
            ctx.blocks.switchEndpoints[i], index, logger);
    }

    for (size_t i = 0; i < ctx.blocks.crossings.size(); ++i)
    {
        if (i >= ctx.blocks.crossingEndpoints.size()) break;
        resolveCrossing(*ctx.blocks.crossings[i],
            ctx.blocks.crossingEndpoints[i], index, logger);
    }

    ctx.endTimer(t0, "Phase8_resolve", ctx.blocks.totalCount(), 0);

    LOG_INFO(logger, "Pointeurs résolus.");
}


// =============================================================================
// 8b — Transfert vers TopologyRepository
// =============================================================================

void Phase8_RepositoryTransfer::transfer(PipelineContext& ctx,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t stCount = ctx.blocks.straights.size();
    const size_t swCount = ctx.blocks.switches.size();
    const size_t crCount = ctx.blocks.crossings.size();

    LOG_INFO(logger, "Transfert vers TopologyRepository — "
        + std::to_string(swCount) + " SwitchBlock(s), "
        + std::to_string(stCount) + " StraightBlock(s), "
        + std::to_string(crCount) + " CrossBlock(s).");

    for (const auto& st : ctx.blocks.straights)
        LOG_DEBUG(logger, st->toString());
    for (const auto& sw : ctx.blocks.switches)
        LOG_DEBUG(logger, sw->toString());
    for (const auto& cr : ctx.blocks.crossings)
        LOG_DEBUG(logger, cr->toString());

    TopologyData& data = TopologyRepository::instance().data();
    data.clear();

    data.straights = std::move(ctx.blocks.straights);
    data.switches = std::move(ctx.blocks.switches);
    data.crossings = std::move(ctx.blocks.crossings);
    // Après move : les raw pointers non-propriétaires distribués restent valides.

    data.buildIndex();

    ctx.endTimer(t0, "Phase8_transfer", stCount + swCount + crCount, 0);

    ctx.blocks.clear();

    // Comptage TJD pour le log de synthèse
    size_t tjdCount = 0;
    for (const auto& cr : data.crossings)
        if (cr->isTJD()) ++tjdCount;

    LOG_INFO(logger, "=== Transfert terminé — "
        + std::to_string(data.switches.size()) + " SwitchBlock(s), "
        + std::to_string(data.straights.size()) + " StraightBlock(s), "
        + std::to_string(data.crossings.size()) + " CrossBlock(s) ("
        + std::to_string(tjdCount) + " TJD) ===");
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

    for (const auto& cr : blocks.crossings)
        index[cr->getId()] = cr.get();

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

    // Ne résout (et ne surécrit) que les endpoints dont neighbourId est non vide.
    //
    // Cas où neighbourId est vide :
    //   1. Terminus réel (nœud de degré 1) — setNeighbour à nullptr est correct,
    //      mais le sous-bloc concerné n'a pas de chaîne à préserver → OK.
    //   2. Endpoint interne d'un sous-bloc de subdivision (frontierNodeId == SIZE_MAX)
    //      — NE PAS appeler setNeighbour : cela écraserait la chaîne prev/next
    //      posée par Phase6_BlockExtractor::registerStraight.
    //
    // La distinction entre cas 1 et cas 2 est gérée implicitement : dans les deux
    // cas neighbourId reste vide après la Passe 1, et on n'appelle rien.
    // Pour le cas 1 (terminus), le pointeur était déjà nullptr à la construction.
    if (!epPrev.neighbourId.empty())
        st.setNeighbourPrev(resolve(epPrev, "prev"));

    if (!epNext.neighbourId.empty())
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

    ShuntingElement* root      = resolve(eps[0], "root");
    ShuntingElement* normal    = resolve(eps[1], "normal");
    ShuntingElement* deviation = resolve(eps[2], "deviation");

    sw.setRootPointer(root);
    sw.setNormalPointer(normal);
    sw.setDeviationPointer(deviation);

    // Renseigne aussi les IDs — compatibilité avec orient() et isOriented()
    if (root)      sw.setRootBranchId(eps[0].neighbourId);
    if (normal)    sw.setNormalBranchId(eps[1].neighbourId);
    if (deviation) sw.setDeviationBranchId(eps[2].neighbourId);
}

void Phase8_RepositoryTransfer::resolveCrossing(CrossBlock& cr,
    const std::array<BlockEndpoint, 4>& eps,
    const std::unordered_map<std::string, ShuntingElement*>& index,
    Logger& logger)
{
    auto resolve = [&](const BlockEndpoint& ep) -> ShuntingElement*
        {
            if (ep.neighbourId.empty()) return nullptr;
            const auto it = index.find(ep.neighbourId);
            if (it == index.end()) { LOG_WARNING(logger,cr.getId() + " - voisin :" 
            + ep.neighbourId + " introuvable"); return nullptr; }
            return it->second;
        };
    cr.setBranchAPointer(resolve(eps[0]));
    cr.setBranchBPointer(resolve(eps[1]));
    cr.setBranchCPointer(resolve(eps[2]));
    cr.setBranchDPointer(resolve(eps[3]));
}
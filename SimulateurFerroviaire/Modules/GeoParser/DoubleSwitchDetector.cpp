/**
 * @file  DoubleSwitchDetector.cpp
 * @brief Implémentation phases 7 et 8 — doubles aiguilles et validation CDC.
 */
#include "DoubleSwitchDetector.h"
#include <algorithm>

DoubleSwitchDetector::DoubleSwitchDetector(
    Logger& logger, std::vector<SwitchBlock>& switches,
    std::vector<StraightBlock>& straights,
    const std::unordered_map<std::string, int>& switchIdToNodeId,
    const TopologyGraph& topologyGraph,
    double doubleLinkMaxMeters, double minBranchLengthMeters)
    : m_logger(logger), m_switches(switches), m_straights(straights)
    , m_switchIdToNodeId(switchIdToNodeId), m_topologyGraph(topologyGraph)
    , m_doubleLinkMaxMeters(doubleLinkMaxMeters)
    , m_minBranchLengthMeters(minBranchLengthMeters)
{
}


// =============================================================================
// Phase 7 — Détection et absorption
// =============================================================================

void DoubleSwitchDetector::detectAndAbsorb()
{
    LOG_INFO(m_logger, "Phase 7 — Détection des doubles aiguilles");

    std::unordered_map<std::string, SwitchBlock*>   switchIndex;
    std::unordered_map<std::string, StraightBlock*> straightIndex;
    for (auto& sw : m_switches)  switchIndex[sw.getId()] = &sw;
    for (auto& st : m_straights) straightIndex[st.getId()] = &st;

    auto clusters = findDoubleSwitchClusters(switchIndex, straightIndex);
    if (clusters.empty())
    {
        LOG_INFO(m_logger, "Aucun double aiguille détecté");
        return;
    }

    LOG_INFO(m_logger, std::to_string(clusters.size()) + " cluster(s) détecté(s)");
    std::set<std::string> segmentsToRemove;

    for (auto& cluster : clusters)
    {
        // Tri : premier switch = côté avec la chaîne root la plus longue
        std::sort(cluster.begin(), cluster.end(),
            [&](const std::string& a, const std::string& b)
            {
                auto* swA = switchIndex.count(a) ? switchIndex.at(a) : nullptr;
                auto* swB = switchIndex.count(b) ? switchIndex.at(b) : nullptr;
                if (!swA || !swB) return a < b;

                const int nA = m_switchIdToNodeId.count(a) ? m_switchIdToNodeId.at(a) : -1;
                const int nB = m_switchIdToNodeId.count(b) ? m_switchIdToNodeId.at(b) : -1;

                const double lA = (swA->getRootBranchId() && nA >= 0)
                    ? estimateAccessibleChainLength(*swA->getRootBranchId(), nA, straightIndex) : 0.0;
                const double lB = (swB->getRootBranchId() && nB >= 0)
                    ? estimateAccessibleChainLength(*swB->getRootBranchId(), nB, straightIndex) : 0.0;

                return (std::abs(lA - lB) > 1e-3) ? (lA > lB) : (a < b);
            });

        for (std::size_t i = 0; i + 1 < cluster.size(); ++i)
        {
            auto* swA = switchIndex.count(cluster[i]) ? switchIndex.at(cluster[i]) : nullptr;
            auto* swB = switchIndex.count(cluster[i + 1]) ? switchIndex.at(cluster[i + 1]) : nullptr;
            if (!swA || !swB) continue;

            // Recherche du segment de liaison : présent dans les branches de swA, voisin de swB
            std::string linkId;
            for (const auto& bid : swA->getBranchIds())
            {
                if (!straightIndex.count(bid)) continue;
                const auto& nb = straightIndex.at(bid)->getNeighbourIds();
                if (std::find(nb.begin(), nb.end(), cluster[i + 1]) != nb.end())
                {
                    linkId = bid;
                    break;
                }
            }

            if (!linkId.empty())
                absorbLinkSegment(cluster[i], cluster[i + 1],
                    linkId, switchIndex, straightIndex, segmentsToRemove);
        }

        LOG_DEBUG(m_logger, "Double : " + cluster[0] + " ↔ " + cluster.back());
    }

    if (!segmentsToRemove.empty())
    {
        m_straights.erase(
            std::remove_if(m_straights.begin(), m_straights.end(),
                [&](const StraightBlock& s) { return segmentsToRemove.count(s.getId()) > 0; }),
            m_straights.end());
        LOG_INFO(m_logger,
            std::to_string(segmentsToRemove.size()) + " segment(s) absorbé(s)");
    }
}


// =============================================================================
// Recherche des clusters de doubles aiguilles
// =============================================================================

std::vector<std::vector<std::string>> DoubleSwitchDetector::findDoubleSwitchClusters(
    const std::unordered_map<std::string, SwitchBlock*>& switchIndex,
    const std::unordered_map<std::string, StraightBlock*>& straightIndex)
{
    std::unordered_map<std::string, std::set<std::string>> adjacency;
    for (const auto& [id, sw] : switchIndex)
        adjacency[id] = {};

    for (const auto& [segId, st] : straightIndex)
    {
        if (st->getLengthMeters() > m_doubleLinkMaxMeters) continue;

        std::vector<std::string> owners;
        for (const auto& [swId, sw] : switchIndex)
        {
            if (!sw->isOriented() || sw->getRootBranchId() == segId) continue;
            for (const auto& bid : sw->getBranchIds())
                if (bid == segId) { owners.push_back(swId); break; }
        }

        if (static_cast<int>(owners.size()) == NodeDegreeThresholds::CROSSOVER_SHARED_BRANCH_COUNT)
        {
            adjacency[owners[0]].insert(owners[1]);
            adjacency[owners[1]].insert(owners[0]);
        }
    }

    std::set<std::string> visited;
    std::vector<std::vector<std::string>> clusters;

    for (const auto& [startId, neighbours] : adjacency)
    {
        if (visited.count(startId) || neighbours.empty()) continue;

        std::vector<std::string> cluster;
        std::vector<std::string> queue{ startId };
        while (!queue.empty())
        {
            auto cur = queue.back(); queue.pop_back();
            if (visited.count(cur)) continue;
            visited.insert(cur);
            cluster.push_back(cur);
            for (const auto& nb : adjacency.at(cur))
                if (!visited.count(nb)) queue.push_back(nb);
        }

        if (static_cast<int>(cluster.size()) >= NodeDegreeThresholds::DOUBLE_SWITCH_MINIMUM_CLUSTER)
            clusters.push_back(std::move(cluster));
    }

    return clusters;
}


// =============================================================================
// Estimation de longueur accessible depuis la branche root
// =============================================================================

double DoubleSwitchDetector::estimateAccessibleChainLength(
    const std::string& branchId,
    int                                                    junctionNodeId,
    const std::unordered_map<std::string, StraightBlock*>& straightIndex) const
{
    auto it = straightIndex.find(branchId);
    if (it == straightIndex.end()) return 0.0;
    double total = it->second->getLengthMeters();

    std::set<int> junctionNodes;
    for (const auto& [id, nodeId] : m_switchIdToNodeId)
        junctionNodes.insert(nodeId);

    int farNode = -1;
    auto adjIt = m_topologyGraph.adjacency.find(junctionNodeId);
    if (adjIt != m_topologyGraph.adjacency.end())
        for (const auto& [nb, eid] : adjIt->second)
            if (!junctionNodes.count(nb)) { farNode = nb; break; }

    if (farNode < 0) return total;

    std::set<int> visited{ junctionNodeId };
    std::vector<int> stack{ farNode };
    while (!stack.empty())
    {
        int cur = stack.back(); stack.pop_back();
        if (visited.count(cur)) continue;
        visited.insert(cur);
        if (junctionNodes.count(cur) && cur != farNode) continue;

        auto it2 = m_topologyGraph.adjacency.find(cur);
        if (it2 == m_topologyGraph.adjacency.end()) continue;

        for (const auto& [nb, eid] : it2->second)
        {
            if (visited.count(nb)) continue;
            auto eit = m_topologyGraph.edges.find(eid);
            if (eit != m_topologyGraph.edges.end())
                total += eit->second.lengthMeters;
            stack.push_back(nb);
        }
    }

    return total;
}


// =============================================================================
// Absorption d'un segment de liaison
// =============================================================================

void DoubleSwitchDetector::absorbLinkSegment(
    const std::string& switchIdA,
    const std::string& switchIdB,
    const std::string& linkSegmentId,
    std::unordered_map<std::string, SwitchBlock*>& switchIndex,
    std::unordered_map<std::string, StraightBlock*>& straightIndex,
    std::set<std::string>& segmentsToRemove)
{
    auto* link = straightIndex.count(linkSegmentId) ? straightIndex.at(linkSegmentId) : nullptr;
    auto* swA = switchIndex.count(switchIdA) ? switchIndex.at(switchIdA) : nullptr;
    auto* swB = switchIndex.count(switchIdB) ? switchIndex.at(switchIdB) : nullptr;
    if (!link || !swA || !swB) return;

    segmentsToRemove.insert(linkSegmentId);

    // Midpoint du segment absorbé — les deux demi-doubles se rejoignent visuellement ici
    const auto& coords = link->getCoordinates();
    const LatLon midpoint = coords.empty()
        ? swA->getJunctionCoordinate()
        : coords[coords.size() / 2];

    // absorbLink : remplace le segment dans branchIds + roles + tips,
    //              et renseigne doubleOnNormal ou doubleOnDeviation
    swA->absorbLink(linkSegmentId, switchIdB, midpoint);
    swB->absorbLink(linkSegmentId, switchIdA, midpoint);

    // Mise à jour des voisins des Straights qui référençaient le segment absorbé
    for (auto& st : m_straights)
    {
        const auto& nb = st.getNeighbourIds();
        if (std::find(nb.begin(), nb.end(), linkSegmentId) == nb.end()) continue;

        st.replaceNeighbourId(linkSegmentId, switchIdA);
        st.addNeighbourId(switchIdB);
    }

    LOG_DEBUG(m_logger,
        "Absorbé : " + linkSegmentId + " (" + switchIdA + " ↔ " + switchIdB + ")");
}


// =============================================================================
// Phase 8 — Validation CDC
// =============================================================================

void DoubleSwitchDetector::validateSwitches()
{
    LOG_INFO(m_logger, "Phase 8 — Validation CDC");

    std::unordered_map<std::string, StraightBlock*> stIdx;
    for (auto& s : m_straights) stIdx[s.getId()] = &s;

    std::set<std::string> swIds;
    for (const auto& sw : m_switches) swIds.insert(sw.getId());

    int warnings = 0;

    for (const auto& sw : m_switches)
    {
        if (!sw.isOriented()) continue;

        const std::string nId = sw.getNormalBranchId().value_or("");
        const std::string dId = sw.getDeviationBranchId().value_or("");

        if (sw.isDouble())
        {
            // Exactement une des deux branches doit pointer vers le partenaire
            const int swCount = static_cast<int>(swIds.count(nId))
                + static_cast<int>(swIds.count(dId));
            if (swCount != 1)
            {
                LOG_WARNING(m_logger,
                    sw.getId() + " : double aiguille avec "
                    + std::to_string(swCount) + " branche(s) Switch (attendu : 1)");
                ++warnings;
            }
        }
        else if (!stIdx.count(nId) || !stIdx.count(dId))
        {
            LOG_WARNING(m_logger,
                sw.getId() + " : type(s) de branche invalide(s) — normal="
                + nId + ", deviation=" + dId);
            ++warnings;
            continue;
        }

        for (const auto& bid : { nId, dId })
        {
            auto it = stIdx.find(bid);
            if (it == stIdx.end()) continue;
            if (it->second->getLengthMeters() < m_minBranchLengthMeters)
            {
                LOG_WARNING(m_logger,
                    sw.getId() + " : branche " + bid + " trop courte ("
                    + std::to_string(static_cast<int>(it->second->getLengthMeters()))
                    + " m < "
                    + std::to_string(static_cast<int>(m_minBranchLengthMeters))
                    + " m min)");
                ++warnings;
            }
        }
    }

    if (warnings == 0) LOG_INFO(m_logger, "Validation CDC : aucune violation");
    else LOG_WARNING(m_logger,
        "Validation CDC : " + std::to_string(warnings) + " violation(s)");
}
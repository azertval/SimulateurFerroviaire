/**
 * @file  Phase7_SwitchProcessor.cpp
 * @brief Implémentation du traitement complet des aiguillages.
 *
 * @par Changement v3 — Support TJD (Traversée Jonction Double)
 * Ajout de la détection et de l'absorption des clusters TJD :
 *  - @c detectTJDClusters : identifie les SwitchCrossBlock entourés de 4 corners.
 *  - @c absorbTJD : applique le mapping C[A B] / D[B A] / A[C D] / B[D C].
 *  - Les paires TJD sont retirées des clusters classiques avant B1.
 *  - @c detectCrossovers exclut les switches TJD-absorbed (double sur les 2 côtés).
 *
 * @see Phase7_SwitchProcessor
 */
#include "Phase7_SwitchProcessor.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>


namespace
{
    constexpr double EARTH_R = 6371000.0;
    constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;

    double haversine(const CoordinateLatLon& a, const CoordinateLatLon& b)
    {
        const double dLat = (b.latitude - a.latitude) * DEG2RAD;
        const double dLon = (b.longitude - a.longitude) * DEG2RAD;
        const double latA = a.latitude * DEG2RAD;
        const double latB = b.latitude * DEG2RAD;
        const double sinDLat = std::sin(dLat / 2.0);
        const double sinDLon = std::sin(dLon / 2.0);
        const double hav = sinDLat * sinDLat
            + std::cos(latA) * std::cos(latB) * sinDLon * sinDLon;
        return EARTH_R * 2.0 * std::atan2(std::sqrt(hav), std::sqrt(1.0 - hav));
    }

    /**
     * @brief Oriente une polyligne depuis une jonction (reverse si nécessaire).
     *
     * Retourne une copie avec le premier point au plus proche de @p junc.
     */
    template <typename Pt>
    std::vector<Pt> orientFromJunction(std::vector<Pt> pts,
        double juncX, double juncY)
    {
        if (pts.size() < 2) return pts;
        const double dFront = std::hypot(
            static_cast<double>(pts.front().x) - juncX,
            static_cast<double>(pts.front().y) - juncY);
        const double dBack = std::hypot(
            static_cast<double>(pts.back().x) - juncX,
            static_cast<double>(pts.back().y) - juncY);
        if (dFront > dBack)
            std::reverse(pts.begin(), pts.end());
        return pts;
    }

    std::vector<CoordinateLatLon> orientWGS84(
        std::vector<CoordinateLatLon> pts, const CoordinateLatLon& junc)
    {
        if (pts.size() < 2) return pts;
        // Proxy sur latitude/longitude — pas besoin de Haversine ici, l'ordre est
        // préservé par la distance euclidienne en degrés pour des points proches.
        const double dFront = std::hypot(
            pts.front().latitude - junc.latitude,
            pts.front().longitude - junc.longitude);
        const double dBack = std::hypot(
            pts.back().latitude - junc.latitude,
            pts.back().longitude - junc.longitude);
        if (dFront > dBack)
            std::reverse(pts.begin(), pts.end());
        return pts;
    }
}


// =============================================================================
// Point d'entrée
// =============================================================================

void Phase7_SwitchProcessor::run(PipelineContext& ctx,
    const ParserConfig& config,
    Logger& logger)
{
    const auto t0 = PipelineContext::startTimer();

    const size_t swCount = ctx.blocks.switches.size();
    LOG_INFO(logger, "Traitement des switches — "
        + std::to_string(swCount) + " switch(es).");

    // G — Orientation géométrique (root/normal/deviation) — en premier
    orientBranches(ctx.blocks, logger);

    // A1 — Doubles classiques (rayon)
    auto classicClusters = detectClusters(ctx.blocks,
        config.doubleSwitchRadius,
        logger);

    // A2 — TJD clusters (via SwitchCrossBlock)
    const auto tjdClusters = detectTJDClusters(ctx.blocks, logger);

    // Filtre : retire des clusters classiques toute paire dont les 2 switches
    // sont corners d'un même TJD (elles seront absorbées en B2).
    if (!tjdClusters.empty())
    {
        const size_t before = classicClusters.size();
        classicClusters.erase(
            std::remove_if(classicClusters.begin(), classicClusters.end(),
                [&](const std::pair<SwitchBlock*, SwitchBlock*>& p)
                {
                    return isPairInTJD(tjdClusters, p.first, p.second);
                }),
            classicClusters.end());
        LOG_INFO(logger, std::to_string(before - classicClusters.size())
            + " paire(s) filtrée(s) (participant à un TJD).");
    }

    LOG_INFO(logger, std::to_string(classicClusters.size())
        + " cluster(s) classique(s), "
        + std::to_string(tjdClusters.size()) + " TJD.");

    // B1 — Absorption doubles classiques
    size_t absorbed = 0;
    for (const auto& [swA, swB] : classicClusters)
    {
        absorbLinkSegment(ctx.blocks, swA, swB, logger);
        ++absorbed;
    }
    if (absorbed > 0)
        LOG_INFO(logger, std::to_string(absorbed) + " segment(s) absorbé(s) (classique).");

    // B2 — Absorption TJD
    for (const auto& tjd : tjdClusters)
        absorbTJD(ctx.blocks, tjd, logger);

    // C — Validation CDC
    validateCDC(ctx.blocks, config.minBranchLength, logger);

    // D/E — Crossovers
    const auto crossovers = detectCrossovers(ctx.blocks, logger);
    enforceCrossoverConsistency(ctx.blocks, crossovers, logger);

    // F — Tips CDC
    computeTips(ctx.blocks, config.switchSideSize, logger);

    size_t oriented = 0;
    for (const auto& sw : ctx.blocks.switches)
        if (sw->isOriented()) ++oriented;

    ctx.endTimer(t0, "Phase7_SwitchProcessor", swCount, oriented);

    LOG_INFO(logger, std::to_string(oriented) + "/" + std::to_string(swCount)
        + " switch(es) orienté(s).");
}


// =============================================================================
// A1 — Détection des clusters double switch (inchangé)
// =============================================================================

std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
Phase7_SwitchProcessor::detectClusters(const BlockSet& blocks,
    double radius,
    Logger& logger)
{
    std::vector<std::pair<SwitchBlock*, SwitchBlock*>> clusters;
    const size_t n = blocks.switches.size();

    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            SwitchBlock* swA = blocks.switches[i].get();
            SwitchBlock* swB = blocks.switches[j].get();

            const CoordinateXY& jA = swA->getJunctionUTM();
            const CoordinateXY& jB = swB->getJunctionUTM();

            const double dist = std::hypot(jB.x - jA.x, jB.y - jA.y);

            if (dist < radius)
            {
                clusters.push_back({ swA, swB });
                LOG_DEBUG(logger, "Cluster potentiel : "
                    + swA->getId() + " ↔ " + swB->getId()
                    + " (" + std::to_string(static_cast<int>(dist)) + " m)");
            }
        }
    }

    return clusters;
}


// =============================================================================
// B1 — Recherche du segment de liaison
// =============================================================================

std::vector<StraightBlock*> Phase7_SwitchProcessor::findLinkSegments(
    const BlockSet& blocks,
    const SwitchBlock* swA,
    const SwitchBlock* swB)
{
    (void)blocks;
    auto branchesOf = [](const SwitchBlock* sw)
        -> std::array<ShuntingElement*, 3>
        {
            return { sw->getRootBlock(),
                     sw->getNormalBlock(),
                     sw->getDeviationBlock() };
        };

    const auto brA = branchesOf(swA);
    const auto brB = branchesOf(swB);

    std::vector<StraightBlock*> links;

    for (ShuntingElement* elemA : brA)
    {
        if (!elemA) continue;
        for (ShuntingElement* elemB : brB)
        {
            if (elemA != elemB) continue;
            auto* st = dynamic_cast<StraightBlock*>(elemA);
            if (st && std::find(links.begin(), links.end(), st) == links.end())
                links.push_back(st);
        }
    }
    return links;
}


// =============================================================================
// B1 — Absorption classique
// =============================================================================

void Phase7_SwitchProcessor::absorbLinkSegment(BlockSet& blocks,
    SwitchBlock* swA,
    SwitchBlock* swB,
    Logger& logger)
{
    const auto links = findLinkSegments(blocks, swA, swB);

    if (links.empty())
    {
        LOG_WARNING(logger, "Segment de liaison introuvable entre "
            + swA->getId() + " et " + swB->getId() + " — absorption ignorée.");
        return;
    }

    for (StraightBlock* link : links)
    {
        LOG_DEBUG(logger, "Absorption " + link->getId() + " : "
            + std::to_string(link->getPointsUTM().size()) + " point(s), "
            + swA->getId() + " ↔ " + swB->getId());

        const auto wgs84 = link->getPointsWGS84();
        const auto utm = link->getPointsUTM();

        swA->absorbLink(link->getId(), swB->getId(), wgs84, utm);
        swB->absorbLink(link->getId(), swA->getId(), wgs84, utm);

        swA->replaceBranchPointer(link, swB);
        swB->replaceBranchPointer(link, swA);

        const auto idxIt = std::find_if(
            blocks.straights.begin(), blocks.straights.end(),
            [link](const std::unique_ptr<StraightBlock>& s)
            { return s.get() == link; });

        if (idxIt != blocks.straights.end())
        {
            const size_t idx = static_cast<size_t>(
                idxIt - blocks.straights.begin());
            blocks.straightEndpoints.erase(
                blocks.straightEndpoints.begin() + idx);
        }

        LOG_DEBUG(logger, "Absorbé : " + link->getId()
            + "  " + swA->getId() + " ↔ " + swB->getId());

        blocks.straights.erase(
            std::remove_if(blocks.straights.begin(), blocks.straights.end(),
                [link](const std::unique_ptr<StraightBlock>& s)
                { return s.get() == link; }),
            blocks.straights.end());

        blocks.rebuildStraightIndex();
    }
}


// =============================================================================
// A2 — Détection des clusters TJD
// =============================================================================

std::vector<Phase7_SwitchProcessor::TJDCluster>
Phase7_SwitchProcessor::detectTJDClusters(const BlockSet& blocks, Logger& logger)
{
    std::vector<TJDCluster> tjds;

    for (const auto& cbUp : blocks.crossings)
    {
        CrossBlock* cb = cbUp.get();
        if (!cb->isTJD()) continue;

        auto* scb = static_cast<SwitchCrossBlock*>(cb);

        // Les 4 branches (A=idx0, B=idx1, C=idx2, D=idx3) pointent vers des
        // StraightBlock* courts après Phase 8 resolve().
        std::array<ShuntingElement*, 4> brElem = {
            cb->getBranchA(), cb->getBranchB(),
            cb->getBranchC(), cb->getBranchD()
        };

        TJDCluster t;
        t.cross = scb;
        bool valid = true;

        for (size_t i = 0; i < 4; ++i)
        {
            auto* st = dynamic_cast<StraightBlock*>(brElem[i]);
            if (!st)
            {
                LOG_WARNING(logger, "TJD " + cb->getId()
                    + " — branche " + std::to_string(i)
                    + " n'est pas un StraightBlock — TJD ignoré.");
                valid = false;
                break;
            }
            t.shortLinks[i] = st;

            // Le SwitchBlock* est à l'extrémité opposée au crossing.
            // StraightBlock::prev/next pointent vers les voisins chaînés.
            ShuntingElement* prev = st->getNeighbourPrev();
            ShuntingElement* next = st->getNeighbourNext();

            SwitchBlock* sw = nullptr;
            if (prev && prev != cb)
                sw = dynamic_cast<SwitchBlock*>(prev);
            if (!sw && next && next != cb)
                sw = dynamic_cast<SwitchBlock*>(next);

            if (!sw)
            {
                LOG_WARNING(logger, "TJD " + cb->getId()
                    + " — branche " + std::to_string(i)
                    + " (" + st->getId() + ") ne mène pas à un SwitchBlock — TJD ignoré.");
                valid = false;
                break;
            }
            t.corners[i] = sw;
        }

        if (!valid) continue;

        // --- Identifie les 2 same-side-links ---
        // A↔B (voie entrée), C↔D (voie sortie) : cherche dans les branches
        // des corners un straight dont l'autre extrémité est le partenaire.
        auto findLinkBetween = [](const SwitchBlock* s1, const SwitchBlock* s2)
            -> StraightBlock*
            {
                for (ShuntingElement* elem : { s1->getRootBlock(),
                                                s1->getNormalBlock(),
                                                s1->getDeviationBlock() })
                {
                    if (!elem) continue;
                    auto* st = dynamic_cast<StraightBlock*>(elem);
                    if (!st) continue;
                    const ShuntingElement* p = st->getNeighbourPrev();
                    const ShuntingElement* n = st->getNeighbourNext();
                    if (p == s2 || n == s2) return st;
                }
                return nullptr;
            };

        t.sameSideAB = findLinkBetween(t.corners[0], t.corners[1]);
        t.sameSideCD = findLinkBetween(t.corners[2], t.corners[3]);

        tjds.push_back(t);

        LOG_DEBUG(logger, "TJD cluster " + cb->getId()
            + " : A=" + t.corners[0]->getId()
            + " B=" + t.corners[1]->getId()
            + " C=" + t.corners[2]->getId()
            + " D=" + t.corners[3]->getId()
            + " sameAB=" + (t.sameSideAB ? t.sameSideAB->getId() : "—")
            + " sameCD=" + (t.sameSideCD ? t.sameSideCD->getId() : "—"));
    }

    return tjds;
}


// =============================================================================
// A2 helper — teste l'appartenance d'une paire à un TJD
// =============================================================================

bool Phase7_SwitchProcessor::isPairInTJD(
    const std::vector<TJDCluster>& tjds,
    SwitchBlock* swA, SwitchBlock* swB)
{
    for (const auto& t : tjds)
    {
        const bool aIn = (swA == t.corners[0] || swA == t.corners[1]
            || swA == t.corners[2] || swA == t.corners[3]);
        const bool bIn = (swB == t.corners[0] || swB == t.corners[1]
            || swB == t.corners[2] || swB == t.corners[3]);
        if (aIn && bIn) return true;
    }
    return false;
}


// =============================================================================
// B2 — Absorption TJD
// =============================================================================

void Phase7_SwitchProcessor::absorbTJD(BlockSet& blocks,
    const TJDCluster& tjd,
    Logger& logger)
{
    // --- Mapping des partenaires (C[A B], D[B A], A[C D], B[D C]) ---
    // corners : [0]=A  [1]=B  [2]=C  [3]=D
    // Pour chaque corner : { normalPartnerIdx, deviationPartnerIdx }
    static constexpr std::array<std::pair<int, int>, 4> partnerMap = {
        std::make_pair(2, 3),  // A : normal=C, deviation=D
        std::make_pair(3, 2),  // B : normal=D, deviation=C
        std::make_pair(0, 1),  // C : normal=A, deviation=B
        std::make_pair(1, 0)   // D : normal=B, deviation=A
    };

    // --- 1. Absorbe les short-links dans chaque corner switch ---
    for (size_t i = 0; i < 4; ++i)
    {
        SwitchBlock*   sw        = tjd.corners[i];
        StraightBlock* shortLink = tjd.shortLinks[i];

        const auto [nIdx, dIdx] = partnerMap[i];
        SwitchBlock* normalPartner    = tjd.corners[nIdx];
        SwitchBlock* deviationPartner = tjd.corners[dIdx];

        // Détermine le same-side-link à supprimer de ce switch :
        //  - corners A(0) et B(1) partagent sameSideAB
        //  - corners C(2) et D(3) partagent sameSideCD
        StraightBlock* sameSide = (i <= 1) ? tjd.sameSideAB : tjd.sameSideCD;
        const std::string sameSideId = sameSide ? sameSide->getId() : "";

        // Polyligne du short-link orientée depuis la jonction de ce switch
        auto wgs84 = orientWGS84(shortLink->getPointsWGS84(), sw->getJunctionWGS84());
        auto utm   = orientFromJunction(shortLink->getPointsUTM(),
                                         sw->getJunctionUTM().x,
                                         sw->getJunctionUTM().y);

        LOG_DEBUG(logger, "TJD absorb : " + sw->getId()
            + " short=" + shortLink->getId()
            + " sameSide=" + (sameSideId.empty() ? "—" : sameSideId)
            + " normal→" + normalPartner->getId()
            + " deviation→" + deviationPartner->getId());

        sw->absorbTJD(shortLink->getId(),
                      sameSideId,
                      normalPartner->getId(),
                      deviationPartner->getId(),
                      std::move(wgs84),
                      std::move(utm));

        // Met à jour les pointeurs résolus sur le switch
        sw->setNormalPointer(normalPartner);
        sw->setDeviationPointer(deviationPartner);
    }

    // --- 2. Met à jour les pointeurs du CrossBlock (A/B/C/D → switches) ---
    tjd.cross->setBranchAPointer(tjd.corners[0]);
    tjd.cross->setBranchBPointer(tjd.corners[1]);
    tjd.cross->setBranchCPointer(tjd.corners[2]);
    tjd.cross->setBranchDPointer(tjd.corners[3]);

    // --- 2bis. Met à jour crossingEndpoints[idx].neighbourId → switch IDs ---
    // Retrouve l'index du crossing dans blocks.crossings pour accéder aux endpoints.
    const auto crIt = std::find_if(blocks.crossings.begin(), blocks.crossings.end(),
        [&](const std::unique_ptr<CrossBlock>& c) { return c.get() == tjd.cross; });
    if (crIt != blocks.crossings.end())
    {
        const size_t crIdx = static_cast<size_t>(crIt - blocks.crossings.begin());
        if (crIdx < blocks.crossingEndpoints.size())
        {
            auto& eps = blocks.crossingEndpoints[crIdx];
            eps[0].neighbourId = tjd.corners[0]->getId();
            eps[1].neighbourId = tjd.corners[1]->getId();
            eps[2].neighbourId = tjd.corners[2]->getId();
            eps[3].neighbourId = tjd.corners[3]->getId();
        }
    }

    // --- 3. Collecte les straights à supprimer (4 short-links + 2 same-side) ---
    std::unordered_set<StraightBlock*> toRemove;
    for (StraightBlock* st : tjd.shortLinks) if (st) toRemove.insert(st);
    if (tjd.sameSideAB) toRemove.insert(tjd.sameSideAB);
    if (tjd.sameSideCD) toRemove.insert(tjd.sameSideCD);

    // --- 4. Supprime de straightEndpoints (en parallèle aux straights) ---
    // Parcours à l'envers pour éviter l'invalidation des indices.
    for (size_t i = blocks.straights.size(); i-- > 0;)
    {
        if (toRemove.count(blocks.straights[i].get()))
        {
            if (i < blocks.straightEndpoints.size())
                blocks.straightEndpoints.erase(blocks.straightEndpoints.begin() + i);
        }
    }

    // --- 5. Supprime les unique_ptr (détruit les StraightBlock) ---
    blocks.straights.erase(
        std::remove_if(blocks.straights.begin(), blocks.straights.end(),
            [&toRemove](const std::unique_ptr<StraightBlock>& s)
            { return toRemove.count(s.get()) > 0; }),
        blocks.straights.end());

    // --- 6. Reconstruit les index straight ---
    blocks.rebuildStraightIndex();

    LOG_INFO(logger, "TJD absorbé : " + tjd.cross->getId()
        + " — 4 short-link(s) + "
        + std::to_string((tjd.sameSideAB ? 1 : 0) + (tjd.sameSideCD ? 1 : 0))
        + " same-side-link(s) supprimés.");
}


// =============================================================================
// C — Validation CDC
// =============================================================================

void Phase7_SwitchProcessor::validateCDC(const BlockSet& blocks,
    double minLength,
    Logger& logger)
{
    LOG_INFO(logger, "Validation CDC — longueur min = "
        + std::to_string(static_cast<int>(minLength)) + " m.");

    int violations = 0;

    for (const auto& sw : blocks.switches)
    {
        auto checkBranch = [&](const ShuntingElement* elem,
            const std::string& branchName)
            {
                if (!elem) return;
                const auto* st = dynamic_cast<const StraightBlock*>(elem);
                if (!st) return;

                const double len = st->getLengthUTM();
                if (len < minLength)
                {
                    LOG_WARNING(logger, sw->getId()
                        + " : branche " + branchName
                        + " trop courte ("
                        + std::to_string(static_cast<int>(len))
                        + " m < "
                        + std::to_string(static_cast<int>(minLength))
                        + " m min)");
                    ++violations;
                }
            };

        checkBranch(sw->getRootBlock(), "root");
        checkBranch(sw->getNormalBlock(), "normal");
        checkBranch(sw->getDeviationBlock(), "deviation");
    }

    if (violations > 0)
        LOG_WARNING(logger, "Validation CDC : "
            + std::to_string(violations) + " violation(s).");
    else
        LOG_INFO(logger, "Validation CDC : OK.");
}


// =============================================================================
// D — Détection des crossovers
// =============================================================================

std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
Phase7_SwitchProcessor::detectCrossovers(const BlockSet& blocks,
    Logger& logger)
{
    std::vector<std::pair<SwitchBlock*, SwitchBlock*>> crossovers;
    const size_t n = blocks.switches.size();

    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            SwitchBlock* swA = blocks.switches[i].get();
            SwitchBlock* swB = blocks.switches[j].get();

            // Skip TJD-absorbed pairs — les 4 corners d'un TJD ont tous
            // doubleOnNormal() ET doubleOnDeviation() renseignés, ce qui ferait
            // matcher les paires diagonales avec (nA == dB && dA == nB) en faux
            // crossover. Cf. note de detectCrossovers dans le .h.
            const bool aFullDouble = swA->getDoubleOnNormal().has_value()
                                  && swA->getDoubleOnDeviation().has_value();
            const bool bFullDouble = swB->getDoubleOnNormal().has_value()
                                  && swB->getDoubleOnDeviation().has_value();
            if (aFullDouble && bFullDouble) continue;

            ShuntingElement* nA = swA->getNormalBlock();
            ShuntingElement* dA = swA->getDeviationBlock();
            ShuntingElement* nB = swB->getNormalBlock();
            ShuntingElement* dB = swB->getDeviationBlock();

            if (!nA || !dA || !nB || !dB) continue;

            const bool sameSet = (nA == nB && dA == dB)
                || (nA == dB && dA == nB);

            if (!sameSet) continue;

            crossovers.push_back({ swA, swB });
            LOG_DEBUG(logger, "Crossover : " + swA->getId()
                + " ↔ " + swB->getId());
        }
    }

    LOG_DEBUG(logger, std::to_string(crossovers.size())
        + " crossover(s) détecté(s).");
    return crossovers;
}


// =============================================================================
// E — Cohérence des crossovers
// =============================================================================

void Phase7_SwitchProcessor::enforceCrossoverConsistency(
    BlockSet& blocks,
    const std::vector<std::pair<SwitchBlock*, SwitchBlock*>>& crossovers,
    Logger& logger)
{
    (void)blocks;
    for (const auto& [swA, swB] : crossovers)
    {
        ShuntingElement* nA = swA->getNormalBlock();
        ShuntingElement* dA = swA->getDeviationBlock();
        ShuntingElement* nB = swB->getNormalBlock();

        ShuntingElement* sharedNormal = (nA == nB) ? nA : dA;
        if (!sharedNormal) continue;

        if (nA == sharedNormal)
        {
            swA->swapNormalDeviation();
            LOG_DEBUG(logger, swA->getId() + " — branche "
                + sharedNormal->getId() + " forcée en DEVIATION.");
        }
        if (nB == sharedNormal)
        {
            swB->swapNormalDeviation();
            LOG_DEBUG(logger, swB->getId() + " — branche "
                + sharedNormal->getId() + " forcée en DEVIATION.");
        }
    }
}


// =============================================================================
// G — Orientation géométrique root/normal/deviation
// =============================================================================

void Phase7_SwitchProcessor::orientBranches(BlockSet& blocks, Logger& logger)
{
    for (const auto& sw : blocks.switches)
    {
        std::array<ShuntingElement*, 3> elems = {
            sw->getRootBlock(),
            sw->getNormalBlock(),
            sw->getDeviationBlock()
        };

        int validCount = 0;
        for (auto* e : elems) if (e) ++validCount;
        if (validCount < 3)
        {
            LOG_DEBUG(logger, sw->getId()
                + " — orientation ignorée (branches incomplètes).");
            continue;
        }

        std::array<CoordinateXY, 3> vecs;
        for (size_t i = 0; i < 3; ++i)
            vecs[i] = branchVector(*sw, elems[i]);

        int rootIdx = 0;
        double minScore = std::numeric_limits<double>::max();

        for (int i = 0; i < 3; ++i)
        {
            const int j = (i + 1) % 3;
            const int k = (i + 2) % 3;

            CoordinateXY resultant{ vecs[j].x + vecs[k].x,
                                     vecs[j].y + vecs[k].y };
            const double rLen = std::hypot(resultant.x, resultant.y);
            if (rLen < 1e-9) continue;
            resultant.x /= rLen;
            resultant.y /= rLen;

            const double score = vecs[i].x * resultant.x
                + vecs[i].y * resultant.y;

            if (score < minScore)
            {
                minScore = score;
                rootIdx = i;
            }
        }

        const int idxA = (rootIdx + 1) % 3;
        const int idxB = (rootIdx + 2) % 3;

        const double dotA = vecs[rootIdx].x * vecs[idxA].x
            + vecs[rootIdx].y * vecs[idxA].y;
        const double dotB = vecs[rootIdx].x * vecs[idxB].x
            + vecs[rootIdx].y * vecs[idxB].y;

        const int normalIdx = (dotA < dotB) ? idxA : idxB;
        const int deviationIdx = (dotA < dotB) ? idxB : idxA;

        ShuntingElement* newRoot = elems[rootIdx];
        ShuntingElement* newNormal = elems[normalIdx];
        ShuntingElement* newDeviation = elems[deviationIdx];

        sw->setRootPointer(newRoot);
        sw->setNormalPointer(newNormal);
        sw->setDeviationPointer(newDeviation);

        if (newRoot)      sw->setRootBranchId(newRoot->getId());
        if (newNormal)    sw->setNormalBranchId(newNormal->getId());
        if (newDeviation) sw->setDeviationBranchId(newDeviation->getId());

        LOG_DEBUG(logger, sw->getId()
            + " — root=" + (newRoot ? newRoot->getId() : "null")
            + " normal=" + (newNormal ? newNormal->getId() : "null")
            + " deviation=" + (newDeviation ? newDeviation->getId() : "null"));
    }
}

CoordinateXY Phase7_SwitchProcessor::branchVector(const SwitchBlock& sw,
    const ShuntingElement* elem)
{
    if (!elem) return { 0.0, 0.0 };

    const CoordinateXY& junc = sw.getJunctionUTM();

    const auto* st = dynamic_cast<const StraightBlock*>(elem);
    if (st && st->getPointsUTM().size() >= 2)
    {
        const CoordinateXY& front = st->getPointsUTM().front();
        const CoordinateXY& back = st->getPointsUTM().back();

        const double dFront = std::hypot(front.x - junc.x, front.y - junc.y);
        const double dBack = std::hypot(back.x - junc.x, back.y - junc.y);

        const CoordinateXY& origin = (dFront <= dBack) ? front : back;
        const CoordinateXY& next = (dFront <= dBack)
            ? st->getPointsUTM()[1]
            : st->getPointsUTM()[st->getPointsUTM().size() - 2];

        const double dx = next.x - origin.x;
        const double dy = next.y - origin.y;
        const double len = std::hypot(dx, dy);
        if (len < 1e-9) return { 0.0, 0.0 };
        return { dx / len, dy / len };
    }

    const auto* swPartner = dynamic_cast<const SwitchBlock*>(elem);
    if (swPartner)
    {
        const CoordinateXY& pJunc = swPartner->getJunctionUTM();
        const double dx = pJunc.x - junc.x;
        const double dy = pJunc.y - junc.y;
        const double len = std::hypot(dx, dy);
        if (len < 1e-9) return { 0.0, 0.0 };
        return { dx / len, dy / len };
    }

    return { 0.0, 0.0 };
}


// =============================================================================
// F — Calcul des tips CDC
// =============================================================================

void Phase7_SwitchProcessor::computeTips(BlockSet& blocks,
    double sideSize,
    Logger& logger)
{
    auto hasTip = [](const ShuntingElement* branch) -> bool
        {
            return branch && branch->getType() == ElementType::STRAIGHT;
        };

    for (const auto& sw : blocks.switches)
    {
        if (!sw->isOriented()) continue;

        const CoordinateLatLon& junctionWGS84 = sw->getJunctionWGS84();
        const CoordinateXY& junctionUTM = sw->getJunctionUTM();

        const ShuntingElement* root = sw->getRootBlock();
        const ShuntingElement* normal = sw->getNormalBlock();
        const ShuntingElement* deviation = sw->getDeviationBlock();

        auto makeTipWGS84 = [&](const ShuntingElement* elem)
            -> std::optional<CoordinateLatLon>
            {
                if (!hasTip(elem)) return std::nullopt;
                const auto* st = static_cast<const StraightBlock*>(elem);
                if (st->getPointsWGS84().size() < 2) return std::nullopt;
                return interpolateTip(st->getPointsWGS84(), junctionWGS84, sideSize);
            };

        auto makeTipUTM = [&](const ShuntingElement* elem)
            -> std::optional<CoordinateXY>
            {
                if (!hasTip(elem)) return std::nullopt;
                const auto* st = static_cast<const StraightBlock*>(elem);
                if (st->getPointsUTM().size() < 2) return std::nullopt;
                return interpolateTipUTM(st->getPointsUTM(), junctionUTM, sideSize);
            };

        sw->setTips(
            makeTipWGS84(root),
            makeTipWGS84(normal),
            makeTipWGS84(deviation));

        sw->setTipsUTM(
            makeTipUTM(root),
            makeTipUTM(normal),
            makeTipUTM(deviation));

        sw->computeTotalLength();

        LOG_DEBUG(logger, sw->getId() + " — tips calculés"
            + " root=" + (hasTip(root) ? "✓" : "—")
            + " normal=" + (hasTip(normal) ? "✓" : "—")
            + " deviation=" + (hasTip(deviation) ? "✓" : "—")
            + " (" + std::to_string(static_cast<int>(sideSize)) + " m).");
    }
}

CoordinateLatLon Phase7_SwitchProcessor::interpolateTip(
    const std::vector<CoordinateLatLon>& pts,
    const CoordinateLatLon& junction,
    double targetDist)
{
    if (pts.size() < 2) return junction;

    const double distFront = haversine(pts.front(), junction);
    const double distBack = haversine(pts.back(), junction);
    const bool   fromFront = (distFront <= distBack);

    const int start = fromFront ? 0 : static_cast<int>(pts.size()) - 1;
    const int step = fromFront ? 1 : -1;
    const int end = fromFront ? static_cast<int>(pts.size()) : -1;

    double           accumulated = 0.0;
    CoordinateLatLon prev = pts[static_cast<size_t>(start)];

    for (int i = start + step; i != end; i += step)
    {
        const CoordinateLatLon& curr = pts[static_cast<size_t>(i)];
        const double            segLen = haversine(prev, curr);

        if (accumulated + segLen >= targetDist)
        {
            const double t = (targetDist - accumulated) / segLen;
            return {
                prev.latitude + t * (curr.latitude - prev.latitude),
                prev.longitude + t * (curr.longitude - prev.longitude)
            };
        }

        accumulated += segLen;
        prev = curr;
    }

    return fromFront ? pts.back() : pts.front();
}

CoordinateXY Phase7_SwitchProcessor::interpolateTipUTM(
    const std::vector<CoordinateXY>& pts,
    const CoordinateXY& junctionUTM,
    double targetDist)
{
    if (pts.size() < 2) return junctionUTM;

    const double dFront = std::hypot(
        pts.front().x - junctionUTM.x,
        pts.front().y - junctionUTM.y);
    const double dBack = std::hypot(
        pts.back().x - junctionUTM.x,
        pts.back().y - junctionUTM.y);
    const bool fromFront = (dFront <= dBack);

    const int start = fromFront ? 0 : static_cast<int>(pts.size()) - 1;
    const int step = fromFront ? 1 : -1;

    double accumulated = 0.0;
    int i = start;

    while (true)
    {
        const int next = i + step;
        if (next < 0 || next >= static_cast<int>(pts.size()))
            return pts[static_cast<std::size_t>(i)];

        const double segLen = std::hypot(
            pts[static_cast<std::size_t>(next)].x - pts[static_cast<std::size_t>(i)].x,
            pts[static_cast<std::size_t>(next)].y - pts[static_cast<std::size_t>(i)].y);

        if (accumulated + segLen >= targetDist)
        {
            const double t = (targetDist - accumulated) / segLen;
            return CoordinateXY{
                pts[static_cast<std::size_t>(i)].x
                    + t * (pts[static_cast<std::size_t>(next)].x
                         - pts[static_cast<std::size_t>(i)].x),
                pts[static_cast<std::size_t>(i)].y
                    + t * (pts[static_cast<std::size_t>(next)].y
                         - pts[static_cast<std::size_t>(i)].y)
            };
        }
        accumulated += segLen;
        i = next;
    }
}

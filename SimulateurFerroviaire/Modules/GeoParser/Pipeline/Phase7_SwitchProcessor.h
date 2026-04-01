/**
 * @file  Phase7_SwitchProcessor.h
 * @brief Phase 7 du pipeline — traitement complet des aiguillages.
 *
 * Fusion de l'ancien @c Phase7_DoubleSwitchDetector et @c Phase8_SwitchOrientator.
 *
 * @par Sous-phases
 *  - G : Orientation géométrique root/normal/deviation (heuristique vecteurs UTM).
 *  - A : Détection des clusters double switch.
 *  - B : Absorption du segment de liaison.
 *  - C : Validation CDC.
 *  - D : Détection des crossovers.
 *  - E : Cohérence des crossovers.
 *  - F : Calcul des tips CDC (@c config.switchSideSize depuis la jonction).
 *
 * @par Ordre d'appel dans GeoParser::parse()
 * @code
 * Phase8_RepositoryTransfer::resolve(ctx, logger);
 * Phase7_SwitchProcessor::run(ctx, config, logger);
 * Phase8_RepositoryTransfer::transfer(ctx, logger);
 * @endcode
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase7_SwitchProcessor
{
public:

    /**
     * @brief Exécute le traitement complet des aiguillages.
     *
     * Enchaîne G → A → B → C → D → E → F.
     *
     * @param ctx     Contexte pipeline. Lit et modifie @c ctx.blocks.
     * @param config  Configuration — utilise @c doubleSwitchRadius,
     *                @c minBranchLength, @c switchSideSize.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase7_SwitchProcessor() = delete;

private:

    // =========================================================================
    // A/B/C — Doubles aiguilles
    // =========================================================================

    /**
     * @brief A — Détecte les paires de switches formant un double switch.
     */
    static std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
        detectClusters(const BlockSet& blocks, double radius, Logger& logger);

    /**
     * @brief B — Trouve le StraightBlock de liaison entre deux switches.
     */
    static StraightBlock* findLinkSegment(const BlockSet& blocks,
        const SwitchBlock* swA,
        const SwitchBlock* swB);

    /**
     * @brief B — Absorbe le segment de liaison d'un cluster.
     */
    static void absorbLinkSegment(BlockSet& blocks,
        SwitchBlock* swA,
        SwitchBlock* swB,
        Logger& logger);

    /**
     * @brief C — Valide les longueurs de branches selon les critères CDC.
     */
    static void validateCDC(const BlockSet& blocks,
        double minLength,
        Logger& logger);

    // =========================================================================
    // D/E — Crossovers
    // =========================================================================

    /**
     * @brief D — Détecte les paires de switches en crossover.
     */
    static std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
        detectCrossovers(const BlockSet& blocks, Logger& logger);

    /**
     * @brief E — Force la cohérence des crossovers (branches partagées → DEVIATION).
     */
    static void enforceCrossoverConsistency(
        BlockSet& blocks,
        const std::vector<std::pair<SwitchBlock*, SwitchBlock*>>& crossovers,
        Logger& logger);

    // =========================================================================
    // G — Orientation géométrique root/normal/deviation
    // =========================================================================

    /**
     * @brief G — Oriente les 3 branches de chaque switch par heuristique géométrique.
     *
     * Algorithme :
     *  -# Pour chaque switch, calcule les 3 vecteurs UTM (jonction → premier point).
     *  -# Root = branche dont le vecteur est le plus opposé à la résultante
     *     normalisée des deux autres (dot product minimal).
     *  -# Normal = des deux restantes, celle dont l'angle avec root est le plus
     *     proche de 180° (continuation directe de la voie principale).
     *  -# Deviation = la troisième (branche déviée).
     *
     * @param blocks  Ensemble des blocs.
     * @param logger  Référence au logger.
     */
    static void orientBranches(BlockSet& blocks, Logger& logger);

    /**
     * @brief Calcule le vecteur UTM unitaire de la jonction vers une branche.
     *
     * @param sw    Switch source.
     * @param elem  Bloc de la branche.
     *
     * @return Vecteur unitaire UTM. {0,0} si indéterminé.
     */
    static CoordinateXY branchVector(const SwitchBlock& sw,
        const ShuntingElement* elem);

    // =========================================================================
    // F — Tips CDC
    // =========================================================================

    /**
     * @brief F — Calcule les tips CDC des 3 branches de chaque switch.
     *
     * Interpole le long de la géométrie WGS84 de chaque branche à
     * @c config.switchSideSize mètres depuis la jonction.
     *
     * @param blocks    Ensemble des blocs.
     * @param sideSize  Distance tip depuis la jonction (mètres Haversine).
     * @param logger    Référence au logger.
     */
    static void computeTips(BlockSet& blocks,
        double sideSize,
        Logger& logger);

    /**
     * @brief Interpole un point WGS84 sur une polyligne à @p targetDist
     *        mètres depuis l'extrémité la plus proche de @p junction.
     *
     * @param pts        Polyligne WGS84 du StraightBlock.
     * @param junction   Position de la jonction — détermine le sens de parcours.
     * @param targetDist Distance cible en mètres.
     *
     * @return Point WGS84 interpolé, ou extrémité distale si branche trop courte.
     */
    static CoordinateLatLon interpolateTip(
        const std::vector<CoordinateLatLon>& pts,
        const CoordinateLatLon& junction,
        double targetDist);
};
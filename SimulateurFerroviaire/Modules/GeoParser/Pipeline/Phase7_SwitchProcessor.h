/**
 * @file  Phase7_SwitchProcessor.h
 * @brief Phase 7 du pipeline — traitement complet des aiguillages.
 *
 * Fusion de l'ancien @c Phase7_DoubleSwitchDetector et @c Phase8_SwitchOrientator.
 *
 * @par Sous-phases
 *  - G  : Orientation géométrique root/normal/deviation (heuristique vecteurs UTM).
 *  - A1 : Détection des clusters double switch « classiques ».
 *  - A2 : Détection des clusters TJD (4 switches autour d'un SwitchCrossBlock).
 *  - B1 : Absorption du segment de liaison des doubles classiques.
 *  - B2 : Absorption TJD — short-links et same-side-links ; normal + deviation doubles.
 *  - C  : Validation CDC.
 *  - D  : Détection des crossovers (TJD-absorbed switches exclus).
 *  - E  : Cohérence des crossovers.
 *  - F  : Calcul des tips CDC (@c config.switchSideSize depuis la jonction).
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
#include "Modules/Elements/ShuntingElements/CrossBlocks/CrossBlock.h"
#include "Modules/Elements/ShuntingElements/CrossBlocks/SwitchCrossBlock.h"

#include <array>

class Phase7_SwitchProcessor
{
public:

    /**
     * @brief Exécute le traitement complet des aiguillages.
     *
     * Enchaîne G → A1/A2 → B1/B2 → C → D → E → F.
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
    // A1/B1 — Doubles aiguilles classiques
    // =========================================================================

    /**
     * @brief A1 — Détecte les paires de switches proches (candidats double switch).
     */
    static std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
        detectClusters(const BlockSet& blocks, double radius, Logger& logger);

    /**
     * @brief B1 — Trouve le(s) StraightBlock(s) de liaison entre deux switches.
     */
    static std::vector<StraightBlock*> findLinkSegments(const BlockSet& blocks,
        const SwitchBlock* swA,
        const SwitchBlock* swB);

    /**
     * @brief B1 — Absorbe le segment de liaison d'un cluster classique.
     */
    static void absorbLinkSegment(BlockSet& blocks,
        SwitchBlock* swA,
        SwitchBlock* swB,
        Logger& logger);

    // =========================================================================
    // A2/B2 — TJD (Traversée Jonction Double)
    // =========================================================================

    /**
     * @struct TJDCluster
     * @brief Descripteur d'un cluster TJD.
     *
     * Regroupe un @ref SwitchCrossBlock avec ses 4 corner switches, ses 4
     * short-links vers le crossing, et ses 2 same-side-links reliant les
     * paires adjacentes (A↔B et C↔D).
     *
     * @par Mapping des rôles (mémorisé depuis la partition Phase 6)
     *  - @c corners[0] = A (voie 1, entrée)
     *  - @c corners[1] = B (voie 2, entrée)
     *  - @c corners[2] = C (voie 1, sortie)
     *  - @c corners[3] = D (voie 2, sortie)
     *
     * @par Mapping des doubles après absorption (Q3=A — diagonale = NORMAL)
     *  - A.normal=C, A.deviation=D
     *  - B.normal=D, B.deviation=C
     *  - C.normal=A, C.deviation=B
     *  - D.normal=B, D.deviation=A
     */
    struct TJDCluster
    {
        SwitchCrossBlock*              cross = nullptr;
        std::array<SwitchBlock*,   4>  corners{};      ///< [A, B, C, D]
        std::array<StraightBlock*, 4>  shortLinks{};   ///< [A, B, C, D]
        StraightBlock*                 sameSideAB = nullptr; ///< Lien A↔B (paire voie 1/2 entrée)
        StraightBlock*                 sameSideCD = nullptr; ///< Lien C↔D (paire voie 1/2 sortie)
    };

    /**
     * @brief A2 — Détecte les clusters TJD dans @c blocks.crossings.
     *
     * Pour chaque @ref SwitchCrossBlock (isTJD() == true) :
     *  -# Les 4 branches A/B/C/D pointent (via Phase 8 resolve) vers des StraightBlock*
     *     courts (s/18, s/33, s/35, s/20 dans le cas exemple).
     *  -# Remonte via @c NeighbourPrev/Next de chaque short-link jusqu'au SwitchBlock*
     *     à l'autre extrémité.
     *  -# Cherche les 2 same-side-links (ex. s/17, s/32) : straights reliant A↔B et C↔D
     *     via leurs branches (root/normal/deviation).
     */
    static std::vector<TJDCluster> detectTJDClusters(const BlockSet& blocks, Logger& logger);

    /**
     * @brief B2 — Absorbe un cluster TJD.
     *
     * Actions :
     *  -# Appelle @c SwitchBlock::absorbTJD sur chacun des 4 corners avec le mapping
     *     C[A B], D[B A], A[C D], B[D C] — normal=diagonale, deviation=same-côté.
     *  -# Met à jour @c cross->setBranchXPointer(corners[i]) pour chaque rôle X=A/B/C/D
     *     afin que le CrossBlock référence désormais les switches.
     *  -# Met à jour @c crossingEndpoints[...].neighbourId vers les IDs de switches.
     *  -# Supprime les 4 short-links et les 2 same-side-links de @c blocks.straights
     *     et @c blocks.straightEndpoints.
     *  -# Appelle @c blocks.rebuildStraightIndex().
     */
    static void absorbTJD(BlockSet& blocks, const TJDCluster& tjd, Logger& logger);

    /**
     * @brief Teste si une paire (swA, swB) participe à un TJD (en tant que corners).
     *
     * Utilisé pour filtrer les paires TJD hors des clusters classiques avant
     * l'absorption B1.
     */
    static bool isPairInTJD(const std::vector<TJDCluster>& tjds,
                             SwitchBlock* swA, SwitchBlock* swB);

    // =========================================================================
    // C — Validation CDC
    // =========================================================================

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
     *
     * @par TJD skip (v3)
     * Les paires dont les 2 switches ont @c doubleOnNormal() ET @c doubleOnDeviation()
     * tous deux renseignés sont ignorées — elles correspondent à des corners TJD déjà
     * traités en B2 et créeraient des faux crossovers.
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
     */
    static CoordinateLatLon interpolateTip(
        const std::vector<CoordinateLatLon>& pts,
        const CoordinateLatLon& junction,
        double targetDist);

    /**
     * @brief Interpole un point UTM sur une polyligne à @p targetDist mètres
     *        depuis l'extrémité la plus proche de @p junctionUTM.
     */
    static CoordinateXY interpolateTipUTM(
        const std::vector<CoordinateXY>& pts,
        const CoordinateXY& junctionUTM,
        double targetDist);
};

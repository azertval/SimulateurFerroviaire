/**
 * @file  Phase7_DoubleSwitchDetector.h
 * @brief Phase 8 du pipeline — détection et absorption des doubles aiguilles.
 *
 * @par Sous-phases
 *  - 8a : Détection des clusters (distance jonction < @c doubleSwitchRadius).
 *  - 8b : Absorption du segment de liaison entre les deux switches.
 *  - 8c : Validation CDC — WARNING si branche < @c minBranchLength.
 *
 * @par Différences avec l'ancien DoubleSwitchDetector
 *  - Lit @c ctx.blocks au lieu de @c TopologyRepository.
 *  - Utilise @c config.doubleSwitchRadius et @c config.minBranchLength
 *    au lieu de constantes hardcodées.
 *  - Signature @c run(ctx, config, logger) uniforme.
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase7_DoubleSwitchDetector
{
public:

    /**
     * @brief Exécute la phase 8 — détection et absorption des doubles aiguilles.
     *
     * Enchaîne les sous-phases 8a → 8b → 8c sur @c ctx.blocks.
     *
     * @param ctx     Contexte pipeline. Lit et modifie blocks.
     * @param config  Configuration — utilise doubleSwitchRadius et minBranchLength.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase7_DoubleSwitchDetector() = delete;

private:

    /**
     * @brief 8a — Détecte les paires de switches formant un double switch.
     *
     * @param blocks  Ensemble des blocs.
     * @param radius  Rayon de détection en mètres UTM.
     * @param logger  Référence au logger.
     *
     * @return Liste des paires (swA*, swB*) détectées.
     */
    static std::vector<std::pair<SwitchBlock*, SwitchBlock*>>
        detectClusters(const BlockSet& blocks, double radius, Logger& logger);

    /**
     * @brief 8b — Trouve le StraightBlock de liaison entre deux switches.
     *
     * Le segment de liaison est le StraightBlock dont les deux extrémités
     * frontières sont les jonctions de @p swA et @p swB.
     *
     * @param blocks  Ensemble des blocs.
     * @param swA     Premier switch du cluster.
     * @param swB     Second switch du cluster.
     *
     * @return Pointeur non-propriétaire vers le segment de liaison,
     *         ou @c nullptr si introuvable.
     */
    static StraightBlock* findLinkSegment(const BlockSet& blocks,
        const SwitchBlock* swA,
        const SwitchBlock* swB);

    /**
     * @brief 8b — Absorbe le segment de liaison d'un cluster.
     *
     * Stocke les coordonnées dans les deux switches, met à jour les
     * pointeurs de branches, et supprime le segment de @c blocks.straights.
     *
     * @param blocks  Ensemble des blocs — modifié en place.
     * @param swA     Premier switch.
     * @param swB     Second switch.
     * @param logger  Référence au logger.
     */
    static void absorbLinkSegment(BlockSet& blocks,
        SwitchBlock* swA,
        SwitchBlock* swB,
        Logger& logger);

    /**
     * @brief 8c — Valide les longueurs de branches selon les critères CDC.
     *
     * Logue un WARNING pour chaque branche plus courte que @p minLength.
     * Ne bloque pas le parsing.
     *
     * @param blocks     Ensemble des blocs.
     * @param minLength  Longueur minimale de branche en mètres.
     * @param logger     Référence au logger.
     */
    static void validateCDC(const BlockSet& blocks,
        double minLength,
        Logger& logger);
};
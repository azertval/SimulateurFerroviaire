/**
 * @file  Phase3_NetworkSplitter.h
 * @brief Phase 3 du pipeline — découpe des segments aux intersections.
 *
 * Responsabilité unique : produire @ref SplitNetwork depuis
 * @ref RawNetwork et @ref IntersectionData.
 *
 * Chaque segment est découpé :
 *  -# Aux points d'intersection détectés en Phase 2 (paramètres t).
 *  -# Aux longueurs dépassant @c ParserConfig::maxSegmentLength.
 *
 * Libère @c ctx.rawNetwork et @c ctx.intersections en fin d'exécution.
 *
 * @par Ce que Phase3 ne fait PAS
 *  - Elle ne fusionne pas les extrémités proches (@ref Phase4_TopologyBuilder).
 *  - Elle ne classifie pas les nœuds (@ref Phase5_SwitchClassifier).
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase3_NetworkSplitter
{
public:

    /**
     * @brief Exécute la phase 3.
     *
     * Découpe les segments de @c ctx.rawNetwork aux intersections de
     * @c ctx.intersections et aux longueurs max, puis écrit le résultat
     * dans @c ctx.splitNetwork.
     * Libère @c ctx.rawNetwork et @c ctx.intersections en fin d'exécution.
     *
     * @param ctx     Contexte pipeline. Lit rawNetwork + intersections,
     *                écrit splitNetwork. Libère rawNetwork + intersections.
     * @param config  Configuration — utilise @c maxSegmentLength et
     *                @c intersectionEpsilon.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase3_NetworkSplitter() = delete;

private:

    /**
     * @brief Collecte, trie et dédoublonne les paramètres de découpe d'un segment.
     *
     * Récupère les t depuis @c ctx.intersections pour le segment global @p globalIdx,
     * ajoute les bornes 0.0 et 1.0, trie, dédoublonne, et filtre les micro-gaps.
     *
     * @param ctx         Contexte pipeline.
     * @param globalIdx   Index global du segment.
     * @param segLen      Longueur du segment en mètres UTM.
     * @param epsilon     Tolérance pour le filtrage des micro-segments.
     *
     * @return Vecteur trié de paramètres t ∈ [0,1], sans doublons, sans micro-gaps.
     */
    static std::vector<double> collectCutParams(const PipelineContext& ctx,
        size_t globalIdx,
        double segLen,
        double epsilon);

    /**
     * @brief Interpolation linéaire d'un point UTM sur un segment.
     *
     * @param A  Premier point UTM.
     * @param B  Second point UTM.
     * @param t  Paramètre ∈ [0,1].
     *
     * @return Point UTM interpolé.
     */
    static CoordinateXY interpolateUTM(const CoordinateXY& A,
        const CoordinateXY& B,
        double t);

    /**
     * @brief Interpolation linéaire d'un point WGS84 sur un segment.
     *
     * Approximation linéaire valable pour des segments < 10 km.
     *
     * @param A  Premier point WGS84.
     * @param B  Second point WGS84.
     * @param t  Paramètre ∈ [0,1].
     *
     * @return Point WGS84 interpolé.
     */
    static LatLon interpolateWGS84(const LatLon& A, const LatLon& B, double t);

    /**
     * @brief Subdivise un sous-segment si sa longueur dépasse @p maxLen.
     *
     * Produit ⌈length / maxLen⌉ portions de longueur égale et les appende
     * à @p out.
     *
     * @param A          Extrémité A UTM.
     * @param Ageo       Extrémité A WGS84.
     * @param B          Extrémité B UTM.
     * @param Bgeo       Extrémité B WGS84.
     * @param maxLen     Longueur maximale autorisée (mètres UTM).
     * @param parentIdx  Indice de la polyligne parente.
     * @param out        Vecteur de sortie — les portions sont appendées.
     */
    static void subdivideLong(const CoordinateXY& A, const LatLon& Ageo,
        const CoordinateXY& B, const LatLon& Bgeo,
        double maxLen,
        size_t parentIdx,
        std::vector<AtomicSegment>& out);
};
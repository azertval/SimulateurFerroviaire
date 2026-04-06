/**
 * @file  Phase5_SwitchClassifier.h
 * @brief Phase 5 du pipeline — classification topologique des nœuds.
 *
 * Responsabilité unique : attribuer une @ref NodeClass à chaque nœud du
 * @ref TopologyGraph en combinant degré et angle entre arêtes sortantes.
 *
 * @par Critères
 *  - ISOLATED  : degré == 0
 *  - TERMINUS  : degré == 1
 *  - STRAIGHT  : degré == 2, angle ≥ (180° - minSwitchAngle)
 *  - SWITCH    : degré == 3, max angle entre paires ≥ minSwitchAngle
 *  - CROSSING  : degré == 4
 *  - AMBIGUOUS : autre
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PipelineContext.h"
#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class Phase5_SwitchClassifier
{
public:

    /**
     * @brief Exécute la phase 5.
     *
     * Lit @c ctx.topoGraph, classe chaque nœud et écrit le résultat
     * dans @c ctx.classifiedNodes.
     *
     * @param ctx     Contexte pipeline. Lit topoGraph, écrit classifiedNodes.
     * @param config  Configuration — utilise @c minSwitchAngle.
     * @param logger  Référence au logger GeoParser.
     */
    static void run(PipelineContext& ctx,
        const ParserConfig& config,
        Logger& logger);

    Phase5_SwitchClassifier() = delete;

private:

    /**
     * @brief Calcule le vecteur sortant depuis un nœud via une arête.
     *
     * Utilise la position UTM du nœud opposé — indépendant de SplitNetwork.
     * Précision suffisante car les segments ont été découpés par maxSegmentLength.
     *
     * @param graph    Graphe topologique.
     * @param nodeId   Nœud de départ.
     * @param edgeIdx  Indice de l'arête dans @c graph.edges.
     *
     * @return Vecteur UTM sortant. {0,0} si arête invalide.
     */
    static CoordinateXY outVector(const TopologyGraph& graph,
                                   size_t nodeId,
                                   size_t edgeIdx);

    /**
     * @brief Calcule l'angle en degrés entre deux vecteurs UTM.
     *
     * @param u  Premier vecteur UTM.
     * @param v  Second vecteur UTM.
     *
     * @return Angle en degrés ∈ [0°, 180°]. 0° si un vecteur est nul.
     */
    static double angleBetween(const CoordinateXY& u, const CoordinateXY& v);

    /**
     * @brief Classifie un nœud de degré 2 — indépendant de SplitNetwork.
     *
     * @param graph          Graphe topologique.
     * @param nodeId         Nœud à classifier.
     * @param minSwitchAngle Angle minimal de bifurcation (degrés).
     *
     * @return @c NodeClass::STRAIGHT ou @c NodeClass::AMBIGUOUS.
     */
    static NodeClass classifyDegree2(const TopologyGraph& graph,
                                      size_t nodeId,
                                      double minSwitchAngle);

    /**
     * @brief Classifie un nœud de degré 3 — indépendant de SplitNetwork.
     *
     * @param graph          Graphe topologique.
     * @param nodeId         Nœud à classifier.
     * @param minSwitchAngle Angle minimal de bifurcation (degrés).
     *
     * @return @c NodeClass::SWITCH ou @c NodeClass::AMBIGUOUS.
     */
    static NodeClass classifyDegree3(const TopologyGraph& graph,
                                      size_t nodeId,
                                      double minSwitchAngle);

    /**
     * @brief Classifie un nœud de degré 4 — vérifie la colinéarité des paires.
     *
     * Teste les 3 partitions possibles des 4 arêtes en 2 paires.La partition
     * retenue est celle qui maximise la somme des angles entre branches de chaque
     * paire(critère de colinéarité).Si le score dépasse le seuil
     * (180° - minSwitchAngle) × 2, le nœud est classifié CROSSING.
     * Sinon, AMBIGUOUS.
     *
     * @param graph          Graphe topologique.
     * @param nodeId         Nœud à classifier.
     * @param minSwitchAngle Angle minimal de bifurcation(degrés).
     *
     * @return @c NodeClass::CROSSING ou @c NodeClass::AMBIGUOUS.
     */
    static NodeClass classifyDegree4(const TopologyGraph & graph,
        size_t nodeId,
        double minSwitchAngle);
};
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
     * Utilise le premier point intermédiaire du segment adjacent au nœud
     * pour approximer la tangente locale (meilleure précision qu'un vecteur
     * vers l'autre extrémité).
     *
     * @param graph    Graphe topologique.
     * @param split    Réseau splité — source des points intermédiaires.
     * @param nodeId   Nœud de départ.
     * @param edgeIdx  Indice de l'arête dans @c graph.edges.
     *
     * @return Vecteur UTM sortant. {0,0} si arête dégénérée.
     */
    static CoordinateXY outVector(const TopologyGraph& graph,
        const SplitNetwork& split,
        size_t nodeId,
        size_t edgeIdx);

    /**
     * @brief Calcule l'angle en degrés entre deux vecteurs UTM.
     *
     * Utilise le produit scalaire. Clamp sur [-1, 1] avant acos
     * pour éviter les NaN dus aux erreurs flottantes.
     *
     * @param u  Premier vecteur UTM.
     * @param v  Second vecteur UTM.
     *
     * @return Angle en degrés ∈ [0°, 180°]. 0° si un vecteur est nul.
     */
    static double angleBetween(const CoordinateXY& u, const CoordinateXY& v);

    /**
     * @brief Classifie un nœud de degré 2.
     *
     * STRAIGHT si l'angle entre les deux arêtes ≥ (180° - minSwitchAngle).
     * AMBIGUOUS sinon.
     *
     * @param graph          Graphe topologique.
     * @param split          Réseau splité.
     * @param nodeId         Nœud à classifier.
     * @param minSwitchAngle Angle minimal de bifurcation (degrés).
     *
     * @return @c NodeClass::STRAIGHT ou @c NodeClass::AMBIGUOUS.
     */
    static NodeClass classifyDegree2(const TopologyGraph& graph,
        const SplitNetwork& split,
        size_t nodeId,
        double minSwitchAngle);

    /**
     * @brief Classifie un nœud de degré 3.
     *
     * SWITCH si au moins une paire d'arêtes forme un angle ≥ minSwitchAngle.
     * AMBIGUOUS sinon.
     *
     * @param graph          Graphe topologique.
     * @param split          Réseau splité.
     * @param nodeId         Nœud à classifier.
     * @param minSwitchAngle Angle minimal de bifurcation (degrés).
     *
     * @return @c NodeClass::SWITCH ou @c NodeClass::AMBIGUOUS.
     */
    static NodeClass classifyDegree3(const TopologyGraph& graph,
        const SplitNetwork& split,
        size_t nodeId,
        double minSwitchAngle);
};
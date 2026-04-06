#pragma once

#include "PCCNode.h"
#include "Modules/Elements/ShuntingElements/CrossBlocks/CrossBlock.h"
#include "Modules/Elements/ShuntingElements/CrossBlocks/SwitchCrossBlock.h"

/**
 * @file  PCCCrossingNode.h
 * @brief Nœud PCC représentant un CrossBlock (croisement plat ou TJD).
 *
 * Expose quatre slots d'arêtes (A, B, C, D) pour un accès O(1)
 * depuis TCORenderer sans parcours de PCCNode::getEdges().
 *
 * @par Accès typé TJD
 * Si getCrossingSource()->isTJD(), on peut caster en SwitchCrossBlock*
 * pour accéder à isPath1/2Active().
 */
class PCCCrossingNode final : public PCCNode
{
public:

    /**
     * @brief Construit un nœud PCC depuis un CrossBlock.
     * @throws std::invalid_argument Si source est nullptr.
     */
    explicit PCCCrossingNode(CrossBlock* source, Logger& logger);

    [[nodiscard]] PCCNodeType getNodeType() const override
    {
        return PCCNodeType::CROSSING;
    }

    /**
     * @brief Accès typé au CrossBlock source (jamais nullptr).
     */
    [[nodiscard]] CrossBlock* getCrossingSource() const { return m_crossSource; }

    // =========================================================================
    // Slots d'arêtes A/B/C/D — accès direct O(1)
    // =========================================================================

    [[nodiscard]] PCCEdge* getEdgeA() const { return m_edgeA; }
    [[nodiscard]] PCCEdge* getEdgeB() const { return m_edgeB; }
    [[nodiscard]] PCCEdge* getEdgeC() const { return m_edgeC; }
    [[nodiscard]] PCCEdge* getEdgeD() const { return m_edgeD; }

    void setEdgeA(PCCEdge* e) { m_edgeA = e; }
    void setEdgeB(PCCEdge* e) { m_edgeB = e; }
    void setEdgeC(PCCEdge* e) { m_edgeC = e; }
    void setEdgeD(PCCEdge* e) { m_edgeD = e; }

    // =========================================================================
    // Navigation — voie traversante
    // =========================================================================

    /**
     * @brief Retourne l'arête de sortie correspondant à une arête d'entrée.
     *
     * Utilisé par PCCLayout::runBFS pour ne propager que sur la voie
     * traversante correcte (A↔C ou B↔D).
     *
     * @param entry  Arête d'arrivée au nœud crossing.
     * @return Arête de sortie sur la même voie, ou nullptr si inconnue.
     */
    [[nodiscard]] PCCEdge* getExitEdgeFor(const PCCEdge* entry) const;

private:
    CrossBlock* m_crossSource = nullptr;

    // Non-propriétaires — owned par PCCGraph::m_edges
    PCCEdge* m_edgeA = nullptr;
    PCCEdge* m_edgeB = nullptr;
    PCCEdge* m_edgeC = nullptr;
    PCCEdge* m_edgeD = nullptr;

    // Compteur d'assignation — pour le dispatch dans PCCGraph::addEdge()
    int m_edgeCount = 0;
};
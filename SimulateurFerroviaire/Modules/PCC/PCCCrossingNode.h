#pragma once

#include "PCCNode.h"
#include "Modules/Elements/ShuntingElements/CrossBlocks/CrossBlock.h"

/**
 * @file  PCCCrossingNode.h
 * @brief Nœud PCC représentant un CrossBlock (croisement plat ou TJD).
 *
 * Expose quatre slots d'arêtes (A, B, C, D) pour un accès O(1)
 * depuis TCORenderer sans parcours de PCCNode::getEdges().
 *
 * @par Navigation voie traversante
 * getExitEdgeFor(entry) retourne l'arête opposée sur la même voie
 * (arrivée via A → sortie via C, arrivée via B → sortie via D).
 * Utilisé par PCCLayout::runBFS pour ne propager que sur la voie correcte.
 */
class PCCCrossingNode final : public PCCNode
{
public:

    /**
     * @brief Construit un nœud PCC depuis un CrossBlock.
     *
     * @param source  Pointeur non-propriétaire. Doit rester valide.
     * @param logger  Référence au logger HMI.
     *
     * @throws std::invalid_argument Si source est nullptr.
     */
    explicit PCCCrossingNode(CrossBlock* source, Logger& logger);

    // =========================================================================
    // Interface PCCNode
    // =========================================================================

    /**
     * @brief Retourne PCCNodeType::CROSSING.
     */
    [[nodiscard]] PCCNodeType getNodeType() const override
    {
        return PCCNodeType::CROSSING;
    }

    // =========================================================================
    // Accesseur typé
    // =========================================================================

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
     * Voie 1 : A ↔ C   |   Voie 2 : B ↔ D
     * Utilisé par PCCLayout::runBFS pour ne propager que sur la voie correcte.
     *
     * @param entry  Arête d'arrivée au nœud crossing (depuis un voisin).
     * @return Arête de sortie sur la même voie, ou nullptr si inconnue.
     */
    [[nodiscard]] PCCEdge* getExitEdgeFor(const PCCEdge* entry) const;

    // =========================================================================
    // Compteur d'assignation — dispatch dans PCCGraph::addEdge()
    // =========================================================================

    /**
     * @brief Assigne la prochaine arête dans l'ordre A→B→C→D.
     * Appelé exclusivement par PCCGraph::addEdge().
     */
    void assignNextEdge(PCCEdge* e);

private:

    CrossBlock* m_crossSource = nullptr;

    // Non-propriétaires — owned par PCCGraph::m_edges
    PCCEdge* m_edgeA = nullptr;
    PCCEdge* m_edgeB = nullptr;
    PCCEdge* m_edgeC = nullptr;
    PCCEdge* m_edgeD = nullptr;

    // Compteur pour assignNextEdge() — 0→A, 1→B, 2→C, 3→D
    int m_edgeCount = 0;
};
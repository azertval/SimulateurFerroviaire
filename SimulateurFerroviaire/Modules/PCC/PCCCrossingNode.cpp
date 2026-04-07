/**
 * @file  PCCCrossingNode.cpp
 * @brief Implémentation du nœud PCC CrossBlock.
 * @see PCCCrossingNode
 */
#include "PCCCrossingNode.h"
#include <stdexcept>


PCCCrossingNode::PCCCrossingNode(CrossBlock* source, Logger& logger)
    : PCCNode(source->getId(), source, logger)
    , m_crossSource(source)
{
    if (!source)
        throw std::invalid_argument("PCCCrossingNode : source nullptr");
}


PCCEdge* PCCCrossingNode::getExitEdgeFor(const PCCEdge* entry) const
{
    // Voie 1 : A ↔ C
    if (entry == m_edgeA) return m_edgeC;
    if (entry == m_edgeC) return m_edgeA;

    // Voie 2 : B ↔ D
    if (entry == m_edgeB) return m_edgeD;
    if (entry == m_edgeD) return m_edgeB;

    return nullptr;
}


void PCCCrossingNode::assignNextEdge(PCCEdge* e)
{
    switch (m_edgeCount++)
    {
    case 0: m_edgeA = e; break;
    case 1: m_edgeB = e; break;
    case 2: m_edgeC = e; break;
    case 3: m_edgeD = e; break;
    default: break;  // Plus de 4 arêtes — ne devrait pas arriver
    }
}

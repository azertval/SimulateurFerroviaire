/**
 * @file  PCCCrossingNode.cpp
 * @brief Implémentation du nœud PCC pour croisement.
 *
 * @see PCCCrossingNode
 */
#include "PCCCrossingNode.h"
#include <stdexcept>

PCCCrossingNode::PCCCrossingNode(CrossBlock* source, Logger& logger)
    : PCCNode(source ? source->getId() : "", source, logger)
    , m_crossSource(source)
{
    if (!source)
    {
        LOG_ERROR(m_logger, "source ne peut pas être nullptr.");
        throw std::invalid_argument("PCCStraightNode — source ne peut pas être nullptr.");
    }

    LOG_DEBUG(m_logger, "PCCSwitchNode créé : " + source->getId());
}

PCCEdge* PCCCrossingNode::getExitEdgeFor(const PCCEdge* entry) const
{
    if (entry == m_edgeA) return m_edgeC;
    if (entry == m_edgeC) return m_edgeA;
    if (entry == m_edgeB) return m_edgeD;
    if (entry == m_edgeD) return m_edgeB;
    LOG_ERROR(m_logger, "getExitEdgeFor — arête d'entrée inconnue pour le nœud " + getSourceId());
    return nullptr;
}
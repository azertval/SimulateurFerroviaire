/**
 * @file  PCCNode.cpp
 * @brief Implémentation du nœud abstrait du graphe PCC.
 *
 * @see PCCNode
 */
#include "PCCNode.h"
#include <stdexcept>

PCCNode::PCCNode(std::string sourceId, ShuntingElement* source, Logger& logger)
    : m_sourceId(std::move(sourceId))
    , m_source(source)
    , m_logger(logger)
{
    if (!source)
    {
        LOG_ERROR(m_logger, "Source ne peut pas être nullptr.");
        throw std::invalid_argument("PCCNode — source ne peut pas être nullptr.");
    }        
}

void PCCNode::addEdge(PCCEdge* edge)
{
    if (edge)
    {
        m_edges.push_back(edge);
        LOG_DEBUG(m_logger, m_sourceId + " — arête ajoutée vers "
            + edge->getTo()->getSourceId());
    }
    else
    {
        // Politique défensive : nullptr ignoré silencieusement.
        // addEdge est un helper interne appelé par PCCGraphBuilder, dont les
        // préconditions sont vérifiées en amont — un throw ici serait redondant.
        LOG_WARNING(m_logger, m_sourceId + " — addEdge appelé avec nullptr, ignoré.");
    }   
}
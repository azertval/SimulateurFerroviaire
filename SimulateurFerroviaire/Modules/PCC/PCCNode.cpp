/**
 * @file  PCCNode.cpp
 * @brief Implémentation du nœud abstrait du graphe PCC.
 *
 * @see PCCNode
 */
#include "PCCNode.h"
#include <stdexcept>

PCCNode::PCCNode(std::string sourceId, ShuntingElement* source)
    : m_sourceId(std::move(sourceId))
    , m_source(source)
{
    if (!source)
        throw std::invalid_argument("PCCNode — source ne peut pas être nullptr.");
}

void PCCNode::addEdge(PCCEdge* edge)
{
    if (edge)
        m_edges.push_back(edge);
    // Politique défensive : nullptr ignoré silencieusement.
    // addEdge est un helper interne appelé par PCCGraphBuilder, dont les
    // préconditions sont vérifiées en amont — un throw ici serait redondant.
}
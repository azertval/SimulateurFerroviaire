/**
 * @file  PCCSwitchNode.cpp
 * @brief Implémentation du nœud PCC pour aiguillage.
 *
 * @see PCCSwitchNode
 */
#include "PCCSwitchNode.h"
#include <stdexcept>

PCCSwitchNode::PCCSwitchNode(SwitchBlock* source)
    : PCCNode(source ? source->getId() : "", source)
    , m_switchSource(source)
{
    if (!source)
        throw std::invalid_argument("PCCSwitchNode — source ne peut pas être nullptr.");
}
/**
 * @file  PCCSwitchNode.cpp
 * @brief Implémentation du nœud PCC pour aiguillage.
 *
 * @see PCCSwitchNode
 */
#include "PCCSwitchNode.h"
#include <stdexcept>

PCCSwitchNode::PCCSwitchNode(SwitchBlock* source, Logger& logger)
    : PCCNode(source ? source->getId() : "", source, logger)
    , m_switchSource(source)
{
    if (!source)
    {
        LOG_ERROR(m_logger, "source ne peut pas être nullptr.");
        throw std::invalid_argument("PCCStraightNode — source ne peut pas être nullptr.");
    }

    LOG_DEBUG(m_logger, "PCCSwitchNode créé : " + source->getId());
}
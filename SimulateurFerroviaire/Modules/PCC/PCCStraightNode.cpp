/**
 * @file  PCCStraightNode.cpp
 * @brief Implémentation du nœud PCC pour voie droite.
 *
 * @see PCCStraightNode
 */
#include "PCCStraightNode.h"
#include <stdexcept>

PCCStraightNode::PCCStraightNode(StraightBlock* source, Logger& logger)
    : PCCNode(source ? source->getId() : "", source, logger)
    , m_straightSource(source)
{
    if (!source)
    {
        LOG_ERROR(m_logger, "source ne peut pas être nullptr.");
        throw std::invalid_argument("PCCStraightNode — source ne peut pas être nullptr.");
    }
        
    LOG_DEBUG(m_logger, "PCCStraightNode créé : " + source->getId());
}
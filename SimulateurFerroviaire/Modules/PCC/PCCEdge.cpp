/**
 * @file  PCCEdge.cpp
 * @brief Implémentation de l'arête orientée du graphe PCC.
 *
 * @see PCCEdge
 */
#include "PCCEdge.h"
#include "PCCNode.h"
#include <stdexcept>

PCCEdge::PCCEdge(PCCNode* from, PCCNode* to, PCCEdgeRole role, Logger& logger)
    : m_from(from)
    , m_to(to)
    , m_role(role)
    , m_logger(logger)
{
    if (!from || !to)
    {
        LOG_ERROR(m_logger, "From ou to est nullptr.");
        throw std::invalid_argument("From et to ne peuvent pas être nullptr.");
    }

    LOG_DEBUG(m_logger, "PCCEdge créée : "
        + from->getSourceId() + " → " + to->getSourceId());
}
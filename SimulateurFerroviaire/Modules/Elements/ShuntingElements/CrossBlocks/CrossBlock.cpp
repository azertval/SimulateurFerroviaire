/**
 * @file  CrossBlock.cpp
 * @brief Implémentation de la classe de base abstraite CrossBlock.
 *
 * @see CrossBlock
 */
#include "CrossBlock.h"

#include <sstream>


 // =============================================================================
 // Requêtes
 // =============================================================================

std::string CrossBlock::toString() const
{
    std::ostringstream s;

    s << "CrossBlock(id=" << m_id
        << ", kind=" << (isTJD() ? "TJD" : "FLAT")
        << ", junction=("
        << m_junctionWGS84.latitude << ", "
        << m_junctionWGS84.longitude << ")"
        << ", A=" << (m_branchA ? m_branchA->getId() : "null")
        << ", B=" << (m_branchB ? m_branchB->getId() : "null")
        << ", C=" << (m_branchC ? m_branchC->getId() : "null")
        << ", D=" << (m_branchD ? m_branchD->getId() : "null")
        << ")";

    return s.str();
}
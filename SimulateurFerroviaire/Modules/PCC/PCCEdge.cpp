/**
 * @file  PCCEdge.cpp
 * @brief Implémentation de l'arête orientée du graphe PCC.
 *
 * @see PCCEdge
 */
#include "PCCEdge.h"
#include "PCCNode.h"
#include <stdexcept>

PCCEdge::PCCEdge(PCCNode* from, PCCNode* to, PCCEdgeRole role)
    : m_from(from)   // Member initializer list (MIL) — construit directement
    , m_to(to)       // plus efficace qu'une affectation dans le corps du constructeur
    , m_role(role)
{
    if (!from || !to)
        throw std::invalid_argument("PCCEdge — from et to ne peuvent pas être nullptr.");
    // Vérification après la MIL — convention : initialiser dans la MIL, vérifier dans le corps.
    // Pour des pointeurs, l'ordre n'a pas d'impact pratique mais améliore la lisibilité.
}
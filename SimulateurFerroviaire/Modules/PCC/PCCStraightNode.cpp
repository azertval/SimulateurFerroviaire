/**
 * @file  PCCStraightNode.cpp
 * @brief Implémentation du nœud PCC pour voie droite.
 *
 * @see PCCStraightNode
 */
#include "PCCStraightNode.h"
#include <stdexcept>

PCCStraightNode::PCCStraightNode(StraightBlock* source)
    : PCCNode(source ? source->getId() : "", source)
    // Appel du constructeur de base en premier dans la MIL — obligatoire.
    // L'expression ternaire évite un déréférencement nullptr avant que
    // PCCNode::PCCNode lève l'exception (source == nullptr).
    , m_straightSource(source)
{
    if (!source)
        throw std::invalid_argument("PCCStraightNode — source ne peut pas être nullptr.");
}
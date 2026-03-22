/**
 * @file  PCCGraph.cpp
 * @brief Implémentation du conteneur propriétaire du graphe PCC.
 *
 * @see PCCGraph
 */
#include "PCCGraph.h"
#include <stdexcept>


 // =============================================================================
 // Construction du graphe
 // =============================================================================

PCCNode* PCCGraph::addStraightNode(StraightBlock* source)
{
    // make_unique : alloue + construit PCCStraightNode sans new nu
    // Le constructeur de PCCStraightNode lève std::invalid_argument si source == nullptr
    auto node = std::make_unique<PCCStraightNode>(source);

    PCCNode* raw = node.get();
    // ^ Récupérer le raw pointer AVANT le std::move.
    //   Après move, node.get() == nullptr — raw reste le seul accès au nœud.

    m_index[source->getId()] = raw;
    // ^ Indexer AVANT le move — source->getId() est encore accessible.

    m_nodes.push_back(std::move(node));
    // ^ Transfère l'ownership vers m_nodes.
    //   unique_ptr ne pouvant être copié, std::move est obligatoire.

    return raw;
    // ^ Retourne un pointeur d'observation — valide tant que PCCGraph est en vie.
}

PCCNode* PCCGraph::addSwitchNode(SwitchBlock* source)
{
    auto node = std::make_unique<PCCSwitchNode>(source);

    PCCNode* raw = node.get();
    m_index[source->getId()] = raw;
    m_nodes.push_back(std::move(node));

    return raw;
}

PCCEdge* PCCGraph::addEdge(PCCNode* from, PCCNode* to, PCCEdgeRole role)
{
    // PCCEdge::PCCEdge lève std::invalid_argument si from ou to == nullptr
    auto edge = std::make_unique<PCCEdge>(from, to, role);
    PCCEdge* raw = edge.get();

    // Câblage sur les deux nœuds — chacun observe l'arête (non-propriétaire)
    from->addEdge(raw);
    to->addEdge(raw);

    // Si from est un PCCSwitchNode, enregistrer aussi l'arête dans le slot dédié
    // pour un accès direct O(1) depuis TCORenderer (sans parcourir getEdges())
    if (auto* sw = dynamic_cast<PCCSwitchNode*>(from))
    {
        switch (role)
        {
        case PCCEdgeRole::ROOT:      sw->setRootEdge(raw);      break;
        case PCCEdgeRole::NORMAL:    sw->setNormalEdge(raw);    break;
        case PCCEdgeRole::DEVIATION: sw->setDeviationEdge(raw); break;
        default: break;
        }
    }

    m_edges.push_back(std::move(edge));
    return raw;
}

// =============================================================================
// Lookup
// =============================================================================

PCCNode* PCCGraph::findNode(const std::string& sourceId) const
{
    // find() et non operator[] — operator[] insèrerait une entrée vide
    // si la clé est absente, ce qui modifierait la map (interdit sur un const this).
    const auto it = m_index.find(sourceId);
    return (it != m_index.end()) ? it->second : nullptr;
}


// =============================================================================
// Remise à zéro
// =============================================================================

void PCCGraph::clear()
{
    // L'ordre de vidage est important :
    // 1. m_index en premier — il contient des raw pointers vers m_nodes
    // 2. m_edges avant m_nodes — les arêtes référencent des PCCNode*
    // 3. m_nodes en dernier — destruction effective des objets
    m_index.clear();
    m_edges.clear();  // Les PCCEdge sont détruits ici — leurs PCCNode* deviennent pendants
    m_nodes.clear();  // Les PCCNode sont détruits ici
    // Après clear(), tous les raw pointers précédemment retournés sont invalidés.
}
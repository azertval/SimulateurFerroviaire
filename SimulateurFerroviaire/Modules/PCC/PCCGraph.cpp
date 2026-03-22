/**
 * @file  PCCGraph.cpp
 * @brief Implémentation du conteneur propriétaire du graphe PCC.
 *
 * @see PCCGraph
 */
#include "PCCGraph.h"
#include <stdexcept>

PCCGraph::PCCGraph(Logger& logger)
    : m_logger(logger)
{
    LOG_DEBUG(m_logger, "PCCGraph initialisé.");
}

 // =============================================================================
 // Construction du graphe
 // =============================================================================

PCCNode* PCCGraph::addStraightNode(StraightBlock* source)
{
    auto node = std::make_unique<PCCStraightNode>(source, m_logger);
    PCCNode* raw = node.get();
    m_index[source->getId()] = raw;
    m_nodes.push_back(std::move(node));
    return raw;
    
}

PCCNode* PCCGraph::addSwitchNode(SwitchBlock* source)
{
    auto node = std::make_unique<PCCSwitchNode>(source, m_logger);
    PCCNode* raw = node.get();
    m_index[source->getId()] = raw;
    m_nodes.push_back(std::move(node));
    return raw;
}

PCCEdge* PCCGraph::addEdge(PCCNode* from, PCCNode* to, PCCEdgeRole role)
{
    // PCCEdge::PCCEdge lève std::invalid_argument si from ou to == nullptr
    auto edge = std::make_unique<PCCEdge>(from, to, role, m_logger);
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
    if (it == m_index.end())
    {
        LOG_WARNING(m_logger, "PCCGraph::findNode — nœud introuvable : " + sourceId);
        return nullptr;
    }
    return (it != m_index.end()) ? it->second : nullptr;
}


// =============================================================================
// Remise à zéro
// =============================================================================

void PCCGraph::clear()
{
    LOG_INFO(m_logger, "PCCGraph::clear — "
        + std::to_string(m_nodes.size()) + " nœuds, "
        + std::to_string(m_edges.size()) + " arêtes supprimés.");

    // L'ordre de vidage est important :
    // 1. m_index en premier — il contient des raw pointers vers m_nodes
    // 2. m_edges avant m_nodes — les arêtes référencent des PCCNode*
    // 3. m_nodes en dernier — destruction effective des objets
    m_index.clear();
    m_edges.clear();  // Les PCCEdge sont détruits ici — leurs PCCNode* deviennent pendants
    m_nodes.clear();  // Les PCCNode sont détruits ici
    // Après clear(), tous les raw pointers précédemment retournés sont invalidés.
}
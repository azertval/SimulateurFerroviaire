/**
 * @file  PCCSwitchNode.h
 * @brief Nœud PCC représentant un @ref SwitchBlock.
 *
 * Étend @ref PCCNode avec un accès typé au bloc source et des pointeurs
 * directs vers les arêtes root / normal / deviation, évitant à
 * @ref TCORenderer de parcourir @ref PCCNode::getEdges() et de filtrer
 * par rôle à chaque frame de rendu.
 *
 * @par Ownership des arêtes de branche
 * @ref m_rootEdge, @ref m_normalEdge et @ref m_deviationEdge sont des
 * raw pointers non-propriétaires — les arêtes sont possédées par @ref PCCGraph.
 * Ces pointeurs sont un sous-ensemble de @ref PCCNode::m_edges, exposés
 * directement pour un accès O(1) sans parcours du vecteur.
 */
#pragma once

#include "PCCNode.h"
#include "Modules/InteractiveElements/ShuntingElements/SwitchBlock.h"

 /**
  * @class PCCSwitchNode
  * @brief Nœud PCC issu d'un @ref SwitchBlock.
  */
class PCCSwitchNode : public PCCNode
{
public:

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construit un nœud PCC depuis un @ref SwitchBlock.
     *
     * @param source  Pointeur non-propriétaire vers le @ref SwitchBlock.
     *                Doit rester valide pendant toute la durée de vie du nœud.
     *
     * @throws std::invalid_argument Si @p source est nullptr.
     */
    explicit PCCSwitchNode(SwitchBlock* source);

    // =========================================================================
    // Interface PCCNode
    // =========================================================================

    /**
     * @brief Retourne @c PCCNodeType::SWITCH.
     *
     * @return @ref PCCNodeType::SWITCH.
     */
    [[nodiscard]] PCCNodeType getNodeType() const override { return PCCNodeType::SWITCH; }

    // =========================================================================
    // Accesseur typé
    // =========================================================================

    /**
     * @brief Retourne le @ref SwitchBlock source avec son type concret.
     *
     * Donne accès direct à @ref SwitchBlock::getActiveBranch,
     * @ref SwitchBlock::isOriented et @ref SwitchBlock::getState
     * sans cast dynamique à l'appelant.
     *
     * @return Pointeur non-propriétaire vers le @ref SwitchBlock source.
     *         Jamais nullptr si le nœud a été correctement construit.
     */
    [[nodiscard]] SwitchBlock* getSwitchSource() const { return m_switchSource; }

    // =========================================================================
    // Arêtes de branche — accès direct O(1)
    // =========================================================================

    /**
     * @brief Retourne l'arête correspondant à la branche root.
     *
     * @return Pointeur non-propriétaire vers l'arête root, ou @c nullptr
     *         si @ref PCCGraphBuilder n'a pas encore résolu cette branche.
     */
    [[nodiscard]] PCCEdge* getRootEdge()      const { return m_rootEdge; }

    /**
     * @brief Retourne l'arête correspondant à la branche normale.
     *
     * @return Pointeur non-propriétaire vers l'arête normale, ou @c nullptr
     *         si @ref PCCGraphBuilder n'a pas encore résolu cette branche.
     */
    [[nodiscard]] PCCEdge* getNormalEdge()    const { return m_normalEdge; }

    /**
     * @brief Retourne l'arête correspondant à la branche déviée.
     *
     * @return Pointeur non-propriétaire vers l'arête déviation, ou @c nullptr
     *         si @ref PCCGraphBuilder n'a pas encore résolu cette branche.
     */
    [[nodiscard]] PCCEdge* getDeviationEdge() const { return m_deviationEdge; }

    // =========================================================================
    // Mutations — appelées uniquement par PCCGraphBuilder
    // =========================================================================

    /**
     * @brief Enregistre l'arête root de ce switch.
     *
     * Appelé par @ref PCCGraphBuilder après création de l'arête dans
     * @ref PCCGraph. Ne prend pas ownership.
     *
     * @param edge  Pointeur non-propriétaire vers l'arête root.
     */
    void setRootEdge(PCCEdge* edge) { m_rootEdge = edge; }

    /**
     * @brief Enregistre l'arête normale de ce switch.
     *
     * @param edge  Pointeur non-propriétaire vers l'arête normale.
     */
    void setNormalEdge(PCCEdge* edge) { m_normalEdge = edge; }

    /**
     * @brief Enregistre l'arête déviation de ce switch.
     *
     * @param edge  Pointeur non-propriétaire vers l'arête déviation.
     */
    void setDeviationEdge(PCCEdge* edge) { m_deviationEdge = edge; }

private:

    /**
     * Pointeur typé non-propriétaire vers le @ref SwitchBlock source.
     * Redondant avec @ref PCCNode::m_source — évite les casts dynamiques.
     * Propriété de @ref TopologyRepository — ne pas delete.
     */
    SwitchBlock* m_switchSource = nullptr;

    /**
     * Arête non-propriétaire vers la branche root.
     * Nullptr tant que @ref PCCGraphBuilder n'a pas résolu la branche.
     * Propriété de @ref PCCGraph — ne pas delete.
     */
    PCCEdge* m_rootEdge = nullptr;

    /**
     * Arête non-propriétaire vers la branche normale.
     * Nullptr tant que @ref PCCGraphBuilder n'a pas résolu la branche.
     * Propriété de @ref PCCGraph — ne pas delete.
     */
    PCCEdge* m_normalEdge = nullptr;

    /**
     * Arête non-propriétaire vers la branche déviée.
     * Nullptr tant que @ref PCCGraphBuilder n'a pas résolu la branche.
     * Propriété de @ref PCCGraph — ne pas delete.
     */
    PCCEdge* m_deviationEdge = nullptr;
};
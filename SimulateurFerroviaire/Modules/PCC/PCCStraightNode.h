/**
 * @file  PCCStraightNode.h
 * @brief Nœud PCC représentant un @ref StraightBlock.
 *
 * Étend @ref PCCNode avec un accès typé au bloc source via
 * @ref getStraightSource, évitant tout cast dynamique dans
 * @ref TCORenderer et @ref PCCLayout.
 *
 * @par Redondance intentionnelle
 * @ref m_straightSource est redondant avec @ref PCCNode::m_source
 * mais de type `StraightBlock*` au lieu de `ShuntingElement*`.
 * Trade-off accepté : légère redondance mémoire (1 pointeur supplémentaire)
 * en échange d'une absence totale de cast dans les consommateurs.
 */
#pragma once

#include "PCCNode.h"
#include "Modules/InteractiveElements/ShuntingElements/StraightBlock.h"

 /**
  * @class PCCStraightNode
  * @brief Nœud PCC issu d'un @ref StraightBlock.
  *
  * La relation "est-un" est exposée publiquement (héritage public) :
  * un @ref PCCStraightNode EST-UN @ref PCCNode au sens du Liskov
  * Substitution Principle — il peut être utilisé partout où un
  * @ref PCCNode* est attendu.
  */
class PCCStraightNode : public PCCNode
{
public:

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construit un nœud PCC depuis un @ref StraightBlock.
     *
     * Le mot-clé @c explicit interdit la conversion implicite :
     * `PCCStraightNode n = ptr;` est une erreur de compilation.
     *
     * @param source  Pointeur non-propriétaire vers le @ref StraightBlock.
     *                Doit rester valide pendant toute la durée de vie du nœud.
     *
     * @throws std::invalid_argument Si @p source est nullptr.
     */
    explicit PCCStraightNode(StraightBlock* source, Logger& logger);

    // =========================================================================
    // Interface PCCNode
    // =========================================================================

    /**
     * @brief Retourne @c PCCNodeType::STRAIGHT.
     *
     * Le mot-clé @c override demande au compilateur de vérifier que cette
     * méthode surcharge bien une virtuelle de @ref PCCNode. Sans @c override,
     * une faute de frappe créerait une nouvelle méthode sans warning.
     *
     * @return @ref PCCNodeType::STRAIGHT.
     */
    [[nodiscard]] PCCNodeType getNodeType() const override { return PCCNodeType::STRAIGHT; }

    // =========================================================================
    // Accesseur typé
    // =========================================================================

    /**
     * @brief Retourne le @ref StraightBlock source avec son type concret.
     *
     * Donne accès direct à @ref StraightBlock::getLengthMeters et aux
     * coordonnées sans cast dynamique à l'appelant.
     *
     * @return Pointeur non-propriétaire vers le @ref StraightBlock source.
     *         Jamais nullptr si le nœud a été correctement construit.
     */
    [[nodiscard]] StraightBlock* getStraightSource() const { return m_straightSource; }

private:

    /**
     * Pointeur typé non-propriétaire vers le @ref StraightBlock source.
     * Redondant avec @ref PCCNode::m_source — évite les casts dynamiques.
     * Propriété de @ref TopologyRepository — ne pas delete.
     */
    StraightBlock* m_straightSource = nullptr;
};
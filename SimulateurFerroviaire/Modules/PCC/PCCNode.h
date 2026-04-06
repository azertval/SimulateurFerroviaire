/**
 * @file  PCCNode.h
 * @brief Nœud abstrait du graphe PCC représentant un bloc ferroviaire.
 *
 * Chaque nœud encapsule :
 *  - une traçabilité vers le bloc source (@ref ShuntingElement*),
 *  - une position logique calculée par @ref PCCLayout,
 *  - une liste d'arêtes adjacentes peuplée par @ref PCCGraphBuilder.
 *
 * @par Traçabilité source
 * Le pointeur @ref m_source est non-propriétaire. Il pointe vers un bloc
 * appartenant à @ref TopologyRepository, dont la durée de vie est garantie
 * tant que le repository n'est pas rechargé.
 *
 * @par Ownership des arêtes
 * Les arêtes sont possédées par @ref PCCGraph. @ref PCCNode n'en stocke
 * que des pointeurs non-propriétaires pour le parcours local.
 *
 * @par Patron de conception — NVI (Non-Virtual Interface)
 * Les accesseurs communs sont non-virtuels (performances, contrat stable).
 * Seul @ref getNodeType est virtuel pur — il force chaque sous-classe à
 * s'identifier sans exposer d'autre surface virtuelle.
 */
#pragma once

#include <string>
#include <vector>

#include "PCCEdge.h"
#include "Modules/Elements/ShuntingElements/ShuntingElement.h"

#include "Engine/Core/Logger/Logger.h"

 /**
  * @brief Type d'un nœud PCC — miroir de @ref ElementType.
  *
  *  - STRAIGHT : nœud issu d'un @ref StraightBlock.
  *  - SWITCH   : nœud issu d'un @ref SwitchBlock.
  */
enum class PCCNodeType { STRAIGHT, SWITCH, CROSSING };

/**
 * @brief Position logique d'un nœud dans le schéma PCC.
 *
 * Calculée par @ref PCCLayout, indépendante des coordonnées GPS.
 *  - x : profondeur BFS depuis le terminus le plus à l'ouest.
 *  - y : rang vertical — 0 = backbone, +1 = branche haute, -1 = branche basse.
 *
 * @note Initialisée à {0, 0} par défaut — non significative tant que
 *       @ref PCCLayout n'a pas été exécuté.
 */
struct PCCPosition
{
    int x = 0;   // Initialisation en place (C++11) — défaut valide sans constructeur explicite
    int y = 0;
};

/**
 * @class PCCNode
 * @brief Nœud abstrait du graphe PCC.
 *
 * Sous-classé en @ref PCCStraightNode et @ref PCCSwitchNode pour exposer
 * les accesseurs spécifiques à chaque type sans cast dynamique à l'appelant.
 *
 * Règles de copie / déplacement :
 *  - Copie interdite — slicing et aliasing du pointeur source.
 *  - Déplacement autorisé — requis par unique_ptr et vector.
 */
class PCCNode
{
public:

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construit un nœud PCC à partir d'un bloc source.
     *
     * @p sourceId est passé par valeur et déplacé dans @ref m_sourceId via
     * @c std::move — O(1) au lieu de O(n) si l'appelant passe une rvalue.
     *
     * @param sourceId  Identifiant du bloc source (ex. "s/0", "sw/3").
     * @param source    Pointeur non-propriétaire vers le bloc source.
     *                  Doit rester valide pendant toute la durée de vie du nœud.
     * @param logger    Référence au logger HMI fourni par @ref PCCGraph.
     *
     * @throws std::invalid_argument Si @p source est nullptr.
     */
    PCCNode(std::string sourceId, ShuntingElement* source, Logger& logger);

    /** @brief Interdit la copie — slicing et aliasing du pointeur source. */
    PCCNode(const PCCNode&) = delete;
    PCCNode& operator=(const PCCNode&) = delete;

    /** @brief Déplacement autorisé — requis par unique_ptr et vector. */
    PCCNode(PCCNode&&) = default;
    PCCNode& operator=(PCCNode&&) = default;

    /**
     * @brief Destructeur virtuel — obligatoire pour le delete via pointeur de base.
     *
     * @ref PCCGraph stocke des `unique_ptr<PCCNode>` pointant sur des
     * @ref PCCStraightNode / @ref PCCSwitchNode. Sans virtual, le destructeur
     * de la sous-classe ne serait pas appelé → undefined behavior.
     */
    virtual ~PCCNode() = default;

    // =========================================================================
    // Interface virtuelle pure
    // =========================================================================

    /**
     * @brief Retourne le type du nœud.
     *
     * Méthode virtuelle pure : rend @ref PCCNode abstraite (non instanciable).
     * Chaque sous-classe concrète doit l'implémenter.
     *
     * @return @ref PCCNodeType::STRAIGHT ou @ref PCCNodeType::SWITCH.
     */
    [[nodiscard]] virtual PCCNodeType getNodeType() const = 0;

    // =========================================================================
    // Accesseurs communs
    // =========================================================================

    /**
     * @brief Retourne l'identifiant du bloc source.
     *
     * @return Référence constante à l'ID source (ex. "s/0", "sw/3").
     *         Référence stable tant que le nœud est en vie.
     */
    [[nodiscard]] const std::string& getSourceId() const { return m_sourceId; }

    /**
     * @brief Retourne le pointeur non-propriétaire vers le bloc source.
     *
     * Permet à @ref TCORenderer d'interroger @ref ShuntingState sans
     * couplage au type concret.
     *
     * @return Pointeur vers @ref ShuntingElement. Jamais nullptr.
     */
    [[nodiscard]] ShuntingElement* getSource() const { return m_source; }

    /**
     * @brief Retourne la position logique du nœud dans le schéma PCC.
     *
     * @return @ref PCCPosition {x, y}. Non significative avant l'exécution
     *         de @ref PCCLayout.
     */
    [[nodiscard]] const PCCPosition& getPosition() const { return m_position; }

    /**
     * @brief Retourne la liste des arêtes adjacentes (non-propriétaires).
     *
     * Peuplée par @ref PCCGraphBuilder via @ref addEdge. L'ordre n'est
     * pas garanti.
     *
     * @return Référence constante à la liste de pointeurs d'arêtes.
     */
    [[nodiscard]] const std::vector<PCCEdge*>& getEdges() const { return m_edges; }

    // =========================================================================
    // Mutations
    // =========================================================================

    /**
     * @brief Assigne la position logique calculée par @ref PCCLayout.
     *
     * @p position est passé par valeur — @ref PCCPosition est un POD de
     * 2 entiers, la copie est équivalente ou meilleure qu'une référence.
     *
     * @param position  Coordonnées logiques (x = profondeur BFS, y = rang vertical).
     */
    void setPosition(PCCPosition position) { m_position = position; }

    /**
     * @brief Enregistre une arête adjacente (pointeur non-propriétaire).
     *
     * Appelé par @ref PCCGraphBuilder après création de l'arête dans
     * @ref PCCGraph. Ne prend pas ownership. Ignore silencieusement les nullptr.
     *
     * @param edge  Pointeur non-propriétaire vers l'arête à enregistrer.
     */
    void addEdge(PCCEdge* edge);

    bool isCrossover() const { return m_isCrossover; }
    void setCrossover(bool v) { m_isCrossover = v; }

protected:

    /** Identifiant du bloc source (ex. "s/0", "sw/3"). */
    std::string m_sourceId;

    /**
     * Pointeur non-propriétaire vers le bloc source.
     * Propriété de @ref TopologyRepository — ne pas delete.
     */
    ShuntingElement* m_source = nullptr;

    /**
     * Position logique dans le schéma PCC.
     * Calculée par @ref PCCLayout — non significative avant son exécution.
     */
    PCCPosition m_position;

    /**
     * Arêtes adjacentes — pointeurs non-propriétaires.
     * Les arêtes sont possédées par @ref PCCGraph via unique_ptr.
     */
    std::vector<PCCEdge*> m_edges;

    /** Logger de la class*/
    Logger& m_logger;

private:
    bool m_isCrossover = false;
};
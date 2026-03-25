/**
 * @file  PCCGraph.h
 * @brief Conteneur propriétaire du graphe PCC.
 *
 * Possède l'ensemble des nœuds (@ref PCCNode) et arêtes (@ref PCCEdge)
 * via `std::unique_ptr`, garantissant une destruction automatique et
 * un polymorphisme correct. Expose un index de lookup O(1) par `sourceId`.
 *
 * @par Responsabilités
 *  - Créer et posséder les nœuds via @ref addStraightNode / @ref addSwitchNode.
 *  - Créer et posséder les arêtes via @ref addEdge.
 *  - Indexer les nœuds pour un accès O(1) par @ref findNode.
 *  - Exposer les collections en lecture pour @ref PCCLayout et @ref TCORenderer.
 *
 * @par Ce que PCCGraph ne fait PAS
 *  - Il ne sait pas dans quel ordre construire le graphe (@ref PCCGraphBuilder).
 *  - Il ne calcule pas les positions logiques (@ref PCCLayout).
 *  - Il ne dessine rien (@ref TCORenderer).
 *
 * @par Cycle de vie
 * Détenu par @ref PCCPanel. Reconstruit intégralement via @ref clear() +
 * @ref PCCGraphBuilder à chaque chargement de fichier GeoJSON.
 */
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "PCCEdge.h"
#include "PCCNode.h"
#include "PCCStraightNode.h"
#include "PCCSwitchNode.h"

#include "Engine/Core/Logger/Logger.h"

#include "Modules/Elements/ShuntingElements/StraightBlock.h"
#include "Modules/Elements/ShuntingElements/SwitchBlock.h"

 /**
  * @class PCCGraph
  * @brief Conteneur propriétaire du graphe PCC — nœuds, arêtes et index.
  *
  * Règles de copie / déplacement :
  *  - Copie interdite — les unique_ptr ne sont pas copiables.
  *  - Déplacement autorisé — transfert d'ownership possible si nécessaire.
  */
class PCCGraph
{
public:

    // =========================================================================
    // Construction
    // =========================================================================

    explicit PCCGraph();
    ~PCCGraph() = default;

    /** @brief Interdit la copie — unique_ptr non copiable. */
    PCCGraph(const PCCGraph&) = delete;
    PCCGraph& operator=(const PCCGraph&) = delete;

    /** @brief Déplacement autorisé. */
    PCCGraph(PCCGraph&&) = default;
    PCCGraph& operator=(PCCGraph&&) = default;

    /**
     * @brief Retourne une référence au logger PCC pour les classes consommatrices.
     *
     * Permet à @ref PCCGraphBuilder et @ref PCCLayout d'utiliser le même
     * logger sans couplage direct à PCCGraph.
     *
     * @return Référence au logger interne @c Logger{"PCC"}.
     */
    [[nodiscard]] Logger& getLogger() { return m_logger; }

    // =========================================================================
    // Construction du graphe — appelée par PCCGraphBuilder
    // =========================================================================

    /**
     * @brief Crée un nœud voie droite, l'indexe et le stocke.
     *
     * Crée un @ref PCCStraightNode depuis @p source, le stocke dans
     * @ref m_nodes (ownership exclusif), l'indexe dans @ref m_index,
     * et retourne un pointeur non-propriétaire vers le nœud créé.
     *
     * Le pointeur retourné reste valide tant que ce @ref PCCGraph est en vie
     * et que @ref clear() n'a pas été appelé.
     *
     * @param source  Pointeur non-propriétaire vers le @ref StraightBlock source.
     *                Ne doit pas être nullptr.
     *
     * @return Pointeur non-propriétaire vers le nœud créé.
     *         Jamais nullptr si @p source est valide.
     *
     * @throws std::invalid_argument Si @p source est nullptr
     *         (propagé depuis @ref PCCStraightNode).
     */
    PCCNode* addStraightNode(StraightBlock* source);

    /**
     * @brief Crée un nœud aiguillage, l'indexe et le stocke.
     *
     * Crée un @ref PCCSwitchNode depuis @p source, le stocke dans
     * @ref m_nodes (ownership exclusif), l'indexe dans @ref m_index,
     * et retourne un pointeur non-propriétaire vers le nœud créé.
     *
     * @param source  Pointeur non-propriétaire vers le @ref SwitchBlock source.
     *                Ne doit pas être nullptr.
     *
     * @return Pointeur non-propriétaire vers le nœud créé.
     *         Jamais nullptr si @p source est valide.
     *
     * @throws std::invalid_argument Si @p source est nullptr
     *         (propagé depuis @ref PCCSwitchNode).
     */
    PCCNode* addSwitchNode(SwitchBlock* source);

    /**
     * @brief Crée une arête orientée entre deux nœuds et la câble sur les deux.
     *
     * Crée une @ref PCCEdge (from → to, rôle @p role), la stocke dans
     * @ref m_edges (ownership exclusif), puis appelle @ref PCCNode::addEdge
     * sur @p from et sur @p to pour enregistrer l'arête dans les deux sens.
     *
     * Si @p role est @c PCCEdgeRole::ROOT, @c NORMAL ou @c DEVIATION et que
     * @p from est un @ref PCCSwitchNode, l'arête correspondante est également
     * enregistrée via @c setRootEdge / @c setNormalEdge / @c setDeviationEdge.
     *
     * @param from  Nœud source. Ne doit pas être nullptr.
     * @param to    Nœud cible. Ne doit pas être nullptr.
     * @param role  Rôle sémantique de la connexion.
     *
     * @return Pointeur non-propriétaire vers l'arête créée.
     *
     * @throws std::invalid_argument Si @p from ou @p to est nullptr
     *         (propagé depuis @ref PCCEdge).
     */
    PCCEdge* addEdge(PCCNode* from, PCCNode* to, PCCEdgeRole role);

    // =========================================================================
    // Lookup
    // =========================================================================

    /**
     * @brief Recherche un nœud par l'identifiant de son bloc source.
     *
     * Lookup O(1) amorti dans @ref m_index.
     * Utilise @c find() et non @c operator[] pour éviter l'insertion
     * implicite d'une entrée vide si la clé est absente.
     *
     * @param sourceId  Identifiant du bloc source (ex. "s/0", "sw/3").
     *
     * @return Pointeur non-propriétaire vers le nœud correspondant,
     *         ou @c nullptr si aucun nœud ne correspond à @p sourceId.
     */
    [[nodiscard]] PCCNode* findNode(const std::string& sourceId) const;

    // =========================================================================
    // Accesseurs — lecture seule pour PCCLayout et TCORenderer
    // =========================================================================

    /**
     * @brief Retourne la collection de nœuds (lecture seule).
     *
     * @return Référence constante au vecteur de unique_ptr<PCCNode>.
     *         Les pointeurs bruts sont accessibles via unique_ptr::get().
     */
    [[nodiscard]] const std::vector<std::unique_ptr<PCCNode>>& getNodes() const { return m_nodes; }

    /**
     * @brief Retourne la collection d'arêtes (lecture seule).
     *
     * @return Référence constante au vecteur de unique_ptr<PCCEdge>.
     */
    [[nodiscard]] const std::vector<std::unique_ptr<PCCEdge>>& getEdges() const { return m_edges; }

    /**
     * @brief Retourne le nombre de nœuds dans le graphe.
     *
     * @return Nombre de nœuds. 0 si le graphe est vide ou non construit.
     */
    [[nodiscard]] std::size_t nodeCount() const { return m_nodes.size(); }

    /**
     * @brief Retourne le nombre d'arêtes dans le graphe.
     *
     * @return Nombre d'arêtes.
     */
    [[nodiscard]] std::size_t edgeCount() const { return m_edges.size(); }

    /**
     * @brief Indique si le graphe est vide.
     *
     * @return @c true si aucun nœud n'a été ajouté, @c false sinon.
     */
    [[nodiscard]] bool isEmpty() const { return m_nodes.empty(); }

    // =========================================================================
    // Remise à zéro
    // =========================================================================

    /**
     * @brief Vide le graphe — nœuds, arêtes et index.
     *
     * Libère tous les nœuds et arêtes (les unique_ptr sont détruits),
     * vide l'index et remet le graphe dans son état initial.
     *
     * À appeler depuis @ref PCCPanel avant chaque reconstruction via
     * @ref PCCGraphBuilder, typiquement en réponse à @c WM_PARSING_SUCCESS.
     *
     * @note Tous les raw pointers précédemment retournés par @ref addStraightNode,
     *       @ref addSwitchNode et @ref addEdge sont invalidés après cet appel.
     */
    void clear();

private:
    /** Logger dédié à la couche PCC, utilisé pour tracer les événements et erreurs liés à l'interface utilisateur. */
    mutable Logger m_logger{ "PCC" };

    /**
     * Nœuds du graphe — propriétaires exclusifs.
     * Chaque entrée est un @ref PCCStraightNode ou @ref PCCSwitchNode.
     * L'ordre d'insertion correspond à l'ordre de parsing.
     */
    std::vector<std::unique_ptr<PCCNode>> m_nodes;

    /**
     * Arêtes du graphe — propriétaires exclusives.
     * Chaque arête est câblée sur ses deux nœuds extrémités via addEdge().
     */
    std::vector<std::unique_ptr<PCCEdge>> m_edges;

    /**
     * Index de lookup O(1) : sourceId → PCCNode*.
     * Construit incrémentalement par addStraightNode() et addSwitchNode().
     * Les pointeurs sont non-propriétaires — ownership dans m_nodes.
     *
     * Utiliser find() pour la lecture — operator[] insèrerait une entrée
     * vide si la clé est absente.
     */
    std::unordered_map<std::string, PCCNode*> m_index;

};
/**
 * @file  PCCEdge.h
 * @brief Connexion orientée entre deux nœuds du graphe PCC.
 *
 * Une arête relie deux @ref PCCNode et porte un rôle sémantique qui
 * correspond à la nature de la connexion dans la topologie ferroviaire.
 * Permet à @ref TCORenderer de choisir couleur et style sans interroger
 * les nœuds source.
 *
 * @par Ownership
 * Les arêtes sont possédées par @ref PCCGraph via `unique_ptr`.
 * Les pointeurs `from` et `to` sont non-propriétaires — leur durée de vie
 * est garantie par @ref PCCGraph qui possède également les nœuds.
 *
 * @note Les arêtes sont orientées (from → to) pour simplifier le parcours
 *       gauche → droite dans @ref PCCLayout. Le graphe reste logiquement
 *       non-orienté — chaque connexion génère une arête dans chaque sens.
 */
#pragma once
#include "Engine/Core/Logger/Logger.h"

class PCCNode;   // Forward declaration — évite la dépendance circulaire avec PCCNode.h


/**
 * @brief Rôle sémantique d'une arête dans la topologie ferroviaire.
 *
 *  - STRAIGHT  : connexion entre deux blocs adjacents sans switch.
 *  - ROOT      : connexion sur la branche root d'un @ref SwitchBlock.
 *  - NORMAL    : connexion sur la branche normale d'un @ref SwitchBlock.
 *  - DEVIATION : connexion sur la branche déviée d'un @ref SwitchBlock.
 */
enum class PCCEdgeRole
{
    STRAIGHT,
    ROOT,
    NORMAL,
    DEVIATION,
    CROSSING
};

/**
 * @class PCCEdge
 * @brief Connexion orientée entre deux @ref PCCNode du @ref PCCGraph.
 *
 * Règles de copie / déplacement :
 *  - Copie interdite — aliasing des pointeurs non-propriétaires.
 *  - Déplacement autorisé — requis par le stockage dans `unique_ptr`.
 */
class PCCEdge
{
public:

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construit une arête orientée entre deux nœuds.
     *
     * @param from    Nœud source. Ne doit pas être nullptr.
     * @param to      Nœud cible. Ne doit pas être nullptr.
     * @param role    Rôle sémantique de la connexion.
     * @param logger  Référence au logger HMI fourni par @ref PCCGraph.
     *
     * @throws std::invalid_argument Si @p from ou @p to est nullptr.
     */
    PCCEdge(PCCNode* from, PCCNode* to, PCCEdgeRole role, Logger& logger);


    /** @brief Interdit la copie — aliasing des pointeurs non-propriétaires. */
    PCCEdge(const PCCEdge&) = delete;
    PCCEdge& operator=(const PCCEdge&) = delete;

    /** @brief Déplacement autorisé — requis par unique_ptr. */
    PCCEdge(PCCEdge&&) = default;
    PCCEdge& operator=(PCCEdge&&) = default;

    ~PCCEdge() = default;

    // =========================================================================
    // Accesseurs
    // =========================================================================

    /**
     * @brief Retourne le nœud source de l'arête.
     *
     * @return Pointeur non-propriétaire vers le nœud source. Jamais nullptr.
     */
    [[nodiscard]] PCCNode* getFrom() const { return m_from; }

    /**
     * @brief Retourne le nœud cible de l'arête.
     *
     * @return Pointeur non-propriétaire vers le nœud cible. Jamais nullptr.
     */
    [[nodiscard]] PCCNode* getTo() const { return m_to; }

    /**
     * @brief Retourne le rôle sémantique de la connexion.
     *
     * @return @ref PCCEdgeRole identifiant la nature de la branche.
     */
    [[nodiscard]] PCCEdgeRole getRole() const { return m_role; }

private:

    /** Nœud source — non-propriétaire, durée de vie garantie par @ref PCCGraph. */
    PCCNode* m_from = nullptr;

    /** Nœud cible — non-propriétaire, durée de vie garantie par @ref PCCGraph. */
    PCCNode* m_to = nullptr;

    /** Rôle sémantique de la connexion dans la topologie ferroviaire. */
    PCCEdgeRole m_role;

    /** Logger de la class*/
    Logger& m_logger;
};
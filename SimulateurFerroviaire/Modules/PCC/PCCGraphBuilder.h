/**
 * @file  PCCGraphBuilder.h
 * @brief Constructeur du graphe PCC depuis TopologyRepository.
 *
 * Classe utilitaire statique — sans état. Son unique responsabilité est
 * de lire @ref TopologyRepository et de peupler un @ref PCCGraph via ses
 * méthodes de construction.
 *
 * @par Pipeline interne
 *  -# @ref buildNodes — crée un nœud PCC pour chaque bloc de la topologie.
 *  -# @ref buildEdges — résout les connexions et crée les arêtes.
 *
 * Les deux passes sont séparées : toutes les arêtes référencent des nœuds
 * par ID — l'index doit être complet avant la résolution des connexions.
 *
 * @par Dépendances
 * @ref PCCGraphBuilder est la **seule classe du module PCC** qui connaît
 * @ref TopologyRepository. @ref TCORenderer et @ref PCCLayout n'en ont
 * pas connaissance — ils consomment uniquement @ref PCCGraph.
 *
 * @note Classe entièrement statique — instanciation interdite.
 */
#pragma once

#include "PCCGraph.h"

#include "Engine/Core/Logger/Logger.h"

class TopologyData;

/**
 * @class PCCGraphBuilder
 * @brief Constructeur statique du @ref PCCGraph depuis @ref TopologyRepository.
 */
class PCCGraphBuilder
{
public:

    /**
     * @brief Construit le @ref PCCGraph depuis le contenu de @ref TopologyRepository.
     *
     * Appelle successivement @ref buildNodes puis @ref buildEdges.
     * Le graphe est d'abord vidé via @ref PCCGraph::clear avant construction.
     *
     * Si @ref TopologyRepository est vide (parsing non encore effectué),
     * le graphe reste vide après l'appel — aucune exception n'est levée.
     *
     * @param graph  Référence au graphe à construire. Modifié en place.
     *               Le graphe est vidé avant construction.
     * @param logger  Référence au logger HMI fourni par @ref PCCPanel.
     */
    static void build(PCCGraph& graph, Logger& logger);

    /** @brief Interdit l'instanciation — classe utilitaire statique. */
    PCCGraphBuilder() = delete;

private:

    /**
     * @brief Passe 1 — crée un nœud PCC pour chaque bloc de la topologie.
     *
     * Parcourt @ref TopologyData::straights et @ref TopologyData::switches
     * et appelle @ref PCCGraph::addStraightNode / @ref PCCGraph::addSwitchNode
     * pour chaque bloc. L'index est complet à la fin de cette passe.
     *
     * @param graph  Graphe en cours de construction.
     * @param topo   Données topologiques issues de @ref TopologyRepository.
     * @param logger  Référence au logger HMI.
     */
    static void buildNodes(PCCGraph& graph, const TopologyData& topo, Logger& logger);

    /**
     * @brief Passe 2 — résout les connexions et crée les arêtes.
     *
     * Parcourt les switches orientés pour créer les arêtes ROOT / NORMAL /
     * DEVIATION. Parcourt ensuite les straights pour créer les arêtes
     * STRAIGHT entre blocs adjacents, avec dédoublonnage via une clé
     * canonique (voir @ref makeEdgeKey).
     *
     * Les nœuds introuvables dans l'index (bloc non parsé ou connexion
     * invalide) sont ignorés silencieusement — un warning est loggé si
     * un Logger est disponible.
     *
     * @param graph  Graphe en cours de construction.
     * @param topo   Données topologiques issues de @ref TopologyRepository.
    * @param logger  Référence au logger HMI.
     */
    static void buildEdges(PCCGraph& graph, const TopologyData& topo, Logger& logger);

    /**
     * @brief Construit une clé canonique pour une paire de blocs.
     *
     * La clé est indépendante de l'ordre des paramètres :
     * @c makeEdgeKey("s/0", "sw/3") == @c makeEdgeKey("sw/3", "s/0").
     * Utilisée pour le dédoublonnage des arêtes STRAIGHT.
     *
     * @param idA  Identifiant du premier bloc.
     * @param idB  Identifiant du second bloc.
     *
     * @return Chaîne de la forme `"min|max"` — toujours identique
     *         quelle que soit l'ordre des paramètres.
     */
    static std::string makeEdgeKey(const std::string& idA, const std::string& idB);
};
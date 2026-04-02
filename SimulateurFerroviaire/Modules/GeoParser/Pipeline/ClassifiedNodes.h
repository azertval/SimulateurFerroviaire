/**
 * @file  ClassifiedNodes.h
 * @brief Structures de données produites par @ref Phase5_SwitchClassifier.
 *
 * Contient la classification topologique de chaque nœud du @ref TopologyGraph.
 */
#pragma once

#include <unordered_map>
#include <cstdint>
#include <cstddef>

 /**
  * @enum NodeClass
  * @brief Classification topologique d'un nœud du graphe planaire.
  *
  * Basée sur le degré (nombre d'arêtes incidentes) et l'angle entre
  * les arêtes sortantes.
  */
enum class NodeClass : uint8_t
{
    TERMINUS,  ///< Degré 1 — extrémité de voie, pas de suite.
    STRAIGHT,  ///< Degré 2, angle ≈ 180° — continuation de voie droite.
    SWITCH,    ///< Degré 3 — aiguillage, bifurcation géométrique réelle.
    CROSSING,  ///< Degré 4 — croisement plat, ignoré en Phase 6.
    ISOLATED,  ///< Degré 0 — nœud sans arête (données incomplètes).
    AMBIGUOUS  ///< Degré > 4 ou géométrie anormale — WARNING en Phase 5.
};

/**
 * @struct ClassifiedNodes
 * @brief Résultat de @ref Phase5_SwitchClassifier.
 *
 * Produit en Phase 5, consommé par @ref Phase6_BlockExtractor.
 * Libérable après Phase 6 via @c clear().
 */
struct ClassifiedNodes
{
    /** nodeId → classe topologique. */
    std::unordered_map<size_t, NodeClass> classification;

    /** Compteurs par classe — pour le log de synthèse Phase 5. */
    size_t countTerminus = 0;
    size_t countStraight = 0;
    size_t countSwitch = 0;
    size_t countCrossing = 0;
    size_t countIsolated = 0;
    size_t countAmbiguous = 0;

    /**
     * @brief Retourne la classe d'un nœud.
     *
     * @param nodeId  Identifiant du nœud.
     *
     * @return Classe du nœud, ou @c NodeClass::AMBIGUOUS si inconnu.
     */
    [[nodiscard]] NodeClass getClass(size_t nodeId) const
    {
        const auto it = classification.find(nodeId);
        return (it != classification.end()) ? it->second : NodeClass::AMBIGUOUS;
    }

    /**
     * @brief Enregistre la classe d'un nœud et incrémente le compteur.
     *
     * @param nodeId  Identifiant du nœud.
     * @param cls     Classe à attribuer.
     */
    void classify(size_t nodeId, NodeClass cls)
    {
        classification[nodeId] = cls;
        switch (cls)
        {
        case NodeClass::TERMINUS:  ++countTerminus;  break;
        case NodeClass::STRAIGHT:  ++countStraight;  break;
        case NodeClass::SWITCH:    ++countSwitch;    break;
        case NodeClass::CROSSING:  ++countCrossing;  break;
        case NodeClass::ISOLATED:  ++countIsolated;  break;
        case NodeClass::AMBIGUOUS: ++countAmbiguous; break;
        }
    }

    /** @brief Vide la classification — libère la mémoire après Phase 6. */
    void clear()
    {
        classification.clear();
        countTerminus = countStraight = countSwitch = 0;
        countCrossing = countIsolated = countAmbiguous = 0;
    }
};
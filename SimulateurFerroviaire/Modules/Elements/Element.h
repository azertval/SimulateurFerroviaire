#pragma once

#include <string>
#include "Engine/Core/Logger/Logger.h"

/**
 * @file  Element.h
 * @brief Classe de base abstraite pour tous les éléments interactifs ferroviaires.
 */

 /**
  * @brief Types d'éléments interactifs ferroviaires.
  *
  *   SWITCH   – aiguillage à 3 branches.
  *   STRAIGHT – tronçon de voie rectiligne.
  */
enum class ElementType
{
    SWITCH,
    STRAIGHT
};

/**
 * @brief Classe de base abstraite pour tous les éléments interactifs ferroviaires.
 *
 * Définit l'interface commune que chaque élément interactif doit implémenter.
 *
 * Règles de copie / déplacement :
 *   - Copie interdite : évite le slicing et la duplication accidentelle.
 *   - Déplacement autorisé : nécessaire au pipeline de construction
 *     (TopologyExtractor, make_unique, etc.).
 */
class Element
{
public:

    // -------------------------------------------------------------------------
    // Construction / Destruction
    // -------------------------------------------------------------------------

    Element() = default;
    virtual ~Element() = default;

    /** @brief Interdit la copie — risque de slicing. */
    Element(const Element&) = delete;
    Element& operator=(const Element&) = delete;

    /** @brief Déplacement autorisé — requis par le pipeline de construction. */
    Element(Element&&) = default;
    Element& operator=(Element&&) = default;

    // -------------------------------------------------------------------------
    // Interface virtuelle pure
    // -------------------------------------------------------------------------

    /**
     * @brief Retourne l'identifiant unique de l'élément (ex. "sw/3", "s/12").
     */
    [[nodiscard]] virtual std::string getId() const = 0;

    /**
     * @brief Retourne le type de l'élément.
     * @return ElementType::SWITCH ou ElementType::STRAIGHT.
     */
    [[nodiscard]] virtual ElementType getType() const = 0;

protected :
     /** 
     * @brief id de l'element
     */
    std::string m_id;

    /**
     * @brief Logger statique partagé par TOUS les éléments interactifs.
     *
     * Une seule instance pour l'ensemble des SwitchBlock et StraightBlock
     * → un seul fichier "Logs/Elements.log".
     *
     * Statique : initialisé une seule fois au démarrage, partagé par toutes
     * les instances de toutes les classes dérivées. Le mutex interne du Logger
     * garantit la thread-safety des écritures concurrentes.
     *
     * Utilisation dans les classes dérivées :
     * @code
     *   LOG_INFO(m_logger, m_id + " orienté");
     * @endcode
     */
    static Logger m_logger;
};
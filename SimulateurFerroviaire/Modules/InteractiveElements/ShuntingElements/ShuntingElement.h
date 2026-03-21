#pragma once

#include "Modules/InteractiveElements/InteractiveElement.h"

/**
 * @file  ShuntingElement.h
 * @brief Interface abstraite pour tous les éléments de shuntage ferroviaire.
 */

 /**
  * @brief État opérationnel d'un élément de shuntage.
  *
  *   FREE     – l'élément est libre et opérationnel.
  *   OCCUPIED – un véhicule est actuellement présent sur l'élément.
  *   INACTIVE – l'élément est hors service (panne / maintenance).
  */
enum class ShuntingState
{
    FREE,
    OCCUPIED,
    INACTIVE
};

/**
 * @brief Interface abstraite pour tous les éléments de shuntage ferroviaire.
 *
 * Étend InteractiveElement avec une requête d'état propre à l'infrastructure
 * de shuntage (sections de voie, détecteurs, signaux de manœuvre, …).
 *
 * Les sous-classes concrètes doivent implémenter getId(), getType() et getState().
 *
 * Règles de copie / déplacement :
 *   - Copie interdite (héritée de InteractiveElement).
 *   - Déplacement explicitement autorisé : la déclaration du destructeur virtuel
 *     supprimerait sinon la génération implicite des opérateurs de déplacement,
 *     rendant StraightBlock et SwitchBlock non-déplaçables.
 */
class ShuntingElement : public InteractiveElement
{
public:
    // -------------------------------------------------------------------------
    // Construction / Destruction
    // -------------------------------------------------------------------------
    ShuntingElement() = default;
    virtual ~ShuntingElement() = default;

    /**
     * Déplacement explicite — nécessaire car le destructeur virtuel supprime
     * la génération implicite des opérateurs de déplacement en C++11/14/17.
     */
    ShuntingElement(ShuntingElement&&) = default;
    ShuntingElement& operator=(ShuntingElement&&) = default;

    // -------------------------------------------------------------------------
    // Interface virtuelle pure
    // -------------------------------------------------------------------------

    /**
     * @brief Retourne l'état opérationnel courant de l'élément.
     *
     * @return ShuntingState::FREE     – libre, opérationnel.
     *         ShuntingState::OCCUPIED – occupé par un véhicule.
     *         ShuntingState::INACTIVE – hors service (panne / maintenance).
     */
    [[nodiscard]] virtual ShuntingState getState() const = 0;

    // -------------------------------------------------------------------------
    // Accesseurs de commodité (non-virtuels, basés sur getState())
    // -------------------------------------------------------------------------

    /** @brief Retourne true si l'élément est libre. */
    [[nodiscard]] bool isFree()     const { return getState() == ShuntingState::FREE; }

    /** @brief Retourne true si l'élément est occupé. */
    [[nodiscard]] bool isOccupied() const { return getState() == ShuntingState::OCCUPIED; }

    /** @brief Retourne true si l'élément est hors service. */
    [[nodiscard]] bool isInactive() const { return getState() == ShuntingState::INACTIVE; }

protected :
    /** État opérationnel courant du bloc (FREE par défaut). */
    ShuntingState m_state = ShuntingState::FREE;
private:
    // Pas de champs supplémentaires dans l'interface abstraite — les sous-classes concrètes
};
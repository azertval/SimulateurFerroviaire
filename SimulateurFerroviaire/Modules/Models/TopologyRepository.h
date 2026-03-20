/**
 * @file  TopologyRepository.h
 * @brief Singleton de stockage des données topologiques ferroviaires.
 *
 * @see TopologyRepository
 */

#pragma once
#include "TopologyData.h"

 /**
  * @class TopologyRepository
  * @brief Singleton fournissant un accès global unique à @ref TopologyData.
  *
  * Garantit qu'il n'existe qu'une seule instance de @ref TopologyData
  * partagée entre tous les modules du pipeline et la couche HMI.
  *
  * Usage :
  * @code
  *   TopologyRepository::instance().data().straights;
  * @endcode
  */
class TopologyRepository
{
public:
    /**
     * @brief Retourne l'instance unique du repository (création paresseuse).
     * @return Référence à l'instance unique.
     */
    static TopologyRepository& instance()
    {
        static TopologyRepository instance;
        return instance;
    }

    /**
     * @brief Retourne une référence mutable aux données topologiques.
     * @return Référence à @ref TopologyData.
     */
    TopologyData& data()
    {
        return m_data;
    }

    /** @brief Interdit la copie — singleton. */
    TopologyRepository(const TopologyRepository&) = delete;

    /** @brief Interdit l'affectation — singleton. */
    TopologyRepository& operator=(const TopologyRepository&) = delete;

private:
    /** @brief Constructeur privé — accès via @ref instance() uniquement. */
    TopologyRepository() = default;

    /** Données topologiques centralisées. */
    TopologyData m_data;
};
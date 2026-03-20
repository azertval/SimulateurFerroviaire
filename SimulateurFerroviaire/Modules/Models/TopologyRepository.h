#pragma once
#include "TopologyData.h"

/**
* @file TopologyRepository.h
* @brief Déclaration du singleton de stockage des données topologiques ferroviaires.
*
* La classe @ref TopologyRepository implémente le pattern Singleton pour fournir un accès global
* et unique à l'instance de @ref TopologyData, qui contient les listes de blocs "straight" et "switch".
*
* Responsabilités :
*  - Garantir qu'il n'existe qu'une seule instance de @ref TopologyData.
*  - Fournir un point d'accès global à cette instance via la méthode statique @ref instance().
*/
class TopologyRepository
{
public:
    static TopologyRepository& instance()
    {   
        static TopologyRepository instance;
        return instance;
    }

    TopologyData& data()
    {
        return m_data;
    }

private:
    TopologyData m_data;
};
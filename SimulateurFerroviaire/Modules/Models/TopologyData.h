#pragma once

#include <vector>

#include "StraightBlock.h"
#include "SwitchBlock.h"

/**
 * @brief Conteneur central des données topologiques ferroviaires.
 *
 * Sert de point de vérité unique partagé entre tous les modules métier et la couche HMI.
 * Contient les listes de blocs de type "straight" et "switch" extraits du parsing GeoJSON.
 */
class TopologyData
{
public:

    std::vector<StraightBlock> straights;
    std::vector<SwitchBlock> switches;

    void clear()
    {
        straights.clear();
        switches.clear();
    }
};
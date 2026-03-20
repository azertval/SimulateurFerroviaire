/**
 * @file  TopologyData.h
 * @brief Conteneur central des données topologiques ferroviaires.
 *
 * @see TopologyData
 */
#pragma once

#include <vector>

#include "StraightBlock.h"
#include "SwitchBlock.h"

 /**
  * @class TopologyData
  * @brief Conteneur partagé entre tous les modules métier et la couche HMI.
  *
  * Sert de point de vérité unique pour les listes de blocs extraites
  * du parsing GeoJSON.
  */
class TopologyData
{
public:
    /** Liste des blocs de voie droite extraits du parsing. */
    std::vector<StraightBlock> straights;

    /** Liste des aiguillages extraits du parsing. */
    std::vector<SwitchBlock> switches;

    /** @brief Vide les deux listes (remise à zéro entre deux parsings). */
    void clear()
    {
        straights.clear();
        switches.clear();
    }
};
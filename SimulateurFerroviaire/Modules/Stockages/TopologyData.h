/**
 * @file  TopologyData.h
 * @brief Conteneur central des données topologiques ferroviaires.
 *
 * @see TopologyData
 */
#pragma once

#include <memory>
#include <vector>

#include "Modules/InteractiveElements/ShuntingElements/StraightBlock.h"
#include "Modules/InteractiveElements/ShuntingElements/SwitchBlock.h"

 /**
  * @class TopologyData
  * @brief Conteneur partagé entre tous les modules métier et la couche HMI.
  *
  * Sert de point de vérité unique pour les listes de blocs extraites
  * du parsing GeoJSON.
  *
  * Les blocs sont détenus par unique_ptr afin de :
  *   - garantir un polymorphisme correct (pas de slicing),
  *   - autoriser le delete sur pointeur de base (destructeur virtuel),
  *   - interdire la copie accidentelle des éléments.
  */
class TopologyData
{
public:
    /** Liste des blocs de voie droite extraits du parsing. */
    std::vector<std::unique_ptr<StraightBlock>> straights;

    /** Liste des aiguillages extraits du parsing. */
    std::vector<std::unique_ptr<SwitchBlock>> switches;

    /** @brief Vide les deux listes (remise à zéro entre deux parsings). */
    void clear()
    {
        straights.clear();
        switches.clear();
    }
};
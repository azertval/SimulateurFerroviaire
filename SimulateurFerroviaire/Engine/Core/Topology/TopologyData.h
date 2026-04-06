/**
 * @file  TopologyData.h
 * @brief Conteneur central des données topologiques ferroviaires.
 *
 * @see TopologyData
 */
#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "Modules/Elements/ShuntingElements/StraightBlock.h"
#include "Modules/Elements/ShuntingElements/SwitchBlock.h"
#include "Modules/Elements/ShuntingElements/CrossBlocks/CrossBlock.h"

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
    std::vector<std::unique_ptr<StraightBlock>> straights;
    std::vector<std::unique_ptr<SwitchBlock>>   switches;
    std::vector<std::unique_ptr<CrossBlock>>   crossings;

    /**
     * Index de lookup rapide id → ptr, construit après résolution des pointeurs.
     * Permet un accès O(1) par ID sans find_if sur les vecteurs.
     */
    std::unordered_map<std::string, SwitchBlock*>   switchIndex;
    std::unordered_map<std::string, StraightBlock*> straightIndex;
    std::unordered_map<std::string, CrossBlock*> crossingsIndex;

    /**
     * @brief Vide les listes et les index (remise à zéro entre deux parsings).
     */
    void clear()
    {
        straights.clear();
        switches.clear();
        crossings.clear();
        switchIndex.clear();
        straightIndex.clear();
        crossingsIndex.clear();
    }

    /**
     * @brief Construit les index id→ptr depuis les vecteurs.
     *
     * Appeler en fin de pipeline GeoParser, après transfert en unique_ptr
     * et résolution de tous les pointeurs (partenaires, branches, voisins).
     * Les adresses des objets doivent être stables — aucune réallocation
     * de vecteur ne doit survenir après cet appel.
     */
    void buildIndex()
    {
        switchIndex.clear();
        straightIndex.clear();
        crossingsIndex.clear();

        switchIndex.reserve(switches.size());
        for (auto& sw : switches)
            switchIndex[sw->getId()] = sw.get();

        straightIndex.reserve(straights.size());
        for (auto& st : straights)
            straightIndex[st->getId()] = st.get();

        crossingsIndex.reserve(crossings.size());
        for (auto& cr : crossings)
            crossingsIndex[cr->getId()] = cr.get();
    }
};
#pragma once 
#include <string>
class Leaflet
{
    public:
        /*
        * @brief Retourne le code HTML complet pour intégrer Leaflet dans le WebView.
         * Ce code inclut les liens vers les ressources Leaflet et une carte centrée sur Paris.
         * Les fonctions JavaScript exposées permettent d'ajouter des marqueurs et des lignes depuis C++.
         *
         * @return std::wstring contenant le code HTML à charger dans le WebView.
        */
        static std::wstring leafletHtml();
};

#pragma once 
#include <string>
class Leaflet
{
    public:
        /*
         * @brief Utility class pou génerer le code HTML nécessaire pour intégrer Leaflet dans un WebView
         * 
         * Ce code inclut les liens vers les ressources Leaflet et une carte centrée sur Paris.
         * Les fonctions JavaScript exposées permettent d'ajouter des marqueurs et des lignes depuis C++.
         *
         * @return std::wstring Retourne le code HTML complet pour intégrer Leaflet dans le WebView.
        */
        static std::wstring leafletHtml();
};

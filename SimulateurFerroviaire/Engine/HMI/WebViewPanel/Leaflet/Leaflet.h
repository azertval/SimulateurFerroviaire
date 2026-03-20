/**
 * @file Leaflet.h
 * @brief Génération du HTML Leaflet pour intégration dans un WebView.
 *
 * @see Leaflet
 */

#pragma once
#include <string>

 /**
  * @class Leaflet
  * @brief Utilitaire statique de génération du code HTML Leaflet.
  *
  * Produit une page HTML autonome intégrant la bibliothèque Leaflet,
  * une carte centrée sur Paris et une API JavaScript exposée à C++.
  *
  * Usage :
  * @code
  *   m_webViewPanel.navigateToString(Leaflet::leafletHtml());
  * @endcode
  */
class Leaflet
{
public:

    /**
     * @brief Génère le code HTML complet pour intégrer Leaflet dans un WebView.
     *
     * Inclut les ressources CDN Leaflet, une carte centrée sur Paris et les
     * fonctions JavaScript @c addMarker, @c drawLine et @c loadGeoJson
     * appelables depuis C++ via @c executeScript.
     *
     * @return Page HTML complète prête à être passée à @c NavigateToString.
     */
    static std::wstring leafletHtml();

    /** @brief Classe non instanciable — constructeur supprimé. */
    Leaflet() = delete;
};
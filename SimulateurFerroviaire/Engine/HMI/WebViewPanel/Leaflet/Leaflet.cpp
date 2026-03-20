/**
 * @file Leaflet.cpp
 * @brief Implémentation du générateur HTML Leaflet.
 *
 * @see Leaflet
 */

#include "Leaflet.h"

std::wstring Leaflet::leafletHtml()
{
    return LR"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="utf-8" />
            <title>Leaflet Integration Test</title>

            <link rel="stylesheet" href="https://unpkg.com/leaflet/dist/leaflet.css"/>

            <style>
                html, body
                {
                    margin: 0;
                    padding: 0;
                    width: 100%;
                    height: 100%;
                    overflow: hidden;
                }

                #map
                {
                    width: 100%;
                    height: 100%;
                }
            </style>
        </head>
        <body>
            <div id="map"></div>

            <script src="https://unpkg.com/leaflet/dist/leaflet.js"></script>
            <script>
                const map = L.map('map').setView([48.8566, 2.3522], 13);

                L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                    attribution: '&copy; OpenStreetMap contributors'
                }).addTo(map);

                // ===== API exposée à C++ =====

                window.addMarker = function(lat, lon) {
                    L.circleMarker([lat, lon], {
                        radius: 6,
                        color: 'red'
                    }).addTo(map);
                };

                window.drawLine = function(coords) {
                    L.polyline(coords, {
                        color: 'blue'
                    }).addTo(map);
                };

                window.loadGeoJson = function(geojson) {
                    // Supprimer ancienne couche si besoin
                    if (window.currentLayer) {
                        map.removeLayer(window.currentLayer);
                    }

                    // Ajouter nouvelle couche
                    const layer = L.geoJSON(geojson);

                    layer.addTo(map);

                    // Sauvegarder référence
                    window.currentLayer = layer;

                    // Zoom automatique
                    const bounds = layer.getBounds();

                    if (bounds.isValid()) {
                        map.fitBounds(bounds);
                    }
                };

            </script>
        </body>
        </html>
    )";
}
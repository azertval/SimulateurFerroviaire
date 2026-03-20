const map = L.map('map').setView([48.8566, 2.3522], 13);

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; OpenStreetMap contributors'
}).addTo(map);

// ===== API exposée à C++ =====

// ================================
// Groupe des straights
// ================================
/** Groupe Leaflet contenant tous les blocs straight rendus. */
window.straightGroup = L.featureGroup().addTo(map);

/**
 * Supprime tous les blocs straight actuellement affichés sur la carte.
 */
window.clearStraightBlocks = function() {
    window.straightGroup.clearLayers();
};

/**
 * Affiche un bloc straight sous forme de polyligne sur la carte.
 *
 * @param {string}                  id      Identifiant du bloc.
 * @param {Array<[number, number]>} coords  Tableau de paires [lat, lon].
 */
window.renderStraightBlock = function(id, coords) {
    const polyline = L.polyline(coords, { color: 'LightSlateGray', weight: 4 });
    polyline.bindPopup("Straight: " + id);
    window.straightGroup.addLayer(polyline);
};

/**
 * Ajuste la vue de la carte pour englober tous les blocs straight visibles.
 */
window.zoomToStraights = function() {
    const bounds = window.straightGroup.getBounds();
    if (bounds.isValid()) {
        map.fitBounds(bounds, { padding: [20, 20], maxZoom: 18 });
    }
};

// ================================
// Groupe des switchs
// ================================
/** Groupe Leaflet contenant les marqueurs de jonction. */
window.switchGroup = L.featureGroup().addTo(map);

/** Groupe Leaflet contenant les branches des switchs (root / normal / deviation). */
window.switchBranchGroup = L.featureGroup().addTo(map);

/**
 * Supprime tous les marqueurs de switch affichés.
 */
window.clearSwitches = function() {
    window.switchGroup.clearLayers();
};

/**
 * Supprime toutes les branches de switch affichées.
 */
window.clearSwitchBranches = function() {
    window.switchBranchGroup.clearLayers();
};

/**
 * Affiche le point de jonction d'un switch.
 *
 * @param {string}  id       Identifiant du switch.
 * @param {number}  lat      Latitude de la jonction.
 * @param {number}  lon      Longitude de la jonction.
 * @param {boolean} isDouble True si double aiguille.
 */
window.renderSwitch = function(id, lat, lon, isDouble) {
    const color = "orange";

    const marker = L.circleMarker([lat, lon], {
        radius: 1,
        color: color,
        weight: 4,
        fillColor: color,
        fillOpacity: 1
    });

    marker.bindPopup(
        "<b>Switch:</b> " + id + "<br>" +
        "<b>Double:</b> " + isDouble
    );

    window.switchGroup.addLayer(marker);
};

/**
 * Affiche les trois branches d'un switch (root / normal / deviation)
 * sous forme de segments colorés depuis la jonction vers chaque tip CDC.
 *
 *
 * Un tip absent (coordonnées nulles) est silencieusement ignoré.
 *
 * @param {string} id          Identifiant du switch.
 * @param {number} jLat        Latitude  de la jonction.
 * @param {number} jLon        Longitude de la jonction.
 * @param {number} rootLat     Latitude  du tip root    (NaN si absent).
 * @param {number} rootLon     Longitude du tip root    (NaN si absent).
 * @param {number} normalLat   Latitude  du tip normal  (NaN si absent).
 * @param {number} normalLon   Longitude du tip normal  (NaN si absent).
 * @param {number} devLat      Latitude  du tip deviation (NaN si absent).
 * @param {number} devLon      Longitude du tip deviation (NaN si absent).
 */
window.renderSwitchBranches = function(
    id,
    jLat, jLon,
    rootLat, rootLon,
    normalLat, normalLon,
    devLat, devLon)
{
    const junction = [jLat, jLon];

    const branches = [
        { role: "deviation", lat: devLat, lon: devLon, color: "Gainsboro" },
        { role: "root", lat: rootLat, lon: rootLon, color: "LightSlateGray"  },
        { role: "normal", lat: normalLat, lon: normalLon, color: "LightSlateGray"  }      
    ];

    for (const branch of branches) {
        if (isNaN(branch.lat) || isNaN(branch.lon)) continue;

        const tip = [branch.lat, branch.lon];
        const options = {
            color:     branch.color,
            weight:    4
        };

        const segment = L.polyline([junction, tip], options);
        segment.bindPopup(
            "<b>Switch:</b> " + id + "<br>" +
            "<b>Branch:</b> " + branch.role
        );
        window.switchBranchGroup.addLayer(segment);
    }
};

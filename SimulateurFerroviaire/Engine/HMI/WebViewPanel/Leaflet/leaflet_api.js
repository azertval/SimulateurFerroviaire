const map = L.map('map').setView([48.8566, 2.3522], 13);

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; OpenStreetMap contributors'
}).addTo(map);

// ===== API exposée à C++ =====

/**
 * Ajoute un marqueur circulaire rouge sur la carte.
 *
 * @param {number} lat  Latitude du point.
 * @param {number} lon  Longitude du point.
*/
window.addMarker = function(lat, lon) {
    L.circleMarker([lat, lon], { radius: 6, color: 'red' }).addTo(map);
};

/**
 * Dessine une polyligne bleue à partir d'un tableau de coordonnées.
 *
 * @param {Array<[number, number]>} coords  Tableau de paires[lat, lon].
 */
window.drawLine = function(coords) {
    L.polyline(coords, { color: 'blue' }).addTo(map);
};

/**
 * Charge et affiche une couche GeoJSON sur la carte.
 * Remplace la couche précédente si elle existe, puis zoom automatiquement.
 *
 * @param {object} geojson  Objet GeoJSON valide à afficher.
*/
window.loadGeoJson = function(geojson) {
    if (window.currentLayer) {
        map.removeLayer(window.currentLayer);
    }
    const layer = L.geoJSON(geojson);
    layer.addTo(map);
    window.currentLayer = layer;
    const bounds = layer.getBounds();
    if (bounds.isValid()) {
        map.fitBounds(bounds);
    }
};

/** Groupe Leaflet contenant tous les blocs straight rendus. */
window.straightGroup = L.featureGroup().addTo(map);

/**
 * Supprime tous les blocs straight actuellement affichés sur la carte.
 */
window.clearStraightBlocks = function() {
    window.straightGroup.clearLayers();
};

/**
 * Réduit une polyligne en coupant une fraction de sa longueur à chaque extrémité.
 * Le découpage suit la géométrie réelle des segments (pas un simple déplacement
 * des points extrêmes), ce qui évite les plis sur les polylignes multi-segments.
 *
 * @param {Array<[number, number]>} coords  Tableau de paires[lat, lon].
 * @param {number}                  ratio   Fraction de la longueur totale à couper
 *                                          de chaque côté (défaut: 0.1 = 10 %).
 * @returns {Array<[number, number]>}        Nouveau tableau de coordonnées réduit.
*/
function shrinkLine(coords, ratio = 0.1) {
    if (coords.length < 2) return coords;

    let totalLength = 0;
    for (let i = 0; i < coords.length - 1; i++) {
        const dx = coords[i + 1][0] - coords[i][0];
        const dy = coords[i + 1][1] - coords[i][1];
        totalLength += Math.sqrt(dx * dx + dy * dy);
    }

    const trim = totalLength * ratio;

    /**
    * Avance le long des segments et insère un nouveau point de départ
    * à exactement `distance` unités du début.
    *
    * @param {Array<[number, number]>} pts       Tableau de coordonnées.
    * @param {number}                  distance  Distance à parcourir depuis le début.
    * @returns {Array<[number, number]>}
    */
    function trimStart(pts, distance) {
        let remaining = distance;
        for (let i = 0; i < pts.length - 1; i++) {
            const dx = pts[i + 1][0] - pts[i][0];
            const dy = pts[i + 1][1] - pts[i][1];
            const segLen = Math.sqrt(dx * dx + dy * dy);
            if (remaining <= segLen) {
                const t = remaining / segLen;
                const newStart = [pts[i][0] + dx * t, pts[i][1] + dy * t];
                return [newStart, ...pts.slice(i + 1)];
            }
            remaining -= segLen;
        }
        return pts;
    }

    const trimmed = trimStart(coords, trim);
    const reversed = trimStart([...trimmed].reverse(), trim);
    return reversed.reverse();
}

/**
 * Affiche un bloc straight sous forme de polyligne sur la carte.
 * Les extrémités sont légèrement tronquées pour visualiser les jonctions
 * entre blocs adjacents.
 *
 * @param {string}                  id      Identifiant du bloc straight (affiché dans le popup).
 * @param {Array<[number, number]>} coords  Tableau de paires[lat, lon]définissant la polyligne.
 */
window.renderStraightBlock = function(id, coords) {
    const polyline = L.polyline(shrinkLine(coords, 0.01), { color: 'LightSlateGray', weight: 4 });
    polyline.bindPopup("Straight: " + id);
    window.straightGroup.addLayer(polyline);
};

/**
 * Ajuste la vue de la carte pour englober tous les blocs straight visibles.
 * Sans effet si aucun bloc n'est affiché.
 */
window.zoomToStraights = function() {
    const bounds = window.straightGroup.getBounds();
    if (bounds.isValid()) {
        map.fitBounds(bounds, { padding: [20, 20], maxZoom: 18 });
    }
};
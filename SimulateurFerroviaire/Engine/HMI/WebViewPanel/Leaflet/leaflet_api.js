const map = L.map('map').setView([48.8566, 2.3522], 13);

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; OpenStreetMap contributors'
}).addTo(map);

// ===== API exposée à C++ =====

// ================================
// Couleurs
// ================================

const COLOR_ACTIVE   = "LightSlateGray";  // branche active   (normal par défaut)
const COLOR_INACTIVE = "Gainsboro";       // branche inactive (deviation par défaut)
const COLOR_SWITCH    = "orange";       // cercle au repos


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
    const polyline = L.polyline(coords, { color: COLOR_ACTIVE, weight: 4 });
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

/** Registre des polylines de branches par switch id : { normal, deviation } */
window.switchBranchMap = {};

/**
 * Registre des fonctions d'application d'état par switch id.
 * Signature : applyStateFn(toDeviation: boolean)
 * Permet la synchronisation entre partenaires d'un double switch.
 */
window.switchToggleMap = {};

/**
 * Supprime tous les marqueurs de switch affichés.
 */
window.clearSwitches = function() {
    window.switchGroup.clearLayers();
    window.switchToggleMap = {};
};

/**
 * Supprime toutes les branches de switch affichées.
 */
window.clearSwitchBranches = function() {
    window.switchBranchGroup.clearLayers();
    window.switchBranchMap = {};
};

/**
 * Affiche le point de jonction d'un switch sous forme de cercle.
 *
 * Couleur au repos  : orange  (branche normale active)
 * Couleur basculée  : bleu    (branche déviée active)
 *
 * Au clic :
 *  - inverse l'état du switch (normal ↔ deviation)
 *  - met à jour la couleur du cercle
 *  - met à jour les couleurs des branches (normal ↔ deviation)
 *  - si double switch, applique le même état au partenaire
 *
 * @param {string}  id               Identifiant du switch.
 * @param {number}  lat              Latitude de la jonction.
 * @param {number}  lon              Longitude de la jonction.
 * @param {boolean} isDouble         True si double aiguille.
 * @param {number}  bearingNormal    Réservé (non utilisé — cercle sans direction).
 * @param {number}  bearingDeviation Réservé (non utilisé — cercle sans direction).
 * @param {string}  partnerId        ID du switch partenaire (double switch), "" sinon.
 */
window.renderSwitch = function(id, lat, lon, isDouble, bearingNormal, bearingDeviation, partnerId) {

    // État interne : false = normal actif, true = deviation active
    let pointingToDeviation = false;

    const circle = L.circleMarker([lat, lon], {
        radius:      3,
        color:       "white",
        weight:      1.5,
        fillColor:   COLOR_SWITCH,
        fillOpacity: 1
    });

    window.switchGroup.addLayer(circle);

    /**
     * Applique un état visuel sur CE switch uniquement.
     * Appelé directement au clic, ou par le partenaire pour synchronisation.
     *
     * @param {boolean} toDeviation  True = deviation active.
     */
    function applyState(toDeviation) {
        // Couleurs des branches
        const branches = window.switchBranchMap[id];
        if (branches) {
            if (branches.normal)
                branches.normal.setStyle({ color: toDeviation ? COLOR_INACTIVE : COLOR_ACTIVE });
            if (branches.deviation)
                branches.deviation.setStyle({ color: toDeviation ? COLOR_ACTIVE : COLOR_INACTIVE });
        }
    }

    // Enregistrement dans le registre global pour synchronisation inter-switches
    window.switchToggleMap[id] = function(toDeviation) {
        pointingToDeviation = toDeviation;
        applyState(toDeviation);
    };

    circle.on("click", function() {
        pointingToDeviation = !pointingToDeviation;

        applyState(pointingToDeviation);

        // Synchronisation du partenaire (double switch)
        if (isDouble && partnerId && window.switchToggleMap[partnerId]) {
            window.switchToggleMap[partnerId](pointingToDeviation);
        }
    });
};

/**
 * Affiche les trois branches d'un switch (root / normal / deviation)
 * sous forme de segments colorés depuis la jonction vers chaque tip CDC.
 *
 * État initial :
 *   normal    → COLOR_ACTIVE   (LightSlateGray)
 *   deviation → COLOR_INACTIVE (Gainsboro)
 *   root      → COLOR_ACTIVE
 *
 * Un tip absent (NaN) est silencieusement ignoré.
 *
 * @param {string} id        Identifiant du switch.
 * @param {number} jLat      Latitude  de la jonction.
 * @param {number} jLon      Longitude de la jonction.
 * @param {number} rootLat   Latitude  du tip root      (NaN si absent).
 * @param {number} rootLon   Longitude du tip root      (NaN si absent).
 * @param {number} normalLat Latitude  du tip normal    (NaN si absent).
 * @param {number} normalLon Longitude du tip normal    (NaN si absent).
 * @param {number} devLat    Latitude  du tip deviation (NaN si absent).
 * @param {number} devLon    Longitude du tip deviation (NaN si absent).
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
        { role: "root",      lat: rootLat,   lon: rootLon,   color: COLOR_ACTIVE   },
        { role: "normal",    lat: normalLat, lon: normalLon, color: COLOR_ACTIVE   },
        { role: "deviation", lat: devLat,    lon: devLon,    color: COLOR_INACTIVE }
    ];

    window.switchBranchMap[id] = {};

    for (const branch of branches) {
        if (isNaN(branch.lat) || isNaN(branch.lon)) continue;

        const segment = L.polyline([[jLat, jLon], [branch.lat, branch.lon]], {
            color:  branch.color,
            weight: 4
        });

        segment.bindPopup(
            "<b>Switch:</b> " + id + "<br>" +
            "<b>Branch:</b> " + branch.role
        );

        window.switchBranchGroup.addLayer(segment);

        // Seules normal et deviation sont concernées par le toggle de couleur
        if (branch.role === "normal" || branch.role === "deviation") {
            window.switchBranchMap[id][branch.role] = segment;
        }
    }
};

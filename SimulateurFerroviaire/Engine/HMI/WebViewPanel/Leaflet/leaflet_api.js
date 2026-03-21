const map = L.map('map').setView([48.8566, 2.3522], 13);

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; OpenStreetMap contributors'
}).addTo(map);

// ===== API exposée à C++ =====

// ================================
// Couleurs
// ================================

const COLOR_ACTIVE   = "LightSlateGray";
const COLOR_BRANCH_INACTIVE = "Gainsboro";
const COLOR_SWITCH_JUNCTION    = "orange";


// ================================
// Groupe des straights
// ================================

window.straightGroup = L.featureGroup().addTo(map);

window.clearStraightBlocks = function() {
    window.straightGroup.clearLayers();
};

/**
 * @param {string}                  id
 * @param {Array<[number, number]>} coords  Tableau de paires [lat, lon].
 */
window.renderStraightBlock = function(id, coords) {
    const polyline = L.polyline(coords, { color: COLOR_ACTIVE, weight: 4 });
    polyline.bindPopup("Straight: " + id);
    window.straightGroup.addLayer(polyline);
};

window.zoomToStraights = function() {
    const bounds = window.straightGroup.getBounds();
    if (bounds.isValid())
        map.fitBounds(bounds, { padding: [20, 20], maxZoom: 18 });
};


// ================================
// Groupe des switchs
// ================================

window.switchGroup       = L.featureGroup().addTo(map);
window.switchBranchGroup = L.featureGroup().addTo(map);

/** id → { normal: polyline|null, deviation: polyline|null } */
window.switchBranchMap = {};

/** id → applyStateFn(toDeviation: boolean) */
window.switchToggleMap = {};

window.clearSwitches = function() {
    window.switchGroup.clearLayers();
    window.switchToggleMap = {};
};

window.clearSwitchBranches = function() {
    window.switchBranchGroup.clearLayers();
    window.switchBranchMap = {};
};

/**
 * Affiche le point de jonction d'un switch sous forme de cercle cliquable.
 *
 * @param {string}  id
 * @param {number}  lat
 * @param {number}  lon
 * @param {boolean} isDouble
 * @param {number}  bearingNormal     (réservé, non utilisé — cercle sans direction)
 * @param {number}  bearingDeviation  (réservé, non utilisé)
 * @param {string}  partnerId         ID du partenaire, "" si pas un double.
 */
window.renderSwitch = function(id, lat, lon, isDouble, bearingNormal, bearingDeviation, partnerId) {

    let pointingToDeviation = false;

    const circle = L.circleMarker([lat, lon], {
        radius:      3,
        color:       "white",
        weight:      1.5,
        fillColor:   COLOR_SWITCH_JUNCTION,
        fillOpacity: 1
    });

    window.switchGroup.addLayer(circle);

    function applyState(toDeviation) {
        circle.setStyle({
            fillColor: COLOR_SWITCH_JUNCTION
        });

        const branches = window.switchBranchMap[id];
        if (branches) {
            if (branches.normal)
                branches.normal.setStyle({ color: toDeviation ? COLOR_BRANCH_INACTIVE : COLOR_ACTIVE });
            if (branches.deviation)
                branches.deviation.setStyle({ color: toDeviation ? COLOR_ACTIVE : COLOR_BRANCH_INACTIVE });
        }
    }

    window.switchToggleMap[id] = function(toDeviation) {
        pointingToDeviation = toDeviation;
        applyState(toDeviation);
    };

    circle.on("click", function() {
        pointingToDeviation = !pointingToDeviation;
        applyState(pointingToDeviation);

        if (isDouble && partnerId && window.switchToggleMap[partnerId])
            window.switchToggleMap[partnerId](pointingToDeviation);

        window.chrome.webview.postMessage(JSON.stringify({
            type: "switch_click",
            id: id,
            partnerId: partnerId //Empty for single switch
        }));
    });  
};

/**
 * Affiche les branches d'un switch depuis la jonction.
 *
 * Chaque branche est transmise comme une polyligne de points intermédiaires
 * à ajouter APRÈS la jonction :
 *   - Switch simple : [[tipLat, tipLon]]  (un seul point)
 *   - Double switch : [[pt1Lat,pt1Lon], [pt2Lat,pt2Lon], …, [junctionPartenaireLat, lon]]
 *   - Absent        : null
 *
 * La jonction ([jLat, jLon]) est toujours le premier point de chaque segment.
 *
 * @param {string}               id
 * @param {number}               jLat
 * @param {number}               jLon
 * @param {Array|null}           rootCoords     Polyligne après junction, null si absent.
 * @param {Array|null}           normalCoords   Idem.
 * @param {Array|null}           devCoords      Idem.
 */
window.renderSwitchBranches = function(id, jLat, jLon, rootCoords, normalCoords, devCoords)
{
    const junction = [jLat, jLon];

    window.switchBranchMap[id] = { normal: null, deviation: null };

    const defs = [
        { role: "root",      coords: rootCoords,   color: COLOR_ACTIVE   },
        { role: "normal",    coords: normalCoords, color: COLOR_ACTIVE   },
        { role: "deviation", coords: devCoords,    color: COLOR_BRANCH_INACTIVE }
    ];

    for (const def of defs) {
        if (!def.coords || def.coords.length === 0) continue;

        // Construit la polyligne complète : jonction + tous les points transmis
        const fullLine = [junction, ...def.coords];

        const segment = L.polyline(fullLine, { color: def.color, weight: 4 });
        segment.bindPopup("<b>Switch:</b> " + id + "<br><b>Branch:</b> " + def.role);
        window.switchBranchGroup.addLayer(segment);

        if (def.role === "normal" || def.role === "deviation")
            window.switchBranchMap[id][def.role] = segment;
    }
};

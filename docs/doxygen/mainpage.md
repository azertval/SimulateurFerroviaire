@mainpage Simulateur Ferroviaire

@tableofcontents

---

# Description {#description}

Reconstruction et visualisation d'un réseau ferroviaire à partir de données GeoJSON.

Le projet repose sur un pipeline de parsing permettant de :
- Charger des données géographiques (WGS-84 / GeoJSON)
- Construire un graphe topologique métrique (UTM)
- Extraire les blocs ferroviaires (voies droites + aiguillages)
- Orienter et valider les aiguillages (root / normal / deviation)
- Détecter les doubles aiguilles et absorber les segments de liaison
- Résoudre les pointeurs inter-blocs et construire les index de lookup
- Stocker le modèle dans un singleton partagé et le visualiser dans un WebView
- Permettre une interaction bidirectionnelle Leaflet ↔ C++ (clic → mise à jour modèle → rendu)

---

# Engine — Moteur de l'application {#engine}

## Core — Cœur applicatif {#core}

Gestion des composants fondamentaux et de la logique applicative transverse.

| Classe | Rôle |
|--------|------|
| Application | Cycle de vie Win32 (enregistrement de classe, boucle de messages) |
| Logger | Journalisation structurée (INFO / DEBUG / WARNING / ERROR / FAILURE) |
| @ref TopologyData | Conteneur `unique_ptr<StraightBlock>` + `unique_ptr<SwitchBlock>` + index de lookup |
| @ref TopologyRepository | Singleton (Meyers). Accès global à @ref TopologyData via `instance().data()` |
| @ref TopologyRenderer | classe statique génére le rendu JS|

### TopologyData

> **Pourquoi `unique_ptr` ?**
> Les blocs sont polymorphes (`InteractiveElement*`). Le stockage par valeur entraînerait
> du slicing et interdirait le déplacement. Le stockage par `unique_ptr` garantit :
> - polymorphisme correct (destructeur virtuel respecté),
> - propriété exclusive et cycle de vie déterministe,
> - interdiction de copie accidentelle.

`TopologyData` expose deux index construits en fin de pipeline via `buildIndex()` :

| Index | Type | Usage |
|-------|------|-------|
| `switchIndex` | `unordered_map<string, SwitchBlock*>` | Lookup O(1) par ID pour `onSwitchClick` |
| `straightIndex` | `unordered_map<string, StraightBlock*>` | Lookup O(1) par ID |

> `buildIndex()` doit être appelé après que toutes les adresses sont stables
> (transfert `unique_ptr` + résolution des pointeurs terminés). `clear()` vide
> également les index.

### TopologyRenderer — Rendu et exportation {#renderer}

@ref TopologyRenderer est une classe statique qui :
- Sérialise @ref StraightBlock → feature GeoJSON `LineString`
- Sérialise @ref SwitchBlock → feature GeoJSON `Point`
- Génère les scripts JavaScript d'injection pour le WebView Leaflet
- Met à jour le rendu d'un switch et de ses partenaires via `updateSwitchBlocks(sw)`

| Méthode | Rôle |
|---------|------|
| `renderAllStraightBlocks()` | Efface et redessine tous les StraightBlocks |
| `renderAllSwitchBranches()` | Efface et redessine toutes les branches de switch |
| `renderAllSwitchBlocksJunctions()` | Efface et redessine tous les marqueurs de jonction |
| `updateSwitchBlocks(sw)` | Met à jour visuellement un switch et ses partenaires double |
| `exportToFile(path)` | Export GeoJSON complet vers un fichier |

----

## HMI — Interface utilisateur {#hmi}

Couche graphique Win32 + WebView2.

| Classe | Rôle |
|--------|------|
| MainWindow | Fenêtre principale, routage des messages Win32, coordination UI ↔ métier |
| ProgressBar | Wrapper du contrôle natif `PROGRESS_CLASS` |
| WebViewPanel | Affichage cartographique Leaflet embarqué via WebView2 |
| AboutDialog | Boîte de dialogue modale "À propos" |
| FileOpenDialog | Sélecteur de fichier GeoJSON (GetOpenFileNameA) |
| FileSaveDialog | Dialogue de sauvegarde GeoJSON (GetSaveFileNameA) |

### Binding bidirectionnel Leaflet ↔ C++ {#binding}

Le binding repose sur deux canaux :

**C++ → Leaflet** : `WebViewPanel::executeScript()` injecte des appels JS
(`window.switchApplyState`, `window.renderSwitchBlock`, etc.) générés par `TopologyRenderer`.

**Leaflet → C++** : `window.chrome.webview.postMessage()` envoie un JSON vers
`WebViewPanel::onWebMessageReceived()`, qui dispatche vers `MainWindow::onWebMessage()`.

```
Clic Leaflet
  → postMessage({type:"switch_click", id:"sw/0"})
  → WebViewPanel::onWebMessageReceived()     [UTF-16 → UTF-8]
  → MainWindow::onWebMessage()               [parse JSON, dispatch]
  → MainWindow::onSwitchClick()              [switchIndex.find() O(1)]
  → SwitchBlock::toggleActiveBranch()        [+ propagation partenaires]
  → TopologyRenderer::updateSwitchBlocks()   [script JS]
  → WebViewPanel::executeScript()
  → window.switchApplyState()                [couleur cercle + branches]
```

Messages JS supportés :

| `type` | Champs | Handler C++ |
|--------|--------|-------------|
| `switch_click` | `id` | `MainWindow::onSwitchClick()` |

> **Conception** : Leaflet ne gère aucun état — il notifie C++ au clic et attend
> une instruction de rendu. C++ est l'autorité sur `ActiveBranch`.

> **WebViewPanel** : `setOnMessageReceived()` permet à MainWindow de brancher
> sa logique sans couplage. La conversion UTF-16 → UTF-8 utilise `WideCharToMultiByte`.

---

# Modules — Fonctionnalités métier {#modules}

---

# GeoParser — Pipeline principal {#geoparser}

Pipeline complet de transformation **GeoJSON → modèle ferroviaire**.

## Tâche asynchrone {#task}

@ref GeoParsingTask lance @ref GeoParser dans un thread détaché.
La communication vers l'UI passe exclusivement par `PostMessage` :

| Message | Contenu |
|---------|---------|
| `WM_PROGRESS_UPDATE` | Avancement 0–100 |
| `WM_PARSING_SUCCESS` | Parsing terminé |
| `WM_PARSING_ERROR` | Pointeur `std::string*` (à libérer par le destinataire) |

## Pipeline global {#pipeline}

Voir @ref GeoParser::parse() pour l'implémentation complète.

| Phase | Classe | Description |
|-------|--------|-------------|
| 1–2 | GraphBuilder | Chargement GeoJSON, projection WGS-84 → UTM, snap + fusion des nœuds |
| 3–5 | TopologyExtractor | Détection des aiguillages (degré == 3), extraction des StraightBlock, câblage |
| 6 | SwitchOrientator | Orientation root / normal / deviation, calcul des tips CDC |
| 7–8 | DoubleSwitchDetector | Détection des doubles aiguilles, absorption du segment de liaison, validation CDC |
| 9 | GeoParser | Clear + transfert vers @ref TopologyRepository |
| 10 | GeoParser | Résolution des pointeurs partenaires, branches, voisins + `buildIndex()` |

> **Phase 3 — Détection des aiguillages** : seuls les nœuds de degré exactement 3
> (jonction en Y) sont des `SwitchBlock`. Les nœuds de degré > 3 (croisements en X)
> sont des frontières de découpe pour les `StraightBlock` mais ne génèrent pas d'aiguillage.

> **Phase 10 — Résolution des pointeurs** : après transfert en `unique_ptr`,
> les adresses sont stables. Chaque bloc reçoit ses pointeurs directs via
> `setBranchPointers()` / `setNeighbourPointers()`, puis `buildIndex()` construit
> les tables de lookup O(1).

## Parsing & Construction {#parsing}

| Classe | Rôle |
|--------|------|
| @ref GeoParser | Orchestrateur du pipeline |
| @ref GeoParsingTask | Exécution asynchrone (évite le blocage UI) |
| @ref GeometryUtils | Projection UTM, interpolation, angles, snap de grille |

## Graphe topologique {#graph}

| Classe | Rôle |
|--------|------|
| @ref GraphBuilder | Construction du graphe depuis le GeoJSON |
| @ref TopologyGraph | Graphe planaire non-orienté (nœuds + arêtes), union-find |
| @ref TopologyEdge | Arête avec géométrie polyligne métrique et longueur planaire |

## Extraction ferroviaire {#extraction}

| Classe | Rôle |
|--------|------|
| @ref TopologyExtractor | Extraction des blocs depuis le graphe |
| @ref SwitchOrientator | Orientation géométrique (phases 6, 6b, 6c, 6d) |
| @ref DoubleSwitchDetector | Détection des doubles aiguilles et validation CDC |

---

# Eléments interactifs {#elements}

| Classe | Rôle |
|--------|------|
| @ref InteractiveElement | Interface abstraite commune (id, type). Logger statique partagé (`Logs/InteractiveElements.log`). Copie interdite, déplacement autorisé |
| @ref ShuntingElement | Étend avec un état opérationnel + helpers `isFree()` / `isOccupied()` / `isInactive()` |
| @ref StraightBlock | Tronçon de voie droite. Longueur géodésique Haversine. Voisins résolus via `StraightNeighbours` |
| @ref SwitchBlock | Aiguillage 3 branches. Orientation + tips CDC + support double aiguille + état opérationnel `ActiveBranch` |

## Hiérarchie des éléments interactifs {#hierarchy}

```
InteractiveElement          getId(), getType(), m_logger (static)
    └── ShuntingElement     getState()  [FREE / OCCUPIED / INACTIVE]
            ├── StraightBlock
            └── SwitchBlock
```

> **Logger statique** : `InteractiveElement::m_logger` est partagé par toutes les
> instances de toutes les classes dérivées → un seul fichier `Logs/InteractiveElements.log`.

## Pointeurs résolus post-parsing {#pointers}

Après `buildIndex()`, chaque bloc dispose de pointeurs directs vers ses voisins :

**`StraightBlock::StraightNeighbours`**

| Champ | Type | Description |
|-------|------|-------------|
| `prev` | `ShuntingElement*` | Bloc adjacent à l'extrémité A |
| `next` | `ShuntingElement*` | Bloc adjacent à l'extrémité B |

**`SwitchBlock::SwitchBranches`**

| Champ | Type | Description |
|-------|------|-------------|
| `root` | `ShuntingElement*` | Bloc sur la branche root |
| `normal` | `ShuntingElement*` | Bloc sur la branche normale (ou partenaire double) |
| `deviation` | `ShuntingElement*` | Bloc sur la branche déviée (ou partenaire double) |

## État opérationnel des aiguillages {#activestate}

`SwitchBlock` expose un état runtime modifiable par l'opérateur :

| Méthode | Description |
|---------|-------------|
| `getActiveBranch()` | Retourne `ActiveBranch::NORMAL` ou `DEVIATION` |
| `isDeviationActive()` | Raccourci booléen |
| `setActiveBranch(branch)` | Assigne l'état + propage aux partenaires |
| `toggleActiveBranch()` | Alterne + propage + retourne le nouvel état |

La propagation aux partenaires est gérée directement par `SwitchBlock` via
`getPartnerOnNormal()` / `getPartnerOnDeviation()` (pointeurs résolus en Phase 10).
`MainWindow` n'a plus à gérer la logique de propagation.

## Énumérations clés {#enums}

| Enum | Valeurs | Usage |
|------|---------|-------|
| `InteractiveElementType` | `SWITCH`, `STRAIGHT` | Typage sans RTTI |
| `ShuntingState` | `FREE`, `OCCUPIED`, `INACTIVE` | État opérationnel infrastructure |
| `ActiveBranch` | `NORMAL`, `DEVIATION` | Position opérationnelle de l'aiguillage |

---

# Coordonnées {#coords}

| Classe | Système |
|--------|---------|
| @ref LatLon | Géographique WGS-84 (latitude, longitude) |
| @ref CoordinateXY | Métrique UTM (x = est, y = nord) |

---

# Licence {#licence}

Ce projet est distribué sous licence :

**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)**

https://creativecommons.org/licenses/by-nc-sa/4.0/

- ✔️ Partager — copier et redistribuer le matériel
- ✔️ Adapter — remixer, transformer et créer à partir du matériel
- 📝 Attribution requise
- 🚫 Usage commercial interdit
- 🔁 Redistribution sous la même licence

---

# Auteur {#auteur}

© 2026 Valentin Eloy
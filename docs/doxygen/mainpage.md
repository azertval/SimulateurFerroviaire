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
- Stocker le modèle dans un singleton partagé et le visualiser dans un WebView

---

# Engine — Moteur de l'application {#engine}

## Core — Cœur applicatif {#core}

Gestion des composants fondamentaux et de la logique applicative transverse.

| Classe | Rôle |
|--------|------|
| Application | Cycle de vie Win32 (enregistrement de classe, boucle de messages) |
| Logger | Journalisation structurée (INFO / DEBUG / WARNING / ERROR / FAILURE) |

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

> **WebView** : `leaflet_api.js` décrit l'ensemble des scripts d'injection JavaScript
> exécutés via `WebViewPanel::executeScript()`.

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
| 3–5 | TopologyExtractor | Détection des aiguillages, extraction des StraightBlock, câblage |
| 6 | SwitchOrientator | Orientation root / normal / deviation, calcul des tips CDC |
| 7–8 | DoubleSwitchDetector | Détection des doubles aiguilles, validation CDC (longueurs min.) |
| 9 | GeoParser | Clear + transfert vers @ref TopologyRepository |

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

# GeoJsonExporter — Exportation {#exporter}

@ref GeoJsonExporter est une classe statique qui :
- Sérialise @ref StraightBlock → feature GeoJSON `LineString`
- Sérialise @ref SwitchBlock → feature GeoJSON `Point`
- Génère les scripts JavaScript d'injection pour le WebView Leaflet

---

# Eléments interactifs {#elements}

| Classe | Rôle |
|--------|------|
| @ref InteractiveElement | Interface abstraite commune (id, type). Copie interdite, déplacement autorisé |
| @ref ShuntingElement | Étend avec un état opérationnel + helpers `isFree()` / `isOccupied()` / `isInactive()` |
| @ref StraightBlock | Tronçon de voie droite. Longueur géodésique calculée via Haversine |
| @ref SwitchBlock | Aiguillage 3 branches. Orientation + tips CDC + support double aiguille |

## Hiérarchie des éléments interactifs {#hierarchy}

Tous les éléments ferroviaires interactifs s'inscrivent dans la hiérarchie suivante :

```
InteractiveElement          getId(), getType()
    └── ShuntingElement     getState()  [FREE / OCCUPIED / INACTIVE]
            ├── StraightBlock
            └── SwitchBlock
```

## Énumérations clés {#enums}

| Enum | Valeurs | Usage |
|------|---------|-------|
| `InteractiveElementType` | `SWITCH`, `STRAIGHT` | Typage sans RTTI |
| `ShuntingState` | `FREE`, `OCCUPIED`, `INACTIVE` | État opérationnel temps réel |

---

## Stockages {#storage}

| Classe | Rôle |
|--------|------|
| @ref TopologyData | Conteneur `unique_ptr<StraightBlock>` + `unique_ptr<SwitchBlock>`. Garantit le polymorphisme sans slicing |
| @ref TopologyRepository | Singleton (Meyers). Accès global à @ref TopologyData via `instance().data()` |

> **Pourquoi `unique_ptr` ?**
> Les blocs sont polymorphes (`ShuntingElement*`). Le stockage par valeur entraînerait
> du slicing et interdirait le déplacement. Le stockage par `unique_ptr` garantit :
> - polymorphisme correct (destructeur virtuel respecté),
> - propriété exclusive et cycle de vie déterministe,
> - interdiction de copie accidentelle.
 
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
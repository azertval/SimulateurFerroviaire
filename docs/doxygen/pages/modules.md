@page modules Modules — Fonctionnalités métier

@tableofcontents

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

## Classes du pipeline {#parsing}

| Classe | Rôle |
|--------|------|
| @ref GeoParser | Orchestrateur du pipeline |
| @ref GeoParsingTask | Exécution asynchrone (évite le blocage UI) |
| @ref GeometryUtils | Projection UTM, interpolation, angles, snap de grille |
| @ref GraphBuilder | Construction du graphe depuis le GeoJSON (phases 1–2) |
| @ref TopologyGraph | Graphe planaire non-orienté (nœuds + arêtes), union-find |
| @ref TopologyEdge | Arête avec géométrie polyligne métrique et longueur planaire |
| @ref TopologyExtractor | Extraction des blocs depuis le graphe (phases 3–5) |
| @ref SwitchOrientator | Orientation géométrique (phases 6, 6b, 6c, 6d) |
| @ref DoubleSwitchDetector | Détection des doubles aiguilles et validation CDC (phases 7–8) |

---

# Éléments interactifs {#elements}

| Classe | Rôle |
|--------|------|
| @ref InteractiveElement | Interface abstraite commune (id, type). Logger statique partagé. Copie interdite, déplacement autorisé |
| @ref ShuntingElement | Étend avec un état opérationnel + helpers `isFree()` / `isOccupied()` / `isInactive()` |
| @ref StraightBlock | Tronçon de voie droite. Longueur géodésique Haversine. Voisins résolus via `StraightNeighbours` |
| @ref SwitchBlock | Aiguillage 3 branches. Orientation + tips CDC + support double aiguille + état `ActiveBranch` |

## Hiérarchie {#hierarchy}

```
InteractiveElement          getId(), getType(), m_logger (static)
    └── ShuntingElement     getState()  [FREE / OCCUPIED / INACTIVE]
            ├── StraightBlock
            └── SwitchBlock
```

> **Logger statique** : `InteractiveElement::m_logger` est partagé par toutes les
> instances → un seul fichier `Logs/InteractiveElements.log`.

## Pointeurs résolus post-parsing {#pointers}

**`StraightBlock::StraightNeighbours`**

| Champ | Type | Description |
|-------|------|-------------|
| `prev` | `ShuntingElement*` | Bloc adjacent à l'extrémité A |
| `next` | `ShuntingElement*` | Bloc adjacent à l'extrémité B |

**`SwitchBlock::SwitchBranches`**

| Champ | Type | Description |
|-------|------|-------------|
| `root` | `ShuntingElement*` | Bloc sur la branche root |
| `normal` | `ShuntingElement*` | Bloc sur la branche normale |
| `deviation` | `ShuntingElement*` | Bloc sur la branche déviée |

## État opérationnel des aiguillages {#activestate}

| Méthode | Description |
|---------|-------------|
| `SwitchBlock::getActiveBranch()` | Retourne `ActiveBranch::NORMAL` ou `DEVIATION` |
| `SwitchBlock::isDeviationActive()` | Raccourci booléen |
| `SwitchBlock::setActiveBranch(branch)` | Assigne l'état + propage aux partenaires |
| `SwitchBlock::toggleActiveBranch()` | Alterne + propage + retourne le nouvel état |

## Énumérations clés {#enums}

| Enum | Valeurs | Usage |
|------|---------|-------|
| `InteractiveElementType` | `SWITCH`, `STRAIGHT` | Typage sans RTTI |
| `ShuntingState` | `FREE`, `OCCUPIED`, `INACTIVE` | État opérationnel infrastructure |
| `ActiveBranch` | `NORMAL`, `DEVIATION` | Position opérationnelle de l'aiguillage |

---

# Coordonnées {#Coordinates}

| Classe | Système |
|--------|---------|
| @ref CoordinateLatLon | Géographique WGS-84 (latitude, longitude) |
| @ref CoordinateXY | Métrique UTM (x = est, y = nord) |

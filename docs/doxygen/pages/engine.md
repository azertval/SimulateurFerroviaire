@page page_engine Engine Core

@tableofcontents

---

## Vue d'ensemble {#engine_overview}

**Engine Core** regroupe les services **transversaux** utilisés par tous les
autres modules. Il ne contient aucune logique métier ferroviaire — seulement
des briques d'infrastructure réutilisables.

| Sous-module | Dossier | Rôle |
|-------------|---------|------|
| @ref engine_logger "Logger" | `Engine/Core/Logger/` | Journalisation à 5 niveaux |
| @ref engine_config "ParserConfig" | `Engine/Core/Config/` | Paramètres du pipeline |
| @ref engine_coords "Coordonnées" | `Engine/Core/Coordinates/` | WGS-84 et UTM |
| @ref engine_topo "Topologie" | `Engine/Core/Topology/` | Repository, rendu |

---

## Logger {#engine_logger}

### Concept

@ref Logger fournit un canal de journalisation **par moteur** : chaque instance
est associée à un nom de moteur (ex. `"GeoParser"`, `"HMI"`) et écrit dans
`Logs/<nomDuMoteur>.log`. Les traces sont simultanément envoyées à la **sortie
de débogage Visual Studio** via `OutputDebugStringA`.

Les écritures sont protégées par un `std::mutex` — le Logger est utilisable
depuis plusieurs threads sans synchronisation externe.

### Niveaux de sévérité

| Niveau | Macro | Utilisation |
|--------|-------|-------------|
| `DEBUG` | `LOG_DEBUG` | Valeurs intermédiaires, état interne |
| `INFO` | `LOG_INFO` | Événements nominaux (fin de phase, résultats) |
| `WARNING` | `LOG_WARNING` | Anomalie non-bloquante (données douteuses, repli) |
| `ERROR` | `LOG_ERROR` | Erreur bloquante récupérable |
| `FAILURE` | `LOG_FAILURE` | Erreur fatale — journalise **puis** lève `std::runtime_error` |

Le niveau minimum de filtrage est configurable via `setMinimumLogLevel()`.
En production, positionner à `INFO` supprime les traces `DEBUG` sans modifier
le code.

### Format d'une ligne

```
[HH:MM:SS] [NIVEAU  ] {NomDeClasse} [NomDeFonction] "Ligne : XX" : Message
```

Exemple :
```
[14:23:01] [INFO   ] {GeoParser} [parse] "Ligne : 47" : Phase 3 terminée — 1842 segments.
```

### Macros d'injection

Sur MSVC, `__FUNCTION__` retourne `"NomDeClasse::NomDeFonction"`. Les macros
`LOG_*` transmettent `__FUNCTION__` et `__LINE__` au Logger, qui extrait
automatiquement le nom de classe et le nom de méthode pour les inclure dans
chaque ligne.

```cpp
// Utilisation type :
LOG_INFO(m_logger,    "Fichier chargé : " + filePath);
LOG_DEBUG(m_logger,   "Nœud " + std::to_string(id) + " créé.");
LOG_WARNING(m_logger, "Branche trop courte ignorée : " + branchId);
LOG_ERROR(m_logger,   "Fichier introuvable : " + path);
LOG_FAILURE(m_logger, "Topologie nulle — arrêt immédiat");
```

> **`LOG_FAILURE`** journalise le message puis appelle
> `Logger::triggerFatalCrash()`, qui lève `std::runtime_error`. Il ne
> retourne jamais — ne pas l'utiliser dans des destructeurs.

### Règle d'unicité

Un fichier de log = une instance de Logger. Les instances ne sont
**pas copiables** (constructeur de copie supprimé). Partager un logger entre
plusieurs objets se fait par **référence** ou **pointeur** — jamais par
valeur.

---

## ParserConfig {#engine_config}

### Paramètres

@ref ParserConfig est un agrégat de paramètres numériques qui gouvernent le
comportement du @ref page_geoparser "pipeline GeoParser". Tous les champs ont
des valeurs par défaut compilées.

| Champ | Type | Défaut | Signification |
|-------|------|--------|---------------|
| `snapTolerance` | `double` | 1,0 m | Distance de fusion de deux extrémités |
| `intersectionEpsilon` | `double` | 0,5 m | Tolérance pour les intersections géométriques |
| `minSwitchAngle` | `double` | 5,0 ° | Angle minimal d'une bifurcation pour être classée SWITCH |
| `minBranchLength` | `double` | 0,5 m | Longueur minimale d'une branche d'aiguillage |
| `maxSegmentLength` | `double` | 200,0 m | Longueur au-delà de laquelle un straight est subdivisé |
| `junctionTrimMargin` | `double` | 3,0 m | Recul des tips CDC depuis la jonction |
| `switchSideSize` | `double` | 15,0 m | Demi-longueur des stubs CDC |
| `doubleSwitchRadius` | `double` | 20,0 m | Rayon de détection des aiguilles doubles |

### Persistance INI

@ref ParserConfigIni gère la sérialisation dans un fichier `.ini` simple
(format `clé=valeur`, un paramètre par ligne). Le chemin par défaut est
retourné par `ParserConfigIni::defaultPath()`. `load()` retourne les valeurs
par défaut si le fichier est absent ou illisible ; `save()` crée le fichier
si nécessaire.

---

## Coordonnées {#engine_coords}

Le projet utilise **deux systèmes de coordonnées** selon le contexte :

### WGS-84 — @ref CoordinateLatLon

Coordonnées géographiques standards (`latitude`, `longitude` en degrés
décimaux). Utilisées pour :
- le stockage dans les fichiers GeoJSON source ;
- les polylignes des @ref StraightBlock et la jonction des @ref SwitchBlock
  destinées au rendu Leaflet ;
- les tips CDC pour `TopologyRenderer`.

### UTM zone 30N — @ref CoordinateXY

Coordonnées planes métriques (`x` = est, `y` = nord, en mètres). Utilisées
pour **tous les calculs métriques** du pipeline (distances, angles, intersections,
subdivisions). La projection UTM est effectuée par les phases amont du pipeline.

> **Règle de cohabitation :** les deux collections (`pointsWGS84` / `pointsUTM`)
> dans @ref StraightBlock ont toujours le même nombre de points et les mêmes
> indices. Opérer toujours sur `UTM` pour les calculs ; utiliser `WGS84`
> uniquement pour le rendu.

### Haversine

La fonction `haversineDistanceMeters(a, b)` calcule la **distance géodésique**
entre deux points WGS-84. Elle est utilisée par Phase 7 pour interpoler les
tips CDC à distance `switchSideSize` de la jonction, et pour calculer
`StraightBlock::getLengthMeters()`.

---

## Topologie — TopologyRepository {#engine_topo}

### TopologyRepository

Singleton global (`TopologyRepository::instance()`) qui détient le
@ref TopologyData produit par le @ref page_geoparser "pipeline". Il expose :

- `data()` — référence constante vers les vecteurs de blocs et les index.
- `load(data)` — transfert par déplacement depuis le pipeline (appelé par
  `Phase8_RepositoryTransfer::transfer()`).
- `clear()` — réinitialisation avant un nouveau parsing.

Après `load()`, les adresses des blocs sont **stables** : aucun vecteur
ne se réalloue. Les pointeurs non-propriétaires distribués par
@ref PCCGraphBuilder restent valides jusqu'au prochain `clear()`.

### TopologyData

@ref TopologyData est le conteneur central :

```cpp
struct TopologyData {
    vector<unique_ptr<StraightBlock>> straights;  // ownership
    vector<unique_ptr<SwitchBlock>>   switches;   // ownership
    unordered_map<string, StraightBlock*> straightIndex;  // lookup O(1)
    unordered_map<string, SwitchBlock*>   switchIndex;    // lookup O(1)
};
```

`buildIndex()` construit les maps `id → ptr` après le transfert. Il doit
être appelé une seule fois, après stabilisation des adresses.

### TopologyRenderer

@ref TopologyRenderer génère les **scripts JavaScript** injectés dans la
page Leaflet via `WebViewPanel::executeScript()`. Il opère exclusivement sur
les collections WGS-84 des blocs.

Point d'entrée principal : `renderAllTopology()` — produit en une passe les
appels `clearStraightBlocks()`, `renderStraightBlock()`, `clearSwitchBranches()`,
`renderSwitchBranches()`, `clearSwitches()`, `renderSwitch()`.

`updateSwitchBlocks(sw)` produit les appels `switchApplyState()` pour mettre
à jour visuellement un aiguillage après `toggleActiveBranch()`.

---

## Références croisées {#engine_refs}

| Classe | Fichier | Rôle |
|--------|---------|------|
| @ref Logger | `Engine/Core/Logger/Logger.h` | Journalisation multi-niveaux |
| @ref LogLevel | `Engine/Core/Logger/Logger.h` | Enum de sévérité |
| @ref ParserConfig | `Engine/Core/Config/ParserConfig.h` | Paramètres numériques |
| @ref ParserConfigIni | `Engine/Core/Config/ParserConfigIni.h` | Persistance INI |
| @ref CoordinateLatLon | `Engine/Core/Coordinates/CoordinateLatLon.h` | WGS-84 |
| @ref CoordinateXY | `Engine/Core/Coordinates/CoordinateXY.h` | UTM (x,y mètres) |
| @ref TopologyRepository | `Engine/Core/Topology/TopologyRepository.h` | Singleton détenteur |
| @ref TopologyData | `Engine/Core/Topology/TopologyData.h` | Vecteurs + index |
| @ref TopologyRenderer | `Engine/Core/Topology/TopologyRenderer.h` | Rendu Leaflet JS |

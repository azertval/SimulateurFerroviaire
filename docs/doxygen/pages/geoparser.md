@page geoparser_v2 GeoParser v2 — Pipeline refactorisé

@tableofcontents

---

# Vue d'ensemble {#gpv2_overview}

GeoParser v2 est la refonte complète du pipeline **GeoJSON → modèle ferroviaire**.

L'ancien pipeline monolithique (`GeoParser::parse()` tout-en-un) est remplacé par
**9 phases indépendantes**, chacune avec une responsabilité unique, une interface
uniforme et une instrumentation de performance intégrée.

```
GeoJSON
  │
  ▼ Phase1  GeoLoader             WGS-84 → UTM, RawNetwork
  ▼ Phase2  GeometricIntersector  intersections géométriques (Cramer + grid binning)
  ▼ Phase3  NetworkSplitter       découpe aux intersections, segments atomiques
  ▼ Phase4  TopologyBuilder       snap + Union-Find → graphe planaire
  ▼ Phase5  SwitchClassifier      NodeClass par nœud (degré + angle)
  ▼ Phase6  BlockExtractor        StraightBlock + SwitchBlock non-orientés
  ▼ Phase9a resolve()             résolution pointeurs inter-blocs
  ▼ Phase8  DoubleSwitchDetector  absorption doubles aiguilles
  ▼ Phase7  SwitchOrientator      orientation root / normal / deviation
  ▼ Phase9b transfer()            TopologyRepository + buildIndex()
  │
  ▼
TopologyRepository  ← inchangé
```

> **Ordre d'exécution réel** : Phase8 précède Phase7 dans `GeoParser::parse()`.
> La résolution des pointeurs (Phase9a) est requise avant l'orientation (Phase7).
> Voir @ref gpv2_phase9 pour le détail.

**Objectifs de la refonte :**

| Axe | Apport |
|-----|--------|
| Modularité | Chaque phase est relançable indépendamment via `PhaseN::run(ctx, config, logger)` |
| Performances | Conversion UTM une seule fois, libération mémoire inter-phases, grid binning O(n·k) |
| Détection topologique | Switches détectés par intersection géométrique réelle, plus par heuristique de degré |
| SOLID | SRP par phase, OCP sur l'orchestrateur, DIP via injection `ParserConfig` |

**Contrat de compatibilité :**

| Composant | Statut |
|-----------|--------|
| `TopologyRepository` / `TopologyData` | **Inchangés** — seul le pipeline change |
| `GeoParsingTask` — interface publique | **Inchangée** — `start(filePath, config)` |
| Module PCC (`PCCGraphBuilder`, `PCCLayout`, `TCORenderer`) | **Aucune régression** |

---

# Architecture des fichiers {#gpv2_files}

```
Engine/Core/Config/
  ├── ParserConfig.h              POD pur — paramètres immuables
  └── ParserConfigIni.h/.cpp      load/save .ini via SimpleIni

External/SimpleIni/
  └── SimpleIni.h                 header-only (github.com/brofield/simpleini)

Engine/HMI/Dialogs/
  └── ParserSettingsDialog.h/.cpp Dialogue modal Win32

Config/
  └── parser_settings.ini         Créé au premier lancement

Modules/GeoParser/
  ├── GeoParser.h/.cpp            Orchestrateur — possède PipelineContext
  ├── GeoParsingTask.h/.cpp       Thread async — interface inchangée
  └── Pipeline/
      ├── PipelineContext.h       Conteneur central inter-phases
      ├── RawNetwork.h            Données Phase 1
      ├── IntersectionMap.h       Données Phase 2
      ├── SplitNetwork.h          Données Phase 3
      ├── TopologyGraph.h/.cpp    Données Phase 4 (refactorisé)
      ├── ClassifiedNodes.h       Données Phase 5
      ├── BlockSet.h              Données Phases 6-8
      ├── Phase1_GeoLoader.h/.cpp
      ├── Phase2_GeometricIntersector.h/.cpp
      ├── Phase3_NetworkSplitter.h/.cpp
      ├── Phase4_TopologyBuilder.h/.cpp
      ├── Phase5_SwitchClassifier.h/.cpp
      ├── Phase6_BlockExtractor.h/.cpp
      ├── Phase7_SwitchOrientator.h/.cpp
      ├── Phase8_DoubleSwitchDetector.h/.cpp
      └── Phase9_RepositoryTransfer.h/.cpp
```

---

# ParserConfig — Paramètres {#gpv2_config}

`ParserConfig` est un **struct POD pur** — transporteur de paramètres sans logique métier.
`ParserConfigIni` gère uniquement la persistence `.ini` via SimpleIni.

```cpp
// ParserConfig — aucune dépendance, copiable librement
struct ParserConfig
{
    double snapTolerance       = 3.0;     // [Topology]
    double maxSegmentLength    = 1000.0;  // [Topology]
    double intersectionEpsilon = 1.5;     // [Intersection]
    double minSwitchAngle      = 15.0;    // [Switch]
    double junctionTrimMargin  = 25.0;    // [Switch]
    double doubleSwitchRadius  = 50.0;    // [Switch]
    double minBranchLength     = 100.0;   // [CDC]
};
```

| Paramètre | Défaut | Description |
|-----------|--------|-------------|
| `snapTolerance` | `3.0 m` | Rayon de fusion des nœuds proches |
| `maxSegmentLength` | `1000.0 m` | Longueur maximale d'un segment avant découpe |
| `intersectionEpsilon` | `1.5 m` | Tolérance de détection d'intersection géométrique |
| `minSwitchAngle` | `15.0°` | Angle minimal pour identifier une bifurcation réelle |
| `junctionTrimMargin` | `25.0 m` | Marge de recadrage des tips CDC au niveau des jonctions |
| `doubleSwitchRadius` | `50.0 m` | Distance maximale entre deux switches pour former une double aiguille |
| `minBranchLength` | `100.0 m` | Longueur minimale de branche pour la validation CDC |

**Séparation POD / persistance (SRP) :**

```
ParserConfig     — décrit les paramètres                  (aucune dépendance)
ParserConfigIni  — lit/écrit parser_settings.ini          (dépend de SimpleIni)
```

`GeoParser` reçoit `ParserConfig` **par valeur** dans son constructeur.
Snapshot immuable pendant toute la durée d'un parsing — thread-safe par construction.

---

# PipelineContext — Transporteur inter-phases {#gpv2_context}

`PipelineContext` est le **seul** objet partagé entre toutes les phases.
Chaque phase lit ses entrées et écrit son résultat dans le contexte.

```
Phase1  écrit  ctx.rawNetwork
Phase2  lit    ctx.rawNetwork     écrit  ctx.intersections
Phase3  lit    ctx.intersections  écrit  ctx.splitNetwork    → ctx.rawNetwork.clear()
Phase4  lit    ctx.splitNetwork   écrit  ctx.topoGraph       → ctx.splitNetwork.clear()
Phase5  lit    ctx.topoGraph      écrit  ctx.classifiedNodes
Phase6  lit    ctx.topoGraph
               ctx.classifiedNodes écrit ctx.blocks          → ctx.topoGraph.clear()
Phase7  lit    ctx.blocks         (modifie les orientations)
Phase8  lit    ctx.blocks         (absorbe les doubles aiguilles)
Phase9  lit    ctx.blocks         → TopologyRepository       → ctx.blocks.clear()
```

**PhaseStats — instrumentation intégrée :**

Chaque phase enregistre sa durée et son compte d'éléments dans `ctx.stats`.
`GeoParser::logPerformanceSummary()` produit le tableau de performance en fin de pipeline.

```
--- Performance pipeline ---
  Phase1_GeoLoader        :  12 ms | in=0    out=847
  Phase2_GeometricIntersector : 38 ms | in=847  out=1203
  Phase3_NetworkSplitter  :   9 ms | in=1203 out=2104
  ...
  TOTAL : 143 ms
```

---

# GeoParser — Orchestrateur {#gpv2_orchestrator}

`GeoParser` possède `PipelineContext` et `ParserConfig`.
Il enchaîne les phases et reporte la progression via callback — il ne connaît pas
les détails internes de chaque phase (OCP).

| Méthode | Rôle |
|---------|------|
| `GeoParser::GeoParser(config, logger, onProgress)` | Construction — snapshot de config |
| `GeoParser::parse(filePath)` | Pipeline complet — phases 1 à 9 |
| `GeoParser::reportProgress(int)` | Callback UI + log de la dernière phase |
| `GeoParser::logPerformanceSummary()` | Tableau de performance final |

**Ajout d'une phase (OCP) :** ajouter uniquement un appel `PhaseN::run(m_ctx, m_config, m_logger)`
dans `parse()` — GeoParser n'a pas besoin de connaître l'implémentation.

---

# Phases du pipeline {#gpv2_phases}

## Phase 1 — GeoLoader {#gpv2_phase1}

**Fichiers :** `Phase1_GeoLoader.h/.cpp` · `RawNetwork.h`

Charge le fichier GeoJSON, projette les coordonnées WGS-84 en UTM et
produit le `RawNetwork` de polylignes.

**Responsabilité :** lire le GeoJSON et projeter — rien d'autre.

| Étape interne | Description |
|---------------|-------------|
| Lecture JSON | `nlohmann::json::parse()` — lève `runtime_error` si GeoJSON invalide |
| Filtrage | Seules les features `LineString` sont retenues |
| Détection zone UTM | Calculée depuis le premier point du premier segment |
| Projection | WGS-84 → UTM (formules ellipsoïde WGS-84 complètes) |

**Sortie :** `ctx.rawNetwork` — polylignes avec points WGS-84 et UTM synchronisés.

---

## Phase 2 — GeometricIntersector {#gpv2_phase2}

**Fichiers :** `Phase2_GeometricIntersector.h/.cpp` · `IntersectionMap.h`

Calcule tous les points d'intersection géométrique entre segments du `RawNetwork`.

**Responsabilité :** produire `ctx.intersections` — rien d'autre.

| Mécanisme | Description |
|-----------|-------------|
| Algorithme | Cramer (résolution système linéaire 2×2) |
| Tolérance | `config.intersectionEpsilon` — évite les faux positifs sur flottants |
| Optimisation | Spatial grid binning : O(n²) → O(n·k) — seuls les segments de cellules adjacentes sont testés |

**Sortie :** `ctx.intersections` — `map<SegmentId, vector<IntersectionPoint>>`


---

## Phase 3 — NetworkSplitter {#gpv2_phase3}

**Fichiers :** `Phase3_NetworkSplitter.h/.cpp` · `SplitNetwork.h`

Découpe les polylignes brutes aux points d'intersection et aux dépassements
de `maxSegmentLength`. Produit des **segments atomiques** sans intersection interne.

| Mécanisme | Description |
|-----------|-------------|
| Découpe aux intersections | Tri + dédoublonnage des points de coupe par distance curviligne |
| Filtrage micro-segments | Segments inférieurs à un seuil minimal supprimés |
| Découpe par longueur | Interpolation linéaire UTM aux multiples de `maxSegmentLength` |
| Libération mémoire | `ctx.rawNetwork.clear()` + `shrink_to_fit()` après production |

**Sortie :** `ctx.splitNetwork` — vecteur d'`AtomicSegment`


---

## Phase 4 — TopologyBuilder {#gpv2_phase4}

**Fichiers :** `Phase4_TopologyBuilder.h/.cpp` · `TopologyGraph.h/.cpp` (refactorisé)

Construit le **graphe planaire** depuis les segments atomiques.
Fusionne les extrémités proches via Union-Find + grid binning.

| Mécanisme | Description |
|-----------|-------------|
| Snap | Grid binning — nœuds dans un rayon `snapTolerance` mis en candidats |
| Union-Find | Fusion des nœuds candidats — path compression + union by rank |
| Graphe | Nœuds (positions UTM fusionnées) + arêtes (segments) |
| Libération | `ctx.splitNetwork.clear()` après production |

**Sortie :** `ctx.topoGraph` — graphe planaire nœuds + arêtes UTM

---

## Phase 5 — SwitchClassifier {#gpv2_phase5}

**Fichiers :** `Phase5_SwitchClassifier.h/.cpp` · `ClassifiedNodes.h`

Attribue une `NodeClass` à chaque nœud du graphe planaire en combinant
**degré** et **angle** entre arêtes incidentes.

```cpp
enum class NodeClass
{
    TERMINUS,   // degré == 1
    STRAIGHT,   // degré == 2, angle ≈ 180° (± minSwitchAngle)
    SWITCH,     // degré == 3, bifurcation géométrique réelle
    CROSSING,   // degré == 4 — croisement plat, ignoré
    ISOLATED,   // degré == 0
    AMBIGUOUS   // autre — WARNING + traitement au cas par cas
};
```

**Apport vs l'ancien pipeline :** les switches ne sont plus détectés par heuristique
de degré GeoJSON (fragile sur données bruitées), mais par **intersection géométrique
réelle** des shapes, confirmée par l'angle entre branches.

**Sortie :** `ctx.classifiedNodes` — `unordered_map<size_t, NodeClass>`

---

## Phase 6 — BlockExtractor {#gpv2_phase6}

**Fichiers :** `Phase6_BlockExtractor.h/.cpp` · `BlockSet.h`

Transforme le graphe planaire classifié en **blocs ferroviaires** —
les objets métier `StraightBlock` et `SwitchBlock`.

| Mécanisme | Description |
|-----------|-------------|
| Nœuds frontières | `SWITCH`, `TERMINUS`, `CROSSING` — délimitent les blocs |
| Nœuds transparents | `STRAIGHT` — traversés lors du DFS |
| DFS entre frontières | Parcours itératif — concatène les segments en voie droite |
| SwitchBlocks | Un bloc par nœud `SWITCH` — ID depuis les coordonnées UTM |
| Câblage topologique | Voisins assignés par ID (pointeurs résolus en Phase 9) |
| Libération | `ctx.topoGraph.clear()` après production |

**Sortie :** `ctx.blocks` — `BlockSet` avec `vector<unique_ptr<StraightBlock>>` + `vector<unique_ptr<SwitchBlock>>`

---

## Phase 7 — SwitchOrientator {#gpv2_phase7}

**Fichiers :** `Phase7_SwitchOrientator.h/.cpp`

Refactorisation de l'ancien `SwitchOrientator`. La **logique métier est conservée intégralement** —
seule l'interface s'adapte au pipeline v2.

| Avant | Après |
|-------|-------|
| Lit `TopologyRepository::instance()` | Lit `ctx.blocks` |
| Constantes hardcodées (`25.0f`, `15.0`) | `config.junctionTrimMargin`, `config.minSwitchAngle` |
| Méthodes d'instance | `static void run(ctx, config, logger)` |

**Phases internes conservées :**

| Phase | Description |
|-------|-------------|
| 6a | Orientation géométrique root / normal / deviation |
| 6b | Vérification de cohérence angulaire |
| 6c | Correction des cas ambigus par `minSwitchAngle` |
| 6d | Calcul des tips CDC avec `junctionTrimMargin` |

---

## Phase 8 — DoubleSwitchDetector {#gpv2_phase8}

**Fichiers :** `Phase8_DoubleSwitchDetector.h/.cpp`

Refactorisation de l'ancien `DoubleSwitchDetector`. Détecte les paires de switches
proches (< `doubleSwitchRadius`) et absorbe le segment de liaison entre eux.

| Avant | Après |
|-------|-------|
| Seuil de distance hardcodé | `config.doubleSwitchRadius` |
| Longueur CDC hardcodée | `config.minBranchLength` |
| Interface standalone | `static void run(ctx, config, logger)` |

---

## Phase 9 — RepositoryTransfer {#gpv2_phase9}

**Fichiers :** `Phase9_RepositoryTransfer.h/.cpp`

Transfère `ctx.blocks` vers `TopologyRepository` et finalise le modèle.

**Ordre réel dans `GeoParser::parse()` :**

```
Phase9::resolve(ctx)      — résolution pointeurs inter-blocs (adresses encore dans ctx)
Phase7::run(ctx, ...)     — orientation (nécessite pointeurs résolus)
Phase9::transfer(ctx)     — std::move → TopologyRepository + buildIndex()
ctx.blocks.clear()        — libération mémoire pipeline
```

> Phase 9 est **volontairement scindée en deux appels** dans l'orchestrateur.
> `resolve()` avant Phase7 (les pointeurs doivent être valides pour l'orientation),
> `transfer()` après Phase7 (le modèle doit être orienté avant export).

| Méthode | Rôle |
|---------|------|
| `Phase9_RepositoryTransfer::resolve()` | Résolution des `ShuntingElement*` inter-blocs depuis les IDs câblés en Phase6 |
| `Phase9_RepositoryTransfer::transfer()` | `std::move` des `unique_ptr` → `TopologyData` + `buildIndex()` |

**Stabilité des adresses :** `TopologyData` stocke en `vector<unique_ptr<T>>`.
Après `std::move`, les adresses des objets alloués sont stables — les pointeurs
résolus par `resolve()` restent valides.

---

# GeoParsingTask — Intégration async {#gpv2_task}

`GeoParsingTask` est le point d'entrée depuis `MainWindow`. Son interface publique
est **inchangée** — seul le paramètre `config` est ajouté à `start()`.

```cpp
// Lancement — snapshot de config au moment du démarrage
void GeoParsingTask::start(const std::string& filePath,
                            const ParserConfig& config);

// Annulation coopérative — thread-safe via atomic
void GeoParsingTask::cancel();
```

**Cancel token partagé :**

```
GeoParsingTask
  └── m_cancelToken : shared_ptr<atomic<bool>>
        └── copié dans le thread détaché → partagé avec GeoParser
```

Le thread vérifie `*m_cancelToken` entre les phases. Si `true`, il envoie
`WM_PARSING_CANCELLED` et s'arrête proprement sans altérer `TopologyRepository`.

| Message Win32 | Contenu | Handler |
|---------------|---------|---------|
| `WM_PROGRESS_UPDATE` | Avancement 0–100 | `MainWindow::onProgressUpdate()` |
| `WM_PARSING_SUCCESS` | — | `MainWindow::onParsingSuccess()` |
| `WM_PARSING_ERROR` | `std::string*` (à libérer) | `MainWindow::onParsingError()` |
| `WM_PARSING_CANCELLED` | — | `MainWindow::onParsingCancelled()` |

---

# ParserSettingsDialog {#gpv2_dialog}

**Fichiers :** `Engine/HMI/Dialogs/ParserSettingsDialog.h/.cpp`

Dialogue modal Win32 permettant à l'utilisateur de modifier les 7 paramètres
de `ParserConfig` depuis l'interface.

**Comportement :**
- Affiche les 7 paramètres avec des contrôles `EDIT` Win32
- Valide les plages (valeurs positives, cohérence `intersectionEpsilon` < `snapTolerance`)
- Boutons **OK** / **Annuler** / **Réinitialiser aux défauts**
- OK → `ParserConfigIni::save()` → rechargement dans `MainWindow` → prochain parsing utilise la nouvelle config

**Pattern :** identique à `AboutDialog` — `DialogBoxParam` / `WM_INITDIALOG` / `IDOK` / `IDCANCEL`.

---

# Références {#gpv2_references}

| Concept | Lien |
|---------|------|
| SRP — Single Responsibility | https://en.wikipedia.org/wiki/Single-responsibility_principle |
| OCP — Open/Closed Principle | https://en.wikipedia.org/wiki/Open%E2%80%93closed_principle |
| DIP — Dependency Inversion | https://en.wikipedia.org/wiki/Dependency_inversion_principle |
| Union-Find | https://en.wikipedia.org/wiki/Disjoint-set_data_structure |
| BFS / DFS | https://en.wikipedia.org/wiki/Breadth-first_search |
| Projection UTM | https://en.wikipedia.org/wiki/Universal_Transverse_Mercator_coordinate_system |
| nlohmann/json | https://github.com/nlohmann/json |
| SimpleIni | https://github.com/brofield/simpleini |

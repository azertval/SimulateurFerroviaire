@page geoparser GeoParser — Pipeline refactorisé

@tableofcontents

---

# GeoParser {#geoparser}

Pipeline complet de transformation **GeoJSON → modèle ferroviaire**,

## Architecture du pipeline {#pipeline_arch}

```
GeoParsingTask (thread async)
  └─► GeoParser (orchestrateur)
        ├─► PipelineContext  (transporteur inter-phases)
        ├─► ParserConfig     (paramètres immuables pendant le parsing)
        └─► Phase1..9        (chacune : run(ctx, config, logger))
```

## Phases {#pipeline}

| Ordre | Phase | Classe | Input → Output |
|-------|-------|--------|----------------|
| 1 | Chargement GeoJSON | @ref Phase1_GeoLoader | fichier → `RawNetwork` (WGS84 + UTM) |
| 2 | Intersections géométriques | @ref Phase2_GeometricIntersector | `RawNetwork` → `IntersectionData` (Cramer + grid binning) |
| 3 | Découpe des segments | @ref Phase3_NetworkSplitter | `RawNetwork` + `IntersectionData` → `SplitNetwork` |
| 4 | Graphe planaire | @ref Phase4_TopologyBuilder | `SplitNetwork` → `TopologyGraph` (Union-Find + snap) |
| 5 | Classification des nœuds | @ref Phase5_SwitchClassifier | `TopologyGraph` → `ClassifiedNodes` (degré + angle) |
| 6 | Extraction des blocs | @ref Phase6_BlockExtractor | `ClassifiedNodes` → `BlockSet` (DFS entre nœuds frontières) |
| 9a | Résolution des pointeurs | @ref Phase9_RepositoryTransfer | `BlockSet` → pointeurs inter-blocs résolus |
| 8 | Doubles aiguilles | @ref Phase7_DoubleSwitchDetector | `BlockSet` → absorption + validation CDC |
| 7 | Orientation des switches | @ref Phase8_SwitchOrientator | `BlockSet` → root / normal / deviation |
| 9b | Transfert final | @ref Phase9_RepositoryTransfer | `BlockSet` → @ref TopologyRepository |

> **Ordre d'exécution réel dans @ref GeoParser::parse() "GeoParser::parse()" :**
> Phase1 → 2 → 3 → 4 → 5 → 6 → 9a::resolve → 8 → 7 → 9b::transfer.
> Phase 8 et Phase 7 nécessitent les pointeurs résolus par 9a.
> Phase 9b doit être la dernière — état final de @ref TopologyRepository.

## Libération mémoire inter-phases {#memory}

| Libéré après | Structure |
|--------------|-----------|
| Phase 3 | `RawNetwork`, `IntersectionData` |
| Phase 4 | `SplitNetwork` |
| Phase 6 | `TopologyGraph`, `ClassifiedNodes` |
| Phase 9b | `BlockSet` |

## Tâche asynchrone {#task}

@ref GeoParsingTask lance @ref GeoParser dans un thread détaché.
Communication vers l'UI via `PostMessage` :

| Message | wParam | lParam | Signification |
|---------|--------|--------|---------------|
| `WM_PROGRESS_UPDATE` | 0-100 | `std::wstring*` label (à libérer) | Avancement |
| `WM_PARSING_SUCCESS` | — | — | Parsing terminé |
| `WM_PARSING_ERROR` | — | `std::wstring*` message (à libérer) | Échec |
| `WM_PARSING_CANCELLED` | — | — | Annulation propre |

**Annulation :** @ref GeoParsingTask::cancel() "GeoParsingTask::cancel()" positionne un `shared_ptr<atomic<bool>>`
partagé avec @ref GeoParser. Vérifié entre chaque phase via @ref GeoParser::checkCancel() "GeoParser::checkCancel()"
→ lève @ref GeoParser::CancelledException "GeoParser::CancelledException".

## Structures de données du pipeline {#pipeline_data}

| Struct | Phase productrice | Contenu |
|--------|-------------------|---------|
| `RawNetwork` | Phase 1 | Polylignes WGS84 + UTM brutes |
| `IntersectionData` | Phase 2 | Points d'intersection + grille spatiale |
| `SplitNetwork` | Phase 3 | Segments atomiques sans intersection interne |
| `TopologyGraph` | Phase 4 | Graphe planaire nœuds + arêtes + adjacence |
| `ClassifiedNodes` | Phase 5 | `NodeClass` par nœud |
| `BlockSet` | Phase 6 | `unique_ptr<StraightBlock>` + `unique_ptr<SwitchBlock>` |

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
      ├── Phase8_SwitchOrientator.h/.cpp
      ├── Phase7_DoubleSwitchDetector.h/.cpp
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
@ref GeoParser::logPerformanceSummary() "GeoParser::logPerformanceSummary()" produit le tableau de performance en fin de pipeline.

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

@ref GeoParser possède `PipelineContext` et `ParserConfig`.
Il enchaîne les phases et reporte la progression via callback — il ne connaît pas
les détails internes de chaque phase (OCP).

| Méthode | Rôle |
|---------|------|
| @ref GeoParser::GeoParser() "GeoParser(config, logger, onProgress)" | Construction — snapshot de config |
| @ref GeoParser::parse() "parse(filePath)" | Pipeline complet — phases 1 à 9 |
| @ref GeoParser::reportProgress() "reportProgress(int)" | Callback UI + log de la dernière phase |
| @ref GeoParser::logPerformanceSummary() "logPerformanceSummary()" | Tableau de performance final |

**Ajout d'une phase (OCP) :** ajouter uniquement un appel `PhaseN::run(m_ctx, m_config, m_logger)`
dans `parse()` — @ref GeoParser n'a pas besoin de connaître l'implémentation.

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

### Intersection segment-segment — algorithme de Cramer

Deux segments AB et CD se croisent si on peut écrire :

```
P = A + t * (B - A)    (point sur AB, t ∈ [0,1])
P = C + u * (D - C)    (point sur CD, u ∈ [0,1])
```

En égalisant les deux expressions :

```
A + t*(B-A) = C + u*(D-C)
```

Ce système 2×2 s'écrit sous forme matricielle (règle de Cramer) :

```
| Bx-Ax   -(Dx-Cx) |   | t |   | Cx-Ax |
| By-Ay   -(Dy-Cy) | × | u | = | Cy-Ay |
```

Le déterminant du système :

```
det = (Bx-Ax)*(-(Dy-Cy)) - (-(Dx-Cx))*(By-Ay)
    = (Bx-Ax)*(Cy-Dy) + (Dx-Cx)*(By-Ay)
```

Si `det == 0`, les segments sont **parallèles** — pas d'intersection.

Sinon :

```
t = ((Cx-Ax)*(Cy-Dy) + (Dx-Cx)*(Cy-Ay)) / det   ← simplifié
u = ((Bx-Ax)*(Cy-Ay) - (By-Ay)*(Cx-Ax)) / det
```

Les segments se croisent si et seulement si `t ∈ [0,1]` et `u ∈ [0,1]`.

> Référence — intersection segment-segment :
> https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection


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

### Interpolation linéaire UTM vs sphérique WGS84

#### UTM — interpolation linéaire exacte

En coordonnées UTM (métriques), l'interpolation linéaire est exacte :

```cpp
CoordinateXY interpolateUTM(const CoordinateXY& A,
                              const CoordinateXY& B,
                              double t)
{
    return { A.x + t * (B.x - A.x),
             A.y + t * (B.y - A.y) };
}
```

UTM est une projection conforme — les distances et angles sont préservés
localement. Sur un segment de < 100 km, l'interpolation linéaire est
exacte à mieux que 1 mm.

#### WGS84 — approximation linéaire acceptable

En WGS84, l'interpolation strictement correcte est la **géodésique** (ligne
courbe sur l'ellipsoïde). Mais pour des segments de < 1 km (notre cas après
découpe par `maxSegmentLength`), l'erreur d'interpolation linéaire est
inférieure à 1 mm — négligeable pour le rendu Leaflet.

```cpp
LatLon interpolateWGS84(const LatLon& A, const LatLon& B, double t)
{
    // Interpolation linéaire — valable sur < ~10 km d'arc
    return { A.lat + t * (B.lat - A.lat),
             A.lon + t * (B.lon - A.lon) };
}
```

**Quand faudrait-il une géodésique ?** Sur des segments transcontinentaux
(> 100 km). Notre pipeline découpe à `maxSegmentLength` (défaut 1000 m)
avant d'arriver ici — pas de risque.

> Référence — géodésique vs linéaire :
> https://en.wikipedia.org/wiki/Geodesics_on_an_ellipsoid


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

### Union-Find — principe et complexité

#### Problème

On a N extrémités de segments. Certaines doivent être fusionnées
(proches < snapTolerance). On veut savoir rapidement : "ces deux
extrémités appartiennent-elles au même nœud topologique ?"

#### Structure Union-Find (disjoint set)

Une forêt d'arbres où chaque élément pointe vers son parent.
La racine d'un arbre représente le **représentant canonique** de l'ensemble.

```
Éléments : [0, 1, 2, 3, 4]
parent   : [0, 0, 2, 2, 4]  ← 0 et 1 sont dans le même ensemble
                               2 et 3 sont dans le même ensemble
                               4 est seul

find(3) → parent[3] = 2 → parent[2] = 2 → racine = 2
find(1) → parent[1] = 0 → parent[0] = 0 → racine = 0
union(1, 3) → parent[0] = 2  (ou l'inverse selon rank)
```

### Complexité naïve

Sans optimisation, `find()` peut parcourir toute la hauteur de l'arbre
→ O(N) dans le pire cas (arbre dégénéré = liste chaînée).

> Référence — Union-Find :
> https://en.wikipedia.org/wiki/Disjoint-set_data_structure

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
les objets métier @ref StraightBlock et @ref SwitchBlock.

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

**Fichiers :** `Phase8_SwitchOrientator.h/.cpp`

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

**Fichiers :** `Phase7_DoubleSwitchDetector.h/.cpp`

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

Transfère `ctx.blocks` vers @ref TopologyRepository et finalise le modèle.

**Ordre réel dans @ref GeoParser::parse() "GeoParser::parse()" :**

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
| @ref Phase9_RepositoryTransfer::resolve() "Phase9_RepositoryTransfer::resolve()" | Résolution des `ShuntingElement*` inter-blocs depuis les IDs câblés en Phase6 |
| @ref Phase9_RepositoryTransfer::transfer() "Phase9_RepositoryTransfer::transfer()" | `std::move` des `unique_ptr` → @ref TopologyData + `buildIndex()` |

**Stabilité des adresses :** @ref TopologyData stocke en `vector<unique_ptr<T>>`.
Après `std::move`, les adresses des objets alloués sont stables — les pointeurs
résolus par `resolve()` restent valides.

---

# GeoParsingTask — Intégration async {#gpv2_task}

@ref GeoParsingTask est le point d'entrée depuis `MainWindow`. Son interface publique
est **inchangée** — seul le paramètre `config` est ajouté à @ref GeoParsingTask::start() "start()".

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
`WM_PARSING_CANCELLED` et s'arrête proprement sans altérer @ref TopologyRepository.

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
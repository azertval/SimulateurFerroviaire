@page geoparser GeoParser — Pipeline

@tableofcontents

---

# GeoParser {#geoparser}

Pipeline complet de transformation **GeoJSON → modèle ferroviaire**.

## Architecture du pipeline {#pipeline_arch}

```
GeoParsingTask (thread async)
  └─► GeoParser (orchestrateur)
        ├─► PipelineContext  (transporteur inter-phases)
        ├─► ParserConfig     (paramètres immuables pendant le parsing)
        └─► Phase1..8        (chacune : run(ctx, config, logger))
```

## Phases {#pipeline}

| Ordre | Phase | Classe | Input → Output |
|-------|-------|--------|----------------|
| 1 | Chargement GeoJSON | Phase1_GeoLoader | fichier → `RawNetwork` (WGS84 + UTM) |
| 2 | Intersections géométriques | Phase2_GeometricIntersector | `RawNetwork` → `IntersectionData` (Cramer + grid binning) |
| 3 | Découpe des segments | Phase3_NetworkSplitter | `RawNetwork` + `IntersectionData` → `SplitNetwork` |
| 4 | Graphe planaire | Phase4_TopologyBuilder | `SplitNetwork` → `TopologyGraph` (Union-Find + snap) |
| 5 | Classification des nœuds | Phase5_SwitchClassifier | `TopologyGraph` → `ClassifiedNodes` (degré + angle) |
| 6 | Extraction des blocs | Phase6_BlockExtractor | `ClassifiedNodes` → `BlockSet` (DFS + subdivision) |
| 8a | Résolution des pointeurs | Phase8_RepositoryTransfer | `BlockSet` → pointeurs inter-blocs résolus |
| 7 | Traitement des switches | Phase7_SwitchProcessor | `BlockSet` → orientation + doubles + crossovers + tips |
| 8b | Transfert final | Phase8_RepositoryTransfer | `BlockSet` → TopologyRepository |

> **Ordre d'exécution réel dans GeoParser::parse() "GeoParser::parse()" :**
> Phase1 → 2 → 3 → 4 → 5 → 6 → 8a::resolve → 7::run → 8b::transfer.
>
> Phase 7 nécessite les pointeurs résolus par 8a (orientation géométrique
> et détection crossovers utilisent `getRootBlock()` / `getNormalBlock()` / `getDeviationBlock()`).
> Phase 8b doit être la dernière — état final de TopologyRepository.

## Libération mémoire inter-phases {#memory}

| Libéré après | Structure | Raison |
|--------------|-----------|--------|
| Phase 3 | `RawNetwork`, `IntersectionData` | Consommées intégralement par Phase 3 |
| Phase 6 | `TopologyGraph`, `ClassifiedNodes`, `SplitNetwork` | Phase 5 utilise encore `SplitNetwork`; Phase 6 est la dernière à en avoir besoin |
| Phase 8b | `BlockSet` | Transféré vers `TopologyRepository` via `std::move` |

> `SplitNetwork` est libéré en Phase 6 (et non Phase 4) parce que
> Phase5_SwitchClassifier l'utilise pour affiner les vecteurs d'angle.

## Tâche asynchrone {#task}

GeoParsingTask lance GeoParser dans un thread détaché.
Communication vers l'UI via `PostMessage` :

| Message | wParam | lParam | Signification |
|---------|--------|--------|---------------|
| `WM_PROGRESS_UPDATE` | 0-100 | `std::wstring*` label (à libérer) | Avancement |
| `WM_PARSING_SUCCESS` | — | — | Parsing terminé |
| `WM_PARSING_ERROR` | — | `std::wstring*` message (à libérer) | Échec |
| `WM_PARSING_CANCELLED` | — | — | Annulation propre |

**Annulation :** GeoParsingTask::cancel() "GeoParsingTask::cancel()" positionne un `shared_ptr<atomic<bool>>`
partagé avec GeoParser. Vérifié entre chaque phase via GeoParser::checkCancel() "GeoParser::checkCancel()"
→ lève GeoParser::CancelledException "GeoParser::CancelledException".

## Structures de données du pipeline {#pipeline_data}

| Struct | Phase productrice | Contenu |
|--------|-------------------|---------|
| `RawNetwork` | Phase 1 | Polylignes WGS84 + UTM brutes |
| `IntersectionData` | Phase 2 | Points d'intersection + grille spatiale |
| `SplitNetwork` | Phase 3 | Segments atomiques sans intersection interne |
| `TopologyGraph` | Phase 4 | Graphe planaire nœuds + arêtes + adjacence |
| `ClassifiedNodes` | Phase 5 | `NodeClass` par nœud |
| `BlockSet` | Phase 6 | `unique_ptr<StraightBlock>` + `unique_ptr<SwitchBlock>` + index lookup |

---

# Architecture des fichiers {#gp_files}

```
Engine/Core/Config/
  ├── ParserConfig.h              POD pur — 8 paramètres
  └── ParserConfigIni.h/.cpp      load/save .ini via SimpleIni

External/SimpleIni/
  └── SimpleIni.h                 header-only (github.com/brofield/simpleini)

Engine/HMI/Dialogs/
  └── ParserSettingsDialog.h/.cpp Dialogue modal Win32

Config/
  └── parser_settings.ini         Créé au premier lancement

Modules/GeoParser/
  ├── GeoParser.h/.cpp            Orchestrateur — possède PipelineContext
  ├── GeoParsingTask.h/.cpp       Thread async
  └── Pipeline/
      ├── PipelineContext.h       Conteneur central inter-phases
      ├── RawNetwork.h            Données Phase 1
      ├── IntersectionMap.h       Données Phase 2
      ├── SplitNetwork.h          Données Phase 3
      ├── TopologyGraph.h         Données Phase 4
      ├── ClassifiedNodes.h       Données Phase 5
      ├── BlockSet.h              Données Phases 6-8
      ├── Phase1_GeoLoader.h/.cpp
      ├── Phase2_GeometricIntersector.h/.cpp
      ├── Phase3_NetworkSplitter.h/.cpp
      ├── Phase4_TopologyBuilder.h/.cpp
      ├── Phase5_SwitchClassifier.h/.cpp
      ├── Phase6_BlockExtractor.h/.cpp
      ├── Phase7_SwitchProcessor.h/.cpp    
      └── Phase8_RepositoryTransfer.h/.cpp
```

---

# ParserConfig — Paramètres {#gp_config}

`ParserConfig` est un **struct POD pur** — transporteur de paramètres sans logique métier.
`ParserConfigIni` gère uniquement la persistence `.ini` via SimpleIni.

```cpp
struct ParserConfig
{
    double snapTolerance       = 3.0;     // [Topology]
    double maxSegmentLength    = 1000.0;  // [Topology]
    double intersectionEpsilon = 1.5;     // [Intersection]
    double minSwitchAngle      = 15.0;    // [Switch]
    double junctionTrimMargin  = 25.0;    // [Switch]
    double doubleSwitchRadius  = 50.0;    // [Switch]
    double switchSideSize      = 15.0;    // [Switch]  ← nouveau
    double minBranchLength     = 100.0;   // [CDC]
};
```

| Paramètre | Défaut | Section .ini | Description |
|-----------|--------|--------------|-------------|
| `snapTolerance` | `3.0 m` | `[Topology]` | Rayon de fusion des nœuds proches |
| `maxSegmentLength` | `1000.0 m` | `[Topology]` | Longueur maximale d'un StraightBlock avant subdivision |
| `intersectionEpsilon` | `1.5 m` | `[Intersection]` | Tolérance de détection d'intersection géométrique |
| `minSwitchAngle` | `15.0°` | `[Switch]` | Angle minimal pour identifier une bifurcation réelle |
| `junctionTrimMargin` | `25.0 m` | `[Switch]` | Marge de recadrage visuel aux jonctions |
| `doubleSwitchRadius` | `50.0 m` | `[Switch]` | Distance maximale entre deux switches pour former une double aiguille |
| `switchSideSize` | `15.0 m` | `[Switch]` | Longueur des branches CDC (tips root/normal/deviation) depuis la jonction |
| `minBranchLength` | `100.0 m` | `[CDC]` | Longueur minimale de branche pour la validation CDC |

**Séparation POD / persistance (SRP) :**

```
ParserConfig     — décrit les paramètres                  (aucune dépendance)
ParserConfigIni  — lit/écrit parser_settings.ini          (dépend de SimpleIni)
```

`GeoParser` reçoit `ParserConfig` **par valeur** — snapshot immuable pendant toute la durée d'un parsing, thread-safe par construction.

---

# PipelineContext — Transporteur inter-phases {#gp_context}

`PipelineContext` est le **seul** objet partagé entre toutes les phases.

```
Phase1  écrit  ctx.rawNetwork
Phase2  lit    ctx.rawNetwork      écrit  ctx.intersections
Phase3  lit    ctx.intersections   écrit  ctx.splitNetwork    → rawNetwork.clear(), intersections.clear()
Phase4  lit    ctx.splitNetwork    écrit  ctx.topoGraph
Phase5  lit    ctx.topoGraph       écrit  ctx.classifiedNodes (lit aussi ctx.splitNetwork)
Phase6  lit    ctx.topoGraph
               ctx.classifiedNodes
               ctx.splitNetwork    écrit  ctx.blocks          → topoGraph.clear(), classifiedNodes.clear(), splitNetwork.clear()
Phase8a lit    ctx.blocks          (résolution pointeurs)
Phase7  lit    ctx.blocks          (modifie orientations, absorbe doubles, calcule tips)
Phase8b lit    ctx.blocks          → TopologyRepository       → blocks.clear()
```

**PhaseStats — instrumentation intégrée :**

Chaque phase enregistre sa durée et son compte d'éléments dans `ctx.stats`.
GeoParser::logPerformanceSummary() "GeoParser::logPerformanceSummary()" produit le tableau de performance en fin de pipeline.

---

# GeoParser — Orchestrateur {#gp_orchestrator}

GeoParser possède `PipelineContext` et `ParserConfig`.
Il enchaîne les phases et reporte la progression via callback.

| Méthode | Rôle |
|---------|------|
| GeoParser::GeoParser() "GeoParser(config, logger, onProgress)" | Construction — snapshot de config |
| GeoParser::parse() "parse(filePath)" | Pipeline complet — phases 1 à 8 |
| GeoParser::reportProgress() "reportProgress(int)" | Callback UI + log de la dernière phase |
| GeoParser::logPerformanceSummary() "logPerformanceSummary()" | Tableau de performance final |

---

# Phases du pipeline {#gp_phases}

## Phase 1 — GeoLoader {#gp_phase1}

**Fichiers :** `Phase1_GeoLoader.h/.cpp` · `RawNetwork.h`

Charge le fichier GeoJSON, projette les coordonnées WGS-84 en UTM et produit le `RawNetwork`.

| Étape interne | Description |
|---------------|-------------|
| Lecture JSON | `nlohmann::json::parse()` — lève `std::runtime_error` si GeoJSON invalide |
| Filtrage | Seules les features `LineString` sont retenues |
| Détection zone UTM | Calculée depuis le premier point du premier segment |
| Projection | WGS-84 → UTM (formules ellipsoïde WGS-84 complètes, Phase1_GeoLoader::project) |

**Sortie :** `ctx.rawNetwork` — polylignes avec points WGS-84 et UTM synchronisés.

---

## Phase 2 — GeometricIntersector {#gp_phase2}

**Fichiers :** `Phase2_GeometricIntersector.h/.cpp` · `IntersectionMap.h`

Calcule tous les points d'intersection géométrique entre segments du `RawNetwork`.

| Mécanisme | Description |
|-----------|-------------|
| Algorithme | Cramer (résolution système linéaire 2×2) |
| Tolérance | `config.intersectionEpsilon` — évite les faux positifs sur flottants |
| Optimisation | Spatial grid binning : O(n·k) — seuls les segments de cellules adjacentes sont testés |

**Sortie :** `ctx.intersections` — `map<SegmentId, vector<IntersectionPoint>>`

### Algorithme de Cramer

Deux segments AB et CD se croisent si on peut écrire :

```
P = A + t * (B - A)    t ∈ [0,1]
P = C + u * (D - C)    u ∈ [0,1]
```

En égalisant :

```
| Bx-Ax   -(Dx-Cx) |   | t |   | Cx-Ax |
| By-Ay   -(Dy-Cy) | × | u | = | Cy-Ay |
```

`det == 0` → segments parallèles. Sinon, `t` et `u` résolus par Cramer.
Les segments se croisent si et seulement si `t ∈ [0,1]` et `u ∈ [0,1]`.

> Référence : https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection

---

## Phase 3 — NetworkSplitter {#gp_phase3}

**Fichiers :** `Phase3_NetworkSplitter.h/.cpp` · `SplitNetwork.h`

Découpe les polylignes brutes aux points d'intersection et aux dépassements de `maxSegmentLength`.

| Mécanisme | Description |
|-----------|-------------|
| Découpe aux intersections | Tri + dédoublonnage des paramètres `t` par distance curviligne |
| Filtrage micro-segments | Segments < `2 * intersectionEpsilon` supprimés |
| Découpe par longueur | Interpolation linéaire UTM aux multiples de `maxSegmentLength` |
| Libération mémoire | `ctx.rawNetwork.clear()` + `ctx.intersections.clear()` après production |

**Correction — cohérence `globalIdx` :**
Les polylignes dégénérées (`pointsUTM.size() < 2`) sont sautées **sans incrémenter**
`globalIdx`, ce qui maintient la cohérence avec l'index calculé par
Phase2_GeometricIntersector::globalSegmentIndex() "Phase2::globalSegmentIndex()".
L'ancienne version incrémentait `globalIdx` dans ce cas, causant un décalage
qui faisait ignorer des intersections valides.

**Sortie :** `ctx.splitNetwork` — vecteur d'`AtomicSegment`

---

## Phase 4 — TopologyBuilder {#gp_phase4}

**Fichiers :** `Phase4_TopologyBuilder.h/.cpp` · `TopologyGraph.h`

Construit le **graphe planaire** depuis les segments atomiques.
Fusionne les extrémités proches via Union-Find + grid binning.

| Mécanisme | Description |
|-----------|-------------|
| Snap | Grid binning — nœuds dans un rayon `snapTolerance` mis en candidats |
| Union-Find | Fusion des nœuds candidats — path compression + union by rank |
| Graphe | Nœuds (positions UTM fusionnées) + arêtes (segments) + adjacence |

**Correction :** `throw EXCEPTION_EXECUTE_FAULT` remplacé par
`throw std::runtime_error(...)` — catchable proprement par les handlers
standards (`catch (const std::exception&)`) dans `GeoParser::parse()`.

**Note mémoire :** `splitNetwork` n'est **pas** libéré ici (contrairement à l'ancienne
version). Phase5_SwitchClassifier en a encore besoin pour les vecteurs d'angle.

**Sortie :** `ctx.topoGraph` — graphe planaire nœuds + arêtes UTM

### Union-Find — principe

```
Éléments : [0, 1, 2, 3, 4]
parent   : [0, 0, 2, 2, 4]  ← 0 et 1 dans le même ensemble, 2 et 3 aussi

find(3) → parent[3] = 2 → parent[2] = 2 → racine = 2
unite(1, 3) → parent[0] = 2
```

Path compression : `parent[x] = find(parent[x])` — aplatit l'arbre récursivement.
Union by rank : la racine de rang inférieur pointe vers la racine de rang supérieur — évite les arbres dégénérés.

> Référence : https://en.wikipedia.org/wiki/Disjoint-set_data_structure

---

## Phase 5 — SwitchClassifier {#gp_phase5}

**Fichiers :** `Phase5_SwitchClassifier.h/.cpp` · `ClassifiedNodes.h`

Attribue une `NodeClass` à chaque nœud du graphe planaire en combinant **degré** et **angle**.

```cpp
enum class NodeClass
{
    TERMINUS,   // degré == 1
    STRAIGHT,   // degré == 2, angle ≈ 180° (± minSwitchAngle)
    SWITCH,     // degré == 3, bifurcation géométrique réelle
    CROSSING,   // degré == 4 — croisement plat, ignoré
    ISOLATED,   // degré == 0
    AMBIGUOUS   // autre — WARNING
};
```

**Correction — suppression de la dépendance `SplitNetwork` :**
`outVector()` calculait auparavant la direction depuis les points intermédiaires
du segment atomique (`SplitNetwork`). Il utilise maintenant directement la
position UTM du nœud opposé dans le graphe — indépendant de `SplitNetwork`,
et suffisamment précis après découpe par `maxSegmentLength`.

```cpp
// Avant (dépendait de SplitNetwork)
static CoordinateXY outVector(const TopologyGraph&, const SplitNetwork&, size_t, size_t);

// Après (indépendant)
static CoordinateXY outVector(const TopologyGraph&, size_t nodeId, size_t edgeIdx);
```

**Sortie :** `ctx.classifiedNodes` — `unordered_map<size_t, NodeClass>`

---

## Phase 6 — BlockExtractor {#gp_phase6}

**Fichiers :** `Phase6_BlockExtractor.h/.cpp` · `BlockSet.h`

Transforme le graphe planaire classifié en blocs ferroviaires.

| Mécanisme | Description |
|-----------|-------------|
| Nœuds frontières | `SWITCH`, `TERMINUS`, `CROSSING` — délimitent les blocs |
| Nœuds transparents | `STRAIGHT` — traversés lors du DFS |
| DFS entre frontières | Parcours itératif — concatène les segments en voie droite |
| Subdivision | Si longueur > `maxSegmentLength` → N sous-blocs chaînés par `prev/next` |
| SwitchBlocks | Un bloc par nœud `SWITCH` |
| Libération | `ctx.topoGraph.clear()`, `ctx.classifiedNodes.clear()`, `ctx.splitNetwork.clear()` |

**Sortie :** `ctx.blocks` — `BlockSet`

### Déduplication — par arêtes (et non par paire de nœuds)

L'ancienne déduplication `processedPairs.insert(pairKey(nodeA, nodeB))` empêchait
la création de deux straights entre les mêmes switches — cassant les configurations
**crossover** (voie double). La marque à la place les arêtes utilisées (`usedEdges`) :

```
Straight créé via startEdge → ... → lastEdge :
  usedEdges.insert(startEdge)   ← empêche une nouvelle traversal depuis cet arête
  usedEdges.insert(lastEdge)    ← empêche la traversal inverse B→A
```

Deux straights empruntant des arêtes de départ distinctes peuvent ainsi coexister.

### Subdivision par longueur cumulée

L'ancienne subdivision découpait par proportion de points (`k * totalPts / N`),
ce qui produisait des sous-blocs inégaux si les points GeoJSON étaient mal répartis.
La calcule d'abord les longueurs cumulées :

```cpp
cumLen[0] = 0
cumLen[i] = cumLen[i-1] + hypot(pts[i] - pts[i-1])
totalLen  = cumLen.back()
```

Puis pour le sous-bloc `k`, cherche le premier point `i` où `cumLen[i] >= k/N * totalLen`.
Chaque sous-bloc a ainsi une longueur UTM quasi-identique indépendamment de la densité des points.

### Index directionnels dans BlockSet

```
straightsByNode        : nodeId → vector<StraightBlock*>
                         Multi-valué (switch adjacent à plusieurs straights)

straightByEndpointPair : Cantor(min(A,B), max(A,B)) → StraightBlock*
                         Un seul élément — utilisé par rebuildStraightIndex()

straightByDirectedPair : (from * 1'000'000 + to) → vector<StraightBlock*>
                         Multi-valué pour les crossovers.
                         directedKey(switchNode, frontier) → sous-bloc adjacent au switch
                         directedKey(frontier, switchNode) → sous-bloc adjacent au frontier
```

`extractSwitches` utilise `straightByDirectedPair` pour résoudre les endpoints
de chaque branche. En cas de crossover (deux straights pour la même clé directionnelle),
un ensemble `usedStraights` par switch garantit que chaque branche reçoit un straight distinct.

### Chaînage des sous-blocs

Les sous-blocs produits par subdivision sont chaînés immédiatement par `prev/next`
dans `registerStraight`. Phase8_RepositoryTransfer::resolveStraight() "Phase8::resolveStraight()"
ne surécrit ces pointeurs que si `neighbourId` est non vide — les endpoints internes
(avec `frontierNodeId == SIZE_MAX`) ne sont jamais touchés, préservant la chaîne.

---

## Phase 7 — SwitchProcessor {#gp_phase7}

**Fichiers :** `Phase7_SwitchProcessor.h/.cpp`

Fusion de l'ancien `Phase7_DoubleSwitchDetector` et `Phase8_SwitchOrientator`.
Les deux classes partageaient les mêmes préconditions (pointeurs résolus par Phase 8a)
et s'enchaînaient naturellement — leur fusion réduit la surface du pipeline.

### Sous-phases G → A → B → C → D → E → F

| Sous-phase | Description |
|------------|-------------|
| **G** | Orientation géométrique root / normal / deviation |
| **A** | Détection des clusters double switch |
| **B** | Absorption du segment de liaison |
| **C** | Validation CDC (`minBranchLength`) |
| **D** | Détection des crossovers |
| **E** | Cohérence des crossovers (branches partagées → DEVIATION) |
| **F** | Calcul des tips CDC (`switchSideSize`) |

### G — Orientation géométrique

Root, normal et deviation sont déterminés par heuristique vectorielle sur les
positions UTM des blocs adjacents — sans dépendance GeoJSON ni donnée de sens de circulation :

1. Calcule 3 vecteurs UTM unitaires depuis la jonction vers chaque branche.
2. **Root** = branche dont le vecteur a le dot product minimal avec la résultante normalisée des deux autres (branche la plus opposée).
3. **Normal** = des deux restantes, celle dont le dot product avec root est le plus négatif (continuation directe, angle ≈ 180°).
4. **Deviation** = la troisième.

```cpp
// Résultante des branches j et k
CoordinateXY resultant = normalize(vecs[j] + vecs[k]);
double score = dot(vecs[i], resultant);  // minimum → root
```

### F — Tips CDC

`interpolateTip()` parcourt la géométrie WGS84 du straight depuis l'extrémité
la plus proche de la jonction et interpole le point à `config.switchSideSize` mètres
(distance Haversine). Retourne l'extrémité distale si la branche est plus courte.

---

## Phase 8 — RepositoryTransfer {#gp_phase8}

**Fichiers :** `Phase8_RepositoryTransfer.h/.cpp`

Transfère `ctx.blocks` vers TopologyRepository. Scindée en deux appels
dans l'orchestrateur pour respecter la contrainte d'ordre avec Phase 7.

**Ordre réel dans GeoParser::parse() "GeoParser::parse()" :**

```
Phase8_RepositoryTransfer::resolve(ctx)   — résolution pointeurs
Phase7_SwitchProcessor::run(ctx, ...)     — orientation (nécessite pointeurs)
Phase8_RepositoryTransfer::transfer(ctx)  — std::move → TopologyRepository + buildIndex()
```

| Méthode | Rôle |
|---------|------|
| Phase8_RepositoryTransfer::resolve() "resolve()" | Résolution des `ShuntingElement*` inter-blocs depuis les IDs câblés en Phase 6 |
| Phase8_RepositoryTransfer::transfer() "transfer()" | `std::move` des `unique_ptr` → TopologyData + `buildIndex()` |

### resolveStraight — préservation de la chaîne

`resolveStraight()` ne modifie `setNeighbourPrev/Next` que si `neighbourId` est non vide.
Les sous-blocs internes d'un straight subdivisé (`frontierNodeId == SIZE_MAX`, `neighbourId == ""`)
ne sont jamais touchés — leur chaîne `prev/next` posée par Phase 6 est préservée.

**Stabilité des adresses :** TopologyData stocke en `vector<unique_ptr<T>>`.
Après `std::move`, les adresses des objets alloués sont stables — les pointeurs
résolus par `resolve()` restent valides après le transfert.

---

# GeoParsingTask — Intégration async {#gp_task}

GeoParsingTask est le point d'entrée depuis `MainWindow`.

```cpp
void GeoParsingTask::start(const std::string& filePath,
                            const ParserConfig& config);
void GeoParsingTask::cancel();
```

**Cancel token partagé :**

```
GeoParsingTask
  └── m_cancelToken : shared_ptr<atomic<bool>>
        └── copié dans le thread détaché → partagé avec GeoParser
```

Le thread vérifie `*m_cancelToken` entre les phases. Si `true`, il envoie
`WM_PARSING_CANCELLED` et s'arrête proprement sans altérer TopologyRepository.

| Message Win32 | Contenu | Handler |
|---------------|---------|---------|
| `WM_PROGRESS_UPDATE` | Avancement 0–100 | `MainWindow::onProgressUpdate()` |
| `WM_PARSING_SUCCESS` | — | `MainWindow::onParsingSuccess()` |
| `WM_PARSING_ERROR` | `std::string*` (à libérer) | `MainWindow::onParsingError()` |
| `WM_PARSING_CANCELLED` | — | `MainWindow::onParsingCancelled()` |

---

# ParserSettingsDialog {#gp_dialog}

**Fichiers :** `Engine/HMI/Dialogs/ParserSettingsDialog.h/.cpp`

Dialogue modal Win32 permettant à l'utilisateur de modifier les 8 paramètres
de `ParserConfig` depuis l'interface.

**Comportement :**
- 8 contrôles `EDIT` Win32 (dont `IDC_EDIT_SWITCH_SIDE_SIZE` pour `switchSideSize`)
- Validation des plages (valeurs positives, cohérence `intersectionEpsilon` < `snapTolerance`)
- Boutons **OK** / **Annuler** / **Réinitialiser aux défauts**
- OK → `ParserConfigIni::save()` → rechargement dans `MainWindow`

**Pattern :** `DialogBoxParam` / `WM_INITDIALOG` / `IDOK` / `IDCANCEL`.

---

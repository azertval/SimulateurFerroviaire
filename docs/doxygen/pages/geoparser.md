@page page_geoparser Pipeline GeoParser v2

@tableofcontents

---

## Vue d'ensemble {#geo_overview}

Le **pipeline GeoParser v2** transforme un fichier GeoJSON contenant la géométrie
brute d'un réseau ferroviaire (polylignes OSM) en une topologie structurée de
blocs (@ref StraightBlock, @ref SwitchBlock) prête à être consommée par le
@ref page_pcc "module PCC" et le rendu WebView.

Le pipeline est **séquentiel** et se déroule en 8 phases numérotées, chacune
encapsulée dans une classe statique `PhaseN_Xxx`. Les données intermédiaires
transitent via le @ref PipelineContext, qui est l'unique objet partagé entre
les phases.

### Ordre d'exécution (immuable)

```
Phase1  → Phase2  → Phase3  → Phase4  → Phase5  → Phase6
   GeoLoader  Intersector  Splitter  TopoBuilder  Classifier  Extractor

Phase8::resolve()  →  Phase7  →  Phase8::transfer()
  ResolvePtrs         SwitchProc    TransferToRepo
```

> **Invariant critique :** `Phase8::resolve()` doit précéder `Phase7`
> (le processeur d'aiguillages a besoin des pointeurs résolus pour le recul
> des tips CDC). `Phase8::transfer()` doit toujours être **en dernier**.

---

## PipelineContext {#geo_context}

@ref PipelineContext est le sac de données partagé entre toutes les phases.
Il évolue à chaque phase : les champs produits par une phase deviennent des
entrées de la suivante. Certains champs sont explicitement libérés par la phase
qui les a consommés (ex. `topoGraph`, `classifiedNodes`, `splitNetwork` sont
effacés en fin de Phase 6 pour libérer la mémoire).

Structure simplifiée :

```
PipelineContext
├── filePath         : string           (Phase 1 → lecture)
├── rawFeatures      : vector<GeoFeature>  (Phase 1 → sortie)
├── splitNetwork     : SplitNetwork     (Phase 3 → sortie, libéré Phase 6)
├── topoGraph        : TopoGraph        (Phase 4 → sortie, libéré Phase 6)
├── classifiedNodes  : ClassifiedNodes  (Phase 5 → sortie, libéré Phase 6)
└── blocks           : BlockSet         (Phase 6 → sortie, Phase 8 → transfert)
```

---

## Phases détaillées {#geo_phases}

### Phase 1 — GeoLoader {#geo_p1}

**Classe :** @ref Phase1_GeoLoader

Charge le fichier GeoJSON depuis le disque et désérialise chaque `Feature`
de type `LineString` en une liste de `GeoFeature` (polyligne WGS-84).

Les features non-linéaires (points, polygones) sont ignorées avec un warning.

**Entrées :** `ctx.filePath`
**Sorties :** `ctx.rawFeatures`

---

### Phase 2 — GeometricIntersector {#geo_p2}

**Classe :** @ref Phase2_GeometricIntersector

Détecte et matérialise les **intersections géométriques** entre segments
de polylignes distincts qui se croisent sans partager de nœud commun dans
le GeoJSON source.

Pour chaque paire de segments, un test d'intersection 2D (sur coordonnées UTM)
est effectué. Si l'intersection tombe dans le segment (hors extrémités à
`intersectionEpsilon` près), un **nouveau point** est inséré dans les deux
polylignes à la position exacte de croisement.

**Paramètre clé :** `config.intersectionEpsilon`

**Entrées :** `ctx.rawFeatures`
**Sorties :** `ctx.rawFeatures` (modifiées in place)

---

### Phase 3 — NetworkSplitter {#geo_p3}

**Classe :** @ref Phase3_NetworkSplitter

**Fusionne** les extrémités proches (`snapTolerance`) en un nœud unique, puis
**découpe** les polylignes aux points de fusion pour obtenir un réseau
d'**segments atomiques** (un segment = deux extrémités, pas de nœud interne).

À l'issue de cette phase, tout point présent à l'intérieur d'une polyligne
est forcément une extrémité d'un autre segment.

**Paramètre clé :** `config.snapTolerance`

**Entrées :** `ctx.rawFeatures`
**Sorties :** `ctx.splitNetwork` (collection de `AtomicSegment`)

---

### Phase 4 — TopologyBuilder {#geo_p4}

**Classe :** @ref Phase4_TopologyBuilder

Construit le **graphe topologique** (`TopoGraph`) à partir du réseau atomique.
Chaque nœud du graphe correspond à une extrémité physique unique (après snap) ;
chaque arête correspond à un `AtomicSegment`.

Le graphe est non-orienté : les adjacences sont bidirectionnelles.

**Entrées :** `ctx.splitNetwork`
**Sorties :** `ctx.topoGraph`

---

### Phase 5 — SwitchClassifier {#geo_p5}

**Classe :** @ref Phase5_SwitchClassifier

Classifie chaque nœud du graphe en l'une des trois catégories :

| `NodeClass` | Degré | Condition |
|-------------|-------|-----------|
| `ENDPOINT` | 1 | Terminus de ligne |
| `STRAIGHT` | 2 | Nœud de passage — pas d'intersection |
| `SWITCH` | 3+ | Bifurcation — angle entre branches ≥ `minSwitchAngle` |

Un nœud de degré 3 dont toutes les branches sont colinéaires (angle < seuil)
est reclassifié en `STRAIGHT` pour éviter de créer un aiguillage fantôme
sur une légère imperfection géométrique OSM.

**Paramètres clés :** `config.minSwitchAngle`, `config.minBranchLength`

**Entrées :** `ctx.topoGraph`
**Sorties :** `ctx.classifiedNodes`

---

### Phase 6 — BlockExtractor {#geo_p6}

**Classe :** @ref Phase6_BlockExtractor

Produit les @ref StraightBlock et @ref SwitchBlock à partir du graphe classifié.

#### Extraction des straights

DFS entre nœuds frontières (classe ≠ `STRAIGHT`). La déduplication repose sur
les **indices d'arêtes** (`usedEdges`) et non sur la clé de paire de nœuds :
cette approche permet de créer deux straights distincts entre les mêmes deux
nœuds frontières dans les configurations **crossover** (voie double).

Si la longueur totale UTM du straight assemblé dépasse `maxSegmentLength`,
il est **subdivisé** en *N* sous-blocs chaînés via `prev`/`next`. Seuls les
sous-blocs de tête et de queue possèdent des `BlockEndpoint` avec
`frontierNodeId` valide (≠ `SIZE_MAX`).

#### Extraction des switches

Un @ref SwitchBlock par nœud `SWITCH`. Pour chaque branche, une traversal
des nœuds intermédiaires remonte jusqu'au nœud frontière adjacent, puis
`straightByDirectedPair` (multi-valué) fournit le @ref StraightBlock
adjacent. Un ensemble `usedStraights` local garantit qu'un même straight
n'est pas attribué à deux branches du même switch (cas crossover).

#### Index produits

| Index | Clé | Valeur |
|-------|-----|--------|
| `straightsByNode` | `nodeId` | `vector<StraightBlock*>` (multi-valué) |
| `straightByEndpointPair` | `Cantor(min,max)` | `StraightBlock*` premier depuis A |
| `straightByDirectedPair` | `from*1e6 + to` | `vector<StraightBlock*>` (multi-valué) |
| `switchByNode` | `nodeId` | `SwitchBlock*` |

**Libération mémoire :** `topoGraph`, `classifiedNodes`, `splitNetwork` sont
vidés en fin de phase.

---

### Phase 8a — RepositoryTransfer::resolve() {#geo_p8a}

**Classe :** @ref Phase8_RepositoryTransfer, méthode `resolve()`

Résout les **pointeurs inter-blocs** en deux passes :

1. **Passe 1 — StraightBlocks :** Pour chaque endpoint d'un straight avec
   `frontierNodeId` valide, cherche le voisin dans `switchByNode` ou
   `straightsByNode` et appelle `setNeighbourPrev/Next`. Les endpoints internes
   (`frontierNodeId == SIZE_MAX`) sont ignorés — la chaîne prev/next posée
   par Phase 6 est préservée.

2. **Passe 2 — SwitchBlocks :** Résout les pointeurs `root`, `normal`,
   `deviation` depuis les IDs stockés dans les endpoints de branches.

> **Correctif v2 :** La vérification de `frontierNodeId != SIZE_MAX` avant
> tout appel à `setNeighbour*` est essentielle pour ne pas écraser la chaîne
> de subdivision posée par Phase 6.

---

### Phase 7 — SwitchProcessor {#geo_p7}

**Classe :** @ref Phase7_SwitchProcessor

Traite les aiguillages en plusieurs sous-phases séquentielles (G→A→B→C→D→E→F) :

| Sous-phase | Rôle |
|------------|------|
| G — DoubleSwitchDetector | Détecte les paires d'aiguilles doubles et marque l'absorption |
| A — OrientationResolver | Identifie root/normal/deviation par analyse angulaire |
| B — CDCTipInterpolator | Interpole les tips CDC à distance `switchSideSize` |
| C — StraightTrimmer | Recule les extrémités des straights adjacents |
| D — OverlapResolver | Résout les chevauchements restants |
| E — BranchValidator | Valide la cohérence des branches |
| F — DoubleAbsorber | Absorbe les coordonnées du segment de liaison |

**Paramètres clés :** `config.junctionTrimMargin`, `config.switchSideSize`,
`config.doubleSwitchRadius`

> **Issue connue (différé) :** `DoubleSwitchDetector` échoue sur certaines
> configurations de crossover où la branche déviation du premier switch pointe
> vers le straight de liaison plutôt que vers le second switch.

---

### Phase 8b — RepositoryTransfer::transfer() {#geo_p8b}

**Classe :** @ref Phase8_RepositoryTransfer, méthode `transfer()`

Transfère l'ownership des blocs depuis `ctx.blocks` vers
@ref TopologyRepository par déplacement (`std::move`). Appelle ensuite
`TopologyData::buildIndex()` pour construire les maps `id → ptr`.

Après `transfer()`, le `PipelineContext` est vide et le `TopologyRepository`
est le seul détenteur des blocs.

---

## Exécution asynchrone — GeoParsingTask {#geo_task}

@ref GeoParsingTask encapsule l'exécution du pipeline dans un **thread
séparé** pour ne pas bloquer le thread UI Win32. Elle :

- Instancie un @ref GeoParser avec la @ref ParserConfig courante.
- Lance `parse()` dans `std::async`.
- Poste des messages `WM_PROGRESS_UPDATE`, `WM_PARSING_SUCCESS` ou
  `WM_PARSING_ERROR` vers la `HWND` de la `MainWindow`.
- Expose un `cancelToken` (`shared_ptr<atomic<bool>>`) vérifié entre chaque
  phase — l'annulation propre lève `GeoParser::CancelledException`.

---

## Progression rapportée {#geo_progress}

| Phase | Progression |
|-------|-------------|
| Phase 1 | 0 % |
| Phase 2 | 5 % |
| Phase 3 | 30 % |
| Phase 4 | 45 % |
| Phase 5 | 58 % |
| Phase 6 | 65 % |
| Phase 8a + Phase 7 | 75 % |
| Phase 8b | 90 % |
| Terminé | 100 % |

---

## Références croisées {#geo_refs}

| Classe | Fichier | Rôle |
|--------|---------|------|
| @ref GeoParser | `Modules/GeoParser/GeoParser.h` | Orchestrateur |
| @ref GeoParsingTask | `Modules/GeoParser/GeoParsingTask.h` | Thread asynchrone |
| @ref PipelineContext | `Modules/GeoParser/Pipeline/PipelineContext.h` | Données partagées |
| @ref BlockSet | `Modules/GeoParser/Pipeline/BlockSet.h` | Blocs produits par Phase 6 |
| @ref BlockEndpoint | `Modules/GeoParser/Pipeline/BlockSet.h` | Extrémité d'un bloc |
| @ref Phase1_GeoLoader | `Modules/GeoParser/Pipeline/Phase1_GeoLoader.h` | Chargement GeoJSON |
| @ref Phase2_GeometricIntersector | `Modules/GeoParser/Pipeline/Phase2_GeometricIntersector.h` | Intersections |
| @ref Phase3_NetworkSplitter | `Modules/GeoParser/Pipeline/Phase3_NetworkSplitter.h` | Découpe et snap |
| @ref Phase4_TopologyBuilder | `Modules/GeoParser/Pipeline/Phase4_TopologyBuilder.h` | Graphe topologique |
| @ref Phase5_SwitchClassifier | `Modules/GeoParser/Pipeline/Phase5_SwitchClassifier.h` | Classification nœuds |
| @ref Phase6_BlockExtractor | `Modules/GeoParser/Pipeline/Phase6_BlockExtractor.h` | Extraction des blocs |
| @ref Phase7_SwitchProcessor | `Modules/GeoParser/Pipeline/Phase7_SwitchProcessor.h` | Traitement aiguillages |
| @ref Phase8_RepositoryTransfer | `Modules/GeoParser/Pipeline/Phase8_RepositoryTransfer.h` | Résolution + transfert |
| @ref ParserConfig | `Engine/Core/Config/ParserConfig.h` | Paramètres pipeline |

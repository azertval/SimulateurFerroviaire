@page page_elements Éléments interactifs ferroviaires

@tableofcontents

---

## Vue d'ensemble {#elem_overview}

Le module **Elements** constitue le **modèle de domaine** du simulateur.
Il fournit la hiérarchie de classes représentant les objets physiques de
l'infrastructure ferroviaire — sections de voie et aiguillages — dans leur
état opérationnel courant.

Ces classes sont produites par le @ref page_geoparser "pipeline GeoParser",
stockées dans @ref TopologyData et consommées par le
@ref page_pcc "module PCC" et le rendu WebView.

---

## Hiérarchie de classes {#elem_hierarchy}

```
Element                     (classe abstraite — id, type)
└── ShuntingElement         (ajoute getState() / ShuntingState)
    ├── StraightBlock       (section de voie droite)
    └── SwitchBlock         (aiguillage 3 branches)
```

La hiérarchie est délibérément plate : deux niveaux d'abstraction suffisent
pour couvrir l'ensemble de l'infrastructure cible.

---

## Element {#elem_element}

@ref Element est la racine abstraite. Elle impose deux contrats minimaux :

- **`getId()`** — identifiant unique sous la forme `"s/N"` (straight) ou
  `"sw/N"` (switch), attribué par le pipeline lors de la Phase 6.
- **`getType()`** — discriminant `ElementType::STRAIGHT` ou
  `ElementType::SWITCH`, utilisé pour éviter le `dynamic_cast` dans le
  code de rendu critique.

La copie est interdite (risque de *slicing*). Le déplacement est autorisé
pour les constructions par `make_unique` et les insertions en vecteur.

### Logger partagé

Tous les éléments partagent un unique `Logger m_logger("Elements")` déclaré
**statique** dans `Element`. Cela garantit qu'un seul fichier
`Logs/Elements.log` est créé, même en présence de centaines d'instances.
Le mutex interne au `Logger` assure la thread-safety.

---

## ShuntingElement {#elem_shunting}

@ref ShuntingElement étend `Element` avec la notion d'**état opérationnel** :

| @ref ShuntingState | Signification |
|--------------------|---------------|
| `FREE` | Élément libre, opérationnel (défaut) |
| `OCCUPIED` | Un véhicule est présent sur l'élément |
| `INACTIVE` | Hors service — panne ou maintenance |

Les accesseurs de commodité `isFree()`, `isOccupied()`, `isInactive()` évitent
les comparaisons répétitives avec l'enum.

L'état est mutable en runtime par l'opérateur (clic Leaflet sur un aiguillage)
ou par la simulation. Les mutations passent par `setState()`.

> **Note de conception :** Le destructeur virtuel de `ShuntingElement`
> supprimerait la génération implicite des opérateurs de déplacement en C++17.
> Ils sont donc réexplicitement déclarés `= default` pour maintenir la
> déplaçabilité requise par `std::unique_ptr` et `std::vector`.

---

## StraightBlock {#elem_straight}

@ref StraightBlock représente une **section de voie droite** (tronçon entre
deux nœuds frontières, ou entre deux sous-blocs de subdivision).

### Géométrie

Chaque straight porte deux polylignes parallèles :

| Collection | Système | Usage |
|------------|---------|-------|
| `pointsWGS84` | WGS-84 (lat/lon) | Rendu Leaflet via `TopologyRenderer` |
| `pointsUTM` | UTM zone 30N (x=est, y=nord, mètres) | Calculs métriques pipeline |

Les deux collections ont **toujours la même taille** et les mêmes indices :
`pointsWGS84[i]` et `pointsUTM[i]` désignent le même point physique.

`getLengthMeters()` retourne la longueur Haversine calculée sur WGS-84.
`getLengthUTM()` retourne la longueur euclidienne calculée sur UTM.

### Chaîne prev/next

Quand `maxSegmentLength` est dépassé, le pipeline subdivise un straight en
*N* sous-blocs. Les sous-blocs sont chaînés par des pointeurs non-propriétaires :

```
  s/0_c0 ──next──► s/0_c1 ──next──► s/0_c2
         ◄──prev──          ◄──prev──
```

Ces pointeurs sont posés par @ref Phase6_BlockExtractor::registerStraight
**avant** la résolution de Phase 8. Seuls les sous-blocs de tête et de queue
ont des endpoints avec un `frontierNodeId` valide.

### Voisins topologiques

`getNeighbourNext()` / `getNeighbourPrev()` retournent les blocs adjacents
(straight ou switch) après résolution par @ref Phase8_RepositoryTransfer.
`getPCCNeighbours()` expose les deux sans distinguer next/prev — utilisé par
@ref PCCGraphBuilder pour construire les arêtes STRAIGHT.

---

## SwitchBlock {#elem_switch}

@ref SwitchBlock représente un **aiguillage ferroviaire à 3 branches**.

### Anatomie d'un aiguillage

```
              ← root
             /
jonction ───┤
             \
              ← normal     (voie directe, position repos)
               \
                ← deviation (voie déviée, position basculée)
```

Le point de **jonction** (`junctionWGS84` / `junctionUTM`) est la position
physique de l'aiguillage sur la carte.

Chaque branche est identifiée par l'ID du bloc adjacent (`rootId`,
`normalId`, `deviationId`) et par un pointeur résolu (`root`, `normal`,
`deviation`) vers le @ref ShuntingElement correspondant.

### État actif

@ref ActiveBranch indique la voie ouverte :

| Valeur | Signification | Couleur TCO |
|--------|---------------|-------------|
| `NORMAL` | Voie directe — position repos | Trait horizontal plein |
| `DEVIATION` | Voie déviée — position basculée | Branche oblique active |

`toggleActiveBranch(propagate)` inverse l'état et propage aux partenaires
double-aiguille si `propagate == true`.

### Tips CDC

Les **tips CDC** (*Centre De Contre*) sont les extrémités des stubs de
l'aiguillage sur le schéma TCO. Ils sont interpolés par
@ref Phase7_SwitchProcessor à distance `switchSideSize` de la jonction,
en direction de chaque branche :

| Accesseur | Branche |
|-----------|---------|
| `getTipOnRoot()` / `getTipOnRootUTM()` | Branche root |
| `getTipOnNormal()` / `getTipOnNormalUTM()` | Branche normale |
| `getTipOnDeviation()` / `getTipOnDeviationUTM()` | Branche déviée |

### Double aiguille

Un switch peut **absorber** le segment de liaison de son partenaire :

```
sw/A ──────────────────── sw/B
     ↑ segment absorbé ↑
```

L'absorption est stockée dans `doubleOnNormal` ou `doubleOnDeviation`
(ID du partenaire) et dans `absorbedNormalCoords` / `absorbedDeviationCoords`
(polyligne WGS-84 absorbée pour le rendu Leaflet).

`isDouble()` retourne `true` si l'une des deux absorptions est renseignée.

### Orientation

Un switch est **orienté** après exécution de @ref Phase7_SwitchProcessor.
`isOriented()` conditionne le rendu des branches dans `TCORenderer` et
`TopologyRenderer`.

---

## Références croisées {#elem_refs}

| Classe / enum | Fichier | Rôle |
|---------------|---------|------|
| @ref Element | `Modules/Elements/Element.h` | Base abstraite |
| @ref ElementType | `Modules/Elements/Element.h` | Discriminant SWITCH/STRAIGHT |
| @ref ShuntingElement | `Modules/Elements/ShuntingElements/ShuntingElement.h` | Ajoute getState() |
| @ref ShuntingState | `Modules/Elements/ShuntingElements/ShuntingElement.h` | FREE/OCCUPIED/INACTIVE |
| @ref StraightBlock | `Modules/Elements/ShuntingElements/StraightBlock.h` | Section de voie |
| @ref SwitchBlock | `Modules/Elements/ShuntingElements/SwitchBlock.h` | Aiguillage 3 branches |
| @ref ActiveBranch | `Modules/Elements/ShuntingElements/SwitchBlock.h` | NORMAL/DEVIATION |

@page elements Éléments — Modèle de domaine ferroviaire

@tableofcontents

---

# Vue d'ensemble {#elements_overview}

Les éléments interactifs constituent le **modèle de domaine ferroviaire**. Ils représentent
les entités physiques de l'infrastructure (tronçons, aiguillages) avec leurs géométries,
leur topologie et leur état opérationnel.

```
GeoParser pipeline                   TopologyRepository         HMI / Rendu
  Phase6_BlockExtractor  ────────►  StraightBlock  ────────►  Leaflet / TCO
  Phase8_SwitchOrientator ──────►  SwitchBlock    ────────►  PCCGraph (via PCCGraphBuilder)
  Phase7_DoubleSwitchDetector ──►  (absorbLink)
  Phase9_RepositoryTransfer ────►  (resolve pointers)
```

**Règle de cycle de vie :**
- Les éléments sont construits avec des données brutes (IDs de voisins sous forme de chaînes).
- Les pointeurs résolus (`prev/next`, `root/normal/deviation`) sont **null** jusqu'à
  `Phase9_RepositoryTransfer::resolve()` — aucun code amont ne doit y accéder.
- `TopologyRepository` est le seul propriétaire des instances (`unique_ptr`) ;
  tous les pointeurs résolus sont **non-propriétaires**.

| Classe | Responsabilité unique |
|--------|----------------------|
| @ref Element | Interface abstraite commune — identifiant, type, logger partagé |
| @ref ShuntingElement | Étend avec un état opérationnel et les helpers @ref ShuntingElement::isFree() "isFree" / @ref ShuntingElement::isOccupied() "isOccupied" / @ref ShuntingElement::isInactive() "isInactive" |
| @ref StraightBlock | Tronçon de voie droite — géométrie duale WGS84/UTM, longueur Haversine, voisins |
| @ref SwitchBlock | Aiguillage 3 branches — orientation, tips CDC, double aiguille, branche active |

---

# Hiérarchie {#elements_hierarchy}

```
Element          getId(), getType(), m_id, m_logger (static)
    └── ShuntingElement     getState(), isFree(), isOccupied(), isInactive(), m_state
            ├── StraightBlock   géométrie WGS84+UTM · voisins prev/next
            └── SwitchBlock     jonction · branches root/normal/deviation · double switch
```

> **Copie interdite / déplacement autorisé** sur toute la hiérarchie.
> La copie est supprimée dans @ref Element (risque de slicing).
> Le déplacement est explicitement déclaré dans @ref ShuntingElement car la présence
> du destructeur virtuel supprimerait sa génération implicite en C++11/14/17 —
> ce qui rendrait @ref StraightBlock et @ref SwitchBlock non-déplaçables et incompatibles
> avec `make_unique` et le pipeline de construction.

---

# Element {#interactive_element}

Classe de base **abstraite**. Définit l'interface commune à tous les éléments ferroviaires.

```
Element
  ├── getId()   → std::string            (pure virtual)
  ├── getType() → ElementType (pure virtual)
  ├── m_id      : std::string            (protected)
  └── m_logger  : Logger                 (protected, static)
```

## Logger statique partagé {#logger}

`Element::m_logger` est une **unique instance statique** partagée par
toutes les classes dérivées (@ref StraightBlock et @ref SwitchBlock confondus).
Cela produit un seul fichier `Logs/Elements.log` regroupant tous les
événements d'infrastructure.

Le mutex interne du `Logger` garantit la thread-safety des écritures concurrentes.

```cpp
// Utilisation dans une classe dérivée
LOG_INFO(m_logger, m_id + " orienté NORMAL→s/1 DEVIATION→s/2");
LOG_DEBUG(m_logger, m_id + " prev=" + m_neighbours.prev->getId());
```

---

# ShuntingElement {#shunting_element}

Couche intermédiaire abstraite. Étend @ref Element avec un **état opérationnel**
(`m_state`, `FREE` par défaut) et trois helpers de commodité non-virtuels basés sur
@ref ShuntingElement::getState() "getState()".

```
ShuntingElement
  ├── getState()  → ShuntingState  (pure virtual)
  ├── isFree()    → bool           basé sur getState()
  ├── isOccupied()→ bool           basé sur getState()
  ├── isInactive()→ bool           basé sur getState()
  └── m_state     : ShuntingState  (protected, FREE par défaut)
```

Les helpers @ref ShuntingElement::isFree() "isFree()" / @ref ShuntingElement::isOccupied() "isOccupied()" /
@ref ShuntingElement::isInactive() "isInactive()" délèguent à @ref ShuntingElement::getState() "getState()"
afin que toute sous-classe surchargeant `getState()` bénéficie automatiquement de la logique correcte.

---

# StraightBlock {#straight_block}

Modèle d'un **tronçon de voie droite**. Identifiant au format `"s/0"`, `"s/1"`, …
ou `"s/0_c1"`, `"s/0_c2"`, … après découpe par le pipeline.

## Géométrie duale WGS84 / UTM {#straight_geometry}

Chaque bloc maintient deux représentations de sa polyligne, pour deux usages distincts :

| Champ | Système | Usage |
|-------|---------|-------|
| `m_pointsWGS84` | WGS-84 (lat, lon) | Rendu Leaflet — `TopologyRenderer` |
| `m_pointsUTM`   | UTM (x = est, y = nord, mètres) | Calculs métriques du pipeline |

Les deux polylignes ont la même taille et le même indexage.
`m_pointsUTM` est vide si le pipeline v2 ne l'a pas renseigné.

**Longueur géodésique :**
`m_lengthMeters` est calculée par somme Haversine sur `m_pointsWGS84`
et mise à jour automatiquement par @ref StraightBlock::setPointsWGS84() "setPointsWGS84()".
@ref StraightBlock::getLengthUTM() "getLengthUTM()" calcule la longueur euclidienne
depuis `m_pointsUTM` à la demande.

## Topologie — IDs de voisins {#straight_topo_ids}

`m_neighbourIds` contient les IDs (chaînes) des blocs adjacents, qu'il s'agisse
de @ref StraightBlock ou de @ref SwitchBlock. La liste est maintenue **triée
lexicographiquement** — @ref StraightBlock::addNeighbourId() "addNeighbourId()" insère
en position correcte et rejette les doublons.

| Méthode | Rôle |
|---------|------|
| @ref StraightBlock::addNeighbourId() "addNeighbourId(id)" | Insertion triée sans doublon |
| @ref StraightBlock::replaceNeighbourId() "replaceNeighbourId(oldId, newId)" | Substitution lors de l'absorption double switch |

## Pointeurs résolus — `StraightNeighbours` {#straight_resolved}

```
struct StraightNeighbours {
    ShuntingElement* prev;   // extrémité A — nullptr si terminus
    ShuntingElement* next;   // extrémité B — nullptr si terminus
};
```

Renseignés par `Phase9_RepositoryTransfer::resolve()` via
@ref StraightBlock::setNeighbourPrev() "setNeighbourPrev()" /
@ref StraightBlock::setNeighbourNext() "setNeighbourNext()" ou
@ref StraightBlock::setNeighbourPointers() "setNeighbourPointers()".
Ils restent `nullptr` jusqu'à cette phase — aucun code antérieur ne doit les lire.

| Méthode | Rôle |
|---------|------|
| @ref StraightBlock::getNeighbours() "getNeighbours()" | Retourne la struct complète |
| @ref StraightBlock::setNeighbourPrev() "setNeighbourPrev(elem)" | Assigne l'extrémité A |
| @ref StraightBlock::setNeighbourNext() "setNeighbourNext(elem)" | Assigne l'extrémité B |
| @ref StraightBlock::setNeighbourPointers() "setNeighbourPointers(n)" | Assigne les deux en une opération |

---

# SwitchBlock {#switch_block}

Modèle d'un **aiguillage ferroviaire à 3 branches**. Identifiant au format `"sw/0"`, `"sw/1"`, …

Un @ref SwitchBlock passe par trois états successifs au fil du pipeline :

```
État 1 — Non orienté   branchIds = ["s/1","s/2","s/3"]   rootId=∅
    ↓  Phase8_SwitchOrientator::orient()
État 2 — Orienté       root="s/1" normal="s/2" deviation="s/3"
    ↓  Phase7_DoubleSwitchDetector::absorbLink()  (si double aiguille)
État 3 — Double        root="s/1" normal="sw/4" deviation="s/3"  ← sw/4 remplace s/2
```

## Géométrie — jonction et tips CDC {#switch_geometry}

Le point de **jonction** (`m_junctionWGS84`, `m_junctionUTM`) est le nœud physique
où convergent les trois branches.

Les **tips CDC** (`m_tipOnRoot`, `m_tipOnNormal`, `m_tipOnDeviation`) sont les
extrémités de chaque branche issues du fichier CDC. Ils sont de type `std::optional`
car absents avant l'orientation (`Phase7`).

La **longueur totale de traversée** est calculée par @ref SwitchBlock::computeTotalLength() "computeTotalLength()" :
```
totalLength = root_leg + max(normal_leg, deviation_leg)
```
où chaque leg est la distance Haversine entre la jonction et le tip correspondant.

| Requête | Description |
|---------|-------------|
| @ref SwitchBlock::getJunctionWGS84() "getJunctionWGS84()" | Coordonnée WGS-84 du nœud de jonction |
| @ref SwitchBlock::getJunctionUTM() "getJunctionUTM()" | Coordonnée UTM du nœud de jonction |
| @ref SwitchBlock::getTipOnRoot() "getTipOnRoot()" | Tip CDC branche root — `std::optional` |
| @ref SwitchBlock::getTipOnNormal() "getTipOnNormal()" | Tip CDC branche normale — `std::optional` |
| @ref SwitchBlock::getTipOnDeviation() "getTipOnDeviation()" | Tip CDC branche déviée — `std::optional` |
| @ref SwitchBlock::getTotalLengthMeters() "getTotalLengthMeters()" | Longueur de traversée — `std::optional` |

## Orientation — rôles des branches {#switch_orientation}

Avant `Phase7`, un @ref SwitchBlock ne connaît que la liste brute `m_branchIds`.
@ref SwitchBlock::orient() "orient()" assigne les rôles sémantiques **root / normal / deviation**
en vérifiant que chaque ID est bien présent dans `m_branchIds`
(lève `std::invalid_argument` sinon).

@ref SwitchBlock::isOriented() "isOriented()" retourne `true` dès que `m_rootBranchId` est renseigné.

@ref SwitchBlock::swapNormalDeviation() "swapNormalDeviation()" permute symétriquement les rôles,
les tips CDC, les polylignes absorbées et les marqueurs de double switch — utilisé par
`Phase8_SwitchOrientator` lors d'une correction de sens.

| Méthode | Rôle |
|---------|------|
| @ref SwitchBlock::orient() "orient(rootId, normalId, deviationId)" | Assigne les trois rôles |
| @ref SwitchBlock::swapNormalDeviation() "swapNormalDeviation()" | Permutation normale ↔ deviation |
| @ref SwitchBlock::isOriented() "isOriented()" | True si les rôles sont assignés |

## Double aiguille — absorption du segment de liaison {#switch_double}

Quand deux aiguillages sont reliés par un court @ref StraightBlock de liaison,
`Phase7_DoubleSwitchDetector` absorbe ce segment dans l'un des deux aiguillages.

@ref SwitchBlock::absorbLink() "absorbLink()" effectue en une seule opération :
1. Remplace `linkId` par `partnerId` dans `m_branchIds`
2. Met à jour `m_normalBranchId` ou `m_deviationBranchId`
3. Met à jour le tip CDC correspondant (extrémité distale du segment absorbé)
4. Mémorise la polyligne absorbée (WGS84 + UTM) pour le rendu
5. Renseigne `m_doubleOnNormal` ou `m_doubleOnDeviation` avec l'ID du partenaire

```
Avant absorption                  Après absorption
sw/0 ──s/link── sw/1              sw/0 (normal→sw/1, absorbedNormal=[s/link coords])
```

@ref SwitchBlock::isDouble() "isDouble()" retourne `true` si au moins un côté a absorbé un segment.
@ref SwitchBlock::getPartnerOnNormal() "getPartnerOnNormal()" /
@ref SwitchBlock::getPartnerOnDeviation() "getPartnerOnDeviation()" retournent le `SwitchBlock*`
partenaire via cast statique (valide uniquement si @ref SwitchBlock::isDouble() "isDouble()" est true).

| Requête | Description |
|---------|-------------|
| @ref SwitchBlock::isDouble() "isDouble()" | True si un segment de liaison a été absorbé |
| @ref SwitchBlock::getDoubleOnNormal() "getDoubleOnNormal()" | ID du partenaire côté normal — `std::optional` |
| @ref SwitchBlock::getDoubleOnDeviation() "getDoubleOnDeviation()" | ID du partenaire côté deviation — `std::optional` |
| @ref SwitchBlock::getAbsorbedNormalCoordinates() "getAbsorbedNormalCoordinates()" | Polyligne WGS84 absorbée côté normal |
| @ref SwitchBlock::getAbsorbedDeviationCoordinates() "getAbsorbedDeviationCoordinates()" | Polyligne WGS84 absorbée côté deviation |
| @ref SwitchBlock::getAbsorbedNormalCoordsUTM() "getAbsorbedNormalCoordsUTM()" | Polyligne UTM absorbée côté normal |
| @ref SwitchBlock::getAbsorbedDeviationCoordsUTM() "getAbsorbedDeviationCoordsUTM()" | Polyligne UTM absorbée côté deviation |
| @ref SwitchBlock::getPartnerOnNormal() "getPartnerOnNormal()" | `SwitchBlock*` partenaire côté normal, nullptr sinon |
| @ref SwitchBlock::getPartnerOnDeviation() "getPartnerOnDeviation()" | `SwitchBlock*` partenaire côté deviation, nullptr sinon |

## Pointeurs résolus — `SwitchBranches` {#switch_resolved}

```
struct SwitchBranches {
    ShuntingElement* root;       // tronc entrant     — nullptr si non résolu
    ShuntingElement* normal;     // sortie directe    — nullptr si non résolu
    ShuntingElement* deviation;  // sortie déviée     — nullptr si non résolu
};
```

Renseignés par `Phase9_RepositoryTransfer::resolve()`.

| Mutation | Rôle |
|----------|------|
| @ref SwitchBlock::setRootPointer() "setRootPointer(elem)" | Assigne la branche root |
| @ref SwitchBlock::setNormalPointer() "setNormalPointer(elem)" | Assigne la branche normale |
| @ref SwitchBlock::setDeviationPointer() "setDeviationPointer(elem)" | Assigne la branche déviée |
| @ref SwitchBlock::setBranchPointers() "setBranchPointers(branches)" | Assigne les trois en une opération |
| @ref SwitchBlock::replaceBranchPointer() "replaceBranchPointer(old, new)" | Substitution après absorption (Phase8) |

## État opérationnel — branche active {#switch_active_branch}

`m_activeBranch` représente la **position physique** de l'aiguillage (`NORMAL` par défaut).
Il est modifié en runtime par l'opérateur via l'IHM.

Lors d'une modification, la valeur est **propagée automatiquement** aux aiguillages
partenaires d'un double switch via @ref SwitchBlock::getPartnerOnNormal() "getPartnerOnNormal()" /
@ref SwitchBlock::getPartnerOnDeviation() "getPartnerOnDeviation()",
sauf si `propagate = false` est passé explicitement (pour éviter la récursion infinie
lors de la réception d'une propagation).

```
Opérateur clique sur sw/0               sw/0.toggleActiveBranch()
  → m_activeBranch = DEVIATION               → sw/0.setActiveBranch(DEVIATION, true)
  → propagation vers sw/1 (partenaire)           → sw/1.setActiveBranch(DEVIATION, false)
```

| Méthode | Rôle |
|---------|------|
| @ref SwitchBlock::getActiveBranch() "getActiveBranch()" | Branche active courante |
| @ref SwitchBlock::isDeviationActive() "isDeviationActive()" | Raccourci booléen |
| @ref SwitchBlock::setActiveBranch() "setActiveBranch(branch, propagate=true)" | Assigne + propage optionnellement |
| @ref SwitchBlock::toggleActiveBranch() "toggleActiveBranch(propagate=true)" | Alterne + propage + retourne la nouvelle valeur |

---

# Pointeurs résolus — récapitulatif {#resolved_pointers}

Les deux structs de pointeurs suivent la même convention :
- **Propriété** : `TopologyRepository` — ne jamais `delete` ces pointeurs.
- **Validité** : garantie uniquement après `Phase9_RepositoryTransfer::resolve()`.
- **Valeur nulle** : `nullptr` indique un terminus ou une branche non résolue.

**@ref StraightBlock::StraightNeighbours**

| Champ | Type | Description |
|-------|------|-------------|
| `prev` | `ShuntingElement*` | Bloc adjacent à l'extrémité A — nullptr si terminus |
| `next` | `ShuntingElement*` | Bloc adjacent à l'extrémité B — nullptr si terminus |

**@ref SwitchBlock::SwitchBranches**

| Champ | Type | Description |
|-------|------|-------------|
| `root` | `ShuntingElement*` | Tronc entrant — nullptr si non résolu |
| `normal` | `ShuntingElement*` | Sortie directe — nullptr si non résolu |
| `deviation` | `ShuntingElement*` | Sortie déviée — nullptr si non résolu |

---

# Énumérations {#enums}

| Enum | Valeurs | Usage |
|------|---------|-------|
| @ref ElementType | `SWITCH`, `STRAIGHT` | Typage sans RTTI — dispatching dans `PCCGraphBuilder` et `GeoParser` |
| @ref ShuntingState | `FREE`, `OCCUPIED`, `INACTIVE` | État opérationnel de l'infrastructure |
| @ref ActiveBranch | `NORMAL`, `DEVIATION` | Position physique de l'aiguillage — modifiée en runtime |
@page page_pcc Module PCC / TCO

@tableofcontents

---

## Vue d'ensemble {#pcc_overview}

Le module **PCC** (Poste de Commandement Centralisé) produit et affiche un
**Tableau de Contrôle Optique (TCO)** schématique du réseau ferroviaire.

Il opère en deux temps :

1. **Construction du graphe** (@ref PCCGraphBuilder) : transformation du
   @ref TopologyData en un graphe logique orienté de nœuds et d'arêtes PCC.
2. **Positionnement BFS** (@ref PCCLayout) : attribution d'une position
   logique `(x, y)` à chaque nœud par parcours en largeur.
3. **Rendu GDI** (@ref TCORenderer) : projection des positions logiques vers
   des coordonnées écran et tracé dans un `HDC` Win32.

> **Principe fondamental :** le layout PCC est **purement topologique**.
> Les coordonnées GPS ne sont autorisées qu'une seule fois — dans
> `PCCGraphBuilder::computeDeviationSides()` — pour déterminer le côté
> géographique (±1) de la branche déviée de chaque aiguillage.
> Toute autre valeur de position découle de la topologie de voisinage.

---

## Modèle de graphe {#pcc_model}

### Nœuds — PCCNode

@ref PCCNode est la classe abstraite de base. Deux sous-classes concrètes :

| Sous-classe | Source | `getNodeType()` |
|-------------|--------|-----------------|
| @ref PCCStraightNode | `StraightBlock` | `PCCNodeType::STRAIGHT` |
| @ref PCCSwitchNode | `SwitchBlock` | `PCCNodeType::SWITCH` |

Chaque nœud porte :
- `m_sourceId` — identifiant du bloc source (`"s/0"`, `"sw/3"`, …)
- `m_source` — pointeur non-propriétaire vers le `ShuntingElement` (durée
  de vie garantie par `TopologyRepository`)
- `m_position` — coordonnées logiques `{x, y}` calculées par `PCCLayout`
- `m_edges` — liste de pointeurs non-propriétaires vers les arêtes adjacentes

### PCCSwitchNode — détails

@ref PCCSwitchNode ajoute :
- Pointeurs d'arêtes typées : `m_rootEdge`, `m_normalEdge`, `m_deviationEdge`
  — accès O(1) sans parcourir `m_edges`.
- `m_deviationSide` — côté géographique de la déviation (+1 nord / -1 sud),
  calculé par `PCCGraphBuilder::computeDeviationSides()` via produit vectoriel
  2D sur coordonnées UTM.

### Arêtes — PCCEdge

@ref PCCEdge est une arête **orientée** (`from` → `to`) portant un rôle
sémantique :

| @ref PCCEdgeRole | Signification |
|------------------|---------------|
| `ROOT` | Connexion switch → branche root |
| `NORMAL` | Connexion switch → branche normale |
| `DEVIATION` | Connexion switch → branche déviée |
| `STRAIGHT` | Connexion straight → straight adjacent |

Les arêtes sont détenues par `PCCGraph` via `unique_ptr`. Les nœuds en
reçoivent des pointeurs non-propriétaires via `PCCNode::addEdge()`.

### PCCGraph

@ref PCCGraph détient l'ensemble des nœuds (`unique_ptr<PCCNode>`) et des
arêtes (`unique_ptr<PCCEdge>`). Il expose un index `id → PCCNode*` pour les
lookups O(1) lors de la construction des arêtes.

---

## Construction du graphe — PCCGraphBuilder {#pcc_builder}

@ref PCCGraphBuilder construit le graphe en **trois passes** :

### Passe 1 — buildNodes()

Un nœud PCC est créé pour chaque `StraightBlock` et `SwitchBlock` présent
dans `TopologyData`. Tous les nœuds sont indexés dans `PCCGraph` avant la
passe 2.

### Passe 2 — buildEdges()

**Arêtes de switch :** pour chaque switch orienté, trois arêtes
ROOT / NORMAL / DEVIATION sont créées depuis le nœud switch vers les nœuds
des blocs branche. Les arêtes sont enregistrées dans `PCCSwitchNode` via
`setRootEdge()` / `setNormalEdge()` / `setDeviationEdge()`.

**Arêtes STRAIGHT :** pour chaque straight, `getNeighbours()` (pointeurs
résolus par Phase 8) fournit les blocs adjacents. Une arête STRAIGHT est
créée pour chaque connexion, avec dédoublonnage par clé canonique
`makeEdgeKey(idA, idB)`.

> **Correctif v2 :** `buildEdges()` utilise `StraightBlock::getNeighbours()`
> (pointeurs résolus) et **non** `getNeighbourIds()` (IDs vides pour les
> sous-blocs de subdivision produits par `Phase6`).

### Passe 3 — computeDeviationSides()

Pour chaque `PCCSwitchNode` avec un switch orienté et des tips CDC UTM
disponibles, calcule le **produit vectoriel 2D** entre le vecteur
jonction → tip root et le vecteur jonction → tip déviation :

```
cross = rootX * devY - rootY * devX
side = (cross > 0) ? +1 : -1
```

`side > 0` → déviation à gauche de root (vers le haut dans le TCO).
`side < 0` → déviation à droite de root (vers le bas dans le TCO).

---

## Positionnement BFS — PCCLayout {#pcc_layout}

### Principe

@ref PCCLayout attribue une position logique `{x, y}` à chaque nœud par
**parcours en largeur** depuis les nœuds terminus (degré 1 dans le graphe PCC).

L'axe **X** représente la profondeur de progression le long du réseau.
L'axe **Y** représente le décalage vertical des branches déviées.

### Recherche des terminus

`findTermini()` collecte tous les nœuds n'ayant qu'**une seule arête**.
En l'absence de terminus (réseau en boucle), le premier nœud du graphe est
utilisé comme point de départ.

Chaque terminus lance un BFS indépendant (`runBFS()`), avec un `offsetX`
croissant pour éviter les collisions entre composantes connexes distinctes.

### Règles de positionnement

Pour chaque nœud courant `(x, y)` et chaque voisin non encore visité :

| Condition | Position voisin | `arrivedViaDeviation` |
|-----------|-----------------|----------------------|
| STRAIGHT / ROOT / NORMAL standard | `(x+1, y)` | `false` |
| DEVIATION → straight ou switch ordinaire | `(x+1, y ± side)` | `false` |
| DEVIATION → switch (double aiguille) | `(x, y ± side)` | `true` |
| ROOT forward, arrivée via déviation | `(x+1, y)` | `false` |
| NORMAL forward, arrivée via déviation | `(x-1, y)` | `false` |

Le drapeau `arrivedViaDeviation` modifie le tri des voisins à la prochaine
itération : ROOT est traité en premier (continuation de la route déviée).

### Résolution de collision

Quand deux nœuds obtiendraient la même position `(x, y)`, le nœud entrant
est décalé verticalement par incrément de ±1 jusqu'à trouver une cellule
libre. Cette résolution n'intervient que dans des topologies exceptionnelles.

---

## Rendu TCO — TCORenderer {#pcc_renderer}

### Projection logique → écran

La structure @ref TCORenderer::Projection est calculée une seule fois par
`computeProjection()` et mise en cache dans `PCCPanel`. Elle contient :

- `originX`, `originY` — décalage en pixels pour centrer le schéma
- `scaleX`, `scaleY` — facteur d'échelle pixels/unité logique
- `stub`, `inactiveGap`, `halfGap` — précalculs des constantes de dessin
  des aiguillages (optimisation Famille E)

```cpp
POINT project(int logicalX, int logicalY, const Projection& proj)
{
    return { proj.originX + logicalX * proj.scaleX,
             proj.originY + logicalY * proj.scaleY };
}
```

### Rendu d'un StraightBlock

`drawStraightBlock()` trace un **trait horizontal** avec un gap central
(`BLOCK_GAP_PX`) représentant la section de voie. La couleur dépend de
`ShuntingState` :

| État | Couleur |
|------|---------|
| `FREE` | Gris clair `RGB(220,220,220)` |
| `OCCUPIED` | Rouge `RGB(220,50,50)` |
| `INACTIVE` | Gris foncé `RGB(80,80,80)` |

### Rendu d'un SwitchBlock

`drawSwitchBlock()` trace le symbole aiguille **junctionX** (forme Y) :

- **Stub root** : trait depuis la position du switch vers la gauche (`stub`)
- **Branche normale** : trait horizontal vers la droite
- **Branche déviée** : trait oblique vers `(x+1, y ± deviationSide)`

La branche inactive est tracée en gris atténué (`branchOff` `RGB(64,64,64)`).
Chaque branche utilise un `PenScope` RAII indépendant (Famille D) — aucun
`CreatePen` redondant, aucune fuite GDI.

Le `static_cast<PCCSwitchNode*>` est garanti sûr par la vérification
préalable de `getNodeType() == SWITCH` (Famille C — RTTI supprimé).

---

## PCCPanel — intégration Win32 {#pcc_panel}

@ref PCCPanel est une fenêtre Win32 enfant de `MainWindow`. Elle gère :

### Rebuild

`rebuild()` est appelé après chaque parsing réussi. Il enchaîne :
1. `PCCGraphBuilder::build()` — construit le graphe PCC
2. `PCCLayout::compute()` — attribue les positions
3. Invalide le cache de projection (`m_projDirty = true`)
4. Déclenche `InvalidateRect()` pour forcer un `WM_PAINT`

### Cache de projection (Famille F)

La projection est coûteuse à recalculer. `PCCPanel` la met en cache dans
`m_cachedProj` et ne la recalcule que si :
- `m_projDirty == true` (après un rebuild), ou
- la taille du client `RECT` a changé (resize fenêtre).

`TCORenderer::draw()` reçoit la projection précalculée — il n'appelle plus
`computeProjection()` en interne.

### Navigation — zoom et pan

- **Molette + Ctrl** → zoom centré sur la position curseur
- **Cliquer-glisser** → pan (pan offset stocké dans `m_panX/Y`)
- **Double-clic** → reset zoom = 1, pan = (0, 0)

Le zoom/pan est appliqué via une **world transform GDI** (`SetWorldTransform`)
avant l'appel à `TCORenderer::draw()`, avec `fillBackground = false` pour
éviter de recouvrir la zone déjà peinte.

---

## Références croisées {#pcc_refs}

| Classe | Fichier | Rôle |
|--------|---------|------|
| @ref PCCGraph | `Modules/PCC/PCCGraph.h` | Conteneur nœuds + arêtes |
| @ref PCCNode | `Modules/PCC/PCCNode.h` | Nœud abstrait |
| @ref PCCStraightNode | `Modules/PCC/PCCStraightNode.h` | Nœud straight concret |
| @ref PCCSwitchNode | `Modules/PCC/PCCSwitchNode.h` | Nœud switch + deviationSide |
| @ref PCCEdge | `Modules/PCC/PCCEdge.h` | Arête orientée avec rôle |
| @ref PCCEdgeRole | `Modules/PCC/PCCEdge.h` | ROOT/NORMAL/DEVIATION/STRAIGHT |
| @ref PCCNodeType | `Modules/PCC/PCCNode.h` | STRAIGHT/SWITCH |
| @ref PCCGraphBuilder | `Modules/PCC/PCCGraphBuilder.h` | Construction graphe (3 passes) |
| @ref PCCLayout | `Modules/PCC/PCCLayout.h` | Positionnement BFS |
| @ref TCORenderer | `Engine/HMI/PCCPanel/TCORenderer.h` | Rendu GDI |
| @ref TCORenderer::Projection | `Engine/HMI/PCCPanel/TCORenderer.h` | Cache de projection |
| @ref PCCPanel | `Engine/HMI/PCCPanel/PCCPanel.h` | Fenêtre Win32 PCC |

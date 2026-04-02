@page pcc Module PCC — Graphe logique et rendu TCO

@tableofcontents

---

# Vue d'ensemble {#pcc_overview}

Le module PCC transforme le modèle topologique (@ref StraightBlock / @ref SwitchBlock)
en un **graphe logique indépendant des coordonnées GPS**, positionnable en schéma
gauche → droite pour l'affichage TCO.

```
TopologyRepository      Modules/PCC       HMI/PCCPanel
  StraightBlock  ────┬─►  PCCNode  ────►  TCORenderer
  SwitchBlock    ────┘       │
                          PCCEdge
                             │
                          PCCGraph
```

**Règle de dépendance :**
- `Modules/PCC` lit @ref TopologyRepository — seul point de couplage vers le GeoParser.
- `HMI/PCCPanel` consomme @ref PCCGraph uniquement — jamais @ref TopologyRepository directement.

| Classe | Responsabilité unique |
|--------|----------------------|
| @ref PCCNode | Représenter un bloc ferroviaire dans le graphe |
| @ref PCCStraightNode | Exposer les données spécifiques d'une voie droite |
| @ref PCCSwitchNode | Exposer les données spécifiques d'un aiguillage |
| @ref PCCEdge | Représenter une connexion orientée entre deux nœuds |
| @ref PCCGraph | Posséder et indexer les nœuds et arêtes |
| @ref PCCGraphBuilder | Construire le graphe depuis @ref TopologyRepository |
| @ref PCCLayout | Calculer les positions logiques X/Y |

---

# Nœuds et arêtes {#pcc_nodes}

```
PCCEdge          — connexion orientée (from → to) + rôle sémantique
PCCNode          — nœud abstrait (bloc ferroviaire + position logique)
PCCStraightNode  — nœud voie droite → getStraightSource() : StraightBlock*
PCCSwitchNode    — nœud aiguillage  → getSwitchSource()   : SwitchBlock*
                                      getRootEdge() / getNormalEdge() / getDeviationEdge()
```

**Hiérarchie :**
```
PCCNode  (abstrait)
  ├── PCCStraightNode
  └── PCCSwitchNode
```

**Rôles d'arête (`PCCEdgeRole`) :**

| Valeur | Description |
|--------|-------------|
| `STRAIGHT` | Connexion entre deux blocs adjacents sans switch |
| `ROOT` | Connexion sur la branche root d'un @ref SwitchBlock |
| `NORMAL` | Connexion sur la branche normale d'un @ref SwitchBlock |
| `DEVIATION` | Connexion sur la branche déviée d'un @ref SwitchBlock |

---

# PCCGraph — Conteneur {#pcc_graph}

@ref PCCGraph possède l'ensemble des nœuds et arêtes via `unique_ptr` et expose
un index de lookup O(1) par `sourceId`.

```
PCCGraph
  ├── m_nodes  : vector<unique_ptr<PCCNode>>      propriétaire
  ├── m_edges  : vector<unique_ptr<PCCEdge>>      propriétaire
  └── m_index  : unordered_map<string, PCCNode*>  lookup O(1)
```

| Méthode | Rôle |
|---------|------|
| @ref PCCGraph::addStraightNode() "addStraightNode()" | Crée, stocke et indexe un @ref PCCStraightNode |
| @ref PCCGraph::addSwitchNode() "addSwitchNode()" | Crée, stocke et indexe un @ref PCCSwitchNode |
| @ref PCCGraph::addEdge() "addEdge()" | Crée, stocke et câble une @ref PCCEdge sur les deux nœuds |
| @ref PCCGraph::findNode() "findNode()" | Lookup O(1) — retourne `nullptr` si absent |
| @ref PCCGraph::clear() "clear()" | Vide nœuds, arêtes et index |

---

# PCCGraphBuilder — Construction {#pcc_builder}

Classe statique. Seule classe du module qui connaît @ref TopologyRepository.

**Pipeline interne de @ref PCCGraphBuilder::build() "PCCGraphBuilder::build()" :**

```
Passe 1 — buildNodes()  : crée un nœud PCC par bloc (index complet)
Passe 2 — buildEdges()  : résout les connexions via les IDs de voisins/branches
```

Les deux passes sont séparées — toutes les arêtes référencent des nœuds par ID,
l'index doit être complet avant la résolution.

---

# PCCLayout — Positionnement {#pcc_layout}

Classe statique. Calcule les positions logiques X/Y par parcours BFS.

```
Avant PCCLayout          Après PCCLayout

s/0  sw/0  s/1           s/0(x=0,y=0) ─ sw/0(x=1,y=0) ─┬─ s/1(x=2,y=0)
                                                       └─ s/2(x=2,y=1)  ← déviation
```

**Règles de positionnement :**

| Arête empruntée | Effet sur Y |
|-----------------|-------------|
| ROOT / NORMAL / STRAIGHT | Y inchangé |
| DEVIATION | Y + 1 (bifurcation vers le haut) |

| Méthode | Rôle |
|---------|------|
| @ref PCCLayout::compute() "compute()" | Point d'entrée — orchestre BFS multi-sources |
| @ref PCCLayout::findTermini() "findTermini()" | Détecte les nœuds de départ (1 seul voisin, non-cible de switch) |
| @ref PCCLayout::runBFS() "runBFS()" | BFS depuis un terminus, assigne PCCPosition |
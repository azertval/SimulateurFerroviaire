@page modules Modules — Fonctionnalités métier

@tableofcontents

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

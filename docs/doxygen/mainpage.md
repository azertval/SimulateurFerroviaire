@mainpage Simulateur Ferroviaire

@tableofcontents

---

# Description {#description}

Reconstruction et visualisation d'un réseau ferroviaire à partir de données GeoJSON.

Le projet repose sur un pipeline de parsing permettant de :
- Charger des données géographiques (WGS-84 / GeoJSON)
- Construire un graphe topologique métrique (UTM)
- Extraire les blocs ferroviaires (voies droites + aiguillages)
- Orienter et valider les aiguillages (root / normal / deviation)
- Détecter les doubles aiguilles et absorber les segments de liaison
- Résoudre les pointeurs inter-blocs et construire les index de lookup
- Stocker le modèle dans un singleton partagé et le visualiser dans un WebView
- Permettre une interaction bidirectionnelle Leaflet ↔ C++ (clic → mise à jour modèle → rendu)
- Afficher une vue PCC type TCO SNCF superposée à la carte, togglable via F2

---
# Architecture du projet {#diag}

![Architecture SimulateurFerroviaire](images/architecture.svg)

---

# Engine — Moteur de l'application {#engine}

## Core — Cœur applicatif {#core}

Gestion des composants fondamentaux et de la logique applicative transverse.

| Classe | Rôle |
|--------|------|
| Application | Cycle de vie Win32 (enregistrement de classe, boucle de messages) |
| Logger | Journalisation structurée (INFO / DEBUG / WARNING / ERROR / FAILURE) |
| @ref TopologyData | Conteneur `std::unique_ptr<StraightBlock>` + `std::unique_ptr<SwitchBlock>` + index de lookup |
| @ref TopologyRepository | Singleton (Meyers). Accès global à @ref TopologyData via `TopologyRepository::instance().data()` |
| @ref TopologyRenderer | Statique génére le rendu JS|

### TopologyData {#data}

> **Pourquoi `std::unique_ptr` ?**
> Les blocs sont polymorphes (`InteractiveElement*`). Le stockage par valeur entraînerait
> du slicing et interdirait le déplacement. Le stockage par `std::unique_ptr` garantit :
> - polymorphisme correct (destructeur virtuel respecté),
> - propriété exclusive et cycle de vie déterministe,
> - interdiction de copie accidentelle.

`TopologyData` expose deux index construits en fin de pipeline via `TopologyData::buildIndex()` :

| Index | Type | Usage |
|-------|------|-------|
| `TopologyData::switchIndex` | `std::unordered_map<string, SwitchBlock *>` | Lookup O(1) par ID |
| `TopologyData::straightIndex` | `std::unordered_map<string, StraightBlock *>` | Lookup O(1) par ID |

> `TopologyData::buildIndex()` doit être appelé après que toutes les adresses sont stables
> (transfert `std::unique_ptr` + résolution des pointeurs terminés). `TopologyData::clear()` vide
> également les index.

### TopologyRenderer — Rendu et exportation {#renderer}

@ref TopologyRenderer est une classe statique qui :
- Sérialise @ref StraightBlock → feature GeoJSON `LineString`
- Sérialise @ref SwitchBlock → feature GeoJSON `Point`
- Génère les scripts JavaScript d'injection pour le WebView Leaflet
- Met à jour le rendu d'un switch et de ses partenaires via `TopologyRenderer::updateSwitchBlocks(sw)`

| Méthode | Rôle |
|---------|------|
| `TopologyRenderer::renderAllStraightBlocks()` | Efface et redessine tous les StraightBlocks |
| `TopologyRenderer::renderAllSwitchBranches()` | Efface et redessine toutes les branches de switch |
| `TopologyRenderer::renderAllSwitchBlocksJunctions()` | Efface et redessine tous les marqueurs de jonction |
| `TopologyRenderer::updateSwitchBlocks()` | Met à jour visuellement un switch et ses partenaires double |
| `TopologyRenderer::exportToFile()` | Export GeoJSON complet vers un fichier |

----

## HMI — Interface utilisateur {#hmi}

Couche graphique Win32 + WebView2.

| Classe | Rôle |
|--------|------|
| MainWindow | Fenêtre principale, routage des messages Win32, coordination UI ↔ métier |
| ProgressBar | Wrapper du contrôle natif `PROGRESS_CLASS` |
| WebViewPanel | Affichage cartographique Leaflet embarqué via WebView2 |
| PCCPanel | Panneau PCC superposé togglable (F2 / menu Vue), child window Win32 |
| TCORenderer | Renderer GDI statique du schéma TCO — projection GPS → pixels + dessin voies/aiguillages |
| AboutDialog | Boîte de dialogue modale "À propos" |
| FileOpenDialog | Sélecteur de fichier GeoJSON (GetOpenFileNameA) |
| FileSaveDialog | Dialogue de sauvegarde GeoJSON (GetSaveFileNameA) |

### Panneau PCC — Vue TCO {#pcc}

Le panneau PCC est une `WS_CHILD` window superposée au `WebViewPanel`, togglée via **F2** ou
le menu **Vue → Panneau PCC**. Il est masqué par défaut et ne perturbe pas la carte Leaflet.

@ref PCCPanel délègue l'intégralité du rendu à @ref TCORenderer (appelé dans `WM_PAINT`).
Le logger HMI (`Logger{"HMI"}`) est partagé par injection de référence depuis `MainWindow`.

**Cycle de vie :**

| Méthode | Déclencheur |
|---------|-------------|
| `PCCPanel::create()` | `MainWindow::create()` — après `WebViewPanel::create()` |
| `PCCPanel::toggle()` | F2 ou `IDM_VIEW_PCC` — place le panneau en `HWND_TOP` à l'affichage |
| `PCCPanel::resize()` | `MainWindow::onSizeUpdate()` — couvre toute la zone cliente parente |
| `PCCPanel::refresh()` | `MainWindow::onParsingSuccess()` — invalide le rect pour forcer `WM_PAINT` |

**Z-order :** lors du toggle affichage, `SetWindowPos(..., HWND_TOP, ..., SWP_SHOWWINDOW)`
place le panneau au-dessus du HWND WebView2, garantissant la visibilité.

### TCORenderer — Rendu GDI {#tco}

Classe utilitaire statique, sans état. Lit @ref TopologyRepository au moment du dessin.

**Conventions de couleurs (style TCO SNCF) :**

| État | Couleur |
|------|---------|
| Fond | Noir `RGB(0,0,0)` |
| Voie libre (FREE) | Blanc cassé `RGB(220,220,220)` |
| Voie occupée (OCCUPIED) | Rouge `RGB(220,50,50)` |
| Voie inactive (INACTIVE) | Gris `RGB(80,80,80)` |
| Branche normale active | Vert `RGB(0,200,80)` |
| Branche déviation active | Jaune `RGB(220,200,0)` |
| Erreur | Magenta `RGB(255, 0, 216)` |

**Projection GPS → pixels :**
Normalisation linéaire min/max sur l'ensemble des coordonnées (straights + jonctions + tips).
Marge de 5 % appliquée de chaque côté. Axe Y inversé (latitude croissante → Y décroissant).

| Méthode | Rôle |
|---------|------|
| `TCORenderer::draw()` | Point d'entrée — fond + délégation aux sous-renderers |
| `TCORenderer::computeProjection()` | Calcul des bornes GPS et paramètres de projection |
| `TCORenderer::drawStraights()` | Polyligne GDI par StraightBlock, colorisée par état |
| `TCORenderer::drawSwitches()` | 3 branches + disque de jonction par SwitchBlock orienté |

### Binding bidirectionnel Leaflet ↔ C++ {#binding}

Le binding repose sur deux canaux :

**C++ → Leaflet** : `WebViewPanel::executeScript()` injecte des appels JS
(`leaflet_api::window.switchApplyState`, `leaflet_api::window.renderSwitchBlock`, etc.) générés par `TopologyRenderer`.

**Leaflet → C++** : `leaflet_api::window.chrome.webview.postMessage()` envoie un JSON vers
`WebViewPanel::onWebMessageReceived()`, qui dispatche vers `MainWindow::onWebMessage()`.

```
Exemple :
Clique on switch junction 
  → postMessage({type:"switch_click", id:"sw/0"})
  → WebViewPanel::onWebMessageReceived()     [UTF-16 → UTF-8]
  → MainWindow::onWebMessage()               [parse JSON, dispatch]
  → MainWindow::onSwitchClick()              [switchIndex.find() O(1)]
  → SwitchBlock::toggleActiveBranch()        [+ propagation partenaires]
  → TopologyRenderer::updateSwitchBlocks()   [script JS]
  → WebViewPanel::executeScript()
  → window.switchApplyState()                [couleur cercle + branches]
```

Messages JS supportés :

| `type` | Champs | Handler C++ |
|--------|--------|-------------|
| `switch_click` | `id` | `MainWindow::onSwitchClick()` |

> **Conception** : Leaflet ne gère aucun état — il notifie C++ au clic et attend
> une instruction de rendu. C++ est l'autorité sur `ActiveBranch`.

> **WebViewPanel** : `WebViewPanel::setOnMessageReceived()` permet à MainWindow de brancher
> sa logique sans couplage. La conversion UTF-16 → UTF-8 utilise `WideCharToMultiByte`.

---

# Modules — Fonctionnalités métier {#modules}

---

# GeoParser — Pipeline principal {#geoparser}

Pipeline complet de transformation **GeoJSON → modèle ferroviaire**.

## Tâche asynchrone {#task}

@ref GeoParsingTask lance @ref GeoParser dans un thread détaché.
La communication vers l'UI passe exclusivement par `PostMessage` :

| Message | Contenu |
|---------|---------|
| `WM_PROGRESS_UPDATE` | Avancement 0–100 |
| `WM_PARSING_SUCCESS` | Parsing terminé |
| `WM_PARSING_ERROR` | Pointeur `std::string*` (à libérer par le destinataire) |

## Pipeline global {#pipeline}

Voir @ref GeoParser::parse() pour l'implémentation complète.

| Phase | Classe | Description |
|-------|--------|-------------|
| 1–2 | GraphBuilder | Chargement GeoJSON, projection WGS-84 → UTM, snap + fusion des nœuds |
| 3–5 | TopologyExtractor | Détection des aiguillages (degré == 3), extraction des StraightBlock, câblage |
| 6 | SwitchOrientator | Orientation root / normal / deviation, calcul des tips CDC |
| 7–8 | DoubleSwitchDetector | Détection des doubles aiguilles, absorption du segment de liaison, validation CDC |
| 9 | GeoParser | Clear + transfert vers @ref TopologyRepository |
| 10 | GeoParser | Résolution des pointeurs partenaires, branches, voisins + `buildIndex()` |

> **Phase 3 — Détection des aiguillages** : seuls les nœuds de degré exactement 3
> (jonction en Y) sont des `SwitchBlock`. Les nœuds de degré > 3 (croisements en X)
> sont des frontières de découpe pour les `StraightBlock` mais ne génèrent pas d'aiguillage.

> **Phase 10 — Résolution des pointeurs** : après transfert en `unique_ptr`,
> les adresses sont stables. Chaque bloc reçoit ses pointeurs directs via
> `setBranchPointers()` / `setNeighbourPointers()`, puis `buildIndex()` construit
> les tables de lookup O(1).

## Parsing & Construction {#parsing}

| Classe | Rôle |
|--------|------|
| @ref GeoParser | Orchestrateur du pipeline |
| @ref GeoParsingTask | Exécution asynchrone (évite le blocage UI) |
| @ref GeometryUtils | Projection UTM, interpolation, angles, snap de grille |

## Graphe topologique {#graph}

| Classe | Rôle |
|--------|------|
| @ref GraphBuilder | Construction du graphe depuis le GeoJSON |
| @ref TopologyGraph | Graphe planaire non-orienté (nœuds + arêtes), union-find |
| @ref TopologyEdge | Arête avec géométrie polyligne métrique et longueur planaire |

## Extraction ferroviaire {#extraction}

| Classe | Rôle |
|--------|------|
| @ref TopologyExtractor | Extraction des blocs depuis le graphe |
| @ref SwitchOrientator | Orientation géométrique (phases 6, 6b, 6c, 6d) |
| @ref DoubleSwitchDetector | Détection des doubles aiguilles et validation CDC |

---

# Eléments interactifs {#elements}

| Classe | Rôle |
|--------|------|
| @ref InteractiveElement | Interface abstraite commune (id, type). Logger statique partagé (`Logs/InteractiveElements.log`). Copie interdite, déplacement autorisé |
| @ref ShuntingElement | Étend avec un état opérationnel + helpers `isFree()` / `isOccupied()` / `isInactive()` |
| @ref StraightBlock | Tronçon de voie droite. Longueur géodésique Haversine. Voisins résolus via `StraightNeighbours` |
| @ref SwitchBlock | Aiguillage 3 branches. Orientation + tips CDC + support double aiguille + état opérationnel `ActiveBranch` |

## Hiérarchie des éléments interactifs {#hierarchy}

```
InteractiveElement          getId(), getType(), m_logger (static)
    └── ShuntingElement     getState()  [FREE / OCCUPIED / INACTIVE]
            ├── StraightBlock
            └── SwitchBlock
```

> **Logger statique** : `InteractiveElement::m_logger` est partagé par toutes les
> instances de toutes les classes dérivées → un seul fichier `Logs/InteractiveElements.log`.

## Pointeurs résolus post-parsing {#pointers}

Après `TopologyData::buildIndex()`, chaque bloc dispose de pointeurs directs vers ses voisins :

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

`SwitchBlock` expose un état runtime modifiable par l'opérateur :

| Méthode | Description |
|---------|-------------|
| `SwitchBlock::getActiveBranch()` | Retourne `ActiveBranch::NORMAL` ou `DEVIATION` |
| `SwitchBlock::isDeviationActive()` | Raccourci booléen |
| `SwitchBlock::setActiveBranch(branch)` | Assigne l'état + propage aux partenaires |
| `SwitchBlock::toggleActiveBranch()` | Alterne + propage + retourne le nouvel état |

La propagation aux partenaires est gérée directement par `SwitchBlock` via
`SwitchBlock::getPartnerOnNormal()` / `SwitchBlock::getPartnerOnDeviation()` (pointeurs résolus en Phase 10)

## Énumérations clés {#enums}

| Enum | Valeurs | Usage |
|------|---------|-------|
| `InteractiveElementType` | `SWITCH`, `STRAIGHT` | Typage sans RTTI |
| `ShuntingState` | `FREE`, `OCCUPIED`, `INACTIVE` | État opérationnel infrastructure |
| `ActiveBranch` | `NORMAL`, `DEVIATION` | Position opérationnelle de l'aiguillage |

---

# Coordonnées {#coords}

| Classe | Système |
|--------|---------|
| @ref LatLon | Géographique WGS-84 (latitude, longitude) |
| @ref CoordinateXY | Métrique UTM (x = est, y = nord) |

---

# PCC {#pcc}

Le module PCC transforme le modèle topologique (`StraightBlock` / `SwitchBlock`)
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

| Classe | Responsabilité unique |
|--------|----------------------|
| `PCCNode` | Représenter un bloc ferroviaire dans le graphe |
| `PCCStraightNode` | Exposer les données spécifiques d'une voie droite |
| `PCCSwitchNode` | Exposer les données spécifiques d'un aiguillage |
| `PCCEdge` | Représenter une connexion entre deux nœuds |
| `PCCGraph` | Posséder et indexer les nœuds et arêtes |
| `PCCGraphBuilder` | Construire le graphe depuis TopologyRepository |
| `PCCLayout` | Calculer les positions logiques X/Y |

`PCCNode` ne construit pas le graphe. `PCCGraph` ne calcule pas les positions. \
`PCCLayout` ne connaît pas TopologyRepository.



## Nodes

```
PCCEdge          — connexion orientée entre deux nœuds
PCCNode          — nœud abstrait (bloc ferroviaire)
PCCStraightNode  — nœud voie droite (StraightBlock source)
PCCSwitchNode    — nœud aiguillage  (SwitchBlock source)
```

**Pourquoi ces quatre classes et pas une seule ?**

TCORenderer a besoin d'opérations communes sur tous les nœuds (position, état, arêtes) et d'opérations spécifiques selon le type (longueur d'une voie droite, branche active d'un aiguillage). La hiérarchie abstraite permet les deux sans cast dynamique (dynamic_cast) à chaque frame de rendu.
```
PCCNode  (abstrait)
  ├── PCCStraightNode   →  PCCStraightNode::getStraightSource() : StraightBlock *
  └── PCCSwitchNode     →  PCCStraightNode::getSwitchSource()   : SwitchBlock *
                           PCCStraightNode::getRootEdge() / PCCStraightNode::getNormalEdge() / PCCStraightNode::getDeviationEdge()
```

## Notion de graphe

Un **graphe** est une structure mathématique composée de :
- **Nœuds** (vertices / nodes) : les entités
- **Arêtes** (edges) : les connexions entre entités

Dans notre cas :
- Nœud = un bloc ferroviaire (`StraightBlock` ou `SwitchBlock`)
- Arête = une connexion physique entre deux blocs

#### Graphe non-orienté vs orienté

| Type | Description | Notre cas |
|------|-------------|-----------|
| Non-orienté | A–B == B–A | Topologie physique ferroviaire |
| Orienté | A→B ≠ B→A | Arêtes PCC (facilite le parcours layout) |

Le réseau ferroviaire est physiquement non-orienté. On crée des arêtes
orientées (une dans chaque sens) pour simplifier le parcours gauche→droite
dans `PCCLayout` sans logique de direction supplémentaire.

#### BFS (Breadth-First Search) — utilisé par PCCLayout

Le **parcours en largeur** explore un graphe niveau par niveau depuis un
nœud de départ. Il sert à calculer la profondeur (coordonnée X logique)
de chaque nœud en partant du terminus le plus à l'ouest.

```
Terminus ──► s/0 ──► sw/0 ──► s/1 ──► ...
  X=0         X=1     X=2     X=3
```

> Référence — Théorie des graphes : https://en.wikipedia.org/wiki/Graph_(discrete_mathematics) \
> Référence — BFS : https://en.wikipedia.org/wiki/Breadth-first_search

---

# Références externes

## C++ moderne

| Sujet | Lien |
|-------|------|
| cppreference — référence complète C++ | https://en.cppreference.com |
| CppCoreGuidelines — bonnes pratiques | https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines |
| C++ Weekly (Jason Turner) | https://www.youtube.com/@cppweekly |

## Concepts spécifiques

| Concept | Lien |
|---------|------|
| enum class | https://en.cppreference.com/w/cpp/language/enum |
| unique_ptr | https://en.cppreference.com/w/cpp/memory/unique_ptr |
| Rule of Five | https://en.cppreference.com/w/cpp/language/rule_of_three |
| Destructeur virtuel | https://isocpp.org/wiki/faq/virtual-functions#virtual-dtors |
| [[nodiscard]] | https://en.cppreference.com/w/cpp/language/attributes/nodiscard |
| std::move | https://en.cppreference.com/w/cpp/utility/move |
| Forward declaration | https://en.wikipedia.org/wiki/Forward_declaration |
| Member initializer list | https://en.cppreference.com/w/cpp/language/constructor |
| Polymorphisme | https://en.cppreference.com/w/cpp/language/virtual |
| explicit | https://en.cppreference.com/w/cpp/language/explicit |
| override | https://en.cppreference.com/w/cpp/language/override |

## Architecture logicielle

| Concept | Lien |
|---------|------|
| SOLID principles | https://en.wikipedia.org/wiki/SOLID |
| Théorie des graphes | https://en.wikipedia.org/wiki/Graph_(discrete_mathematics) |
| BFS algorithm | https://en.wikipedia.org/wiki/Breadth-first_search |
| Ownership semantics | https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r-resource-management |
| Liskov Substitution Principle | https://en.wikipedia.org/wiki/Liskov_substitution_principle |

---


# Licence {#licence}

Ce projet est distribué sous licence :

**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)**

https://creativecommons.org/licenses/by-nc-sa/4.0/

- ✔️ Partager — copier et redistribuer le matériel
- ✔️ Adapter — remixer, transformer et créer à partir du matériel
- 📝 Attribution requise
- 🚫 Usage commercial interdit
- 🔁 Redistribution sous la même licence

---

# Auteur {#auteur}

© 2026 Valentin Eloy
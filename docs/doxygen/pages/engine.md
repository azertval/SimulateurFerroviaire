@page engine Engine — Moteur de l'application

@tableofcontents

---

# Core — Cœur applicatif {#core}

Gestion des composants fondamentaux et de la logique applicative transverse.

| Classe | Rôle |
|--------|------|
| Application | Cycle de vie Win32 (enregistrement de classe, boucle de messages) |
| Logger | Journalisation structurée (INFO / DEBUG / WARNING / ERROR / FAILURE) |
| @ref TopologyData | Conteneur `std::unique_ptr<StraightBlock>` + `std::unique_ptr<SwitchBlock>` + index de lookup |
| @ref TopologyRepository | Singleton (Meyers). Accès global à @ref TopologyData via `TopologyRepository::instance().data()` |
| @ref TopologyRenderer | Classe statique — génère les scripts JS d'injection pour le WebView Leaflet |

## TopologyData {#data}

> **Pourquoi `std::unique_ptr` ?**
> Les blocs sont polymorphes (`InteractiveElement*`). Le stockage par valeur entraînerait
> du slicing et interdirait le déplacement. Le stockage par `std::unique_ptr` garantit :
> - polymorphisme correct (destructeur virtuel respecté),
> - propriété exclusive et cycle de vie déterministe,
> - interdiction de copie accidentelle.

`TopologyData` expose deux index construits en fin de pipeline via `TopologyData::buildIndex()` :

| Index | Type | Usage |
|-------|------|-------|
| `TopologyData::switchIndex` | `std::unordered_map<string, SwitchBlock*>` | Lookup O(1) par ID |
| `TopologyData::straightIndex` | `std::unordered_map<string, StraightBlock*>` | Lookup O(1) par ID |

> `TopologyData::buildIndex()` doit être appelé après que toutes les adresses sont stables.
> `TopologyData::clear()` vide également les index.

## TopologyRenderer {#renderer}

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

---

# HMI — Interface utilisateur {#hmi}

Couche graphique Win32 + WebView2.

| Classe | Rôle |
|--------|------|
| MainWindow | Fenêtre principale, routage des messages Win32, coordination UI ↔ métier |
| ProgressBar | Wrapper du contrôle natif `PROGRESS_CLASS` |
| WebViewPanel | Affichage cartographique Leaflet embarqué via WebView2 |
| PCCPanel | Panneau PCC superposé togglable (F2 / menu Vue), child window Win32 |
| TCORenderer | Renderer GDI statique du schéma TCO — consomme @ref PCCGraph |
| AboutDialog | Boîte de dialogue modale "À propos" |
| FileOpenDialog | Sélecteur de fichier GeoJSON (GetOpenFileNameA) |
| FileSaveDialog | Dialogue de sauvegarde GeoJSON (GetSaveFileNameA) |

## Panneau PCC {#pcc_panel}

Le panneau PCC est une `WS_CHILD` window superposée au `WebViewPanel`, togglée via **F2** ou
le menu **Vue → Panneau PCC**. Il est masqué par défaut et ne perturbe pas la carte Leaflet.

@ref PCCPanel possède un @ref PCCGraph qu'il reconstruit à chaque chargement GeoJSON via
@ref PCCGraphBuilder et @ref PCCLayout. Le rendu est délégué à @ref TCORenderer.
Le logger HMI est partagé par injection de référence depuis `MainWindow`.

**Séquence d'appel complète :**

```
MainWindow::onParsingSuccess()
  ├─► m_webViewPanel.executeScript(...)    — rendu Leaflet inchangé
  └─► m_pccPanel.refresh()
        ├─► rebuild()
        │     ├─► build(m_graph, m_logger)
        │     └─► compute(m_graph, m_logger)
        └─► InvalidateRect()  →  WM_PAINT
              └─► draw(hdc, rc, m_graph, m_logger)
```

**Cycle de vie :**

| Méthode | Déclencheur |
|---------|-------------|
| `PCCPanel::create()` | `MainWindow::create()` — après `WebViewPanel::create()` |
| `PCCPanel::toggle()` | F2 ou `IDM_VIEW_PCC` — place le panneau en `HWND_TOP` |
| `PCCPanel::resize()` | `MainWindow::onSizeUpdate()` |
| `PCCPanel::refresh()` | `MainWindow::onParsingSuccess()` |

## TCORenderer {#tco}

Classe utilitaire statique. Consomme `const PCCGraph&` — n'accède pas à `TopologyRepository`.

**Conventions de couleurs (style TCO SNCF) :**

| État | Couleur |
|------|---------|
| Fond | Noir `RGB(0,0,0)` |
| Voie libre (FREE) | Blanc cassé `RGB(220,220,220)` |
| Voie occupée (OCCUPIED) | Rouge `RGB(220,50,50)` |
| Voie inactive (INACTIVE) | Gris `RGB(80,80,80)` |
| Branche normale active | Vert `RGB(0,200,80)` |
| Branche déviation active | Jaune `RGB(220,200,0)` |

**Projection positions logiques X/Y → pixels :**
```
pixelX = marginX + logicalX * cellWidth
pixelY = centerY - logicalY * cellHeight   (Y inversé)
```

| Méthode | Rôle |
|---------|------|
| `TCORenderer::draw()` | Point d'entrée — fond + arêtes + nœuds |
| `TCORenderer::computeProjection()` | Calcul bornes X/Y logiques et taille des cellules |
| `TCORenderer::drawEdges()` | Un segment GDI par @ref PCCEdge, colorisé par rôle et état |
| `TCORenderer::drawNodes()` | Un disque GDI par @ref PCCNode |

## Binding bidirectionnel Leaflet ↔ C++ {#binding}

**C++ → Leaflet** : `WebViewPanel::executeScript()` injecte des appels JS générés par `TopologyRenderer`.

**Leaflet → C++** : `window.chrome.webview.postMessage()` → `WebViewPanel::onWebMessageReceived()` → `MainWindow::onWebMessage()`.

```
Clic sur jonction switch
  → postMessage({type:"switch_click", id:"sw/0"})
  → MainWindow::onSwitchClick()         [switchIndex.find() O(1)]
  → SwitchBlock::toggleActiveBranch()   [+ propagation partenaires]
  → TopologyRenderer::updateSwitchBlocks()
  → WebViewPanel::executeScript()
  → window.switchApplyState()
```

| type | Champs | Handler C++ |
|--------|--------|-------------|
| `switch_click` | `id` | `MainWindow::onSwitchClick()` |

@page page_hmi Interface graphique (HMI)

@tableofcontents

---

## Vue d'ensemble {#hmi_overview}

Le module **HMI** (Human-Machine Interface) regroupe l'ensemble des composants
de l'interface graphique Win32. Il orchestre l'affichage, les interactions
utilisateur et la communication entre les modules métier.

| Composant | Classe | Rôle |
|-----------|--------|------|
| Fenêtre principale | @ref MainWindow | Dispatcher WM_*, menu, dialogs |
| Carte interactive | @ref WebViewPanel + @ref Leaflet | Carte ferroviaire Leaflet.js |
| Panneau PCC | @ref PCCPanel | TCO schématique GDI |
| Barre de progression | @ref ProgressBar | Avancement du parsing |
| Dialogs | `AboutDialog`, `ParserSettingsDialog`, … | Boîtes de dialogue modales |

---

## MainWindow {#hmi_mainwindow}

@ref MainWindow est la fenêtre principale Win32. Elle est instanciée dans
`WinMain` et ne doit exister qu'en une seule instance.

### Cycle de vie

```
WinMain
  └── MainWindow::create()
        ├── CreateWindowW()         → m_hWnd
        ├── ShowWindow/UpdateWindow
        ├── ProgressBar::create()
        ├── ParserConfigIni::load() → m_parserConfig
        ├── GeoParsingTask (instanciation différée)
        ├── WebViewPanel::create()  → WebView2 COM async
        └── PCCPanel::create()
```

`create()` est synchrone pour tout sauf l'initialisation de WebView2, qui
s'effectue de manière asynchrone via des callbacks COM (voir
@ref hmi_webview).

### Dispatcher de messages

Le `WndProc` statique récupère le pointeur `this` depuis `GWLP_USERDATA`
(positionné dans `WM_NCCREATE`) et délègue chaque message à une méthode
membre :

| Message Win32 | Méthode | Déclencheur |
|---------------|---------|-------------|
| `WM_COMMAND` | `onCommand()` | Menu, boutons |
| `WM_SIZE` | `onSizeUpdate()` | Redimensionnement |
| `WM_DESTROY` | `onDestroy()` | Fermeture |
| `WM_PROGRESS_UPDATE` | `onProgressUpdate()` | Thread parsing (PostMessage) |
| `WM_PARSING_SUCCESS` | `onParsingSuccess()` | Thread parsing (PostMessage) |
| `WM_PARSING_ERROR` | `onParsingError()` | Thread parsing (PostMessage) |
| `WM_PARSING_CANCELLED` | `onParsingCancelled()` | Thread parsing (PostMessage) |

### Communication inter-threads

Le pipeline GeoParser s'exécute dans un **thread séparé**
(@ref GeoParsingTask). La communication vers le thread UI passe exclusivement
par `PostMessage()` avec des messages personnalisés définis dans `Resource.h` :

| Message | wParam | lParam |
|---------|--------|--------|
| `WM_PROGRESS_UPDATE` | progression 0-100 | `wstring*` label (alloué par tâche, libéré par `onProgressUpdate`) |
| `WM_PARSING_SUCCESS` | 0 | 0 |
| `WM_PARSING_ERROR` | 0 | `wstring*` message (alloué par tâche, libéré par `onParsingError`) |
| `WM_PARSING_CANCELLED` | 0 | 0 |

> **Règle mémoire :** tout `wstring*` transmis en `lParam` est alloué par
> la tâche avec `new` et **libéré par le handler** après extraction. Ne
> jamais accéder à un `lParam` après le `delete`.

### Interaction aiguillage (Leaflet → C++)

Un clic sur un marqueur aiguillage dans la carte Leaflet déclenche :

```
JS: window.chrome.webview.postMessage(JSON.stringify({type:"switch", id:"sw/3"}))
   ↓
WebViewPanel::onWebMessageReceived()
   ↓
m_onMessageReceived callback → MainWindow::onWebMessage()
   ↓
MainWindow::onSwitchClick("sw/3")
   ↓
SwitchBlock::toggleActiveBranch(true)
   ↓
TopologyRenderer::updateSwitchBlocks(*sw) → executeScript()
```

---

## WebViewPanel et Leaflet {#hmi_webview}

### WebViewPanel

@ref WebViewPanel encapsule le contrôle **WebView2** (navigateur Chromium
embarqué via COM). Il fournit une API C++ de haut niveau indépendante des
interfaces `ICoreWebView2*` :

| Méthode | Rôle |
|---------|------|
| `create(parentHwnd)` | Lance l'initialisation asynchrone WebView2 |
| `navigate(url)` | Navigation vers une URL |
| `navigateToString(html)` | Injection directe de HTML |
| `executeScript(js)` | Exécute du JavaScript dans la page courante |
| `setOnMessageReceived(cb)` | Enregistre le handler des messages `postMessage` JS |
| `setVirtualHostMapping(host, dir)` | Mappe un hôte virtuel vers un dossier local |
| `resize()` | Adapte WebView à la taille de la fenêtre parente |
| `close()` | Libère les ressources COM |

**Initialisation asynchrone :** `create()` appelle
`CreateCoreWebView2EnvironmentWithOptions()`. Le résultat arrive dans
`onEnvironmentCreated()` (callback WRL), qui enchaîne
`CreateCoreWebView2Controller()`, puis `onControllerCreated()` finalise
l'initialisation et appelle `m_onInitialized` si enregistré.

**Conversion de messages :** les messages JavaScript sont reçus en UTF-16.
Le handler `onWebMessageReceived()` les convertit en UTF-8 (`std::string`)
avant de les transmettre au callback `m_onMessageReceived`.

### Leaflet

@ref Leaflet est un utilitaire statique qui génère la **page HTML complète**
intégrant la bibliothèque Leaflet.js (chargée depuis un CDN). La page expose
une API JavaScript appelable depuis C++ via `executeScript()` :

| Fonction JS | Appelée depuis | Rôle |
|-------------|----------------|------|
| `renderStraightBlock(id, coords)` | `TopologyRenderer` | Trace une section de voie |
| `renderSwitch(id, lat, lon)` | `TopologyRenderer` | Place un marqueur aiguillage |
| `renderSwitchBranches(id, ...)` | `TopologyRenderer` | Trace les stubs CDC |
| `switchApplyState(id, state)` | `TopologyRenderer` | Met à jour la couleur d'un switch |
| `clearStraightBlocks()` | `TopologyRenderer` | Supprime toutes les voies |
| `clearSwitches()` | `TopologyRenderer` | Supprime tous les aiguillages |
| `zoomToStraights()` | `TopologyRenderer` | Ajuste la vue à l'emprise |
| `loadGeoJson(json)` | `TopologyRenderer` | Charge un GeoJSON complet |

La page est initialement vide (fond OpenStreetMap, centre Paris). Le premier
rendu est déclenché dans `MainWindow::onParsingSuccess()` via
`TopologyRenderer::renderAllTopology()`.

---

## Panneau PCC {#hmi_pcc}

Voir @ref page_pcc pour la documentation détaillée du module PCC/TCO.

@ref PCCPanel est une fenêtre Win32 enfant (style `WS_CHILD | WS_VISIBLE`)
positionnée par `MainWindow::onSizeUpdate()` pour couvrir la zone d'affichage.
Elle est togglée visible/invisible via `F2` ou le menu `Vue → PCC`.

---

## Dialogs {#hmi_dialogs}

| Classe | Raccourci | Description |
|--------|-----------|-------------|
| `AboutDialog` | Menu Aide | Boîte À propos |
| `FileOpenDialog` | `Ctrl+O` | Sélecteur GeoJSON (IFileOpenDialog COM) |
| `FileSaveDialog` | `Ctrl+E` | Sélecteur de destination pour l'export GeoJSON |
| `ParserSettingsDialog` | `Ctrl+P` | Formulaire de paramètres `ParserConfig` avec bouton Réinitialiser |

`ParserSettingsDialog` expose des contrôles `EDIT` pour chaque paramètre
numérique de @ref ParserConfig. À la validation, la config est sauvegardée
via `ParserConfigIni::save()` dans `m_parserIniPath`.

---

## Barre de progression {#hmi_progressbar}

@ref ProgressBar encapsule un contrôle natif `PROGRESS_CLASS` Win32 avec
un bouton **Annuler** (`IDC_CANCEL_PARSING`). Elle affiche un label de phase
en superposition.

`show(true)` / `show(false)` gèrent la visibilité de l'ensemble
(barre + bouton + label) pour ne pas encombrer l'interface hors parsing.

---

## Références croisées {#hmi_refs}

| Classe | Fichier | Rôle |
|--------|---------|------|
| @ref MainWindow | `Engine/HMI/MainWindow.h` | Fenêtre principale |
| @ref WebViewPanel | `Engine/HMI/WebViewPanel/WebViewPanel.h` | Contrôle WebView2 |
| @ref Leaflet | `Engine/HMI/WebViewPanel/Leaflet/Leaflet.h` | Générateur HTML Leaflet |
| @ref PCCPanel | `Engine/HMI/PCCPanel/PCCPanel.h` | Panneau TCO Win32 |
| @ref ProgressBar | `Engine/HMI/Utils/ProgressBar.h` | Barre de progression |
| @ref GeoParsingTask | `Modules/GeoParser/GeoParsingTask.h` | Thread parsing |
| @ref TopologyRenderer | `Engine/Core/Topology/TopologyRenderer.h` | Scripts Leaflet |

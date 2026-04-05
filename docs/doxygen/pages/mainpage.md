@mainpage SimulateurFerroviaire — Documentation technique

@tableofcontents

---

## Présentation {#mainpage_intro}

**SimulateurFerroviaire** est un simulateur ferroviaire Win32/C++ ciblant la ligne
**Saujon – La Tremblade** (Charente-Maritime). Il reconstitue la topologie réelle
du réseau à partir de données géographiques ouvertes (GeoJSON OpenStreetMap) et en
produit une représentation schématique interactive conforme aux standards SNCF de
contrôle du mouvement (PCC/TCO).

L'application est développée sous **Visual Studio 2022**, compilée en **x64** sur
**Windows 11**, avec les bibliothèques système Win32, WebView2 (COM) et GDI.

---

## Modules {#mainpage_modules}

Le projet est découpé en quatre grandes zones fonctionnelles, reflétées dans
l'arborescence des sources :

| Module | Espace de noms / dossier | Rôle |
|--------|--------------------------|------|
| @ref page_elements "Éléments interactifs" | `Modules/Elements/` | Modèle de domaine — blocs de voie et aiguillages |
| @ref page_geoparser "GeoParser v2" | `Modules/GeoParser/` | Pipeline de conversion GeoJSON → topologie |
| @ref page_pcc "PCC / TCO" | `Modules/PCC/` | Graphe logique et rendu du tableau de contrôle |
| @ref page_engine "Engine Core" | `Engine/Core/` | Logger, coordonnées, topologie, configuration |
| @ref page_hmi "HMI" | `Engine/HMI/` | Fenêtre principale, WebView2, panneau PCC |

---

## Flux de données {#mainpage_flux}

```
Fichier GeoJSON
      │
      ▼
┌─────────────────┐
│  GeoParser v2   │   8 phases de transformation
│  (pipeline)     │──────────────────────────────────► TopologyRepository
└─────────────────┘                                         (singleton)
                                                               │
                    ┌──────────────────────────────────────────┤
                    │                                          │
                    ▼                                          ▼
            ┌──────────────┐                       ┌─────────────────┐
            │  PCCPanel    │◄──── PCCGraphBuilder   │  WebViewPanel   │
            │  (Win32/GDI) │       + PCCLayout      │  (Leaflet.js)   │
            │  TCORenderer │                        │  TopologyRender │
            └──────────────┘                        └─────────────────┘
```

L'unique point de partage entre les modules est le singleton
@ref TopologyRepository, qui expose un @ref TopologyData immuable après
le parsing.

---

## Démarrage rapide {#mainpage_quickstart}

### Charger un GeoJSON

1. Lancer l'application.
2. Fichier → Ouvrir (`Ctrl+O`) → sélectionner un fichier `.geojson`.
3. La barre de progression affiche l'avancement des 8 phases du pipeline.
4. En cas de succès, la carte Leaflet affiche la topologie et le panneau
   PCC est disponible via `F2`.

### Ajuster les paramètres du pipeline

Menu **Fichier → Paramètres du parser** (`Ctrl+P`) ouvre
@ref ParserSettingsDialog. Les valeurs sont persistées dans
`parser_config.ini` (voir @ref ParserConfigIni).

Paramètres clés :

| Paramètre | Défaut | Description |
|-----------|--------|-------------|
| `snapTolerance` | 1,0 m | Distance maximale entre deux extrémités pour les fusionner |
| `maxSegmentLength` | 200,0 m | Longueur au-delà de laquelle un straight est subdivisé |
| `minSwitchAngle` | 5,0 ° | Angle minimum pour qu'une bifurcation soit classée SWITCH |
| `junctionTrimMargin` | 3,0 m | Recul des tips CDC depuis la jonction |
| `switchSideSize` | 15,0 m | Demi-longueur des stubs pour l'interpolation des tips |
| `doubleSwitchRadius` | 20,0 m | Rayon de détection des aiguilles doubles |

### Interagir avec la topologie

- **Clic sur un aiguillage** dans la carte Leaflet → bascule
  `NORMAL` ↔ `DEVIATION` et propage aux partenaires double-aiguille.
- **Panneau PCC** (`F2`) → TCO schématique zoomable/pannable
  (molette + glisser).

---

## Conventions de code {#mainpage_conventions}

| Élément | Convention |
|---------|------------|
| Noms de variables et fonctions | **Anglais** |
| Docstrings Doxygen | **Français** |
| Système de coordonnées métrique | **UTM zone 30N** |
| Système de coordonnées géographique | **WGS-84** |
| Gestion d'erreur fatale | `LOG_FAILURE` → `std::runtime_error` |
| Ownership des blocs | `std::unique_ptr` dans `TopologyData` |
| Pointeurs non-propriétaires | Pointeurs bruts annotés `non-propriétaire` |

---

## Voir aussi {#mainpage_seealso}

- @ref page_geoparser — pipeline de transformation GeoJSON
- @ref page_pcc — graphe PCC et rendu TCO
- @ref page_elements — hiérarchie des blocs ferroviaires
- @ref page_engine — services transversaux (Logger, coordonnées, config)
- @ref page_hmi — interface graphique Win32 / WebView2

# 🚆 Simulateur Ferroviaire

## 📖 Description
Reconstruction et visualisation d’un réseau ferroviaire à partir de données GeoJSON.

Le projet repose sur un pipeline de parsing permettant de :
- Charger des données géographiques
- Construire un graphe topologique
- Extraire des blocs ferroviaires (voies + aiguillages)
- Produire une représentation exploitable et visualisable

---

# 🧠 Engine — Moteur de l’application

## 🔹 Core — Cœur applicatif
Gestion des composants fondamentaux et de la logique métier.

- Application → cycle de vie de l’application  
- Logger → système de journalisation (logs, erreurs, debug)

## 🖥️ HMI — Interface utilisateur
Gestion de l’interface graphique (Win32 + WebView).

- MainWindow → fenêtre principale  
- ProgressBar → affichage des tâches longues  
- WebViewPanel → affichage de openstreetview (Leaflet)  
    - leaflet.js décrit l'ensemble des scripts d'injections      
- Dialogs
    - AboutDialog  
  	- FileOpenDialog  
    - FileSaveDialog

---

# 🧩 Modules — Fonctionnalités métier

Modules indépendants responsables du traitement ferroviaire.

---

# 🌍 GeoParser — Pipeline principal

Pipeline complet de transformation GeoJSON → modèle ferroviaire.

GeoParsingTask
- Tâche asynchrone de parsing

## 🔄 Pipeline global

1. Chargement GeoJSON  
2. Construction du graphe  
3. Extraction topologique  
4. Orientation des aiguillages  
5. Détection des doubles aiguilles  
6. Clear + Sauvegarde du modèle ferroviaire dans le singleton TopologyRepository

👉 Voir implémentation dans : GeoParser::parse()

---

## 🗺️ Parsing & Construction

GeoParser
- Orchestrateur du pipeline complet

GeoParsingTask
- Parsing asynchrone (évite le blocage UI)

GeometryUtils
- Outils géométriques (conversion, validation, calculs)

---

## 🔗 Graphe topologique

GraphBuilder
- Construction du graphe à partir du GeoJSON  
- Conversion WGS84 → UTM  
- Snap + fusion des nœuds  

TopologyGraph
- Représentation du graphe (nœuds + arêtes)

TopologyEdge
- Arête entre deux nœuds avec géométrie métrique

---

## 🧠 Extraction ferroviaire

TopologyExtractor
- Extraction des blocs à partir du graphe

SwitchOrientator
- Orientation des aiguillages (root / normal / deviation)

DoubleSwitchDetector
- Détection des doubles aiguilles  
- Validation des contraintes métier (CDC)

---

# 🌍 GeoJsonExporter — Exportation des données

GeoJsonExporter
- Exportation du modèle ferroviaire en GeoJSON

---

# 📦 Models — Modèle de données

Représentation des entités ferroviaires manipulées par le pipeline.

## 📍 Coordonnées

CoordinateXY
- Coordonnées métriques (UTM)
LatLon
- Coordonnées géographiques WGS84

---

## 🚧 Blocs ferroviaires

TopologyRepository
- Stockage des données topologiques (TopologyData) en singleton

StraightBlock
- Tronçon de voie droite
- Longueur calculée via Haversine


SwitchBlock
- Aiguillage ferroviaire (3 branches)
- Orientation + calculs géométriques

---

# 📜 Licence

Ce projet est distribué sous licence :

**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)**

👉 https://creativecommons.org/licenses/by-nc-sa/4.0/

## Vous êtes autorisé à :
- ✔️ Partager — copier et redistribuer le matériel
- ✔️ Adapter — remixer, transformer et créer à partir du matériel

## Sous les conditions suivantes :
- 📝 Attribution — vous devez créditer l’auteur
- 🚫 Non commercial — pas d’utilisation commerciale
- 🔁 Partage dans les mêmes conditions — redistribution sous la même licence

---

# ✨ Auteur

© 2026 Valentin Eloy

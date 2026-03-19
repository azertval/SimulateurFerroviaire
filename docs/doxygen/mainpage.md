# 🚆 Simulateur Ferroviaire

## 📖 Description
Reconstruction et visualisation d’un réseau ferroviaire à partir de données **GeoJSON**.

Le projet repose sur un pipeline de parsing permettant de :
- Charger des données géographiques
- Construire un graphe topologique
- Extraire des blocs ferroviaires (voies + aiguillages)
- Produire une représentation exploitable et visualisable

---

# 🧠 Engine — Moteur de l’application

## 🔹 Core — Cœur applicatif
Gestion des composants fondamentaux et de la logique métier.

- **Application** → cycle de vie de l’application  
- **Logger** → système de journalisation (logs, erreurs, debug)

## 🖥️ HMI — Interface utilisateur
Gestion de l’interface graphique (Win32 + WebView).

- **MainWindow** → fenêtre principale  
- **ProgressBar** → affichage des tâches longues  
- **WebViewPanel** → affichage de la carte (Leaflet)  
- **Dialogs**
  - AboutDialog  
  - FileOpenDialog  

---

# 🧩 Modules — Fonctionnalités métier

Modules indépendants responsables du traitement ferroviaire.

## 🌍 GeoParser — Pipeline principal

Pipeline complet de transformation GeoJSON → modèle ferroviaire.

### 🔄 Pipeline global
1. Chargement GeoJSON  
2. Construction du graphe  
3. Extraction topologique  
4. Orientation des aiguillages  
5. Détection des doubles aiguilles  

👉 Voir implémentation dans :  
`GeoParser::parse()` :contentReference[oaicite:0]{index=0}

---

## 🗺️ Parsing & Construction

### **GeoParser**
- Orchestrateur du pipeline complet

### **GeoParsingTask**
- Parsing asynchrone (évite le blocage UI)

### **GeometryUtils**
- Outils géométriques (conversion, validation, calculs)

---

## 🔗 Graphe topologique

### **GraphBuilder**
- Construction du graphe à partir du GeoJSON  
- Conversion WGS84 → UTM  
- Snap + fusion des nœuds  

👉 Voir phases 1 & 2 :contentReference[oaicite:1]{index=1}

### **TopologyGraph**
- Représentation du graphe (nœuds + arêtes)

### **TopologyEdge**
- Arête entre deux nœuds avec géométrie métrique

---

## 🧠 Extraction ferroviaire

### **TopologyExtractor**
- Extraction des blocs à partir du graphe

### **SwitchOrientator**
- Orientation des aiguillages (root / normal / deviation)

### **DoubleSwitchDetector**
- Détection des doubles aiguilles  
- Validation des contraintes métier (CDC)

👉 Phases 7 & 8 :contentReference[oaicite:2]{index=2}

---

# 📦 Models — Modèle de données

Représentation des entités ferroviaires manipulées par le pipeline.

## 📍 Coordonnées

- **CoordinateXY**
  - Coordonnées métriques (UTM)
- **LatLon**
  - Coordonnées géographiques WGS84

---

## 🚧 Blocs ferroviaires

### **StraightBlock**
- Tronçon de voie droite
- Longueur calculée via Haversine

👉 Implémentation :contentReference[oaicite:3]{index=3}

### **SwitchBlock**
- Aiguillage ferroviaire (3 branches)
- Orientation + calculs géométriques

👉 Implémentation :contentReference[oaicite:4]{index=4}

---

# 🧭 Architecture globale

```text
GeoJSON
   ↓
GraphBuilder
   ↓
TopologyGraph
   ↓
TopologyExtractor
   ↓
SwitchOrientator
   ↓
DoubleSwitchDetector
   ↓
Models (Straight / Switch)
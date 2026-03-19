# Simulateur Ferroviaire

## 🚆 Description

Le projet **Simulateur Ferroviaire** permet de reconstruire un réseau ferroviaire à partir de données GeoJSON.

Il s'appuie sur :

* Un parsing géographique (WGS-84)
* Une reconstruction topologique (graphe)
* Une détection et orientation des aiguillages

---

## 📊 Pipeline

Le pipeline de traitement est le suivant :

```
GeoJSON
   ↓
GeoParser
   ↓
TopologyGraph
   ↓
Switch Detection
   ↓
SwitchOrientator
```

---

## 🧱 Modules

* **GeoParser**
  Lecture et parsing des fichiers GeoJSON.

* **TopologyGraph**
  Construction du graphe ferroviaire à partir des données géographiques.

* **SwitchBlock**
  Modélisation des aiguillages ferroviaires.

* **SwitchOrientator**
  Détermination de l’orientation des aiguillages (root, normal, deviation).

---

## 📐 Concepts clés

* Coordonnées WGS-84 (`LatLon`)
* Distance Haversine
* Graphe planaire
* Orientation géométrique

---

## ⚙️ Fonctionnement global

1. Lecture du fichier GeoJSON
2. Extraction des coordonnées
3. Construction du graphe topologique
4. Détection des nœuds complexes (aiguillages)
5. Orientation des branches
6. Préparation pour la simulation ferroviaire

---

## 📁 Structure du projet

```
SimulateurFerroviaire/
├── Engine/
│   ├── Core/
│   ├── Graph/
│   └── Utils/
├── Modules/
│   └── GeoParser/
├── External/
│   └── nlohmann/
```

---

## 👨‍💻 Auteur

Valentin Eloy

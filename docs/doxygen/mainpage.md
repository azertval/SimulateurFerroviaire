# Simulateur Ferroviaire

## Description

Reconstruction et visualisation d’un réseau ferroviaire depuis GeoJSON.

---
# Engine : Moteur de l’application, gestion des tâches, etc

## Core : Coeur de l’application, gestion des données, logique métier, etc.
- Application : gestion de l’application, cycle de vie, etc.
- Logger : gestion des logs, erreurs, etc.

## HMI : Interface Homme-Machine, gestion de l’interface utilisateur, etc.

- MainWindow : fenêtre principale de l’application
- ProgressBar : barre de progression pour les tâches longues
- WebViewPanel : panneau pour afficher la carte interactive (Leaflet)
- Dialogs : gestion des dialogues (AboutDialog, FileOpenDialog)

---
# Modules : Block de fonctionnalités indépendante spécifiques, comme le parsing, la détection d’aiguillages, etc.

## GeoParser : analyse et extraction des données GeoJSON
- GeoParsingTask : tâche de parsing asynchrone pour éviter de bloquer l’interface utilisateur
- GeoParser : analyse et extraction des données GeoJSON
	- GeometryUtils : fonctions utilitaires pour le module GeoParser (par exemple, pour la validation des données, la conversion de coordonnées, etc.)
- GraphBuilder : construction du graphe à partir des données extraites
	- TopologyGraph : représentation du réseau ferroviaire sous forme de graphe topologique
	- TopologyEdge : représentation d’une arête du graphe topologique (liaison entre deux nœuds)
- TopologyExtractor : extraction de la topologie du réseau ferroviaire à partir des données extraites
- SwitchOrientator : orientation des aiguillages détectés pour une visualisation correcte sur la carte
- DoubleSwitchDetector : détection des aiguillages doubles à partir du graphe topologique

## Models : classes représentant les models de données ferroviaires
- CoordinateXY : classe représentant une coordonnée en X et Y
- LatLon : classe représentant une coordonnée en latitude et longitude
- StraightBlock : classe représentant un bloc droit du réseau ferroviaire
- SwitchBlock : classe représentant un bloc d’aiguillage du réseau ferroviaire

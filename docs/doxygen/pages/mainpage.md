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

![Architecture SimulateurFerroviaire](architecture.svg)

---

# Documentation

- @subpage engine                  — Moteur de l'application
- @subpage geoparser               — Module de parsing GeoJson
- @subpage elements                — Module Éléments — Modèle de domaine ferroviaire
- @subpage pcc                     — Module PCC
- @subpage references              — Références externes

---

# Licence {#licence}

Ce projet est distribué sous licence :

**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)**

https://creativecommons.org/licenses/by-nc-sa/4.0/

- Partager — copier et redistribuer le matériel
- Adapter — remixer, transformer et créer à partir du matériel
- Attribution requise
- Usage commercial interdit
- Redistribution sous la même licence

---

# Auteur {#auteur}

© 2026 Valentin Eloy
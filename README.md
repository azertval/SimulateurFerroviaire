![C++](https://img.shields.io/badge/C%2B%2B-20-blue)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![CI](https://github.com/azertval/SimulateurFerroviaire/actions/workflows/build_debug.yml/badge.svg)
[![Documentation](https://img.shields.io/badge/Documentation-Doxygen-blue)](https://azertval.github.io/SimulateurFerroviaire/)
![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/license-CC--BY--NC--SA%204.0-lightgrey.svg)

# 🚆 Simulateur Ferroviaire

## 🎯 Overview

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
## 📚 Documentation

La documentation du projet est générée avec **Doxygen**.
👉 Ouvrir la documentation : https://azertval.github.io/SimulateurFerroviaire/

👉 Genérer la doc en local :
    ```bash
    doxygen -g Doxyfile
    ```
👉  docs/doxygen/mainpage.md : 
La mainpage de Doxygen est configurée pour présenter une vue d’ensemble du projet, avec des sections dédiées à la description, à l’architecture, aux modules, et à l’utilisation.

---
# ⚙️ Prérequis

* Windows 10/11
* Visual Studio 2022 ou Visual Studio Insiders
* CMake ≥ 3.20

### Workloads requis

* ✔ Desktop development with C++

### Composants nécessaires

* ✔ MSVC (v143 ou v180)
* ✔ Windows SDK
* ✔ CMake tools

---

# 🛠️ Première compilation

## 1. Cloner le projet

```bash
git clone <repo_url>
cd SimulateurFerroviaire
```

---

## 2. Générer le projet

### Visual Studio 2022

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
```

### Visual Studio Insiders

```bash
cmake -B build -S . -G "Visual Studio 18 2026" -A x64
```

---

## 3. Compiler

```bash
cmake --build build --config Debug
```

---

## 4. Exécuter

```text
build/Debug/SimulateurFerroviaire.exe
```

---

# 🧪 CI / Build automatique

Le projet utilise GitHub Actions :

* Compilation automatique des Pull Requests
* Validation du build
* Génération de la documentation
* Validation des issues associées

---

# 🐞 Dépannage

## ❌ Toolset MSVC introuvable

Installer :

* Desktop development with C++
* MSVC toolset

---

## ❌ Erreur `main` non trouvé

Le projet est une application Win32 :

```cmake
add_executable(SimulateurFerroviaire WIN32 ...)
```

---

## ❌ Pas de .exe généré

Vérifier le dossier :

```text
out/build/Debug/
```

---

## ❌ Erreur CMake (generator mismatch)

```bash
rm -rf out/
```

---

# 🧩 Bonnes pratiques

* Ne pas modifier les fichiers `.vcxproj`
* Toujours utiliser CMake
* Faire un clean build en cas de problème

---

# 🚀 Roadmap

Voir les issues du projet pour les fonctionnalités à venir et les améliorations prévues.

---

# 🤝 CONTRIBUTING

## Workflow

1. Créer issue
2. Créer branche feature/#ID
3. PR avec Closes #ID

## Standard Issue

- Planning
- Dependencies
- Specs
- Steps
- Acceptance

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
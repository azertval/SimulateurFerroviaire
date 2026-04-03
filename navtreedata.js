/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Simulateur Ferroviaire", "index.html", [
    [ "Description", "index.html#description", null ],
    [ "Architecture du projet", "index.html#diag", null ],
    [ "Documentation", "index.html#documentation", null ],
    [ "Licence", "index.html#licence", null ],
    [ "Auteur", "index.html#auteur", null ],
    [ "Engine — Moteur de l'application", "d3/d03/engine.html", [
      [ "Core — Cœur applicatif", "d3/d03/engine.html#core", [
        [ "TopologyData", "d3/d03/engine.html#data", null ],
        [ "TopologyRenderer", "d3/d03/engine.html#renderer", null ]
      ] ],
      [ "HMI — Interface utilisateur", "d3/d03/engine.html#hmi", [
        [ "Panneau PCC", "d3/d03/engine.html#pcc_panel", null ],
        [ "TCORenderer", "d3/d03/engine.html#tco", null ],
        [ "Binding bidirectionnel Leaflet ↔ C++", "d3/d03/engine.html#binding", null ]
      ] ],
      [ "Coordonnées", "d3/d03/engine.html#Coordinates", null ]
    ] ],
    [ "GeoParser — Pipeline", "d4/d01/geoparser.html", [
      [ "GeoParser", "d4/d01/geoparser.html#geoparser", [
        [ "Architecture du pipeline", "d4/d01/geoparser.html#pipeline_arch", null ],
        [ "Phases", "d4/d01/geoparser.html#pipeline", null ],
        [ "Libération mémoire inter-phases", "d4/d01/geoparser.html#memory", null ],
        [ "Tâche asynchrone", "d4/d01/geoparser.html#task", null ],
        [ "Structures de données du pipeline", "d4/d01/geoparser.html#pipeline_data", null ]
      ] ],
      [ "Architecture des fichiers", "d4/d01/geoparser.html#gp_files", null ],
      [ "ParserConfig — Paramètres", "d4/d01/geoparser.html#gp_config", null ],
      [ "PipelineContext — Transporteur inter-phases", "d4/d01/geoparser.html#gp_context", null ],
      [ "GeoParser — Orchestrateur", "d4/d01/geoparser.html#gp_orchestrator", null ],
      [ "Phases du pipeline", "d4/d01/geoparser.html#gp_phases", [
        [ "Phase 1 — GeoLoader", "d4/d01/geoparser.html#gp_phase1", null ],
        [ "Phase 2 — GeometricIntersector", "d4/d01/geoparser.html#gp_phase2", [
          [ "Algorithme de Cramer", "d4/d01/geoparser.html#algorithme-de-cramer", null ]
        ] ],
        [ "</blockquote>", "d4/d01/geoparser.html#blockquote-1", null ],
        [ "Phase 3 — NetworkSplitter", "d4/d01/geoparser.html#gp_phase3", null ],
        [ "Phase 4 — TopologyBuilder", "d4/d01/geoparser.html#gp_phase4", [
          [ "Union-Find — principe", "d4/d01/geoparser.html#union-find--principe", null ]
        ] ],
        [ "</blockquote>", "d4/d01/geoparser.html#blockquote-2", null ],
        [ "Phase 5 — SwitchClassifier", "d4/d01/geoparser.html#gp_phase5", null ],
        [ "Phase 6 — BlockExtractor", "d4/d01/geoparser.html#gp_phase6", [
          [ "Déduplication — par arêtes (et non par paire de nœuds)", "d4/d01/geoparser.html#déduplication--par-arêtes-et-non-par-paire-de-nœuds", null ],
          [ "Subdivision par longueur cumulée", "d4/d01/geoparser.html#subdivision-par-longueur-cumulée", null ],
          [ "Index directionnels dans BlockSet", "d4/d01/geoparser.html#index-directionnels-dans-blockset", null ],
          [ "Chaînage des sous-blocs", "d4/d01/geoparser.html#chaînage-des-sous-blocs", null ]
        ] ],
        [ "Phase 7 — SwitchProcessor", "d4/d01/geoparser.html#gp_phase7", [
          [ "Sous-phases G → A → B → C → D → E → F", "d4/d01/geoparser.html#sous-phases-g--a--b--c--d--e--f", null ],
          [ "G — Orientation géométrique", "d4/d01/geoparser.html#g--orientation-géométrique", null ],
          [ "F — Tips CDC", "d4/d01/geoparser.html#f--tips-cdc", null ]
        ] ],
        [ "Phase 8 — RepositoryTransfer", "d4/d01/geoparser.html#gp_phase8", [
          [ "resolveStraight — préservation de la chaîne", "d4/d01/geoparser.html#resolvestraight--préservation-de-la-chaîne", null ]
        ] ]
      ] ],
      [ "GeoParsingTask — Intégration async", "d4/d01/geoparser.html#gp_task", null ],
      [ "ParserSettingsDialog", "d4/d01/geoparser.html#gp_dialog", null ]
    ] ],
    [ "Éléments — Modèle de domaine ferroviaire", "da/d02/elements.html", [
      [ "Vue d'ensemble", "da/d02/elements.html#elements_overview", null ],
      [ "Hiérarchie", "da/d02/elements.html#elements_hierarchy", [
        [ "</blockquote>", "da/d02/elements.html#blockquote", null ]
      ] ],
      [ "Element", "da/d02/elements.html#interactive_element", [
        [ "Logger statique partagé", "da/d02/elements.html#logger", null ]
      ] ],
      [ "ShuntingElement", "da/d02/elements.html#shunting_element", null ],
      [ "StraightBlock", "da/d02/elements.html#straight_block", [
        [ "Géométrie duale WGS84 / UTM", "da/d02/elements.html#straight_geometry", null ],
        [ "Topologie — IDs de voisins", "da/d02/elements.html#straight_topo_ids", null ],
        [ "Pointeurs résolus — <tt>StraightNeighbours</tt>", "da/d02/elements.html#straight_resolved", null ]
      ] ],
      [ "SwitchBlock", "da/d02/elements.html#switch_block", [
        [ "Géométrie — jonction et tips CDC", "da/d02/elements.html#switch_geometry", null ],
        [ "Orientation — rôles des branches", "da/d02/elements.html#switch_orientation", null ],
        [ "Double aiguille — absorption du segment de liaison", "da/d02/elements.html#switch_double", null ],
        [ "Pointeurs résolus — <tt>SwitchBranches</tt>", "da/d02/elements.html#switch_resolved", null ],
        [ "État opérationnel — branche active", "da/d02/elements.html#switch_active_branch", null ]
      ] ],
      [ "Pointeurs résolus — récapitulatif", "da/d02/elements.html#resolved_pointers", null ],
      [ "Énumérations", "da/d02/elements.html#enums", null ]
    ] ],
    [ "Module PCC — Graphe logique et rendu TCO", "d8/d02/pcc.html", [
      [ "Vue d'ensemble", "d8/d02/pcc.html#pcc_overview", null ],
      [ "Nœuds et arêtes", "d8/d02/pcc.html#pcc_nodes", null ],
      [ "PCCGraph — Conteneur", "d8/d02/pcc.html#pcc_graph", null ],
      [ "PCCGraphBuilder — Construction", "d8/d02/pcc.html#pcc_builder", null ],
      [ "PCCLayout — Positionnement", "d8/d02/pcc.html#pcc_layout", null ]
    ] ],
    [ "Références", "d5/d01/references.html", [
      [ "Références externes", "d5/d01/references.html#rextern", [
        [ "C++ moderne", "d5/d01/references.html#cpp", null ],
        [ "Architecture logicielle", "d5/d01/references.html#archiconcept", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Liste des classes", "annotated.html", "annotated_dup" ],
      [ "Index des classes", "classes.html", null ],
      [ "Hiérarchie des classes", "hierarchy.html", "hierarchy" ],
      [ "Membres de classe", "functions.html", [
        [ "Tout", "functions.html", "functions_dup" ],
        [ "Fonctions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ]
      ] ]
    ] ],
    [ "Fichiers", "files.html", [
      [ "Liste des fichiers", "files.html", "files_dup" ],
      [ "Membres de fichier", "globals.html", [
        [ "Tout", "globals.html", null ],
        [ "Fonctions", "globals_func.html", null ],
        [ "Définitions de type", "globals_type.html", null ],
        [ "Énumérations", "globals_enum.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"d3/d03/engine.html#renderer",
"d7/d03/classTopologyRenderer.html#ab35534539fde31c290ce7e531fce86fe",
"da/d03/classGeoParsingTask.html#af6c79fee7b3bc3c7db9755d301244bed",
"functions_func_f.html"
];

var SYNCONMSG = 'cliquez pour désactiver la synchronisation du panel';
var SYNCOFFMSG = 'cliquez pour activer la synchronisation du panel';
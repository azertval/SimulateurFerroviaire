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
      ] ]
    ] ],
    [ "Modules — Fonctionnalités métier", "d1/d00/modules.html", [
      [ "GeoParser — Pipeline principal", "d1/d00/modules.html#geoparser", [
        [ "Tâche asynchrone", "d1/d00/modules.html#task", null ],
        [ "Pipeline global", "d1/d00/modules.html#pipeline", null ],
        [ "Classes du pipeline", "d1/d00/modules.html#parsing", null ]
      ] ],
      [ "Éléments interactifs", "d1/d00/modules.html#elements", [
        [ "Hiérarchie", "d1/d00/modules.html#hierarchy", null ],
        [ "Pointeurs résolus post-parsing", "d1/d00/modules.html#pointers", null ],
        [ "État opérationnel des aiguillages", "d1/d00/modules.html#activestate", null ],
        [ "Énumérations clés", "d1/d00/modules.html#enums", null ]
      ] ],
      [ "Coordonnées", "d1/d00/modules.html#coords", null ]
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
        [ "Variables", "functions_vars.html", null ],
        [ "Définitions de type", "functions_type.html", null ]
      ] ]
    ] ],
    [ "Fichiers", "files.html", [
      [ "Liste des fichiers", "files.html", "files_dup" ],
      [ "Membres de fichier", "globals.html", [
        [ "Tout", "globals.html", null ],
        [ "Fonctions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
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
"d4/d01/classLatLon.html#a72ddde59606bc3160a064f74654bd88b",
"d9/d02/classSwitchBlock.html#af65a62f314ce4544ce687eacc13786e8",
"df/d01/classCoordinateXY.html#ad8cc600d7896c329e410cce0c1e45a70"
];

var SYNCONMSG = 'cliquez pour désactiver la synchronisation du panel';
var SYNCOFFMSG = 'cliquez pour activer la synchronisation du panel';
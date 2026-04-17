/**
 * @file  TCORenderer.h
 * @brief Renderer GDI du schéma TCO — style SNCF, blocs fixes.
 *
 * @par Rendu par bloc fixe
 * Chaque nœud occupe une cellule de même largeur, séparées par un gap.
 *  - Straight  : trait horizontal plein.
 *  - Switch    : root + branche active (couleur état voie) + branche inactive
 *                (gris foncé, raccourcie côté jonction).
 *  - Crossing  : deux voies se croisant — StraightCrossBlock (✕ plat) ou
 *                SwitchCrossBlock/TJD (✕ avec coloration par voie active).
 *
 * @par Architecture du rendu des crossings
 * drawCrossingBlock() route vers drawFlatCrossingBlock() ou drawTJDCrossingBlock()
 * selon le type de source. Les bras (StraightBlock adjacents au flat,
 * SwitchBlock adjacents au TJD) sont dessinés par leurs propres fonctions —
 * drawCrossingBlock() dessine uniquement le symbole ✕ central.
 *
 * @par Cache de projection
 * computeProjection() est exposée publiquement pour permettre à PCCPanel de
 * mettre le résultat en cache et éviter un recalcul O(n) à chaque WM_PAINT.
 * draw() reçoit une Projection précalculée — computeProjection() n'est pas
 * rappelé en interne.
 */
#pragma once

#include "framework.h"
#include "Modules/PCC/PCCGraph.h"
#include "Modules/PCC/PCCSwitchNode.h"
#include "Modules/PCC/PCCCrossingNode.h"
#include "Engine/Core/Logger/Logger.h"

class TCORenderer
{
public:

    /**
     * @brief Paramètres de projection logique → écran.
     *
     * Calculé une seule fois par computeProjection() et mis en cache dans
     * PCCPanel::m_cachedProj. Invalide après resize ou rebuild().
     *
     * Les champs stub/inactiveGap/halfGap sont dérivés de cellWidth et
     * précalculés dans computeProjection() — évite N recalculs identiques
     * par WM_PAINT.
     */
    struct Projection
    {
        int minX      = 0;
        int maxX      = 0;
        int minY      = 0;
        int maxY      = 0;
        int width     = 0;
        int height    = 0;
        int cellWidth = 1;
        int cellHeight = 1;
        int marginX   = 0;
        int centerY   = 0;

        /** Demi-gap entre blocs adjacents (pixels). = BLOCK_GAP_PX / 2, arrondi. */
        int halfGap = 0;

        /**
         * Longueur du stub horizontal en fin de branche déviée (pixels).
         * = max(4, cellWidth * STUB_RATIO).
         */
        int stub = 0;

        /**
         * Gap côté jonction de la branche inactive (pixels).
         * = max(4, cellWidth * INACTIVE_GAP_RATIO).
         */
        int inactiveGap = 0;
    };

    /**
     * @brief Calcule la projection logique → écran depuis le graphe et la RECT.
     *
     * Exposée publiquement pour permettre à PCCPanel de mettre le résultat
     * en cache et éviter un recalcul O(n) à chaque WM_PAINT.
     *
     * @param rect   Rectangle client de la fenêtre de dessin.
     * @param graph  Graphe PCC source des positions logiques.
     * @param logger Logger pour les traces de débogage.
     * @return Projection calculée.
     */
    static Projection computeProjection(const RECT& rect,
                                        const PCCGraph& graph,
                                        Logger& logger);

    /**
     * @brief Point d'entrée du rendu — remplit le fond puis dessine tous les nœuds.
     *
     * La projection est passée depuis le cache de PCCPanel — computeProjection()
     * n'est pas rappelé en interne.
     *
     * @param hdc            Contexte de dessin GDI.
     * @param rect           Rectangle client (pour FillRect uniquement).
     * @param graph          Graphe PCC.
     * @param proj           Projection précalculée par PCCPanel.
     * @param logger         Logger.
     * @param fillBackground Si false, le remplissage du fond est ignoré.
     *                       Mettre à false quand PCCPanel remplit le fond
     *                       avant d'appliquer une world transform (zoom/pan),
     *                       afin d'éviter de recouvrir la zone avec la
     *                       transform active.
     */
    static void draw(HDC hdc, const RECT& rect,
                     const PCCGraph& graph,
                     const Projection& proj,
                     Logger& logger,
                     bool fillBackground = true);

    TCORenderer() = delete;

private:

    /**
     * @brief Projette une position logique (x, y) en coordonnées écran.
     * @param logicalX  Position X logique (colonne BFS).
     * @param logicalY  Position Y logique (rang vertical).
     * @param proj      Paramètres de projection.
     * @return Point écran correspondant.
     */
    static POINT project(int logicalX, int logicalY, const Projection& proj);

    /**
     * @brief Dessine tous les nœuds du graphe avec la projection fournie.
     * @param hdc    Contexte de dessin.
     * @param proj   Projection courante.
     * @param graph  Graphe PCC.
     * @param logger Logger.
     */
    static void drawNodes(HDC hdc, const Projection& proj,
                          const PCCGraph& graph, Logger& logger);

    /**
     * @brief Dessine un nœud StraightBlock (trait horizontal avec gap).
     *
     * S'étend jusqu'au mi-chemin de chaque voisin de même Y.
     * Exception : les bras de crossing (voisin CROSSING présent) lèvent
     * le filtre Y pour assurer la jonction visuelle avec le ✕ central,
     * même si le voisin switch est à un Y différent.
     *
     * @param hdc    Contexte de dessin.
     * @param proj   Projection courante.
     * @param node   Nœud straight à dessiner.
     * @param logger Logger.
     */
    static void drawStraightBlock(HDC hdc, const Projection& proj,
                                  const PCCNode* node, Logger& logger);

    /**
     * @brief Dessine un nœud SwitchBlock (root + normal + déviation).
     *
     * Utilise static_cast (type garanti par getNodeType() == SWITCH).
     * Instancie un PenScope par branche — aucun CreatePen redondant.
     *
     * @param hdc    Contexte de dessin.
     * @param proj   Projection courante (stub/inactiveGap/halfGap précalculés).
     * @param sw     Nœud switch à dessiner.
     * @param logger Logger.
     */
    static void drawSwitchBlock(HDC hdc, const Projection& proj,
                                const PCCSwitchNode* sw, Logger& logger);

    /**
     * @brief Route vers drawFlatCrossingBlock() ou drawTJDCrossingBlock()
     *        selon le type de source du nœud crossing.
     *
     * @param hdc    Contexte de dessin GDI.
     * @param proj   Projection courante.
     * @param cr     Nœud crossing à dessiner.
     * @param logger Logger.
     */
    static void drawCrossingBlock(HDC hdc, const Projection& proj,
                                  const PCCCrossingNode* cr, Logger& logger);

    /**
     * @brief Dessine un croisement plat (StraightCrossBlock) — symbole ✕.
     *
     * Dessine deux voies se croisant dans la colonne [crX-1 … crX+1].
     * Les bords du bloc sont les mid-points avec les colonnes voisines (crX±1),
     * symétrique de la logique normalBorderX / devBorderX de drawSwitchBlock.
     *
     * @par Topologie
     * @code
     *   A [crX-1, Y_A]  ───╲───╱──  C [crX+1, Y_C]   (voie 1)
     *                       ╲ ╱
     *                       ╱ ╲
     *   B [crX-1, Y_B]  ───╱───╲──  D [crX+1, Y_D]   (voie 2)
     * @endcode
     *
     * Chaque voie : stub horizontal en entrée/sortie + segment principal
     * (diagonal ou horizontal selon les Y relatifs des ports gauche et droit).
     * Les Y des ports sont lus depuis les positions BFS des bras — aucun
     * hardcoding.
     *
     * @param hdc    Contexte de dessin GDI.
     * @param proj   Projection courante.
     * @param cr     Nœud crossing (StraightCrossBlock).
     * @param logger Logger.
     */
    static void drawFlatCrossingBlock(HDC hdc, const Projection& proj,
                                      const PCCCrossingNode* cr, Logger& logger);

    /**
     * @brief Dessine un croisement TJD (SwitchCrossBlock) — symbole ✕ avec
     *        coloration par voie active.
     *
     * Les 4 bras sont des PCCSwitchNode déjà dessinés par drawSwitchBlock().
     * Cette fonction dessine uniquement les deux diagonales du ✕ central.
     *
     * @par Topologie après fixCrossingLayout TJD
     * @code    
     *                            ──── D[crX+1, crY+1]
     *                          /
     *   A[crX, crY]  ──────────────── C[crX+1, crY]
     *                       ╱  
     * B[crX, crY-1]────────       
     * @endcode
     *
     * Couleur de chaque voie : stateToColor() si la voie est active,
     * branchOff sinon (isPath1Active() / isPath2Active()).
     *
     * @param hdc    Contexte de dessin GDI.
     * @param proj   Projection courante.
     * @param cr     Nœud crossing (SwitchCrossBlock / TJD).
     * @param logger Logger.
     */
    static void drawTJDCrossingBlock(HDC hdc, const Projection& proj,
                                     const PCCCrossingNode* cr, Logger& logger);

    /**
     * @brief Calcule la direction verticale écran de la branche déviée.
     *
     * Retourne -1 si la déviation remonte à l'écran (Y écran diminue),
     * +1 si elle descend. Déduit le sens depuis la position logique Y de
     * la cible si elle diffère de celle du switch, sinon depuis deviationSide.
     * L'inversion (-1/+1) est due au repère écran (Y croît vers le bas).
     *
     * @param sw      Nœud switch courant.
     * @param devEdge Arête de déviation (peut être null).
     * @return Direction verticale écran : -1 (remonte) ou +1 (descend).
     */
    static int computeDevScreenDir(const PCCSwitchNode* sw,
                                   const PCCEdge* devEdge);

    /**
     * @brief Retourne la couleur GDI correspondant à un état ShuntingState.
     * @param state  État opérationnel du bloc.
     * @return COLORREF associé.
     */
    static COLORREF stateToColor(ShuntingState state);
};

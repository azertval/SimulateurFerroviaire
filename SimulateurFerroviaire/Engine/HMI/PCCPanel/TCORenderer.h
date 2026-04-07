/**
 * @file  TCORenderer.h
 * @brief Renderer GDI du schéma TCO — style SNCF, blocs fixes.
 *
 * @par Rendu par bloc fixe
 * Chaque nœud occupe une cellule de même largeur, séparées par un gap.
 *  - Straight : trait horizontal plein.
 *  - Switch   : root + branche active (couleur état voie) + branche
 *               inactive (gris foncé, raccourcie côté jonction).
 *
 * @par Optimisations
 *  - static_cast dans drawNodes (getNodeType() garantit le type).
 *  - PenScope RAII dans le namespace anonyme du .cpp — un seul pen par groupe.
 *  - Projection étendue avec stub/inactiveGap/halfGap précalculés.
 *  - computeProjection() exposée publiquement pour le cache de PCCPanel.
 *  - draw() reçoit une Projection précalculée depuis PCCPanel.
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
     * précalculés dans computeProjection() — évite N recalculs par paint.
     */
    struct Projection
    {
        int minX = 0;
        int maxX = 0;
        int minY = 0;
        int maxY = 0;
        int width = 0;
        int height = 0;
        int cellWidth = 1;
        int cellHeight = 1;
        int marginX = 0;
        int centerY = 0;

        // --- Constantes dérivées de cellWidth — précalculées une fois ---

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
     * n'est plus appelé en interne.
     *
     * @param hdc            Contexte de dessin GDI.
     * @param rect           Rectangle client (pour FillRect uniquement).
     * @param graph          Graphe PCC.
     * @param proj           Projection précalculée par PCCPanel.
     * @param logger         Logger.
     * @param fillBackground Si false, le remplissage du fond est ignoré.
     *                       Mettre à false quand PCCPanel remplit le fond
     *                       avant d'appliquer une world transform (zoom/pan),
     *                       afin d'éviter de recouvrir la zone avec la transform active.
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
     * @param hdc   Contexte de dessin.
     * @param proj  Projection courante.
     * @param node  Nœud straight à dessiner.
     */
    static void drawStraightBlock(HDC hdc, const Projection& proj,
        const PCCNode* node, Logger& logger);

    /**
     * @brief Dessine un nœud SwitchBlock (root + normal + déviation).
     *
     * Utilise static_cast (type garanti par getNodeType() == SWITCH).
     * Instancie un PenScope par branche — aucun CreatePen redondant.
     *
     * @param hdc  Contexte de dessin.
     * @param proj Projection courante (stub/inactiveGap/halfGap précalculés).
     * @param sw   Nœud switch à dessiner.
     */
    static void drawSwitchBlock(HDC hdc, const Projection& proj,
        const PCCSwitchNode* sw, Logger& logger);

    /**
     * @brief Dessine un nœud CrossBlock (croisement plat ou TJD).
     *
     * StraightCrossBlock : symbole ✕ (deux diagonales).
     * SwitchCrossBlock   : symbole ⊠ (X encadré) avec coloration des voies
     *                      actives (isPath1Active / isPath2Active).
     *
     * @param hdc   Contexte de dessin GDI.
     * @param proj  Projection courante.
     * @param cr    Nœud crossing à dessiner.
     * @param logger Logger.
     */
    static void drawCrossingBlock(HDC hdc, const Projection& proj,
        const PCCCrossingNode* cr, Logger& logger);

    /**
     * @brief Retourne la couleur GDI correspondant à un état ShuntingState.
     * @param state  État opérationnel du bloc.
     * @return COLORREF associé.
     */
    static COLORREF stateToColor(ShuntingState state);
};
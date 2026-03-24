/**
 * @file  TCORenderer.h
 * @brief Renderer GDI du schéma TCO — style SNCF, blocs fixes.
 *
 * @par Rendu par bloc fixe
 * Chaque nœud occupe une cellule de même largeur, séparées par un gap.
 *  - Straight : trait horizontal plein.
 *  - Switch   : root + branche active (couleur état voie) + branche
 *               inactive (gris foncé, raccourcie côté jonction).
 */
#pragma once

#include "framework.h"
#include "Modules/PCC/PCCGraph.h"
#include "Modules/PCC/PCCSwitchNode.h"
#include "Engine/Core/Logger/Logger.h"

class TCORenderer
{
public:

    static void draw(HDC hdc, const RECT& rect,
        const PCCGraph& graph, Logger& logger);

    TCORenderer() = delete;

private:

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
    };

    static Projection computeProjection(const RECT& rect,
        const PCCGraph& graph, Logger& logger);

    static POINT project(int logicalX, int logicalY, const Projection& proj);

    static void drawNodes(HDC hdc, const Projection& proj,
        const PCCGraph& graph, Logger& logger);

    static void drawStraightBlock(HDC hdc, const Projection& proj,
        const PCCNode* node);

    static void drawSwitchBlock(HDC hdc, const Projection& proj,
        const PCCSwitchNode* sw);

    static COLORREF stateToColor(ShuntingState state);

    static void drawLine(HDC hdc, POINT from, POINT to,
        COLORREF color, int lineWidth = 3);
};
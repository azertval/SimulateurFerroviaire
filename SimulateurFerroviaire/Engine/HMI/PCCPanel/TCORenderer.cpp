/**
 * @file  TCORenderer.cpp
 * @brief Implémentation du renderer GDI TCO depuis PCCGraph.
 *
 * @see TCORenderer
 */
#include "framework.h"
#include "TCORenderer.h"

#include "Modules/PCC/PCCStraightNode.h"
#include "Modules/PCC/PCCSwitchNode.h"
#include "Modules/InteractiveElements/ShuntingElements/SwitchBlock.h"

#include <algorithm>
#include <limits>


 // =============================================================================
 // Constantes locales
 // =============================================================================

namespace
{
    struct TCOColors
    {
        COLORREF background = RGB(0, 0, 0);
        COLORREF free = RGB(220, 220, 220);
        COLORREF occupied = RGB(220, 50, 50);
        COLORREF inactive = RGB(80, 80, 80);
        COLORREF normal = RGB(0, 200, 80);
        COLORREF deviation = RGB(220, 200, 0);
    };

    const TCOColors COLORS;

    constexpr int LINE_WIDTH_EDGE = 2;
    constexpr int DOT_RADIUS_SWITCH = 5;
    constexpr int DOT_RADIUS_STRAIGHT = 3;
    constexpr int MARGIN_PX = 40;
}


// =============================================================================
// Point d'entrée
// =============================================================================

void TCORenderer::draw(HDC hdc, const RECT& rect,
    const PCCGraph& graph, Logger& logger)
{
    HBRUSH bgBrush = CreateSolidBrush(COLORS.background);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    if (graph.isEmpty())
    {
        LOG_DEBUG(logger, "Graphe vide, fond seul dessiné.");
        return;
    }

    const Projection proj = computeProjection(rect, graph, logger);

    drawEdges(hdc, proj, graph, logger);
    // ^ Arêtes en premier — les disques de nœuds passent par-dessus

    drawNodes(hdc, proj, graph, logger);

    LOG_DEBUG(logger, std::to_string(graph.nodeCount()) + " nœuds, "
        + std::to_string(graph.edgeCount()) + " arêtes dessinés.");
}


// =============================================================================
// Projection
// =============================================================================

TCORenderer::Projection TCORenderer::computeProjection(
    const RECT& rect, const PCCGraph& graph, Logger& logger)
{
    int minX = INT_MAX, maxX = INT_MIN;
    int minY = INT_MAX, maxY = INT_MIN;

    for (const auto& nodePtr : graph.getNodes())
    {
        const PCCPosition& pos = nodePtr->getPosition();
        minX = std::min(minX, pos.x);  maxX = std::max(maxX, pos.x);
        minY = std::min(minY, pos.y);  maxY = std::max(maxY, pos.y);
    }

    Projection proj;
    proj.minX = minX;
    proj.maxX = maxX;
    proj.minY = minY;
    proj.maxY = maxY;
    proj.width = rect.right - rect.left;
    proj.height = rect.bottom - rect.top;

    const int rangeX = std::max(1, maxX - minX);
    const int rangeY = std::max(3, maxY - minY + 1);
    // ^ Min 3 rangs — évite les cellules géantes sur réseau sans déviation

    proj.cellWidth = (proj.width - 2 * MARGIN_PX) / rangeX;
    proj.cellHeight = (proj.height - 2 * MARGIN_PX) / rangeY;
    proj.marginX = MARGIN_PX;
    proj.centerY = proj.height / 2;
    // ^ centerY : le backbone (y=0) passe par le centre vertical de la zone

    LOG_DEBUG(logger, "Projection — cellule "
        + std::to_string(proj.cellWidth) + "x"
        + std::to_string(proj.cellHeight) + "px.");

    return proj;
}

POINT TCORenderer::project(int logicalX, int logicalY, const Projection& proj)
{
    POINT pt;
    pt.x = proj.marginX + (logicalX - proj.minX) * proj.cellWidth;
    pt.y = proj.centerY - logicalY * proj.cellHeight;
    // Y inversé : y positif (déviation) monte sur l'écran
    return pt;
}


// =============================================================================
// Dessin des arêtes
// =============================================================================

void TCORenderer::drawEdges(HDC hdc, const Projection& proj,
    const PCCGraph& graph, Logger& logger)
{
    for (const auto& edgePtr : graph.getEdges())
    {
        const PCCEdge* edge = edgePtr.get();
        const PCCPosition& fromPos = edge->getFrom()->getPosition();
        const PCCPosition& toPos = edge->getTo()->getPosition();

        const POINT pFrom = project(fromPos.x, fromPos.y, proj);
        const POINT pTo = project(toPos.x, toPos.y, proj);
        const COLORREF color = resolveEdgeColor(edge);

        drawLine(hdc, pFrom, pTo, color, LINE_WIDTH_EDGE);
    }
}


// =============================================================================
// Dessin des nœuds
// =============================================================================

void TCORenderer::drawNodes(HDC hdc, const Projection& proj,
    const PCCGraph& graph, Logger& logger)
{
    for (const auto& nodePtr : graph.getNodes())
    {
        const PCCNode* node = nodePtr.get();
        const PCCPosition& pos = node->getPosition();
        const POINT        pt = project(pos.x, pos.y, proj);
        const COLORREF     color = stateToColor(node->getSource()->getState());

        if (node->getNodeType() == PCCNodeType::SWITCH)
            drawDot(hdc, pt, DOT_RADIUS_SWITCH, color);
        else
            drawDot(hdc, pt, DOT_RADIUS_STRAIGHT, color);
    }
}


// =============================================================================
// Résolution couleur d'une arête
// =============================================================================

COLORREF TCORenderer::resolveEdgeColor(const PCCEdge* edge)
{
    PCCNode* from = edge->getFrom();

    if (edge->getRole() == PCCEdgeRole::STRAIGHT)
        return stateToColor(from->getSource()->getState());

    // Arête switch — teste la branche active
    if (auto* sw = dynamic_cast<PCCSwitchNode*>(from))
    {
        const ActiveBranch active = sw->getSwitchSource()->getActiveBranch();

        if (edge->getRole() == PCCEdgeRole::NORMAL
            && active == ActiveBranch::NORMAL)
            return COLORS.normal;

        if (edge->getRole() == PCCEdgeRole::DEVIATION
            && active == ActiveBranch::DEVIATION)
            return COLORS.deviation;
    }

    return stateToColor(from->getSource()->getState());
}


// =============================================================================
// Helpers GDI
// =============================================================================

COLORREF TCORenderer::stateToColor(ShuntingState state)
{
    switch (state)
    {
    case ShuntingState::OCCUPIED: return COLORS.occupied;
    case ShuntingState::INACTIVE: return COLORS.inactive;
    case ShuntingState::FREE:
    default:                      return COLORS.free;
    }
}

void TCORenderer::drawLine(HDC hdc, POINT from, POINT to,
    COLORREF color, int lineWidth)
{
    HPEN pen = CreatePen(PS_SOLID, lineWidth, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, from.x, from.y, nullptr);
    LineTo(hdc, to.x, to.y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void TCORenderer::drawDot(HDC hdc, POINT center, int radius, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    HPEN   pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN   oldPen = static_cast<HPEN>  (SelectObject(hdc, pen));

    Ellipse(hdc,
        center.x - radius, center.y - radius,
        center.x + radius, center.y + radius);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

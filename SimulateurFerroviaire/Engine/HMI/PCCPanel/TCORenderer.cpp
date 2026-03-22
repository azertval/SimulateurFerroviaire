/**
 * @file  TCORenderer.cpp
 * @brief Renderer GDI TCO — style SNCF, blocs fixes.
 *
 * Choix validés :
 *  - Gap 6px entre tous les blocs (straights et switches).
 *  - Switch : root + branche active se touchent à la jonction (0 gap).
 *  - Branche inactive : gap côté jonction, va jusqu'au bord de cellule.
 *  - Branche active : couleur d'état de la voie (blanc/rouge/gris).
 *  - Branche inactive : gris foncé (#404040).
 *  - cellHeight plafonné à cellWidth/2 (diagonales ~30°).
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
 // Constantes
 // =============================================================================

namespace
{
    struct TCOColors
    {
        COLORREF background = RGB(0, 0, 0);
        COLORREF free = RGB(220, 220, 220);
        COLORREF occupied = RGB(220, 50, 50);
        COLORREF inactive = RGB(80, 80, 80);
        COLORREF branchOff = RGB(64, 64, 64);   // branche inactive (#404040)
    };

    const TCOColors COLORS;

    constexpr int    LINE_WIDTH_ACTIVE = 3;
    constexpr int    LINE_WIDTH_INACTIVE = 2;
    constexpr int    MARGIN_PX = 40;

    // Gap entre blocs adjacents (en pixels fixes)
    constexpr double BLOCK_GAP_PX = 6.0;

    // Bout droit horizontal en fin de déviation (fraction de cellWidth)
    constexpr double STUB_RATIO = 0.20;

    // Gap de la branche inactive côté jonction (fraction de cellWidth)
    constexpr double INACTIVE_GAP_RATIO = 0.1;
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

    drawNodes(hdc, proj, graph, logger);

    LOG_DEBUG(logger, std::to_string(graph.nodeCount()) + " blocs dessinés.");
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

    proj.cellWidth = (proj.width - 2 * MARGIN_PX) / rangeX;
    proj.cellHeight = (proj.height - 2 * MARGIN_PX) / rangeY;

    // Plafonne cellHeight à cellWidth/2 — diagonales à ~30°
    proj.cellHeight = std::min(proj.cellHeight, proj.cellWidth / 2);

    proj.marginX = MARGIN_PX;

    const double midLogicalY = (minY + maxY) / 2.0;
    proj.centerY = static_cast<int>(proj.height / 2.0 + midLogicalY * proj.cellHeight);

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
    return pt;
}


// =============================================================================
// Dessin de tous les nœuds
// =============================================================================

void TCORenderer::drawNodes(HDC hdc, const Projection& proj,
    const PCCGraph& graph, Logger& logger)
{
    for (const auto& nodePtr : graph.getNodes())
    {
        const PCCNode* node = nodePtr.get();

        if (node->getNodeType() == PCCNodeType::SWITCH)
        {
            if (auto* sw = dynamic_cast<const PCCSwitchNode*>(node))
                drawSwitchBlock(hdc, proj, sw);
        }
        else
        {
            drawStraightBlock(hdc, proj, node);
        }
    }
}


// =============================================================================
// Straight — trait horizontal avec gap aux deux bords
//
//    |←————————— cellWidth ——————————→|
//    gap ══════════════════════════ gap
//
// =============================================================================

void TCORenderer::drawStraightBlock(HDC hdc, const Projection& proj,
    const PCCNode* node)
{
    const int halfCell = proj.cellWidth / 2;
    const int halfGap = static_cast<int>(BLOCK_GAP_PX / 2.0);

    const POINT center = project(
        node->getPosition().x, node->getPosition().y, proj);

    const COLORREF color = stateToColor(node->getSource()->getState());

    // Bornes par défaut : 1 cellule fixe
    int leftX = center.x - halfCell;
    int rightX = center.x + halfCell;

    // Ajuster les bornes vers le mi-chemin de chaque voisin sur le même Y.
    // Permet de couvrir correctement les cas où un voisin est à 2+ cellules
    // (ex. straight entre deux switches séparés par un double).
    for (const PCCEdge* edge : node->getEdges())
    {
        const PCCNode* nb = (edge->getFrom() == node)
            ? edge->getTo() : edge->getFrom();
        if (!nb || nb->getPosition().y != node->getPosition().y)
            continue;

        const POINT nbPt = project(nb->getPosition().x, nb->getPosition().y, proj);
        const int mid = (center.x + nbPt.x) / 2;

        if (nbPt.x < center.x)
            leftX = std::min(leftX, mid);
        else if (nbPt.x > center.x)
            rightX = std::max(rightX, mid);
    }

    const POINT left = { leftX + halfGap, center.y };
    const POINT right = { rightX - halfGap, center.y };
    drawLine(hdc, left, right, color, LINE_WIDTH_ACTIVE);
}


// =============================================================================
// Switch — symbole d'aiguillage
//
//    Normal actif :
//    |←————————— cellWidth ——————————→|
//                          __________
//                   ╱  (stub)   |gap
//                gap    
//    gap ══════════════════════════ gap
//     (root)    ↑     (active)
//            jonction
//
//    Deviation actif :
//    |←————————— cellWidth ——————————→|
//                          __________
//                       ╱  (stub)    |gap
//    gap ═══════ gap________________ gap
//     (root)         (inactive)
//
// =============================================================================

void TCORenderer::drawSwitchBlock(HDC hdc, const Projection& proj,
    const PCCSwitchNode* sw)
{
    const int halfCell = proj.cellWidth / 2;
    const int halfGap = static_cast<int>(BLOCK_GAP_PX / 2.0);
    const int STUB = std::max(4, static_cast<int>(proj.cellWidth * STUB_RATIO));
    const int INACTIVE_GAP = std::max(4, static_cast<int>(proj.cellWidth * INACTIVE_GAP_RATIO));

    const POINT center = project(
        sw->getPosition().x, sw->getPosition().y, proj);

    // -----------------------------------------------------------------
    // Couleurs
    // -----------------------------------------------------------------
    const COLORREF stateColor = stateToColor(sw->getSource()->getState());
    const ActiveBranch active = sw->getSwitchSource()->getActiveBranch();
    const bool normalIsActive = (active == ActiveBranch::NORMAL);

    // -----------------------------------------------------------------
    // Helper : calcule le X du bord de la branche (mi-chemin vers la cible)
    // -----------------------------------------------------------------
    auto edgeXToward = [&](const PCCEdge* edge) -> int
        {
            if (!edge || !edge->getTo()) return center.x;
            const POINT tgt = project(
                edge->getTo()->getPosition().x,
                edge->getTo()->getPosition().y, proj);
            return (center.x + tgt.x) / 2;
        };

    // -----------------------------------------------------------------
    // 1. Root : bord → jonction
    // -----------------------------------------------------------------
    const PCCEdge* rootEdge = sw->getRootEdge();
    {
        const int rootBorderX = edgeXToward(rootEdge);
        const int dirFromRoot = (rootBorderX < center.x) ? 1 : -1;

        const POINT pA = { rootBorderX + dirFromRoot * halfGap, center.y };
        const POINT pB = { center.x, center.y };
        drawLine(hdc, pA, pB, stateColor, LINE_WIDTH_ACTIVE);
    }

    // -----------------------------------------------------------------
    // 2. Normal : jonction → bord (ou jonction+gap → bord si inactive)
    // -----------------------------------------------------------------
    const PCCEdge* normalEdge = sw->getNormalEdge();
    {
        const int normalBorderX = edgeXToward(normalEdge);
        const int dirToNormal = (normalBorderX > center.x) ? 1 : -1;

        const int startX = normalIsActive
            ? center.x
            : center.x + dirToNormal * INACTIVE_GAP;

        const POINT pA = { startX, center.y };
        const POINT pB = { normalBorderX - dirToNormal * halfGap, center.y };

        drawLine(hdc, pA, pB,
            normalIsActive ? stateColor : COLORS.branchOff,
            normalIsActive ? LINE_WIDTH_ACTIVE : LINE_WIDTH_INACTIVE);
    }

    // -----------------------------------------------------------------
    // 3. Deviation
    // -----------------------------------------------------------------
    const PCCEdge* devEdge = sw->getDeviationEdge();
    {
        const COLORREF devColor = normalIsActive ? COLORS.branchOff : stateColor;
        const int      devWidth = normalIsActive ? LINE_WIDTH_INACTIVE : LINE_WIDTH_ACTIVE;

        const bool isDouble = devEdge
            && devEdge->getTo()
            && devEdge->getTo()->getNodeType() == PCCNodeType::SWITCH;

        // Direction vers la cible déviation
        int devBorderX = edgeXToward(devEdge);
        int dirToDev = (devBorderX > center.x) ? 1 : -1;

        // Point de départ
        const int devScreenDir = [&]() -> int {
            if (devEdge && devEdge->getTo()) {
                const int tgtLogY = devEdge->getTo()->getPosition().y;
                if (tgtLogY != sw->getPosition().y)
                    return (tgtLogY > sw->getPosition().y) ? -1 : 1; // Y écran inversé
            }
            return -sw->getDeviationSide(); // getDeviationSide() est en logique, on inverse
            }();
        const int startY = normalIsActive 
            ? center.y + devScreenDir * INACTIVE_GAP 
            : center.y;

        const POINT pStart = {center.x,startY};

        if (isDouble)
        {
            // Double : diagonale vers le point milieu avec le partenaire
            const PCCNode* partner = devEdge->getTo();
            const POINT partnerCenter = project(
                partner->getPosition().x, partner->getPosition().y, proj);

            const POINT midPt = {
                (center.x + partnerCenter.x) / 2,
                (center.y + partnerCenter.y) / 2
            };
            drawLine(hdc, pStart, midPt, devColor, devWidth);
        }
        else
        {
            // Y de la déviation
            int devY = center.y - proj.cellHeight;
            if (devEdge && devEdge->getTo())
            {
                const int tgtLogY = devEdge->getTo()->getPosition().y;
                if (tgtLogY != sw->getPosition().y)
                    devY = project(0, tgtLogY, proj).y;
                else
                    devY = center.y - sw->getDeviationSide() * proj.cellHeight;
            }

            const int endX = devBorderX - dirToDev * halfGap;

            // Standard : diagonale + bout droit
            const POINT pStubBeg = { endX - dirToDev * STUB, devY };
            const POINT pStubEnd = { endX,                     devY };

            drawLine(hdc, pStart, pStubBeg, devColor, devWidth);
            drawLine(hdc, pStubBeg, pStubEnd, devColor, devWidth);
        }
    }
}


// =============================================================================
// Helpers
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
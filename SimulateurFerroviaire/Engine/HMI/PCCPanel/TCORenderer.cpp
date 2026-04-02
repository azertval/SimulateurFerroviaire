/**
 * @file  TCORenderer.cpp
 * @brief Renderer GDI TCO — style SNCF, blocs fixes.
 *
 * Optimisations v2 :
 *  - static_cast dans drawNodes (Famille C) — RTTI supprimé.
 *  - PenScope RAII (Famille D) — un seul pen GDI par branche.
 *  - stub/inactiveGap/halfGap précalculés dans Projection (Famille E).
 *  - computeProjection() exposée publiquement pour le cache PCCPanel (Famille F).
 *  - draw() reçoit une Projection précalculée — computeProjection() non rappelé.
 *
 * @see TCORenderer
 */
#include "framework.h"
#include "TCORenderer.h"

#include "Modules/PCC/PCCStraightNode.h"
#include "Modules/PCC/PCCSwitchNode.h"
#include "Modules/Elements/ShuntingElements/SwitchBlock.h"

#include <algorithm>
#include <limits>


 // =============================================================================
 // Constantes et helpers locaux
 // =============================================================================

namespace
{
    struct TCOColors
    {
        COLORREF background = RGB(0, 0, 0);
        COLORREF free = RGB(220, 220, 220);
        COLORREF occupied = RGB(220, 50, 50);
        COLORREF inactive = RGB(80, 80, 80);
        COLORREF branchOff = RGB(64, 64, 64);  // branche inactive #404040
    };

    const TCOColors COLORS;

    constexpr int    LINE_WIDTH_ACTIVE = 3;
    constexpr int    LINE_WIDTH_INACTIVE = 2;
    constexpr int    MARGIN_PX = 40;
    constexpr double BLOCK_GAP_PX = 6.0;
    constexpr double STUB_RATIO = 0.20;
    constexpr double INACTIVE_GAP_RATIO = 0.10;

    // =========================================================================
    // PenScope — wrapper RAII autour d'un HPEN GDI (Famille D)
    //
    // Crée et installe le pen dans le constructeur, le restaure et le libère
    // dans le destructeur. Garantit l'absence de fuite GDI.
    // Non copiable — un handle GDI appartient à un seul scope.
    // =========================================================================
    struct PenScope
    {
        /**
         * @brief Crée un pen PS_SOLID et l'installe sur le HDC.
         * @param hdc    Contexte de dessin cible.
         * @param color  Couleur du trait (COLORREF).
         * @param width  Épaisseur du trait en pixels.
         */
        PenScope(HDC hdc, COLORREF color, int width)
            : m_hdc(hdc)
            , m_pen(CreatePen(PS_SOLID, width, color))
            , m_old(static_cast<HPEN>(SelectObject(hdc, m_pen)))
        {
        }

        /** @brief Restaure l'ancien pen et libère le pen créé (RAII). */
        ~PenScope()
        {
            SelectObject(m_hdc, m_old);
            DeleteObject(m_pen);
        }

        /** @brief Déplace le curseur GDI sans tracer. */
        void moveTo(POINT p) const { MoveToEx(m_hdc, p.x, p.y, nullptr); }

        /** @brief Trace une ligne depuis la position courante jusqu'à p. */
        void lineTo(POINT p) const { LineTo(m_hdc, p.x, p.y); }

        PenScope(const PenScope&) = delete;
        PenScope& operator=(const PenScope&) = delete;

    private:
        HDC   m_hdc;
        HPEN  m_pen;
        HPEN  m_old;
    };

} // namespace


// =============================================================================
// Point d'entrée (Famille F — reçoit la projection depuis PCCPanel)
// =============================================================================

void TCORenderer::draw(HDC hdc, const RECT& rect,
    const PCCGraph& graph,
    const Projection& proj,
    Logger& logger)
{
    HBRUSH bgBrush = CreateSolidBrush(COLORS.background);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    if (graph.isEmpty())
    {
        LOG_DEBUG(logger, "Graphe vide, fond seul dessiné.");
        return;
    }

    // proj vient du cache PCCPanel — computeProjection() non rappelé ici.
    drawNodes(hdc, proj, graph, logger);

    LOG_DEBUG(logger, std::to_string(graph.nodeCount()) + " blocs dessinés.");
}


// =============================================================================
// Projection (Famille E — stub/inactiveGap/halfGap précalculés)
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

    // --- Constantes dérivées de cellWidth (Famille E) ---
    // Précalculées une fois ici — évite N recalculs identiques dans drawSwitchBlock.
    proj.halfGap = static_cast<int>(BLOCK_GAP_PX / 2.0);
    proj.stub = std::max(4, static_cast<int>(proj.cellWidth * STUB_RATIO));
    proj.inactiveGap = std::max(4, static_cast<int>(proj.cellWidth * INACTIVE_GAP_RATIO));

    LOG_DEBUG(logger, "Projection — cellule "
        + std::to_string(proj.cellWidth) + "x"
        + std::to_string(proj.cellHeight) + "px"
        + " | stub=" + std::to_string(proj.stub)
        + " halfGap=" + std::to_string(proj.halfGap));

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
// Dessin de tous les nœuds (Famille C — static_cast)
// =============================================================================

void TCORenderer::drawNodes(HDC hdc, const Projection& proj,
    const PCCGraph& graph, Logger& logger)
{
    for (const auto& nodePtr : graph.getNodes())
    {
        const PCCNode* node = nodePtr.get();

        if (node->getNodeType() == PCCNodeType::SWITCH)
        {
            // static_cast : sûr — getNodeType() == SWITCH garantit PCCSwitchNode.
            // dynamic_cast retiré : vérification RTTI redondante et coûteuse.
            drawSwitchBlock(hdc, proj,
                static_cast<const PCCSwitchNode*>(node));
        }
        else
        {
            drawStraightBlock(hdc, proj, node);
        }
    }
}


// =============================================================================
// Straight — trait horizontal avec gap (Famille D — PenScope)
// =============================================================================

void TCORenderer::drawStraightBlock(HDC hdc, const Projection& proj,
    const PCCNode* node)
{
    const POINT center = project(
        node->getPosition().x, node->getPosition().y, proj);

    const COLORREF color = stateToColor(node->getSource()->getState());

    int leftX = center.x - proj.cellWidth / 2;
    int rightX = center.x + proj.cellWidth / 2;

    // Ajuste les bornes vers le mi-chemin de chaque voisin sur le même Y
    for (const PCCEdge* edge : node->getEdges())
    {
        const PCCNode* nb = (edge->getFrom() == node)
            ? edge->getTo() : edge->getFrom();
        if (!nb || nb->getPosition().y != node->getPosition().y)
            continue;

        const POINT nbPt = project(
            nb->getPosition().x, nb->getPosition().y, proj);
        const int mid = (center.x + nbPt.x) / 2;

        if (nbPt.x < center.x) leftX = std::min(leftX, mid);
        else if (nbPt.x > center.x) rightX = std::max(rightX, mid);
    }

    // PenScope : creation unique pour ce straight — destructeur en fin de bloc
    PenScope pen(hdc, color, LINE_WIDTH_ACTIVE);
    pen.moveTo({ leftX + proj.halfGap, center.y });
    pen.lineTo({ rightX - proj.halfGap, center.y });
}


// =============================================================================
// Switch — symbole d'aiguillage (Famille D + E)
// =============================================================================

void TCORenderer::drawSwitchBlock(HDC hdc, const Projection& proj,
    const PCCSwitchNode* sw)
{
    // Constantes depuis Projection — précalculées dans computeProjection (Famille E)
    const int halfGap = proj.halfGap;
    const int STUB = proj.stub;
    const int INACT_GAP = proj.inactiveGap;

    const POINT center = project(
        sw->getPosition().x, sw->getPosition().y, proj);

    const COLORREF stateColor = stateToColor(sw->getSource()->getState());
    const ActiveBranch active = sw->getSwitchSource()->getActiveBranch();
    const bool normalIsActive = (active == ActiveBranch::NORMAL);

    // Helper : X du bord de branche (mi-chemin vers la cible)
    auto edgeXToward = [&](const PCCEdge* edge) -> int
        {
            if (!edge || !edge->getTo()) return center.x;
            const POINT tgt = project(
                edge->getTo()->getPosition().x,
                edge->getTo()->getPosition().y, proj);
            return (center.x + tgt.x) / 2;
        };

    const PCCEdge* rootEdge = sw->getRootEdge();
    const PCCEdge* normEdge = sw->getNormalEdge();
    const PCCEdge* devEdge = sw->getDeviationEdge();

    const int rootBorderX = edgeXToward(rootEdge);
    const int normalBorderX = edgeXToward(normEdge);
    const int devBorderX = edgeXToward(devEdge);

    // Direction racine → jonction (±1)
    const int dirFromRoot = (rootBorderX < center.x) ? 1 : -1;
    const int dirToNormal = -dirFromRoot;
    const int dirToDev = -dirFromRoot;

    // X de jonction = bord racine + STUB vers la jonction
    const int junctionX = rootBorderX + dirFromRoot * STUB;

    const bool isDouble = sw->getSwitchSource()->isDouble();

    // =========================================================================
    // Branche root (Famille D — PenScope par branche)
    // =========================================================================
    {
        const COLORREF col = normalIsActive ? stateColor : COLORS.branchOff;
        const int      width = normalIsActive ? LINE_WIDTH_ACTIVE : LINE_WIDTH_INACTIVE;
        PenScope pen(hdc, col, width);
        pen.moveTo({ rootBorderX + dirFromRoot * halfGap, center.y });
        pen.lineTo({ junctionX, center.y });
    }

    // =========================================================================
    // Branche normale
    // =========================================================================
    {
        const COLORREF col = normalIsActive ? stateColor : COLORS.branchOff;
        const int      width = normalIsActive ? LINE_WIDTH_ACTIVE : LINE_WIDTH_INACTIVE;
        PenScope pen(hdc, col, width);
        pen.moveTo({ junctionX, center.y });
        pen.lineTo({ normalBorderX - dirToNormal * halfGap, center.y });
    }

    // =========================================================================
    // Branche déviation
    // =========================================================================
    {
        const COLORREF devColor = normalIsActive ? COLORS.branchOff : stateColor;
        const int      devWidth = normalIsActive ? LINE_WIDTH_INACTIVE : LINE_WIDTH_ACTIVE;

        // Direction verticale de la déviation (écran : Y inversé)
        const int devScreenDir = [&]() -> int
            {
                if (devEdge && devEdge->getTo())
                {
                    const int tgtLogY = devEdge->getTo()->getPosition().y;
                    if (tgtLogY != sw->getPosition().y)
                        return (tgtLogY > sw->getPosition().y) ? -1 : 1;
                }
                return -sw->getDeviationSide();
            }();

        // Point de départ Y (branche inactive : léger retrait depuis la jonction)
        const int startY = normalIsActive
            ? center.y + devScreenDir * INACT_GAP
            : center.y;

        const POINT pStart = { junctionX, startY };

        PenScope pen(hdc, devColor, devWidth);

        if (isDouble)
        {
            // Double switch : diagonale vers le milieu avec le partenaire
            const PCCNode* partner = devEdge->getTo();
            const POINT    partCenter = project(
                partner->getPosition().x, partner->getPosition().y, proj);

            const POINT midPt = {
                (center.x + partCenter.x) / 2,
                (center.y + partCenter.y) / 2
            };
            pen.moveTo(pStart);
            pen.lineTo(midPt);
        }
        else
        {
            // Switch simple : diagonale + stub horizontal
            int devY = center.y - proj.cellHeight;
            if (devEdge && devEdge->getTo())
            {
                const int tgtLogY = devEdge->getTo()->getPosition().y;
                devY = (tgtLogY != sw->getPosition().y)
                    ? project(0, tgtLogY, proj).y
                    : center.y - sw->getDeviationSide() * proj.cellHeight;
            }

            const int endX = devBorderX - dirToDev * halfGap;
            const POINT pStubBeg = { endX - dirToDev * STUB, devY };
            const POINT pStubEnd = { endX,                    devY };

            pen.moveTo(pStart);
            pen.lineTo(pStubBeg);
            pen.lineTo(pStubEnd);
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
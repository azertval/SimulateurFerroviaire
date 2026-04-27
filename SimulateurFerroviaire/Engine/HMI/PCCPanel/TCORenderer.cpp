/**
 * @file  TCORenderer.cpp
 * @brief Renderer GDI TCO — style SNCF, blocs fixes.
 *
 * @see TCORenderer
 */
#include "framework.h"
#include "TCORenderer.h"

#include "Modules/PCC/PCCStraightNode.h"
#include "Modules/PCC/PCCSwitchNode.h"
#include "Modules/PCC/PCCCrossingNode.h"
#include "Modules/Elements/ShuntingElements/SwitchBlock.h"
#include "Modules/Elements/ShuntingElements/CrossBlocks/CrossBlock.h"
#include "Modules/Elements/ShuntingElements/CrossBlocks/SwitchCrossBlock.h"

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
        COLORREF free       = RGB(220, 220, 220);
        COLORREF occupied   = RGB(220, 50, 50);
        COLORREF inactive   = RGB(80, 80, 80);
        COLORREF branchOff  = RGB(64, 64, 64);
    };

    const TCOColors COLORS;

    constexpr int    LINE_WIDTH_ACTIVE   = 3;
    constexpr int    LINE_WIDTH_INACTIVE = 2;
    constexpr int    MARGIN_PX           = 40;
    constexpr double BLOCK_GAP_PX        = 6.0;
    constexpr double STUB_RATIO          = 0.20;
    constexpr double INACTIVE_GAP_RATIO  = 0.10;

    // =========================================================================
    // PenScope — wrapper RAII autour d'un HPEN GDI.
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

        PenScope(const PenScope&)            = delete;
        PenScope& operator=(const PenScope&) = delete;

    private:
        HDC   m_hdc;
        HPEN  m_pen;
        HPEN  m_old;
    };

} // namespace


// =============================================================================
// Point d'entrée
// =============================================================================

void TCORenderer::draw(HDC hdc, const RECT& rect,
    const PCCGraph& graph,
    const Projection& proj,
    Logger& logger,
    bool fillBackground)
{
    // Remplissage du fond ignoré si fillBackground == false.
    // PCCPanel met fillBackground = false quand il remplit le fond lui-même
    // avant d'activer la world transform (zoom / pan) — évite de recouvrir
    // la zone noire après transformation.
    if (fillBackground)
    {
        HBRUSH bgBrush = CreateSolidBrush(COLORS.background);
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
    }

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
// Projection — calcul + helper de projection d'un point logique
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
    proj.minX   = minX;
    proj.maxX   = maxX;
    proj.minY   = minY;
    proj.maxY   = maxY;
    proj.width  = rect.right  - rect.left;
    proj.height = rect.bottom - rect.top;

    const int rangeX = std::max(1, maxX - minX);
    const int rangeY = std::max(3, maxY - minY + 1);

    proj.cellWidth  = (proj.width  - 2 * MARGIN_PX) / rangeX;
    proj.cellHeight = (proj.height - 2 * MARGIN_PX) / rangeY;

    // Plafonne cellHeight à cellWidth/2 — diagonales à ~30°.
    proj.cellHeight = std::min(proj.cellHeight, proj.cellWidth / 2);

    proj.marginX = MARGIN_PX;

    const double midLogicalY = (minY + maxY) / 2.0;
    proj.centerY = static_cast<int>(proj.height / 2.0 + midLogicalY * proj.cellHeight);

    // Constantes dérivées de cellWidth — précalculées une fois ici pour éviter
    // N recalculs identiques dans drawSwitchBlock / drawCrossingBlock.
    proj.halfGap    = static_cast<int>(BLOCK_GAP_PX / 2.0);
    proj.stub       = std::max(4, static_cast<int>(proj.cellWidth * STUB_RATIO));
    proj.inactiveGap = std::max(4, static_cast<int>(proj.cellWidth * INACTIVE_GAP_RATIO));

    LOG_DEBUG(logger, "Projection — cellule "
        + std::to_string(proj.cellWidth)  + "x"
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
// Dessin de tous les nœuds
// =============================================================================

void TCORenderer::drawNodes(HDC hdc, const Projection& proj,
    const PCCGraph& graph, Logger& logger)
{
    int straightCount = 0;
    int switchCount   = 0;
    int crossingCount = 0;

    for (const auto& nodePtr : graph.getNodes())
    {
        const PCCNode* node = nodePtr.get();

        if (node->getNodeType() == PCCNodeType::SWITCH)
        {
            // static_cast sûr : getNodeType() == SWITCH garantit PCCSwitchNode.
            const auto* sw = static_cast<const PCCSwitchNode*>(node);

            auto edgeDest = [](const PCCEdge* e) -> std::string {
                return e && e->getTo() ? e->getTo()->getSourceId() : "null";
            };

            LOG_DEBUG(logger, "drawSwitch " + node->getSourceId()
                + " pos=[" + std::to_string(node->getPosition().x)
                + ","      + std::to_string(node->getPosition().y) + "]"
                + " isDoubleOnDev=" + (sw->getSwitchSource()->getDoubleOnDeviation().has_value() ? "true" : "false")
                + " root="   + edgeDest(sw->getRootEdge())
                + " normal=" + edgeDest(sw->getNormalEdge())
                + " dev="    + edgeDest(sw->getDeviationEdge()));

            drawSwitchBlock(hdc, proj, sw, logger);
            ++switchCount;
        }
        else if (node->getNodeType() == PCCNodeType::CROSSING)
        {
            // static_cast sûr : getNodeType() == CROSSING garantit PCCCrossingNode.
            const auto* cr = static_cast<const PCCCrossingNode*>(node);

            LOG_DEBUG(logger, "drawCrossing " + node->getSourceId()
                + " pos=[" + std::to_string(node->getPosition().x)
                + ","      + std::to_string(node->getPosition().y) + "]");

            drawCrossingBlock(hdc, proj, cr, logger);
            ++crossingCount;
        }
        else
        {
            // Straight standard — dessiné normalement.
            // Les bras de crossing (StraightBlock adjacents à un CrossBlock) sont
            // dessinés ici comme des straights normaux : ils remplissent l'espace
            // entre le ✕ central et les switches/straights adjacents. Sans eux,
            // il y aurait un trou visuel entre le bord du crossing et le switch voisin.
            LOG_DEBUG(logger, "drawStraight " + node->getSourceId()
                + " pos=[" + std::to_string(node->getPosition().x)
                + ","      + std::to_string(node->getPosition().y) + "]");

            drawStraightBlock(hdc, proj, node, logger);
            ++straightCount;
        }
    }

    LOG_DEBUG(logger, "drawNodes — "
        + std::to_string(straightCount) + " straight(s), "
        + std::to_string(switchCount)   + " switch(es), "
        + std::to_string(crossingCount) + " crossing(s).");
}


// =============================================================================
// Straight — trait horizontal avec gap
//
// Dessine un StraightBlock comme un trait horizontal centré sur sa position
// logique, s'étendant jusqu'au mi-chemin de chaque voisin.
//
// Cas standard : n'étend que vers les voisins coplanaires (même Y).
//
// Bras de crossing (straight adjacent à un CrossBlock) :
//   Le filtre Y est levé — le bras s'étend vers ses voisins (switch/straight)
//   même si ceux-ci sont à un Y différent. Le nœud CROSSING lui-même est
//   toujours ignoré : le crossing gère ses propres frontières.
//   Cela permet aux bras de déviation (Y≠0) de faire la jonction entre le
//   ✕ central et le switch adjacent au Y principal (Y=0).
// =============================================================================

void TCORenderer::drawStraightBlock(HDC hdc, const Projection& proj,
    const PCCNode* node, Logger& logger)
{
    const POINT    center = project(node->getPosition().x, node->getPosition().y, proj);
    const COLORREF color  = stateToColor(node->getSource()->getState());

    int leftX  = center.x - proj.cellWidth / 2;
    int rightX = center.x + proj.cellWidth / 2;

    // Détermine si ce straight est un bras de crossing (voisin CROSSING présent).
    bool isCrossingArm = false;
    for (const PCCEdge* edge : node->getEdges())
    {
        const PCCNode* nb = (edge->getFrom() == node)
            ? edge->getTo() : edge->getFrom();
        if (nb && nb->getNodeType() == PCCNodeType::CROSSING)
        {
            isCrossingArm = true;
            break;
        }
    }

    for (const PCCEdge* edge : node->getEdges())
    {
        const PCCNode* nb = (edge->getFrom() == node)
            ? edge->getTo() : edge->getFrom();
        if (!nb) continue;

        // Le nœud CROSSING gère ses propres frontières — jamais pris en compte.
        if (nb->getNodeType() == PCCNodeType::CROSSING) continue;

        // Pour un straight standard, filtre Y : n'étend que vers les voisins coplanaires.
        // Pour un bras de crossing, le filtre est levé : le bras doit s'étendre
        // vers son switch/straight voisin même si celui-ci est à un Y différent.
        if (!isCrossingArm && nb->getPosition().y != node->getPosition().y)
            continue;

        const POINT nbPt = project(nb->getPosition().x, nb->getPosition().y, proj);
        const int   mid  = (center.x + nbPt.x) / 2;

        if      (nbPt.x < center.x) leftX  = std::min(leftX,  mid);
        else if (nbPt.x > center.x) rightX = std::max(rightX, mid);
    }

    PenScope pen(hdc, color, LINE_WIDTH_ACTIVE);
    pen.moveTo({ leftX  + proj.halfGap, center.y });
    pen.lineTo({ rightX - proj.halfGap, center.y });
}


// =============================================================================
// Switch — symbole d'aiguillage
// =============================================================================

int TCORenderer::computeDevScreenDir(const PCCSwitchNode* sw,
    const PCCEdge* devEdge)
{
    // Si la cible de déviation est à un Y différent du switch, on lit le sens
    // directement depuis les positions BFS — garantit la cohérence avec le layout.
    // Sinon, on se rabat sur deviationSide (coordonnées géographiques).
    // L'inversion (-1/+1) est due au repère écran (Y croît vers le bas).
    if (devEdge && devEdge->getTo())
    {
        const int tgtLogY = devEdge->getTo()->getPosition().y;
        if (tgtLogY != sw->getPosition().y)
            return (tgtLogY > sw->getPosition().y) ? -1 : 1;
    }
    return -sw->getDeviationSide();
}

void TCORenderer::drawSwitchBlock(HDC hdc, const Projection& proj,
    const PCCSwitchNode* sw, Logger& logger)
{
    const int halfGap  = proj.halfGap;
    const int STUB     = proj.stub;
    const int INACT_GAP = proj.inactiveGap;

    const POINT center     = project(sw->getPosition().x, sw->getPosition().y, proj);
    const COLORREF stateColor = stateToColor(sw->getSource()->getState());
    const ActiveBranch active = sw->getSwitchSource()->getActiveBranch();
    const bool normalIsActive = (active == ActiveBranch::NORMAL);

    // Helper : X du bord de branche (mi-chemin vers la cible de l'arête).
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
    const PCCEdge* devEdge  = sw->getDeviationEdge();

    const int rootBorderX   = edgeXToward(rootEdge);
    const int normalBorderX = edgeXToward(normEdge);
    const int devBorderX    = edgeXToward(devEdge);

    // Direction racine → jonction (±1) et son inverse jonction → branches aval.
    const int dirFromRoot = (rootBorderX < center.x) ? 1 : -1;
    const int dirToNormal = -dirFromRoot;

    // X de jonction = bord racine + STUB vers la jonction.
    const int junctionX = rootBorderX + dirFromRoot * STUB;

    // Vérifie si la cible de la branche déviée est un switch (double liaison sw↔sw).
    const bool devTargetIsSwitch = devEdge
        && devEdge->getTo()
        && devEdge->getTo()->getNodeType() == PCCNodeType::SWITCH;

    LOG_DEBUG(logger, sw->getSourceId()
        + " junctionX=" + std::to_string(junctionX)
        + " root="   + (rootEdge ? std::to_string(rootBorderX)   : "no-edge")
        + " normal=" + (normEdge ? std::to_string(normalBorderX) : "no-edge")
        + " dev="    + (devEdge  ? std::to_string(devBorderX)    : "no-edge")
        + " devTargetIsSwitch=" + (devTargetIsSwitch ? "true" : "false"));

    // -------------------------------------------------------------------------
    // Branche root — toujours active (le train passe toujours par la racine,
    // quelle que soit la branche sélectionnée).
    // -------------------------------------------------------------------------
    {
        PenScope pen(hdc, stateColor, LINE_WIDTH_ACTIVE);
        pen.moveTo({ rootBorderX + dirFromRoot * halfGap, center.y });
        pen.lineTo({ junctionX, center.y });
    }

    // -------------------------------------------------------------------------
    // Branche normale
    // -------------------------------------------------------------------------
    {
        const COLORREF col   = normalIsActive ? stateColor    : COLORS.branchOff;
        const int      width = normalIsActive ? LINE_WIDTH_ACTIVE : LINE_WIDTH_INACTIVE;
        PenScope pen(hdc, col, width);

        const bool normalTargetIsSwitch = normEdge
            && normEdge->getTo()
            && normEdge->getTo()->getNodeType() == PCCNodeType::SWITCH;

        if (normalTargetIsSwitch)
        {
            // Double liaison sw↔sw via NORMAL : demi-diagonale vers le mid-point.
            const PCCNode* partner    = normEdge->getTo();
            const POINT    partCenter = project(
                partner->getPosition().x, partner->getPosition().y, proj);
            const POINT midPt = {
                (center.x + partCenter.x) / 2,
                (center.y + partCenter.y) / 2
            };
            pen.moveTo({ junctionX, center.y });
            pen.lineTo(midPt);
        }
        else
        {
            // Cas standard : branche normale horizontale.
            pen.moveTo({ junctionX, center.y });
            pen.lineTo({ normalBorderX + dirToNormal * halfGap, center.y });
        }
    }

    // -------------------------------------------------------------------------
    // Branche déviation
    // -------------------------------------------------------------------------
    {
        const COLORREF devColor = normalIsActive ? COLORS.branchOff : stateColor;
        const int      devWidth = normalIsActive ? LINE_WIDTH_INACTIVE : LINE_WIDTH_ACTIVE;

        const int devScreenDir = computeDevScreenDir(sw, devEdge);

        // Point de départ Y (branche inactive : léger retrait depuis la jonction).
        const int startY  = normalIsActive
            ? center.y + devScreenDir * INACT_GAP
            : center.y;
        const POINT pStart = { junctionX, startY };

        PenScope pen(hdc, devColor, devWidth);

        if (devTargetIsSwitch)
        {
            // Double liaison sw↔sw via DEVIATION : demi-diagonale vers le mid-point.
            // Symétrique du cas normalTargetIsSwitch pour la branche normale.
            LOG_DEBUG(logger, sw->getSourceId()
                + " dev-double → " + devEdge->getTo()->getSourceId());
            const PCCNode* partner    = devEdge->getTo();
            const POINT    partCenter = project(
                partner->getPosition().x, partner->getPosition().y, proj);
            const POINT midPt = {
                (center.x + partCenter.x) / 2,
                (center.y + partCenter.y) / 2
            };
            pen.moveTo(pStart);
            pen.lineTo(midPt);
        }
        else if (devEdge && devEdge->getTo())
        {
            // Déviation simple (straight) : diagonale + stub horizontal.
            const int tgtLogY = devEdge->getTo()->getPosition().y;
            const int devY = (tgtLogY != sw->getPosition().y)
                ? project(0, tgtLogY, proj).y
                : center.y - sw->getDeviationSide() * proj.cellHeight;

            // Direction réelle jonction → devBorderX — indépendante de dirFromRoot.
            const int stubDir = (devBorderX >= junctionX) ? 1 : -1;

            const int   endX      = devBorderX - stubDir * halfGap;
            const POINT pStubBeg  = { endX - stubDir * STUB, devY };
            const POINT pStubEnd  = { endX,                   devY };

            LOG_DEBUG(logger, sw->getSourceId()
                + " dev-simple stubDir=" + std::to_string(stubDir)
                + " endX="    + std::to_string(endX)
                + " pStubBeg=" + std::to_string(pStubBeg.x)
                + " devY="    + std::to_string(devY));

            pen.moveTo(pStart);
            pen.lineTo(pStubBeg);
            pen.lineTo(pStubEnd);
        }
        else
        {
            LOG_DEBUG(logger, sw->getSourceId() + " dev SKIP — devEdge null");
        }
    }
}


// =============================================================================
// Crossing — routeur (StraightCrossBlock ou SwitchCrossBlock/TJD)
// =============================================================================

void TCORenderer::drawCrossingBlock(HDC hdc, const Projection& proj,
    const PCCCrossingNode* cr, Logger& logger)
{
    const CrossBlock* source = cr->getCrossingSource();

    if (source && source->isTJD())
        drawTJDCrossingBlock(hdc, proj, cr, logger);
    else
        drawFlatCrossingBlock(hdc, proj, cr, logger);
}


// =============================================================================
// Crossing plat (StraightCrossBlock) — symbole ✕
// =============================================================================

void TCORenderer::drawFlatCrossingBlock(HDC hdc, const Projection& proj,
    const PCCCrossingNode* cr, Logger& logger)
{
    const CrossBlock* source = cr->getCrossingSource();
    const int crX = cr->getPosition().x;
    const int crY = cr->getPosition().y;

    const PCCNode* armA = cr->getEdgeA() ? cr->getEdgeA()->getTo() : nullptr;
    const PCCNode* armB = cr->getEdgeB() ? cr->getEdgeB()->getTo() : nullptr;
    const PCCNode* armC = cr->getEdgeC() ? cr->getEdgeC()->getTo() : nullptr;
    const PCCNode* armD = cr->getEdgeD() ? cr->getEdgeD()->getTo() : nullptr;

    if (!armA || !armB || !armC || !armD)
    {
        LOG_WARNING(logger, "drawFlatCrossing " + source->getId()
            + " — slot(s) null, dessin ignoré.");
        return;
    }

    const COLORREF color = stateToColor(source->getState());
    const int GAP  = proj.halfGap;
    const int STUB = proj.stub;

    // Pixels des bords gauche et droit du bloc crossing.
    // Bord = mid-point entre le crossing (crX) et la colonne voisine (crX±1).
    const int crPx    = project(crX,     crY, proj).x;
    const int leftPx  = project(crX - 1, crY, proj).x;
    const int rightPx = project(crX + 1, crY, proj).x;

    const int leftBorder  = (crPx + leftPx)  / 2;
    const int rightBorder = (crPx + rightPx) / 2;

    // Y pixels des 4 ports (lus depuis les positions post-fixCrossingLayout).
    const int pyA = project(crX, armA->getPosition().y, proj).y;
    const int pyB = project(crX, armB->getPosition().y, proj).y;
    const int pyC = project(crX, armC->getPosition().y, proj).y;
    const int pyD = project(crX, armD->getPosition().y, proj).y;

    // Helper : calcule les X de stub pour un bras (gauche ou droit du crossing).
    auto stubBorderX = [&](const PCCNode* arm) -> std::pair<int, int>
    {
        if (arm->getPosition().x > crX)
            return { rightBorder - GAP, rightBorder - GAP - STUB };
        else
            return { leftBorder + GAP,  leftBorder  + GAP + STUB };
    };

    // Voie 1 (A→C) : A côté droit, C côté gauche.
    {
        auto [axOuter, axInner] = stubBorderX(armA);
        auto [cxOuter, cxInner] = stubBorderX(armC);
        PenScope pen(hdc, color, LINE_WIDTH_ACTIVE);
        pen.moveTo({ axOuter, pyA });
        pen.lineTo({ axInner, pyA });
        pen.lineTo({ cxInner, pyC });
        pen.lineTo({ cxOuter, pyC });
    }

    // Voie 2 (B→D) : B côté droit, D côté gauche.
    {
        auto [bxOuter, bxInner] = stubBorderX(armB);
        auto [dxOuter, dxInner] = stubBorderX(armD);
        PenScope pen(hdc, color, LINE_WIDTH_ACTIVE);
        pen.moveTo({ bxOuter, pyB });
        pen.lineTo({ bxInner, pyB });
        pen.lineTo({ dxInner, pyD });
        pen.lineTo({ dxOuter, pyD });
    }

    LOG_DEBUG(logger, "drawFlatCrossing " + source->getId()
        + " cr=[" + std::to_string(crX) + "," + std::to_string(crY) + "]"
        + " A=" + armA->getSourceId() + " pyA=" + std::to_string(pyA)
        + " B=" + armB->getSourceId() + " pyB=" + std::to_string(pyB)
        + " C=" + armC->getSourceId() + " pyC=" + std::to_string(pyC)
        + " D=" + armD->getSourceId() + " pyD=" + std::to_string(pyD));
}


// =============================================================================
// Crossing TJD (SwitchCrossBlock) — symbole ✕ avec coloration par voie
// =============================================================================

void TCORenderer::drawTJDCrossingBlock(HDC hdc, const Projection& proj,
    const PCCCrossingNode* cr, Logger& logger)
{
    const CrossBlock* source = cr->getCrossingSource();
    const auto* tjd = static_cast<const SwitchCrossBlock*>(source);

    const PCCNode* armA = cr->getEdgeA() ? cr->getEdgeA()->getTo() : nullptr;
    const PCCNode* armB = cr->getEdgeB() ? cr->getEdgeB()->getTo() : nullptr;
    const PCCNode* armC = cr->getEdgeC() ? cr->getEdgeC()->getTo() : nullptr;
    const PCCNode* armD = cr->getEdgeD() ? cr->getEdgeD()->getTo() : nullptr;

    if (!armA || !armB || !armC || !armD)
    {
        LOG_WARNING(logger, "drawTJDCrossing " + source->getId()
            + " — slot(s) null, dessin ignoré.");
        return;
    }

    const int crX  = cr->getPosition().x;
    const int crY  = cr->getPosition().y;
    const int GAP  = proj.halfGap;
    const int STUB = proj.stub;

    // Bords du bloc crossing : crX (gauche) et mid(crX, crX+1) (droit).
    const int crPx      = project(crX,     crY, proj).x;
    const int rightPx   = project(crX + 1, crY, proj).x;
    const int leftBorder  = crPx;
    const int rightBorder = (crPx + rightPx) / 2;

    // Y pixels des 4 ports (depuis positions post-fixCrossingLayout TJD).
    const int pyA = project(crX, armA->getPosition().y, proj).y;  // haut gauche
    const int pyB = project(crX, armB->getPosition().y, proj).y;  // bas  gauche
    const int pyC = project(crX, armC->getPosition().y, proj).y;  // bas  droite (= pyB)
    const int pyD = project(crX, armD->getPosition().y, proj).y;  // haut droite (= pyA)

    // Voie 1 (A→C) : A gauche-haut → C droite-bas — colorée selon isPath1Active().
    {
        const COLORREF c1 = tjd->isPathACActive()
            ? stateToColor(source->getState())
            : COLORS.branchOff;
        PenScope pen(hdc, c1, LINE_WIDTH_ACTIVE);
        pen.moveTo({ leftBorder  + GAP,         pyA });
        pen.lineTo({ leftBorder  + GAP + STUB,  pyA });
        pen.lineTo({ rightBorder - GAP - STUB,  pyC });
        pen.lineTo({ rightBorder - GAP,         pyC });
    }

    // Voie 2 (B→D) : B gauche-bas → D droite-haut — colorée selon isPath2Active().
    {
        const COLORREF c2 = tjd->isPathBDActive()
            ? stateToColor(source->getState())
            : COLORS.branchOff;
        PenScope pen(hdc, c2, LINE_WIDTH_ACTIVE);
        pen.moveTo({ leftBorder  + GAP,         pyB });
        pen.lineTo({ leftBorder  + GAP + STUB,  pyB });
        pen.lineTo({ rightBorder - GAP - STUB,  pyD });
        pen.lineTo({ rightBorder - GAP,         pyD });
    }

    LOG_DEBUG(logger, "drawTJDCrossing " + source->getId()
        + " pathAC=" + (tjd->isPathACActive() ? "ON" : "off")
        + " pathBD=" + (tjd->isPathBDActive() ? "ON" : "off")
        + " pyA=" + std::to_string(pyA)
        + " pyB=" + std::to_string(pyB));
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

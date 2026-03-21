/**
 * @file  TCORenderer.cpp
 * @brief Implémentation du renderer GDI TCO.
 *
 * @see TCORenderer
 */
#include "TCORenderer.h"

#include <algorithm>
#include <limits>


 // =============================================================================
 // Constantes locales
 // =============================================================================

struct TCOColors
{
    COLORREF background = RGB(0, 0, 0);
    COLORREF free = RGB(220, 220, 220);
    COLORREF occupied = RGB(220, 50, 50);
    COLORREF inactive = RGB(80, 80, 80);
    COLORREF normal = RGB(0, 200, 80);
    COLORREF deviation = RGB(220, 200, 0);
    COLORREF error = RGB(255, 0, 216);
};

namespace
{
    const TCOColors COLORS;
    constexpr int    LINE_WIDTH_STRAIGHT = 2;
    constexpr int    LINE_WIDTH_SWITCH = 3;
    constexpr int    DOT_RADIUS_JUNCTION = 4;
    constexpr double PROJECTION_MARGIN = 0.05;
}


// =============================================================================
// Point d'entrée
// =============================================================================

void TCORenderer::draw(HDC hdc, const RECT& rect, Logger& logger)
{
    // Fond noir
    HBRUSH bgBrush = CreateSolidBrush(COLORS.background);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    const TopologyData& topo = TopologyRepository::instance().data();

    if (topo.straights.empty() && topo.switches.empty())
    {
        LOG_WARNING(logger, "TCORenderer::draw — repository vide, fond seul dessiné.");
        return;
    }

    const Projection proj = computeProjection(rect, logger);

    drawStraights(hdc, proj, logger);
    drawSwitches(hdc, proj, logger);
}


// =============================================================================
// Projection
// =============================================================================

TCORenderer::Projection TCORenderer::computeProjection(const RECT& rect, Logger& logger)
{
    const TopologyData& topo = TopologyRepository::instance().data();

    double minLat = std::numeric_limits<double>::max();
    double maxLat = -std::numeric_limits<double>::max();
    double minLon = std::numeric_limits<double>::max();
    double maxLon = -std::numeric_limits<double>::max();

    auto expand = [&](double lat, double lon)
        {
            minLat = std::min(minLat, lat);  maxLat = std::max(maxLat, lat);
            minLon = std::min(minLon, lon);  maxLon = std::max(maxLon, lon);
        };

    for (const auto& st : topo.straights)
        for (const auto& pt : st->getCoordinates())
            expand(pt.latitude, pt.longitude);

    for (const auto& sw : topo.switches)
    {
        const LatLon& jct = sw->getJunctionCoordinate();
        expand(jct.latitude, jct.longitude);

        if (sw->getTipOnRoot())
            expand(sw->getTipOnRoot()->latitude, sw->getTipOnRoot()->longitude);
        if (sw->getTipOnNormal())
            expand(sw->getTipOnNormal()->latitude, sw->getTipOnNormal()->longitude);
        if (sw->getTipOnDeviation())
            expand(sw->getTipOnDeviation()->latitude, sw->getTipOnDeviation()->longitude);
    }

    Projection proj;
    proj.minLat = minLat;  proj.maxLat = maxLat;
    proj.minLon = minLon;  proj.maxLon = maxLon;
    proj.width = rect.right - rect.left;
    proj.height = rect.bottom - rect.top;

    LOG_DEBUG(logger, "TCORenderer — bounds lat["
        + std::to_string(minLat) + ", " + std::to_string(maxLat) + "] lon["
        + std::to_string(minLon) + ", " + std::to_string(maxLon) + "]");

    return proj;
}

POINT TCORenderer::project(double lat, double lon, const Projection& proj)
{
    const double rangeLat = proj.maxLat - proj.minLat;
    const double rangeLon = proj.maxLon - proj.minLon;

    const double usableW = proj.width * (1.0 - 2.0 * Projection::MARGIN);
    const double usableH = proj.height * (1.0 - 2.0 * Projection::MARGIN);
    const double offX = proj.width * Projection::MARGIN;
    const double offY = proj.height * Projection::MARGIN;

    const double normLon = (rangeLon > 1e-10) ? (lon - proj.minLon) / rangeLon : 0.5;
    const double normLat = (rangeLat > 1e-10) ? (lat - proj.minLat) / rangeLat : 0.5;

    POINT pt;
    pt.x = static_cast<LONG>(offX + normLon * usableW);
    pt.y = static_cast<LONG>(offY + (1.0 - normLat) * usableH); // Y inversé
    return pt;
}


// =============================================================================
// Dessin des StraightBlocks
// =============================================================================

void TCORenderer::drawStraights(HDC hdc, const Projection& proj, Logger& logger)
{
    const TopologyData& topo = TopologyRepository::instance().data();

    for (const auto& st : topo.straights)
    {
        const auto& coords = st->getCoordinates();
        if (coords.size() < 2)
        {
            LOG_WARNING(logger, "TCORenderer — straight " + st->getId()
                + " ignoré (coords < 2).");
            continue;
        }

        const COLORREF color = stateToColor(st->getState());

        for (std::size_t i = 1; i < coords.size(); ++i)
        {
            const POINT from = project(coords[i - 1].latitude, coords[i - 1].longitude, proj);
            const POINT to = project(coords[i].latitude, coords[i].longitude, proj);
            drawLine(hdc, from, to, color, LINE_WIDTH_STRAIGHT);
        }
    }
}


// =============================================================================
// Dessin des SwitchBlocks
// =============================================================================

void TCORenderer::drawSwitches(HDC hdc, const Projection& proj, Logger& logger)
{
    const TopologyData& topo = TopologyRepository::instance().data();

    for (const auto& sw : topo.switches)
    {
        if (!sw->isOriented())
        {
            LOG_WARNING(logger, "TCORenderer — switch " + sw->getId()
                + " ignoré (non orienté).");
            continue;
        }

        const LatLon& jct = sw->getJunctionCoordinate();
        const POINT   pJct = project(jct.latitude, jct.longitude, proj);
        const COLORREF swColor = stateToColor(sw->getState());

        // Branche root → jonction
        if (sw->getTipOnRoot())
        {
            const POINT pRoot = project(
                sw->getTipOnRoot()->latitude,
                sw->getTipOnRoot()->longitude, proj);
            drawLine(hdc, pRoot, pJct, swColor, LINE_WIDTH_SWITCH);
        }

        // Jonction → tip normal
        if (sw->getTipOnNormal())
        {
            const bool     isActive = (sw->getActiveBranch() == ActiveBranch::NORMAL);
            const COLORREF color = isActive ? COLORS.normal : swColor;
            const POINT    pNorm = project(
                sw->getTipOnNormal()->latitude,
                sw->getTipOnNormal()->longitude, proj);
            drawLine(hdc, pJct, pNorm, color, LINE_WIDTH_SWITCH);
        }

        // Jonction → tip déviation
        if (sw->getTipOnDeviation())
        {
            const bool     isActive = (sw->getActiveBranch() == ActiveBranch::DEVIATION);
            const COLORREF color = isActive ? COLORS.deviation : swColor;
            const POINT    pDev = project(
                sw->getTipOnDeviation()->latitude,
                sw->getTipOnDeviation()->longitude, proj);
            drawLine(hdc, pJct, pDev, color, LINE_WIDTH_SWITCH);
        }

        // Disque de jonction
        drawDot(hdc, pJct, DOT_RADIUS_JUNCTION, COLORS.free);
    }
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
    case ShuntingState::FREE:     return COLORS.free;
    default:                      return COLORS.error;
    }
}

void TCORenderer::drawLine(HDC hdc, POINT from, POINT to, COLORREF color, int lineWidth)
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
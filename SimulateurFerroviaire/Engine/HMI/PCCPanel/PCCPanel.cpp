/**
 * @file  PCCPanel.cpp
 * @brief Implémentation du panneau PCC superposé togglable.
 *
 * Modification v2 : onPaint() met en cache la projection TCO.
 * computeProjection() n'est recalculé que si la RECT a changé (resize)
 * ou si m_projDirty est vrai (nouveau parsing via rebuild()).
 *
 * @see PCCPanel
 */
#include "framework.h"
#include "PCCPanel.h"
#include "TCORenderer.h"

#include "Modules/PCC/PCCGraphBuilder.h"
#include "Modules/PCC/PCCLayout.h"

#include <stdexcept>


 // =============================================================================
 // Construction
 // =============================================================================

PCCPanel::PCCPanel(Logger& logger)
    : m_logger(logger)
    , m_graph()
{
}

void PCCPanel::create(HWND hParent, HINSTANCE hInstance)
{
    m_hParent = hParent;
    m_hInstance = hInstance;

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;  // CS_DBLCLKS requis pour WM_LBUTTONDBLCLK
    wcex.lpfnWndProc = PCCPanel::windowProc;
    wcex.hInstance = hInstance;
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wcex.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wcex);

    RECT rc{};
    GetClientRect(hParent, &rc);

    m_hWnd = CreateWindowExW(0, CLASS_NAME, L"PCC", WS_CHILD,
        0, 0, rc.right, rc.bottom,
        hParent, nullptr, hInstance, this);

    if (!m_hWnd)
    {
        const DWORD err = GetLastError();
        LOG_ERROR(m_logger, "CreateWindowExW échoué. Code = " + std::to_string(err));
        throw std::runtime_error("PCCPanel::create — CreateWindowExW échoué.");
    }

    ShowWindow(m_hWnd, SW_HIDE);
    LOG_INFO(m_logger, "Créé — "
        + std::to_string(rc.right) + "x" + std::to_string(rc.bottom));
}


// =============================================================================
// Contrôle de visibilité
// =============================================================================

void PCCPanel::toggle()
{
    if (!m_hWnd) return;

    const bool visible = (IsWindowVisible(m_hWnd) != FALSE);
    LOG_DEBUG(m_logger, visible ? "Masquage." : "Affichage.");

    if (visible)
    {
        ShowWindow(m_hWnd, SW_HIDE);
    }
    else
    {
        if (m_graph.isEmpty())
            rebuild();

        resize();
        SetWindowPos(m_hWnd, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        InvalidateRect(m_hWnd, nullptr, TRUE);
    }
}

void PCCPanel::resize()
{
    if (!m_hWnd || !m_hParent) return;

    RECT rc{};
    GetClientRect(m_hParent, &rc);
    LOG_DEBUG(m_logger, "Resize — "
        + std::to_string(rc.right) + "x" + std::to_string(rc.bottom));
    SetWindowPos(m_hWnd, HWND_TOP, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

void PCCPanel::refresh()
{
    rebuild();

    if (m_hWnd && IsWindowVisible(m_hWnd))
        InvalidateRect(m_hWnd, nullptr, TRUE);
}

bool PCCPanel::isVisible() const
{
    return m_hWnd && (IsWindowVisible(m_hWnd) != FALSE);
}


// =============================================================================
// Reconstruction du graphe PCC
// =============================================================================

void PCCPanel::rebuild()
{
    LOG_DEBUG(m_logger, "Rebuild PCC — début.");
    PCCGraphBuilder::build(m_graph, m_logger);
    PCCLayout::compute(m_graph, m_logger);
    // Invalide le cache — la topologie a changé, la projection doit être recalculée.
    m_projDirty = true;
    LOG_DEBUG(m_logger, "Rebuild PCC — terminé.");
}


// =============================================================================
// Zoom (molette)
// =============================================================================

void PCCPanel::onMouseWheel(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    // Coordonnées souris en espace client (lParam est en espace écran pour WM_MOUSEWHEEL).
    POINT cursor = {
        static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
        static_cast<LONG>(static_cast<short>(HIWORD(lParam)))
    };
    ScreenToClient(hWnd, &cursor);

    // Delta positif = zoom avant, négatif = zoom arrière.
    const int   delta    = GET_WHEEL_DELTA_WPARAM(wParam);
    const float factor   = (delta > 0) ? (1.0f + ZOOM_STEP) : (1.0f / (1.0f + ZOOM_STEP));
    const float newZoom  = std::clamp(m_zoom * factor, ZOOM_MIN, ZOOM_MAX);

    // Conservation du point sous le curseur :
    //   worldPos = (cursor - pan) / zoom   [invariant]
    //   newPan   = cursor - worldPos * newZoom
    //            = cursor - (cursor - pan) * (newZoom / zoom)
    const float ratio = newZoom / m_zoom;
    m_panX = static_cast<float>(cursor.x) - (static_cast<float>(cursor.x) - m_panX) * ratio;
    m_panY = static_cast<float>(cursor.y) - (static_cast<float>(cursor.y) - m_panY) * ratio;
    m_zoom = newZoom;

    LOG_DEBUG(m_logger, "Zoom → " + std::to_string(m_zoom)
        + " | pan (" + std::to_string(m_panX) + ", " + std::to_string(m_panY) + ")");

    // Pas d'effacement du fond (TRUE invaliderait via WM_ERASEBKGND qu'on supprime) :
    // on remplit le fond manuellement dans onPaint, donc FALSE suffit.
    InvalidateRect(hWnd, nullptr, FALSE);
}


// =============================================================================
// Pan (drag & drop bouton gauche)
// =============================================================================

void PCCPanel::onLButtonDown(HWND hWnd, LPARAM lParam)
{
    // Annule un double-clic accidentel qui aurait déclenché onLButtonDown
    // juste après onLButtonDblClk — vérification optionnelle de cohérence.
    m_isDragging   = true;
    m_dragAnchor   = {
        static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
        static_cast<LONG>(static_cast<short>(HIWORD(lParam)))
    };
    m_panXAtDrag   = m_panX;
    m_panYAtDrag   = m_panY;

    // SetCapture : garantit la réception de WM_MOUSEMOVE même hors de la fenêtre.
    SetCapture(hWnd);

    LOG_DEBUG(m_logger, "Drag début — ancre ("
        + std::to_string(m_dragAnchor.x) + ", "
        + std::to_string(m_dragAnchor.y) + ")");
}

void PCCPanel::onMouseMove(HWND hWnd, LPARAM lParam)
{
    if (!m_isDragging) return;

    const int dx = static_cast<int>(static_cast<short>(LOWORD(lParam))) - m_dragAnchor.x;
    const int dy = static_cast<int>(static_cast<short>(HIWORD(lParam))) - m_dragAnchor.y;

    m_panX = m_panXAtDrag + static_cast<float>(dx);
    m_panY = m_panYAtDrag + static_cast<float>(dy);

    // FALSE : pas de WM_ERASEBKGND — le fond est rempli dans onPaint.
    InvalidateRect(hWnd, nullptr, FALSE);
}

void PCCPanel::onLButtonUp(HWND hWnd)
{
    if (!m_isDragging) return;

    m_isDragging = false;
    ReleaseCapture();

    LOG_DEBUG(m_logger, "Drag fin — pan ("
        + std::to_string(m_panX) + ", "
        + std::to_string(m_panY) + ")");
}


// =============================================================================
// Reset de vue (double-clic)
// =============================================================================

void PCCPanel::resetView()
{
    m_zoom = 1.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    LOG_DEBUG(m_logger, "Vue réinitialisée — zoom 1:1, pan (0, 0).");

    if (m_hWnd && IsWindowVisible(m_hWnd))
        InvalidateRect(m_hWnd, nullptr, FALSE);
}
// =============================================================================

LRESULT CALLBACK PCCPanel::windowProc(HWND hWnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    PCCPanel* self = nullptr;

    if (msg == WM_NCCREATE)
    {
        const auto* cs = reinterpret_cast<const CREATESTRUCT*>(lParam);
        self = reinterpret_cast<PCCPanel*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<PCCPanel*>(
            GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self) return self->handleMessage(hWnd, msg, wParam, lParam);
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT PCCPanel::handleMessage(HWND hWnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
        onPaint(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;   // No-op — supprime le flickering

    // -------------------------------------------------------------------------
    // Zoom : Ctrl + molette, centré sur le curseur
    // -------------------------------------------------------------------------
    case WM_MOUSEWHEEL:
        onMouseWheel(hWnd, wParam, lParam);
        return 0;

    // -------------------------------------------------------------------------
    // Pan : drag & drop bouton gauche
    // -------------------------------------------------------------------------
    case WM_LBUTTONDOWN:
        onLButtonDown(hWnd, lParam);
        return 0;

    case WM_MOUSEMOVE:
        onMouseMove(hWnd, lParam);
        return 0;

    case WM_LBUTTONUP:
        onLButtonUp(hWnd);
        return 0;

    case WM_CAPTURECHANGED:
        // Capture volée par une autre fenêtre — annule proprement le drag.
        m_isDragging = false;
        return 0;

    // -------------------------------------------------------------------------
    // Reset : double-clic gauche → retour zoom 1:1, pan (0, 0)
    // -------------------------------------------------------------------------
    case WM_LBUTTONDBLCLK:
        resetView();
        return 0;

    // -------------------------------------------------------------------------
    // Curseur : main ouverte au repos, main fermée en drag
    // -------------------------------------------------------------------------
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            const LPCWSTR id = m_isDragging ? IDC_SIZEALL : IDC_HAND;
            SetCursor(LoadCursorW(nullptr, id));
            return TRUE;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

void PCCPanel::onPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);

    // --- Cache de projection (inchangé depuis v2) ----------------------------
    // Recalcule uniquement si le graphe a changé (m_projDirty) ou si la
    // fenêtre a été redimensionnée. Comparaison sur right/bottom uniquement :
    // top/left sont toujours 0 pour un WS_CHILD couvrant toute la zone cliente.
    if (m_projDirty
        || rc.right  != m_lastRect.right
        || rc.bottom != m_lastRect.bottom)
    {
        m_cachedProj = TCORenderer::computeProjection(rc, m_graph, m_logger);
        m_lastRect   = rc;
        m_projDirty  = false;
        LOG_DEBUG(m_logger, "Projection recalculée.");
    }

    // --- 1. Fond en espace écran (avant world transform) ---------------------
    // FillRect est appelé ici avec la transform identité : il couvre toujours
    // l'intégralité du rectangle client, quelle que soit la vue.
    // TCORenderer::draw() reçoit fillBackground = false pour ne pas réécraser.
    {
        HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);
    }

    // --- 2. World transform : zoom centré + translation (pan) ----------------
    // GM_ADVANCED est requis par SetWorldTransform.
    // SaveDC / RestoreDC garantit la restauration complète du contexte GDI
    // (mode graphique, transform, pen/brush) après le rendu.
    const int savedDC = SaveDC(hdc);

    SetGraphicsMode(hdc, GM_ADVANCED);

    XFORM xf      = {};
    xf.eM11       = static_cast<FLOAT>(m_zoom);   // scale X
    xf.eM22       = static_cast<FLOAT>(m_zoom);   // scale Y
    xf.eDx        = static_cast<FLOAT>(m_panX);   // translation X (pixels écran)
    xf.eDy        = static_cast<FLOAT>(m_panY);   // translation Y (pixels écran)
    SetWorldTransform(hdc, &xf);

    // --- 3. Rendu TCO avec projection cachée ---------------------------------
    // Le fond a déjà été rempli — fillBackground = false.
    TCORenderer::draw(hdc, rc, m_graph, m_cachedProj, m_logger, false);

    RestoreDC(hdc, savedDC);
    EndPaint(hWnd, &ps);
}
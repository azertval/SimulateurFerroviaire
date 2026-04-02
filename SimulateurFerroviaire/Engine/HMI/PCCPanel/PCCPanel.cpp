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
    wcex.style = CS_HREDRAW | CS_VREDRAW;
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
// WndProc
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

    // Recalcule la projection uniquement si nécessaire :
    //   - premier paint (m_projDirty initialisé à true)
    //   - nouveau parsing (rebuild() a mis m_projDirty = true)
    //   - resize de la fenêtre (right ou bottom différents)
    // Comparaison sur right/bottom : top/left sont toujours 0 pour WS_CHILD.
    if (m_projDirty
        || rc.right != m_lastRect.right
        || rc.bottom != m_lastRect.bottom)
    {
        m_cachedProj = TCORenderer::computeProjection(rc, m_graph, m_logger);
        m_lastRect = rc;
        m_projDirty = false;
        LOG_DEBUG(m_logger, "Projection recalculée.");
    }

    // Passe le graphe PCC et la projection cachée à TCORenderer
    TCORenderer::draw(hdc, rc, m_graph, m_cachedProj, m_logger);

    EndPaint(hWnd, &ps);
}
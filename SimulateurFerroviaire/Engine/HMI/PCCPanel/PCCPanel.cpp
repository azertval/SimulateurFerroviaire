/**
 * @file PCCPanel.cpp
 * @brief Implémentation du panneau PCC superposé togglable.
 *
 * @see PCCPanel
 */
#include "framework.h"
#include "PCCPanel.h"
#include "TCORenderer.h"
#include <stdexcept>


 // =============================================================================
 // Création
 // =============================================================================

void PCCPanel::create(HWND hParent, HINSTANCE hInstance)
{
    LOG_INFO(m_logger, "Creation du Pannel PCC");
    m_hParent = hParent;
    m_hInstance = hInstance;

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = PCCPanel::windowProc;
    wcex.hInstance = hInstance;
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wcex.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wcex); // ERROR_CLASS_ALREADY_EXISTS ignoré volontairement

    RECT rc{};
    GetClientRect(hParent, &rc);

    m_hWnd = CreateWindowExW(
        0,              
        CLASS_NAME,
        L"PCC",
        WS_CHILD,
        0, 0, rc.right, rc.bottom,
        hParent, nullptr, hInstance, this);

    if (!m_hWnd)
    {
        LOG_FAILURE(m_logger, "PCCPanel::create — CreateWindowExW échoué.");
        throw std::runtime_error("PCCPanel::create — CreateWindowExW failed.");
    }

    ShowWindow(m_hWnd, SW_HIDE);
}


// =============================================================================
// Contrôle de visibilité
// =============================================================================

void PCCPanel::toggle()
{
    if (!m_hWnd) return;

    const bool visible = (IsWindowVisible(m_hWnd) != FALSE);

    if (visible)
    {
        ShowWindow(m_hWnd, SW_HIDE);
    }
    else
    {
        resize();
        // Place le panneau au-dessus de tous les autres enfants (WebView2 inclus)
        SetWindowPos(m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        InvalidateRect(m_hWnd, nullptr, TRUE);
    }
}

void PCCPanel::resize()
{
    if (!m_hWnd || !m_hParent) return;

    RECT rc{};
    GetClientRect(m_hParent, &rc);
    SetWindowPos(m_hWnd, HWND_TOP, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

void PCCPanel::refresh()
{
    if (m_hWnd && IsWindowVisible(m_hWnd))
        InvalidateRect(m_hWnd, nullptr, TRUE);
}

bool PCCPanel::isVisible() const
{
    return m_hWnd && (IsWindowVisible(m_hWnd) != FALSE);
}


// =============================================================================
// WndProc statique
// =============================================================================

LRESULT CALLBACK PCCPanel::windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
        self = reinterpret_cast<PCCPanel*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self) return self->handleMessage(hWnd, msg, wParam, lParam);
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT PCCPanel::handleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
        onPaint(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1; // No-op — supprime le flickering blanc entre deux frames

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}


// =============================================================================
// Rendu
// =============================================================================

void PCCPanel::onPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);
    TCORenderer::draw(hdc, rc, m_logger);

    EndPaint(hWnd, &ps);
}
/**
 * @file Application.cpp
 * @brief Implémentation du cycle de vie de l'application Win32.
 *
 * @see Application
 */

#include "framework.h"
#include "Application.h"
#include "SimulateurFerroviaire.h"

#include <stdexcept>


// =============================================================================
// Construction
// =============================================================================

Application::Application(HINSTANCE hInstance, int nCmdShow)
    : m_hInstance(hInstance)
    , m_nCmdShow(nCmdShow)
{
    LoadStringW(m_hInstance, IDS_APP_TITLE,             m_szTitle,       MAX_LOADSTRING);
    LoadStringW(m_hInstance, IDC_SIMULATEURFERROVIAIRE, m_szWindowClass, MAX_LOADSTRING);

    registerWindowClass();
}


// =============================================================================
// Boucle principale
// =============================================================================

int Application::run()
{
    m_mainWindow = std::make_unique<MainWindow>(m_hInstance, m_szWindowClass, m_szTitle, m_nCmdShow);
    m_mainWindow->create();

    HACCEL hAccelTable = LoadAccelerators(
        m_hInstance,
        MAKEINTRESOURCE(IDC_SIMULATEURFERROVIAIRE));

    MSG msg = {};

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}


// =============================================================================
// Enregistrement de classe
// =============================================================================

void Application::registerWindowClass()
{
    WNDCLASSEXW wcex = {};

    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = MainWindow::windowProc;   // Procédure statique de MainWindow
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = m_hInstance;
    wcex.hIcon         = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_SIMULATEURFERROVIAIRE));
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszMenuName  = MAKEINTRESOURCEW(IDC_SIMULATEURFERROVIAIRE);
    wcex.lpszClassName = m_szWindowClass;
    wcex.hIconSm       = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_SMALL));

    if (!RegisterClassExW(&wcex))
    {
         DWORD err = GetLastError();  // ← immédiatement après l'échec
        throw std::runtime_error("RegisterClassExW failed. Code = " + std::to_string(err));
    }
}

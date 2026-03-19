/**
 * @file MainWindow.cpp
 * @brief Implémentation de la fenêtre principale.
 *
 * @see MainWindow
 */

#include "framework.h"
#include "MainWindow.h"
#include "Dialogs/AboutDialog.h"
#include "Dialogs/FileOpenDialog.h"
#include "SimulateurFerroviaire.h"

#include <string>
#include <stdexcept>

// Messages utilisateur inter-threads (définis ici pour rester locaux à MainWindow)

/** @brief Demande de mise à jour de la ProgressBar émise par le thread de parsing. */
static constexpr UINT WM_PROGRESS_UPDATE = WM_USER + 1;

/** @brief Notification de fin de parsing réussie. */
static constexpr UINT WM_PARSING_SUCCESS = WM_USER + 2;

/**
 * @brief Notification d'échec de parsing.
 * Le LPARAM transporte un pointeur vers un @c std::string alloué dynamiquement,
 * libéré par @ref MainWindow::onParsingError.
 */
static constexpr UINT WM_PARSING_ERROR   = WM_USER + 3;


// =============================================================================
// Construction & création
// =============================================================================

MainWindow::MainWindow(HINSTANCE hInstance,
                       const WCHAR* className,
                       const WCHAR* title,
                       int nCmdShow)
    : m_hInstance(hInstance)
    , m_className(className)
    , m_title(title)
    , m_nCmdShow(nCmdShow)
    , m_progressBar()
{
}

void MainWindow::create()
{
    m_hWnd = CreateWindowW(
        m_className,
        m_title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0,
        900, 650,
        nullptr, nullptr,
        m_hInstance, this);   // 'this' récupéré dans WM_NCCREATE via GWLP_USERDATA

    if (!m_hWnd)
    {
        throw std::runtime_error("Échec de CreateWindowW.");
    }

    ShowWindow(m_hWnd, m_nCmdShow);
    UpdateWindow(m_hWnd);

    // Barre de progression positionnée sous la barre de menu, masquée par défaut.
    m_progressBar.create(m_hWnd, 20, 50, 320, 24);
    m_progressBar.show(false);
    m_progressBar.setProgress(0);

    // Initialisation du panneau WebView
    m_webViewPanel.setOnInitialized([this]()
        {
            m_webViewPanel.navigate(L"https://www.openstreetmap.org/#map=5/51.500/-0.100"); // Carte centrée sur Londres par défaut
            m_webViewPanel.resize();
        });
    m_webViewPanel.create(m_hWnd);

}


// =============================================================================
// Procédure statique Win32 — point d'entrée imposé par le système
// =============================================================================

LRESULT CALLBACK MainWindow::windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;

    if (message == WM_NCCREATE)
    {
        // Lors de la toute première création, lParam contient le CREATESTRUCT
        // dont lpCreateParams est le 'this' passé à CreateWindowW.
        const CREATESTRUCT* cs = reinterpret_cast<const CREATESTRUCT*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self)
    {
        return self->handleMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}


// =============================================================================
// Dispatcher de messages d'instance
// =============================================================================

LRESULT MainWindow::handleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        onCommand(hWnd, LOWORD(wParam));
        return 0;

    case WM_PROGRESS_UPDATE:
        onProgressUpdate(static_cast<int>(wParam));
        return 0;

    case WM_PARSING_SUCCESS:
        onParsingSuccess(hWnd);
        return 0;

    case WM_PARSING_ERROR:
        onParsingError(hWnd, lParam);
        return 0;
    case WM_SIZE:
        onSizeUpdate();
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // Zone de rendu future (carte ferroviaire, etc.)
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        onDestroy();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}


// =============================================================================
// Gestionnaires spécialisés
// =============================================================================

void MainWindow::onCommand(HWND hWnd, int commandId)
{
    switch (commandId)
    {
    case IDM_FILE_OPEN:
        onFileOpen(hWnd);
        break;

    case IDM_ABOUT:
        AboutDialog::show(hWnd, m_hInstance);
        break;

    case IDM_EXIT:
        DestroyWindow(hWnd);
        break;

    default:
        DefWindowProc(hWnd, WM_COMMAND, MAKEWPARAM(commandId, 0), 0);
        break;
    }
}

void MainWindow::onFileOpen(HWND hWnd)
{
    const std::optional<std::string> selectedPath = FileOpenDialog::open(hWnd);

    if (!selectedPath.has_value())
    {
        return; // Annulation par l'utilisateur
    }

    m_progressBar.setProgress(0);
    m_progressBar.show(true);

    GeoParsingTask::launch(hWnd, selectedPath.value());
}

void MainWindow::onProgressUpdate(int progressValue)
{
    m_progressBar.setProgress(progressValue);
}

void MainWindow::onParsingSuccess(HWND hWnd)
{
    m_progressBar.setProgress(100);
    MessageBoxA(hWnd, "Parsing terminé avec succès.", "Succès", MB_OK | MB_ICONINFORMATION);
    m_progressBar.show(false);
}

void MainWindow::onParsingError(HWND hWnd, LPARAM lParam)
{
    m_progressBar.show(false);
    m_progressBar.setProgress(0);

    std::string* errorMessage = reinterpret_cast<std::string*>(lParam);

    if (errorMessage)
    {
        MessageBoxA(hWnd, errorMessage->c_str(), "Erreur", MB_OK | MB_ICONERROR);
        delete errorMessage;
    }
    else
    {
        MessageBoxA(hWnd, "Erreur inconnue.", "Erreur", MB_OK | MB_ICONERROR);
    }
}


void MainWindow::onSizeUpdate()
{
    if (m_webViewPanel.isInitialized())
    {
        m_webViewPanel.resize();
    }
}

void MainWindow::onDestroy()
{
}
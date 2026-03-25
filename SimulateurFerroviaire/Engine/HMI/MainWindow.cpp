/**
 * @file MainWindow.cpp
 * @brief Implémentation de la fenêtre principale.
 *
 * @see MainWindow
 */

#include "framework.h"
#include "MainWindow.h"
#include "SimulateurFerroviaire.h"

#include "Engine/HMI/Utils/PathUtils.h"

#include "Engine/HMI/Dialogs/AboutDialog.h"
#include "Engine/HMI/Dialogs/FileOpenDialog.h"
#include "Engine/HMI/Dialogs/FileSaveDialog.h"
#include "Engine/HMI/Dialogs/ParserSettingsDialog.h"

#include "Engine/HMI/WebViewPanel/Leaflet/Leaflet.h"

#include "Engine/Core/Topology/TopologyRenderer.h"
#include "Engine/Core/Topology/TopologyRepository.h"

#include <string>
#include <stdexcept>


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
        throw std::runtime_error("Échec de CreateWindowW.");

    ShowWindow(m_hWnd, m_nCmdShow);
    UpdateWindow(m_hWnd);

    // ---- ProgressBar --------------------------------------------------------
    // Positionnée sous la barre de menu, masquée par défaut.
    // IDC_CANCEL_PARSING est l'ID du bouton "Annuler" géré dans onCommand().
    m_progressBar.create(m_hWnd, m_hInstance,
        50, 10, 320,
        IDC_CANCEL_PARSING);
    // show(false) implicite — les contrôles sont créés sans WS_VISIBLE

    // ---- Configuration du parser -------------------------------------------
    m_parserIniPath = ParserConfigIni::defaultPath();
    m_parserConfig = ParserConfigIni::load(m_parserIniPath);
    // Si le fichier n'existe pas, load() retourne les valeurs par défaut.
    // Il sera créé au prochain appel à ParserConfigIni::save().

    // ---- Tâche asynchrone --------------------------------------------------
    // Instanciée ici car elle nécessite m_hWnd (destinataire des PostMessage).
    m_parserTask.emplace(m_hWnd);

    // ---- WebView -----------------------------------------------------------
    m_webViewPanel.setOnMessageReceived([this](const std::string& message)
        {
            onWebMessage(message);
        });
    m_webViewPanel.setOnInitialized([this]()
        {
            m_webViewPanel.setVirtualHostMapping(
                L"app.local",
                (executableDirectory() / "Resources").wstring()
            );
            m_webViewPanel.navigate(L"https://app.local/leaflet.html");
            m_webViewPanel.resize();
        });
    m_webViewPanel.create(m_hWnd);

    // ---- PCC Panel ---------------------------------------------------------
    m_pccPanel.create(m_hWnd, m_hInstance);
}


// =============================================================================
// Procédure statique Win32 — point d'entrée imposé par le système
// =============================================================================

LRESULT CALLBACK MainWindow::windowProc(HWND hWnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;

    if (message == WM_NCCREATE)
    {
        const CREATESTRUCT* cs = reinterpret_cast<const CREATESTRUCT*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self)
        return self->handleMessage(hWnd, message, wParam, lParam);

    return DefWindowProc(hWnd, message, wParam, lParam);
}


// =============================================================================
// Dispatcher de messages d'instance
// =============================================================================

LRESULT MainWindow::handleMessage(HWND hWnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        onCommand(hWnd, LOWORD(wParam));
        return 0;

    case WM_PROGRESS_UPDATE:
        // wParam = progression (int)
        // lParam = std::wstring* label (alloué par GeoParsingTask — libéré ici)
        onProgressUpdate(static_cast<int>(wParam),
            reinterpret_cast<std::wstring*>(lParam));
        return 0;

    case WM_PARSING_SUCCESS:
        onParsingSuccess(hWnd);
        return 0;

    case WM_PARSING_ERROR:
        // lParam = std::wstring* message d'erreur (libéré dans onParsingError)
        onParsingError(hWnd, lParam);
        return 0;

    case WM_PARSING_CANCELLED:
        onParsingCancelled();
        return 0;

    case WM_SIZE:
        onSizeUpdate();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_F2)
        {
            onTogglePCC();
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
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

    case IDM_FILE_EXPORT:
        onFileExport(hWnd);
        break;

    case IDC_CANCEL_PARSING:
        onCancelButtonClick();
        break;

    case IDM_PARSER_SETTINGS:
        onParserSettings();
        break;

    case IDM_VIEW_PCC:
        onTogglePCC();
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
    if (!m_webViewPanel.isInitialized())
    {
        MessageBoxW(hWnd,
            L"Le panneau WebView n'est pas encore initialisé.\n"
            L"Veuillez réessayer dans quelques instants.",
            L"Erreur", MB_OK | MB_ICONERROR);
        return;
    }

    const std::optional<std::string> selectedPath = FileOpenDialog::open(hWnd);
    if (!selectedPath.has_value())
        return;  // Annulation par l'utilisateur

    m_progressBar.reset();
    m_progressBar.show(true);
    m_progressBar.showCancelButton();

    m_parserTask->start(selectedPath.value(), m_parserConfig);
}

void MainWindow::onFileExport(HWND hWnd)
{
    const std::optional<std::string> selectedPath = FileSaveDialog::save(hWnd);
    if (!selectedPath.has_value())
        return;

    TopologyRenderer::exportToFile(selectedPath.value());
}

void MainWindow::onProgressUpdate(int progress, std::wstring* label)
{
    m_progressBar.setProgress(progress);

    if (label)
    {
        m_progressBar.setLabel(*label);
        delete label;   // Propriété transférée depuis GeoParsingTask
    }
}

void MainWindow::onParsingSuccess(HWND hWnd)
{
    m_progressBar.hideCancelButton();

    // Update de l'affichage leaflet
    std::wstring script;
    script += TopologyRenderer::renderAllStraightBlocks();
    script += TopologyRenderer::renderAllSwitchBranches();
    script += TopologyRenderer::renderAllSwitchBlocksJunctions();
    m_webViewPanel.executeScript(script);

    // Update de l'affichage PCC
    m_pccPanel.refresh();

    m_progressBar.show(false);
}

void MainWindow::onParsingError(HWND hWnd, LPARAM lParam)
{
    m_progressBar.reset();  // Masque + remet à zéro + cache Cancel

    std::wstring* errorMessage = reinterpret_cast<std::wstring*>(lParam);
    if (errorMessage)
    {
        MessageBoxW(hWnd, errorMessage->c_str(), L"Erreur de parsing",
            MB_OK | MB_ICONERROR);
        delete errorMessage;
    }
    else
    {
        MessageBoxW(hWnd, L"Erreur inconnue.", L"Erreur de parsing",
            MB_OK | MB_ICONERROR);
    }
}

void MainWindow::onParsingCancelled()
{
    m_progressBar.reset();  // Masque + remet à zéro + cache Cancel
    LOG_INFO(m_logger, "Parsing annulé par l'utilisateur.");
}

void MainWindow::onCancelButtonClick()
{
    if (m_parserTask)
        m_parserTask->cancel();
}

void MainWindow::onParserSettings()
{
    const bool accepted = ParserSettingsDialog::show(
        m_hWnd, m_parserConfig, m_parserIniPath);

    if (accepted)
    {
        // m_parserConfig a été mis à jour et sauvegardé par le dialogue.
        // Le prochain appel à start() utilisera automatiquement les nouveaux paramètres.
        LOG_INFO(m_logger, "Configuration parser mise à jour.");
    }
}

void MainWindow::onSizeUpdate()
{
    if (m_webViewPanel.isInitialized())
        m_webViewPanel.resize();

    m_pccPanel.resize();
}

void MainWindow::onDestroy()
{
    // Annulation propre si un parsing est en cours au moment de la fermeture
    if (m_parserTask && m_parserTask->isCancelling() == false)
        m_parserTask->cancel();
}

void MainWindow::onWebMessage(const std::string& jsonMessage)
{
    try
    {
        const JsonDocument msg = JsonDocument::parse(jsonMessage);

        const std::string type = msg.value("type", "");

        if (type == "switch_click")
            onSwitchClick(msg.value("id", ""));
        else
            LOG_WARNING(m_logger, "Message JS de type inconnu : " + type);
    }
    catch (const JsonDocument::exception& e)
    {
        LOG_ERROR(m_logger, "Parse message JS échoué : " + std::string(e.what()));
    }
}

void MainWindow::onSwitchClick(const std::string& switchId)
{
    const auto& index = TopologyRepository::instance().data().switchIndex;

    const auto it = index.find(switchId);
    if (it == index.end())
    {
        LOG_WARNING(m_logger, "onSwitchClick — introuvable : " + switchId);
        return;
    }

    SwitchBlock& sw = *it->second;
    sw.toggleActiveBranch();

    m_webViewPanel.executeScript(TopologyRenderer::updateSwitchBlocks(sw));
}

void MainWindow::onTogglePCC()
{
    m_pccPanel.toggle();
}
/**
 * @file SimulateurFerroviaire.cpp
 * @brief Point d'entrée de l'application Win32.
 *
 * Ce fichier gère :
 *   - l'initialisation de la fenêtre principale
 *   - la boucle de messages Win32
 *   - le menu "Fichier > Ouvrir"
 *   - l'ouverture d'un fichier GeoJSON via l'explorateur Windows
 *   - l'exécution asynchrone du GeoParser
 *   - l'affichage d'une barre de progression native Win32
 */

#include "framework.h"
#include "SimulateurFerroviaire.h"

#include "Modules/GeoParser/GeoParser.h"
#include "Modules/GeoParser/Exceptions/GeoParserException.h"
#include "Engine/Core/Logger.h"
#include "Engine/HMI/ProgressBar.h"

#include <commdlg.h>
#include <thread>
#include <string>

#define MAX_LOADSTRING 100

 /**
  * @brief Message utilisateur interne pour mettre à jour la ProgressBar.
  *
  * Le thread de parsing ne doit jamais manipuler directement l'UI.
  * Il publie donc ce message vers le thread principal via PostMessage.
  */
#define WM_PROGRESS_UPDATE (WM_USER + 1)

  /**
   * @brief Message utilisateur interne indiquant la fin du parsing avec succès.
   */
#define WM_PARSING_SUCCESS (WM_USER + 2)

   /**
    * @brief Message utilisateur interne indiquant un échec du parsing.
    *
    * Le LPARAM transporte un pointeur vers std::string alloué dynamiquement,
    * libéré côté thread UI après affichage.
    */
#define WM_PARSING_ERROR   (WM_USER + 3)


    // =============================================================================
    // Variables globales
    // =============================================================================

    /** Instance actuelle de l'application. */
HINSTANCE hInst;

/** Texte de la barre de titre. */
WCHAR szTitle[MAX_LOADSTRING];

/** Nom de la classe de fenêtre principale. */
WCHAR szWindowClass[MAX_LOADSTRING];

/** Barre de progression globale attachée à la fenêtre principale. */
ProgressBar g_progressBar;


// =============================================================================
// Déclarations anticipées
// =============================================================================

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

/**
 * @brief Lance un parsing GeoJSON dans un thread séparé.
 *
 * @param hWnd         Fenêtre principale destinataire des messages UI.
 * @param geoJsonPath  Chemin du fichier GeoJSON sélectionné.
 *
 * La progression est renvoyée via WM_PROGRESS_UPDATE.
 * Le résultat final est renvoyé via WM_PARSING_SUCCESS ou WM_PARSING_ERROR.
 */
static void LaunchGeoParsingAsync(HWND hWnd, const std::string& geoJsonPath)
{
    std::thread([hWnd, geoJsonPath]()
        {
            try
            {
                Logger parserLogger("GeoParser");

                GeoParser parser(
                    parserLogger,
                    geoJsonPath,
                    ParserDefaultValues::SNAP_GRID_METERS,
                    ParserDefaultValues::ENDPOINT_SNAP_METERS,
                    ParserDefaultValues::MAX_STRAIGHT_LENGTH_METERS,
                    ParserDefaultValues::MIN_BRANCH_LENGTH_METERS,
                    ParserDefaultValues::DOUBLE_LINK_MAX_METERS,
                    ParserDefaultValues::BRANCH_TIP_DISTANCE_METERS);

                parser.setProgressCallback([hWnd](int progressValue)
                    {
                        PostMessage(hWnd, WM_PROGRESS_UPDATE, static_cast<WPARAM>(progressValue), 0);
                    });

                parser.parse();

                PostMessage(hWnd, WM_PARSING_SUCCESS, 0, 0);
            }
            catch (const RailwayParserException& exception)
            {
                std::string* errorMessage =
                    new std::string(std::string("Erreur GeoParser : ") + exception.what());

                PostMessage(hWnd, WM_PARSING_ERROR, 0, reinterpret_cast<LPARAM>(errorMessage));
            }
            catch (const std::exception& exception)
            {
                std::string* errorMessage =
                    new std::string(std::string("Exception standard : ") + exception.what());

                PostMessage(hWnd, WM_PARSING_ERROR, 0, reinterpret_cast<LPARAM>(errorMessage));
            }
            catch (...)
            {
                std::string* errorMessage =
                    new std::string("Erreur inconnue durant le parsing.");

                PostMessage(hWnd, WM_PARSING_ERROR, 0, reinterpret_cast<LPARAM>(errorMessage));
            }
        }).detach();
}


// =============================================================================
// Point d'entrée
// =============================================================================

/**
 * @brief Point d'entrée principal de l'application Win32.
 *
 * @param hInstance     Instance de l'application.
 * @param hPrevInstance Non utilisé.
 * @param lpCmdLine     Ligne de commande.
 * @param nCmdShow      Mode d'affichage initial de la fenêtre.
 *
 * @return Code retour du programme.
 */
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialise les chaînes globales
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SIMULATEURFERROVIAIRE, szWindowClass, MAX_LOADSTRING);

    MyRegisterClass(hInstance);

    // Effectue l'initialisation de l'application
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SIMULATEURFERROVIAIRE));

    MSG msg;

    // Boucle de messages principale
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

/**
 * @brief Enregistre la classe de fenêtre principale.
 *
 * @param hInstance Instance de l'application.
 * @return Atome représentant la classe enregistrée.
 */
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW windowClassDescriptor;

    windowClassDescriptor.cbSize = sizeof(WNDCLASSEX);

    windowClassDescriptor.style = CS_HREDRAW | CS_VREDRAW;
    windowClassDescriptor.lpfnWndProc = WndProc;
    windowClassDescriptor.cbClsExtra = 0;
    windowClassDescriptor.cbWndExtra = 0;
    windowClassDescriptor.hInstance = hInstance;
    windowClassDescriptor.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SIMULATEURFERROVIAIRE));
    windowClassDescriptor.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClassDescriptor.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClassDescriptor.lpszMenuName = MAKEINTRESOURCEW(IDC_SIMULATEURFERROVIAIRE);
    windowClassDescriptor.lpszClassName = szWindowClass;
    windowClassDescriptor.hIconSm = LoadIcon(windowClassDescriptor.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&windowClassDescriptor);
}


// =============================================================================
// Initialisation de l'instance
// =============================================================================

/**
 * @brief Crée et affiche la fenêtre principale.
 *
 * @param hInstance Instance de l'application.
 * @param nCmdShow  Mode d'affichage initial.
 * @return TRUE si succès, FALSE sinon.
 */
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HWND hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        0,
        900,
        650,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Création de la barre de progression sous la barre de menu.
    g_progressBar.create(hWnd, 20, 50, 320, 24);
    g_progressBar.show(false); // Masquée par défaut, affichée lors du parsing
    g_progressBar.setProgress(0);

    return TRUE;
}


// =============================================================================
// Procédure principale de fenêtre
// =============================================================================

/**
 * @brief Traite les messages de la fenêtre principale.
 *
 * @param hWnd    Fenêtre concernée.
 * @param message Identifiant du message.
 * @param wParam  Paramètre WPARAM.
 * @param lParam  Paramètre LPARAM.
 * @return Résultat du traitement du message.
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        const int commandId = LOWORD(wParam);

        switch (commandId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_FILE_OPEN:
        {
            char filePathBuffer[MAX_PATH] = { 0 };

            OPENFILENAMEA openFileDescriptor = {};
            openFileDescriptor.lStructSize = sizeof(openFileDescriptor);
            openFileDescriptor.hwndOwner = hWnd;
            openFileDescriptor.lpstrFilter =
                "GeoJSON Files (*.geojson)\0*.geojson\0"
                "JSON Files (*.json)\0*.json\0"
                "All Files (*.*)\0*.*\0";
            openFileDescriptor.lpstrFile = filePathBuffer;
            openFileDescriptor.nMaxFile = MAX_PATH;
            openFileDescriptor.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            openFileDescriptor.lpstrTitle = "Sélectionner un fichier GeoJSON";

            if (GetOpenFileNameA(&openFileDescriptor))
            {
                g_progressBar.setProgress(0);
                g_progressBar.show(true);
                LaunchGeoParsingAsync(hWnd, filePathBuffer);
            }
            break;
        }

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_PROGRESS_UPDATE:
    {
        const int progressValue = static_cast<int>(wParam);
        g_progressBar.setProgress(progressValue);
        return 0;
    }

    case WM_PARSING_SUCCESS:
    {
        g_progressBar.setProgress(100);
        MessageBoxA(hWnd, "Parsing terminé avec succès.", "Succès", MB_OK | MB_ICONINFORMATION);
        g_progressBar.show(false);
        return 0;
    }

    case WM_PARSING_ERROR:
    {
        std::string* errorMessage = reinterpret_cast<std::string*>(lParam);

        g_progressBar.show(false);
        g_progressBar.setProgress(0);
        

        if (errorMessage != nullptr)
        {
            MessageBoxA(hWnd, errorMessage->c_str(), "Erreur", MB_OK | MB_ICONERROR);
            delete errorMessage;
        }
        else
        {
            MessageBoxA(hWnd, "Erreur inconnue.", "Erreur", MB_OK | MB_ICONERROR);
        }

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT paintStruct;
        HDC deviceContext = BeginPaint(hWnd, &paintStruct);

        // Zone de dessin future de l'application.
        EndPaint(hWnd, &paintStruct);
    }
    return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}


// =============================================================================
// Boîte de dialogue "À propos"
// =============================================================================

/**
 * @brief Gestionnaire de messages de la boîte de dialogue "À propos".
 *
 * @param hDlg    Handle de la boîte de dialogue.
 * @param message Message reçu.
 * @param wParam  Paramètre WPARAM.
 * @param lParam  Paramètre LPARAM.
 * @return TRUE si le message est traité, FALSE sinon.
 */
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }

    return (INT_PTR)FALSE;
}
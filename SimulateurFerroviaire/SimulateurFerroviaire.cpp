/**
 * @file SimulateurFerroviaire.cpp
 * @brief Point d'entrée unique de l'application Win32.
 *
 * Ce fichier a une seule responsabilité : instancier l'objet @ref Application
 * et lui déléguer l'intégralité du démarrage. Toute logique métier, UI ou
 * système est confinée dans les classes dédiées.
 *
 * @see Application
 */

#include "framework.h"
#include "SimulateurFerroviaire.h"
#include "Engine/Core/Application.h"


/**
 * @brief Point d'entrée principal de l'application Win32.
 *
 * @param hInstance     Handle de l'instance courante du processus.
 * @param hPrevInstance Toujours nullptr depuis Win32 (héritage Win16, non utilisé).
 * @param lpCmdLine     Arguments de la ligne de commande (non utilisés).
 * @param nCmdShow      Indique comment la fenêtre doit être affichée initialement.
 *
 * @return Code de sortie du programme (0 = succès, autre = erreur).
 */
int APIENTRY wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    try
    {
        Application app(hInstance, nCmdShow);
        return app.run();
    }
    catch (const std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);  // ← e.what() au lieu de GetLastError()
        return -1;
    }   
}

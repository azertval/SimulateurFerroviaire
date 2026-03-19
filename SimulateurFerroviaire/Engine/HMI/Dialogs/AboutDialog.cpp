/**
 * @file AboutDialog.cpp
 * @brief Implémentation de la boîte de dialogue "À propos".
 *
 * @see AboutDialog
 */

#include "framework.h"
#include "AboutDialog.h"
#include "SimulateurFerroviaire.h"


// =============================================================================
// Interface publique
// =============================================================================

void AboutDialog::show(HWND hParent, HINSTANCE hInstance)
{
    DialogBox(
        hInstance,
        MAKEINTRESOURCE(IDD_ABOUTBOX),
        hParent,
        AboutDialog::dialogProc);
}


// =============================================================================
// Procédure de dialogue Win32
// =============================================================================

INT_PTR CALLBACK AboutDialog::dialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
        return static_cast<INT_PTR>(TRUE);

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return static_cast<INT_PTR>(TRUE);
        }
        break;
    }

    return static_cast<INT_PTR>(FALSE);
}

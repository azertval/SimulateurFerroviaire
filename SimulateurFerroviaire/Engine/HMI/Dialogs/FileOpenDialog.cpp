/**
 * @file FileOpenDialog.cpp
 * @brief Implémentation du sélecteur de fichier GeoJSON natif Win32.
 *
 * @see FileOpenDialog
 */

#include "framework.h"
#include "FileOpenDialog.h"

#include <commdlg.h>


// =============================================================================
// Interface publique
// =============================================================================

std::optional<std::string> FileOpenDialog::open(HWND hParent)
{
    char filePathBuffer[MAX_PATH] = { 0 };

    OPENFILENAMEA descriptor = {};
    descriptor.lStructSize  = sizeof(descriptor);
    descriptor.hwndOwner    = hParent;
    descriptor.lpstrFilter  =
        "GeoJSON Files (*.geojson)\0*.geojson\0"
        "JSON Files (*.json)\0*.json\0"
        "All Files (*.*)\0*.*\0";
    descriptor.lpstrFile    = filePathBuffer;
    descriptor.nMaxFile     = MAX_PATH;
    descriptor.Flags        = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    descriptor.lpstrTitle   = "Sélectionner un fichier GeoJSON";

    if (!GetOpenFileNameA(&descriptor))
    {
        return std::nullopt; // Annulation utilisateur ou erreur système
    }

    return std::string(filePathBuffer);
}

/**
 * @file FileSaveDialog.cpp
 * @brief Implémentation du sélecteur de fichier GeoJSON natif Win32.
 *
 * @see FileSaveDialog
 */

#include "framework.h"
#include "FileSaveDialog.h"

#include <commdlg.h>


 // =============================================================================
 // Interface publique
 // =============================================================================

std::optional<std::string> FileSaveDialog::save(HWND hParent)
{
    char filePathBuffer[MAX_PATH] = { 0 };

    OPENFILENAMEA descriptor = {};
    descriptor.lStructSize = sizeof(descriptor);
    descriptor.hwndOwner = hParent;
    descriptor.lpstrFilter =
        "GeoJSON Files (*.geojson)\0*.geojson\0"
        "JSON Files (*.json)\0*.json\0"
        "All Files (*.*)\0*.*\0";
    descriptor.lpstrFile = filePathBuffer;
    descriptor.nMaxFile = MAX_PATH;
    descriptor.lpstrDefExt = "geojson";   // extension ajoutée si absente
    descriptor.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    descriptor.lpstrTitle = "Créer un fichier GeoJSON";

    if (!GetSaveFileNameA(&descriptor))
    {
        return std::nullopt; // Annulation utilisateur ou erreur système
    }

    return std::string(filePathBuffer);
}

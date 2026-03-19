/**
 * @file FileOpenDialog.h
 * @brief Déclaration du sélecteur de fichier GeoJSON.
 *
 * La classe @ref FileOpenDialog encapsule l'appel à @c GetOpenFileNameA
 * derrière une interface moderne retournant un @c std::optional.
 * Cela évite de mélanger la logique de sélection de fichier avec le
 * gestionnaire de commandes de @ref MainWindow.
 */

#pragma once

#include "framework.h"

#include <optional>
#include <string>


/**
 * @class FileOpenDialog
 * @brief Sélecteur de fichier GeoJSON (ou JSON) natif Win32.
 *
 * Cette classe est purement statique : elle n'a pas d'état et ne doit
 * pas être instanciée.
 *
 * Usage :
 * @code
 *   std::optional<std::string> path = FileOpenDialog::open(hWnd);
 *   if (path.has_value())
 *   {
 *       // Utiliser path.value()
 *   }
 * @endcode
 */
class FileOpenDialog
{
public:

    /**
     * @brief Affiche le sélecteur de fichier natif Windows.
     *
     * Filtre par défaut : fichiers @c .geojson, @c .json et @c *.*.
     * Si l'utilisateur annule, retourne @c std::nullopt.
     *
     * @param hParent  Fenêtre propriétaire du dialogue (pour le centrage et la modalité).
     *
     * @return Chemin absolu du fichier sélectionné, ou @c std::nullopt si l'utilisateur
     *         a annulé ou si une erreur système est survenue.
     */
    static std::optional<std::string> open(HWND hParent);

    /** @brief Classe non instanciable — constructeur supprimé. */
    FileOpenDialog() = delete;
};

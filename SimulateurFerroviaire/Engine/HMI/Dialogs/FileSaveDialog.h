/**
 * @file FileSaveDialog.h
 * @brief Déclaration du sélecteur de sauvegarde de fichier GeoJSON.
 *
 * La classe @ref FileSaveDialog encapsule l'appel à @c GetExporteFileNameA
 * derrière une interface moderne retournant un @c std::optional.
 * Cela évite de mélanger la logique de sélection de fichier avec le
 * gestionnaire de commandes de @ref MainWindow.
 */

#pragma once

#include "framework.h"

#include <optional>
#include <string>


 /**
  * @class FileSaveDialog
  * @brief Sauvegarde de fichier GeoJSON (ou JSON) natif Win32.
  *
  * Cette classe est purement statique : elle n'a pas d'état et ne doit
  * pas être instanciée.
  *
  * Usage :
  * @code
  *   std::optional<std::string> path = FileSaveDialog::save(hWnd);
  *   if (path.has_value())
  *   {
  *       // Utiliser path.value()
  *   }
  * @endcode
  */
class FileSaveDialog
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
    static std::optional<std::string> save(HWND hParent);

    /** @brief Classe non instanciable — constructeur supprimé. */
    FileSaveDialog() = delete;
};

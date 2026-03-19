/**
 * @file GeoParsingTask.h
 * @brief Déclaration de la tâche de parsing GeoJSON asynchrone.
 *
 * La classe @ref GeoParsingTask isole l'exécution du @ref GeoParser dans un
 * thread détaché. Elle pilote la communication inter-threads via @c PostMessage
 * pour garantir que l'UI reste toujours manipulée depuis le thread principal.
 *
 * @par Protocole de messages
 * | Message              | WPARAM          | LPARAM                                  |
 * |----------------------|-----------------|-----------------------------------------|
 * | @c WM_PROGRESS_UPDATE | valeur 0–100   | —                                       |
 * | @c WM_PARSING_SUCCESS | —              | —                                       |
 * | @c WM_PARSING_ERROR   | —              | @c std::string* (à libérer par le destinataire) |
 *
 * @note Les identifiants de messages (@c WM_USER+1/2/3) doivent correspondre
 *       exactement à ceux déclarés dans @ref MainWindow.cpp.
 */

#pragma once

#include "framework.h"

#include <string>


/**
 * @class GeoParsingTask
 * @brief Tâche asynchrone encapsulant l'exécution du GeoParser.
 *
 * Cette classe est purement statique : elle n'a pas d'état et ne doit
 * pas être instanciée.
 *
 * Usage :
 * @code
 *   GeoParsingTask::launch(hWnd, "C:/data/reseau.geojson");
 * @endcode
 *
 * Le thread lancé est détaché (@c std::thread::detach). La communication
 * vers l'UI passe exclusivement par @c PostMessage ; le thread de parsing
 * ne touche jamais directement aux éléments Win32.
 */
class GeoParsingTask
{
public:

    /**
     * @brief Lance le parsing GeoJSON dans un thread séparé et détaché.
     *
     * Le thread émet les messages suivants vers @p hWnd :
     *  - @c WM_PROGRESS_UPDATE (WPARAM = avancement 0–100) à chaque étape.
     *  - @c WM_PARSING_SUCCESS à la fin d'un parsing réussi.
     *  - @c WM_PARSING_ERROR (LPARAM = pointeur @c std::string) en cas d'échec.
     *
     * @param hWnd         Fenêtre destinataire des messages de progression.
     *                     Doit rester valide pendant toute la durée du parsing.
     * @param geoJsonPath  Chemin absolu vers le fichier GeoJSON à analyser.
     *
     * @warning Le @c std::string* transporté par @c WM_PARSING_ERROR doit être
     *          libéré (@c delete) par le destinataire du message.
     */
    static void launch(HWND hWnd, const std::string& geoJsonPath);

    /** @brief Classe non instanciable — constructeur supprimé. */
    GeoParsingTask() = delete;
};

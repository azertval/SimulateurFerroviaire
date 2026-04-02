/**
 * @file  GeoParsingTask.h
 * @brief Tâche asynchrone d'exécution du pipeline GeoParser v2.
 *
 * Lance @ref GeoParser dans un thread détaché et communique la progression
 * vers @ref MainWindow via @c PostMessage :
 *
 * | Message               | Contenu                                    |
 * |-----------------------|--------------------------------------------|
 * | WM_PROGRESS_UPDATE    | wParam = progression 0-100                |
 * | WM_PARSING_SUCCESS    | —                                          |
 * | WM_PARSING_ERROR      | wParam = std::wstring* (à libérer)         |
 * | WM_PARSING_CANCELLED  | —                                          |
 *
 * @par Annulation
 * @c cancel() positionne le token atomique partagé avec @ref GeoParser.
 * Le thread GeoParser lève @c ParseCancelledException entre les phases
 * et @c GeoParsingTask poste @c WM_PARSING_CANCELLED.
 */
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <windows.h>

#include "Engine/Core/Config/ParserConfig.h"
#include "Engine/Core/Logger/Logger.h"

class GeoParsingTask
{
public:

    /**
     * @brief Construit la tâche avec la fenêtre de destination des messages.
     *
     * @param hwndTarget  Handle de @ref MainWindow — destinataire des PostMessage.
     * @param logger      Référence au logger GeoParser.
     */
    explicit GeoParsingTask(HWND hwndTarget);

    /**
     * @brief Lance le parsing dans un thread détaché.
     *
     * Prend un snapshot de @p config (immuable pendant le parsing).
     * Réinitialise le cancel token si un parsing précédent avait été annulé.
     *
     * @param filePath  Chemin absolu vers le fichier GeoJSON.
     * @param config    Configuration du pipeline — copiée.
     */
    void start(const std::string& filePath, const ParserConfig& config);

    /**
     * @brief Demande l'annulation du parsing en cours.
     *
     * Positionne le cancel token partagé avec @ref GeoParser.
     * L'annulation est asynchrone — le thread se termine proprement
     * entre deux phases et poste @c WM_PARSING_CANCELLED.
     *
     * No-op si aucun parsing n'est en cours.
     */
    void cancel();

    /**
     * @brief Indique si une annulation a été demandée.
     *
     * @return @c true si @c cancel() a été appelé et que le thread n'a pas encore terminé.
     */
    [[nodiscard]] bool isCancelling() const;

private:

    HWND    m_hwndTarget;   ///< Fenêtre destinataire des PostMessage.
    Logger m_logger{ "GeoParser" };       ///< Logger GeoParser.

    /**
     * Cancel token partagé avec le thread GeoParser.
     * shared_ptr — garantit que le token reste valide même si GeoParsingTask
     * est détruit avant la fin du thread.
     */
    std::shared_ptr<std::atomic<bool>> m_cancelToken;
};
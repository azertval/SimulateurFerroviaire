/**
 * @file  GeoParsingTask.cpp
 * @brief Implémentation de la tâche asynchrone GeoParsingTask v2.
 *
 * @see GeoParsingTask
 */
#include "GeoParsingTask.h"

#include <thread>

#include "GeoParser.h"
#include "Resource.h"


 // =============================================================================
 // Construction
 // =============================================================================

GeoParsingTask::GeoParsingTask(HWND hwndTarget)
    : m_hwndTarget(hwndTarget)
    , m_cancelToken(std::make_shared<std::atomic<bool>>(false))
{
}

// =============================================================================
// Lancement
// =============================================================================

void GeoParsingTask::start(const std::string& filePath,
    const ParserConfig& config)
{
    // Réinitialise le token — un parsing précédent annulé laisse le token à true
    m_cancelToken->store(false);

    // Copies pour le thread — durée de vie garantie
    const std::string  pathCopy = filePath;
    const ParserConfig configCopy = config;
    const HWND         hwnd = m_hwndTarget;
    auto               token = m_cancelToken;  // shared_ptr copié
    Logger& logger = m_logger;

    std::thread([pathCopy, configCopy, hwnd, token, &logger]()
        {
            try
            {
                GeoParser parser(configCopy, logger,
                    [hwnd](int progress, const std::wstring& label)
                    {
                        // Alloue le label sur le tas — libéré par MainWindow::onProgressUpdate
                        auto* labelCopy = new std::wstring(label);
                        PostMessageW(hwnd, WM_PROGRESS_UPDATE,
                            static_cast<WPARAM>(progress),
                            reinterpret_cast<LPARAM>(labelCopy));
                    });

                parser.parse(pathCopy, token);

                PostMessageW(hwnd, WM_PARSING_SUCCESS, 0, 0);
            }
            catch (const GeoParser::CancelledException&)
            {
                // Annulation propre — pas une erreur
                PostMessageW(hwnd, WM_PARSING_CANCELLED, 0, 0);
            }
            catch (const std::exception& e)
            {
                // Erreur réelle — alloue le message (MainWindow::onParsingError le libère)
                const char* what = e.what();
                auto* msg = new std::wstring(what, what + std::strlen(what));
                PostMessageW(hwnd, WM_PARSING_ERROR,
                    0,
                    reinterpret_cast<LPARAM>(msg));
            }
        }).detach();
}


// =============================================================================
// Annulation
// =============================================================================

void GeoParsingTask::cancel()
{
    m_cancelToken->store(true);
}

bool GeoParsingTask::isCancelling() const
{
    return m_cancelToken->load();
}
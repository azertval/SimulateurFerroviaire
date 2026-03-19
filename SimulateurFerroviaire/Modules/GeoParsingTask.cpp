/**
 * @file GeoParsingTask.cpp
 * @brief Implémentation de la tâche de parsing GeoJSON asynchrone.
 *
 * @see GeoParsingTask
 */

#include "framework.h"
#include "GeoParsingTask.h"

#include "Modules/GeoParser/GeoParser.h"
#include "Modules/GeoParser/Exceptions/GeoParserException.h"
#include "Engine/Core/Logger/Logger.h"

#include <thread>

// Messages inter-threads (doivent correspondre à ceux de MainWindow.cpp)
static constexpr UINT WM_PROGRESS_UPDATE = WM_USER + 1;
static constexpr UINT WM_PARSING_SUCCESS = WM_USER + 2;
static constexpr UINT WM_PARSING_ERROR   = WM_USER + 3;


// =============================================================================
// Interface publique
// =============================================================================

void GeoParsingTask::launch(HWND hWnd, const std::string& geoJsonPath)
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

            // Le callback est le seul point de contact entre le thread de parsing
            // et l'UI. PostMessage est thread-safe ; aucune référence directe
            // à un objet UI n'est tolérée ici.
            parser.setProgressCallback([hWnd](int progressValue)
            {
                PostMessage(hWnd, WM_PROGRESS_UPDATE, static_cast<WPARAM>(progressValue), 0);
            });

            parser.parse();

            PostMessage(hWnd, WM_PARSING_SUCCESS, 0, 0);
        }
        catch (const RailwayParserException& exception)
        {
            auto* errorMessage = new std::string(
                std::string("Erreur GeoParser : ") + exception.what());

            PostMessage(hWnd, WM_PARSING_ERROR, 0, reinterpret_cast<LPARAM>(errorMessage));
        }
        catch (const std::exception& exception)
        {
            auto* errorMessage = new std::string(
                std::string("Exception standard : ") + exception.what());

            PostMessage(hWnd, WM_PARSING_ERROR, 0, reinterpret_cast<LPARAM>(errorMessage));
        }
        catch (...)
        {
            auto* errorMessage = new std::string("Erreur inconnue durant le parsing.");

            PostMessage(hWnd, WM_PARSING_ERROR, 0, reinterpret_cast<LPARAM>(errorMessage));
        }
    }).detach();
}

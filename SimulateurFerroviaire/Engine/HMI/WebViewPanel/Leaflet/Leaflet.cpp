/**
 * @file Leaflet.cpp
 * @brief Implémentation du générateur HTML Leaflet.
 *
 * @see Leaflet
 */

#include "Leaflet.h"
#include "Engine/HMI/Utils/PathUtils.h"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

// -----------------------------------------------------------------------------
// Leaflet
// -----------------------------------------------------------------------------

std::wstring Leaflet::leafletHtml()
{
    std::filesystem::path htmlPath =
        executableDirectory() / "Resources" / "leaflet.html";

    std::wifstream file(htmlPath);
    if (!file.is_open())
    {
        throw std::runtime_error(
            "Cannot open leaflet.html at: " + htmlPath.string()
        );
    }

    std::wstringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
/**
 * @file Leaflet.cpp
 * @brief Implémentation du générateur HTML Leaflet.
 *
 * @see Leaflet
 */

#include "Leaflet.h"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

 // -----------------------------------------------------------------------------
 // Internal helpers
 // -----------------------------------------------------------------------------

static std::filesystem::path executableDirectory()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

// -----------------------------------------------------------------------------
// Leaflet
// -----------------------------------------------------------------------------

std::wstring Leaflet::leafletHtml()
{
    std::filesystem::path htmlPath =
        executableDirectory() / "Resources" / "Leaflet.html";

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
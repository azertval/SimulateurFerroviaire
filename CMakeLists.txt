cmake_minimum_required(VERSION 3.20)

# -----------------------------------------------------------------------------
# Projet
# -----------------------------------------------------------------------------
project(SimulateurFerroviaire LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Standard C++
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Configuration des dossiers
# -----------------------------------------------------------------------------
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SimulateurFerroviaire)

# -----------------------------------------------------------------------------
# Récupération automatique des fichiers
# -----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES
    "${SOURCE_DIR}/*.cpp"
    "${SOURCE_DIR}/*.h"
    "${SOURCE_DIR}/*.rc"
)

# -----------------------------------------------------------------------------
# Executable
# -----------------------------------------------------------------------------
add_executable(SimulateurFerroviaire WIN32 ${SOURCES})

# -----------------------------------------------------------------------------
# Include directories
# -----------------------------------------------------------------------------
target_include_directories(SimulateurFerroviaire PRIVATE
    ${SOURCE_DIR}
    ${SOURCE_DIR}/External
    ${CMAKE_CURRENT_SOURCE_DIR}/External   # <-- important pour CI
)

# -----------------------------------------------------------------------------
# WebView2 (compatible LOCAL + CI)
# -----------------------------------------------------------------------------

# 1. Chemin principal (CI / repo root)
set(WEBVIEW2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/External/WebView2")

# 2. Fallback local (si tu avais un autre layout)
if(NOT EXISTS "${WEBVIEW2_ROOT}/lib/x64/WebView2LoaderStatic.lib")
    set(WEBVIEW2_ROOT "${SOURCE_DIR}/External/WebView2")
endif()

# 3. Vérification stricte
if(NOT EXISTS "${WEBVIEW2_ROOT}/lib/x64/WebView2LoaderStatic.lib")
    message(FATAL_ERROR
        "WebView2 NOT FOUND.\n"
        "Expected:\n"
        "  ${CMAKE_CURRENT_SOURCE_DIR}/External/WebView2\n"
        "or\n"
        "  ${SOURCE_DIR}/External/WebView2\n"
    )
endif()

message(STATUS "WebView2 found at: ${WEBVIEW2_ROOT}")

# 4. Import lib
add_library(WebView2 STATIC IMPORTED)

set_target_properties(WebView2 PROPERTIES
    IMPORTED_LOCATION "${WEBVIEW2_ROOT}/lib/x64/WebView2LoaderStatic.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${WEBVIEW2_ROOT}/include"
)

# 5. Link
target_link_libraries(SimulateurFerroviaire PRIVATE WebView2)

# -----------------------------------------------------------------------------
# Dépendances Windows
# -----------------------------------------------------------------------------
if (WIN32)
    target_link_libraries(SimulateurFerroviaire PRIVATE
        user32
        gdi32
        comctl32
        comdlg32
        ole32       
        oleaut32   
        version    
    )
endif()

# -----------------------------------------------------------------------------
# Définitions de compilation
# -----------------------------------------------------------------------------
target_compile_definitions(SimulateurFerroviaire PRIVATE
    # Debug / Release
    $<$<CONFIG:Debug>:DEBUG_BUILD>
    $<$<CONFIG:Release>:NDEBUG>

    # Supprime les macros min/max de <windows.h> qui écrasent std::min/std::max
    NOMINMAX

    # Unicode Win32 — force les versions W des APIs (CreateWindowExW, etc.)
    UNICODE
    _UNICODE

    # Cible Windows 10 minimum — active les APIs modernes
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00

    # Exclut les parties rarement utilisées de <windows.h> — accélère la compilation
    WIN32_LEAN_AND_MEAN
    VC_EXTRA_LEAN
)

# -----------------------------------------------------------------------------
# Options de compilation MSVC
# -----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(SimulateurFerroviaire PRIVATE
        /W4             # Niveau d'avertissement élevé
        /WX-            # Avertissements non fatals (ne bloque pas le build)
        /utf-8          # Source et exécution en UTF-8 — évite les pb d'encodage
        /Zc:__cplusplus # Expose la vraie valeur de __cplusplus (sinon MSVC triche)
        /permissive-    # Mode strict — rejette les extensions non-standard
        /wd4100         # Silence C4100 : paramètre non référencé (trop verbeux)
        /wd4505         # Silence C4505 : fonction statique non référencée
    )
endif()

# -----------------------------------------------------------------------------
# Logs dans le bon dossier (exe)
# -----------------------------------------------------------------------------
add_custom_command(TARGET SimulateurFerroviaire POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:SimulateurFerroviaire>/Logs
)

# -----------------------------------------------------------------------------
# Dossier Config (parser_settings.ini)
# -----------------------------------------------------------------------------
add_custom_command(TARGET SimulateurFerroviaire POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:SimulateurFerroviaire>/Config"
    COMMENT "Creating Config directory for parser_settings.ini"
)

# -----------------------------------------------------------------------------
# Ressources HTML
# -----------------------------------------------------------------------------
add_custom_command(TARGET SimulateurFerroviaire POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:SimulateurFerroviaire>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SOURCE_DIR}/Engine/HMI/WebViewPanel/Leaflet/leaflet.html"
        "$<TARGET_FILE_DIR:SimulateurFerroviaire>/Resources/Leaflet.html"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SOURCE_DIR}/Engine/HMI/WebViewPanel/Leaflet/leaflet_api.js"
        "$<TARGET_FILE_DIR:SimulateurFerroviaire>/Resources/leaflet_api.js"
    COMMENT "Copying leaflet.html to output directory"
)
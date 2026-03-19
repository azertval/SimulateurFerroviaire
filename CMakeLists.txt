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
    )
endif()

# -----------------------------------------------------------------------------
# Définition Debug / Release
# -----------------------------------------------------------------------------
target_compile_definitions(SimulateurFerroviaire PRIVATE
    $<$<CONFIG:Debug>:DEBUG_BUILD>
    $<$<CONFIG:Release>:NDEBUG>
)

# -----------------------------------------------------------------------------
# Logs dans le bon dossier (exe)
# -----------------------------------------------------------------------------
add_custom_command(TARGET SimulateurFerroviaire POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:SimulateurFerroviaire>/Logs
)
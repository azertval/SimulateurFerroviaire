#pragma once
#include <filesystem>
#include <windows.h>

static std::filesystem::path executableDirectory()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

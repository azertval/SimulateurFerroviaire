/**
 * @file  ParserConfigIni.cpp
 * @brief Implémentation de la persistence .ini de ParserConfig.
 *
 * @see ParserConfigIni
 */
#include "ParserConfigIni.h"

#include "External/SimpleIni/SimpleIni.h"

#include <stdexcept>
#include <windows.h>


 // =============================================================================
 // Helpers privés
 // =============================================================================

double ParserConfigIni::getDouble(const void* iniPtr,
    const char* section,
    const char* key,
    double defaultVal)
{
    const auto* ini = static_cast<const CSimpleIniA*>(iniPtr);
    const char* raw = ini->GetValue(section, key, nullptr);
    if (!raw) return defaultVal;
    try { return std::stod(raw); }
    catch (...) { return defaultVal; }
}


// =============================================================================
// Chemin par défaut
// =============================================================================

std::string ParserConfigIni::defaultPath()
{
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    std::string path(exePath);
    const size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos)
        path = path.substr(0, lastSlash + 1);

    return path + "Config\\parser_settings.ini";
}


// =============================================================================
// Chargement
// =============================================================================

ParserConfig ParserConfigIni::load(const std::string& path)
{
    ParserConfig cfg;

    CSimpleIniA ini;
    ini.SetUnicode(false);

    if (ini.LoadFile(path.c_str()) < 0)
        return cfg;

    // [Topology]
    cfg.snapTolerance = getDouble(&ini, "Topology", "SnapTolerance", cfg.snapTolerance);
    cfg.maxSegmentLength = getDouble(&ini, "Topology", "MaxSegmentLength", cfg.maxSegmentLength);

    // [Intersection]
    cfg.intersectionEpsilon = getDouble(&ini, "Intersection", "IntersectionEpsilon", cfg.intersectionEpsilon);

    // [Switch]
    cfg.minSwitchAngle = getDouble(&ini, "Switch", "MinSwitchAngle", cfg.minSwitchAngle);
    cfg.junctionTrimMargin = getDouble(&ini, "Switch", "JunctionTrimMargin", cfg.junctionTrimMargin);
    cfg.doubleSwitchRadius = getDouble(&ini, "Switch", "DoubleSwitchRadius", cfg.doubleSwitchRadius);
    cfg.switchSideSize = getDouble(&ini, "Switch", "SwitchSideSize", cfg.switchSideSize);

    // [CDC]
    cfg.minBranchLength = getDouble(&ini, "CDC", "MinBranchLength", cfg.minBranchLength);

    return cfg;
}


// =============================================================================
// Sauvegarde
// =============================================================================

void ParserConfigIni::save(const std::string& path, const ParserConfig& cfg)
{
    CSimpleIniA ini;
    ini.SetUnicode(false);
    ini.LoadFile(path.c_str());

    auto setVal = [&](const char* s, const char* k, double v)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.4g", v);
            ini.SetValue(s, k, buf);
        };

    setVal("Topology", "SnapTolerance", cfg.snapTolerance);
    setVal("Topology", "MaxSegmentLength", cfg.maxSegmentLength);
    setVal("Intersection", "IntersectionEpsilon", cfg.intersectionEpsilon);
    setVal("Switch", "MinSwitchAngle", cfg.minSwitchAngle);
    setVal("Switch", "JunctionTrimMargin", cfg.junctionTrimMargin);
    setVal("Switch", "DoubleSwitchRadius", cfg.doubleSwitchRadius);
    setVal("Switch", "SwitchSideSize", cfg.switchSideSize);
    setVal("CDC", "MinBranchLength", cfg.minBranchLength);

    if (ini.SaveFile(path.c_str()) < 0)
        throw std::runtime_error("ParserConfigIni::save — impossible d'écrire : " + path);
}
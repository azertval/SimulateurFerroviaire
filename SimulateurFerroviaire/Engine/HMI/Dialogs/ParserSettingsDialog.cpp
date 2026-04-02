/**
 * @file  ParserSettingsDialog.cpp
 * @brief Implémentation du dialogue de paramètres GeoParser.
 *
 * @see ParserSettingsDialog
 */
#include "ParserSettingsDialog.h"

#include <cstdio>
#include <stdexcept>

#include "Engine/Core/Config/ParserConfigIni.h"
#include "resource.h"   // IDD_PARSER_SETTINGS, IDC_EDIT_SNAP_TOLERANCE, etc.


 // =============================================================================
 // Point d'entrée
 // =============================================================================

bool ParserSettingsDialog::show(HWND hParent,
    ParserConfig& config,
    const std::string& iniPath)
{
    DlgData data{ &config, iniPath, false };

    DialogBoxParamW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_PARSER_SETTINGS),
        hParent,
        dialogProc,
        reinterpret_cast<LPARAM>(&data));

    return data.accepted;
}


// =============================================================================
// DialogProc
// =============================================================================

INT_PTR CALLBACK ParserSettingsDialog::dialogProc(HWND hDlg, UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    // Stocke DlgData* dans GWLP_USERDATA lors de WM_INITDIALOG
    DlgData* data = reinterpret_cast<DlgData*>(
        GetWindowLongPtrW(hDlg, GWLP_USERDATA));

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        data = reinterpret_cast<DlgData*>(lParam);
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
        populateFields(hDlg, *data->config);
        return TRUE;
    }

    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);

        if (id == IDOK)
        {
            ParserConfig newCfg = readFields(hDlg);
            std::wstring errorMsg;

            if (!validate(newCfg, errorMsg))
            {
                MessageBoxW(hDlg, errorMsg.c_str(),
                    L"Paramètres invalides", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            // Sauvegarde + mise à jour de la config parente
            try { ParserConfigIni::save(data->iniPath, newCfg); }
            catch (...) { /* Ignoré — la config reste valide en mémoire */ }

            *data->config = newCfg;
            data->accepted = true;
            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        if (id == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }

        if (id == IDC_BTN_RESET)
        {
            // Réinitialise les champs aux valeurs par défaut
            populateFields(hDlg, ParserConfig{});
            return TRUE;
        }

        return FALSE;
    }

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}


// =============================================================================
// Helpers
// =============================================================================

void ParserSettingsDialog::populateFields(HWND hDlg, const ParserConfig& cfg)
{
    auto setField = [&](int id, double value)
        {
            wchar_t buf[64];
            std::swprintf(buf, 64, L"%.4g", value);
            SetDlgItemTextW(hDlg, id, buf);
        };

    setField(IDC_EDIT_SNAP_TOLERANCE, cfg.snapTolerance);
    setField(IDC_EDIT_MAX_SEGMENT_LENGTH, cfg.maxSegmentLength);
    setField(IDC_EDIT_INTERSECTION_EPSILON, cfg.intersectionEpsilon);
    setField(IDC_EDIT_MIN_SWITCH_ANGLE, cfg.minSwitchAngle);
    setField(IDC_EDIT_JUNCTION_TRIM_MARGIN, cfg.junctionTrimMargin);
    setField(IDC_EDIT_DOUBLE_SWITCH_RADIUS, cfg.doubleSwitchRadius);
    setField(IDC_EDIT_MIN_BRANCH_LENGTH, cfg.minBranchLength);
    setField(IDC_EDIT_MIN_SWITCH_SIDE_SIZE, cfg.switchSideSize);
}

ParserConfig ParserSettingsDialog::readFields(HWND hDlg)
{
    ParserConfig cfg;   // Valeurs par défaut si champ invalide

    auto getField = [&](int id, double defaultVal) -> double
        {
            wchar_t buf[64] = {};
            GetDlgItemTextW(hDlg, id, buf, 64);
            try { return std::stod(buf); }
            catch (...) { return defaultVal; }
        };

    cfg.snapTolerance = getField(IDC_EDIT_SNAP_TOLERANCE, cfg.snapTolerance);
    cfg.maxSegmentLength = getField(IDC_EDIT_MAX_SEGMENT_LENGTH, cfg.maxSegmentLength);
    cfg.intersectionEpsilon = getField(IDC_EDIT_INTERSECTION_EPSILON, cfg.intersectionEpsilon);
    cfg.minSwitchAngle = getField(IDC_EDIT_MIN_SWITCH_ANGLE, cfg.minSwitchAngle);
    cfg.junctionTrimMargin = getField(IDC_EDIT_JUNCTION_TRIM_MARGIN, cfg.junctionTrimMargin);
    cfg.doubleSwitchRadius = getField(IDC_EDIT_DOUBLE_SWITCH_RADIUS, cfg.doubleSwitchRadius);
    cfg.minBranchLength = getField(IDC_EDIT_MIN_BRANCH_LENGTH, cfg.minBranchLength);
    cfg.switchSideSize = getField(IDC_EDIT_MIN_SWITCH_SIDE_SIZE, cfg.switchSideSize);

    return cfg;
}

bool ParserSettingsDialog::validate(const ParserConfig& cfg,
    std::wstring& errorMsg)
{
    if (cfg.snapTolerance <= 0.0) { errorMsg = L"Tolérance snap doit être > 0"; return false; }
    if (cfg.maxSegmentLength <= 0.0) { errorMsg = L"Longueur max segment doit être > 0"; return false; }
    if (cfg.intersectionEpsilon <= 0.0) { errorMsg = L"Epsilon intersection doit être > 0"; return false; }
    if (cfg.minSwitchAngle <= 0.0 ||
        cfg.minSwitchAngle >= 90.0) {
        errorMsg = L"Angle switch : ]0°, 90°["; return false;
    }
    if (cfg.junctionTrimMargin <= 0.0) { errorMsg = L"Marge trim doit être > 0"; return false; }
    if (cfg.doubleSwitchRadius <= 0.0) { errorMsg = L"Rayon double switch doit être > 0"; return false; }
    if (cfg.minBranchLength <= 0.0) { errorMsg = L"Longueur min branche doit être > 0"; return false; }
    if (cfg.switchSideSize <= 0.0) { errorMsg = L"Longueur min branche doit être > 0"; return false; }

    if (cfg.intersectionEpsilon >= cfg.snapTolerance)
    {
        errorMsg = L"Epsilon intersection doit être < tolérance snap ("
            + std::to_wstring(cfg.snapTolerance) + L" m)";
        return false;
    }

    return true;
}
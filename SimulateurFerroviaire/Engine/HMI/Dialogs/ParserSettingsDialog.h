/**
 * @file  ParserSettingsDialog.h
 * @brief Dialogue modal Win32 de configuration du pipeline GeoParser.
 *
 * Affiche les 7 paramètres de @ref ParserConfig dans des champs EDIT.
 * Valide les valeurs avant d'accepter. Sauvegarde via @ref ParserConfigIni.
 */
#pragma once

#include <string>
#include <windows.h>

#include "Engine/Core/Config/ParserConfig.h"

class ParserSettingsDialog
{
public:

    /**
     * @brief Affiche le dialogue modal.
     *
     * Bloque jusqu'à la fermeture. Si l'utilisateur clique OK et que
     * les valeurs sont valides, @p config est mis à jour et sauvegardé.
     *
     * @param hParent  Fenêtre parente.
     * @param config   Configuration à éditer — modifiée si OK.
     * @param iniPath  Chemin du fichier .ini pour la sauvegarde.
     *
     * @return @c true si l'utilisateur a confirmé (OK), @c false si annulé.
     */
    static bool show(HWND hParent,
        ParserConfig& config,
        const std::string& iniPath);

    ParserSettingsDialog() = delete;

private:

    /** Données transmises à la DialogProc via LPARAM de DialogBoxParam. */
    struct DlgData
    {
        ParserConfig* config = nullptr;
        std::string   iniPath;
        bool          accepted = false;
    };

    /**
     * @brief Procédure de dialogue Win32 (callback).
     *
     * @param hDlg    Handle du dialogue.
     * @param msg     Message Win32.
     * @param wParam  WPARAM.
     * @param lParam  LPARAM — pointe vers DlgData lors de WM_INITDIALOG.
     */
    static INT_PTR CALLBACK dialogProc(HWND hDlg, UINT msg,
        WPARAM wParam, LPARAM lParam);

    /**
     * @brief Remplit les champs EDIT depuis la config.
     *
     * @param hDlg    Handle du dialogue.
     * @param config  Configuration source.
     */
    static void populateFields(HWND hDlg, const ParserConfig& config);

    /**
     * @brief Lit les champs EDIT et produit une ParserConfig.
     *
     * @param hDlg  Handle du dialogue.
     *
     * @return Config lue. Valeurs invalides → valeur par défaut.
     */
    static ParserConfig readFields(HWND hDlg);

    /**
     * @brief Valide la cohérence des paramètres.
     *
     * @param cfg       Config à valider.
     * @param errorMsg  Message d'erreur en cas d'échec.
     *
     * @return @c true si valide.
     */
    static bool validate(const ParserConfig& cfg, std::wstring& errorMsg);
};

/**
 * @file AboutDialog.h
 * @brief Déclaration de la boîte de dialogue "À propos".
 *
 * La classe @ref AboutDialog expose une unique méthode statique @ref show
 * permettant d'afficher la boîte de dialogue modale depuis n'importe quel
 * contexte disposant d'un @c HWND parent valide.
 */

#pragma once

#include "framework.h"


/**
 * @class AboutDialog
 * @brief Boîte de dialogue modale "À propos" de l'application.
 *
 * Cette classe est purement statique : elle n'a pas d'état et ne doit
 * pas être instanciée. Elle encapsule la procédure de dialogue Win32
 * (@ref dialogProc) et la masque derrière une interface publique simple.
 *
 * Usage :
 * @code
 *   AboutDialog::show(hWnd, hInstance);
 * @endcode
 */
class AboutDialog
{
public:

    /**
     * @brief Affiche la boîte de dialogue "À propos" de façon modale.
     *
     * Bloque jusqu'à la fermeture de la boîte de dialogue (bouton OK ou
     * croix de fermeture).
     *
     * @param hParent   Fenêtre propriétaire (utilisée pour le centrage et la
     *                  modalité de la boîte de dialogue).
     * @param hInstance Handle d'instance Win32 nécessaire pour localiser la
     *                  ressource de dialogue (@c IDD_ABOUTBOX).
     */
    static void show(HWND hParent, HINSTANCE hInstance);

private:

    /**
     * @brief Procédure de dialogue Win32 (callback système).
     *
     * Gère @c WM_INITDIALOG pour initialiser la boîte et @c WM_COMMAND pour
     * fermer via OK ou Annuler.
     *
     * @param hDlg    Handle de la boîte de dialogue.
     * @param message Identifiant du message Win32.
     * @param wParam  Paramètre mot du message.
     * @param lParam  Paramètre long du message (non utilisé ici).
     *
     * @return @c TRUE si le message est traité, @c FALSE sinon.
     */
    static INT_PTR CALLBACK dialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

    /** @brief Classe non instanciable — constructeur supprimé. */
    AboutDialog() = delete;
};

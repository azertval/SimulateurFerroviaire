/**
 * @file  ProgressBar.h
 * @brief Barre de progression Win32 avec label de phase et bouton Cancel.
 *
 * Layout vertical :
 *   [Label phase — "Phase 2/9 — Intersections géométriques..."]
 *   [████████░░░░░░░░░░░░░░░░░ 33%                           ]
 *   [                         Annuler                         ]
 *
 * Le bouton Cancel est masqué par défaut — visible uniquement pendant le parsing.
 */
#pragma once

#include <string>
#include <windows.h>

class ProgressBar
{
public:

    /**
     * @brief Crée les contrôles Win32 (label, barre, bouton Cancel).
     *
     * @param hParent    Fenêtre parente.
     * @param hInstance  Handle d'instance Win32.
     * @param x, y       Position dans la fenêtre parente.
     * @param width      Largeur en pixels.
     * @param cancelId   Identifiant de commande du bouton Cancel (ex. IDC_CANCEL_PARSING).
     */
    void create(HWND hParent, HINSTANCE hInstance,
        int x, int y, int width, int cancelId);

    /**
     * @brief Met à jour la progression (0-100).
     *
     * @param value  Valeur de progression.
     */
    void setProgress(int value);

    /**
     * @brief Met à jour le label de phase.
     *
     * @param text  Texte affiché (ex. L"Phase 2/9 — Intersections géométriques...").
     */
    void setLabel(const std::wstring& text);

    /**
     * @brief Affiche le bouton Cancel — à appeler au début du parsing.
     */
    void showCancelButton();

    /**
     * @brief Masque le bouton Cancel — à appeler à la fin / erreur / annulation.
     */
    void hideCancelButton();

    /**
     * @brief Réinitialise la barre à 0 et vide le label.
     */
    void reset();

    /**
    * @brief Show or hide the ProgressBar
    *
    * @param visible @c true pour afficher, @c false pour masquer.
    */
    void show(bool visible);

    /**
     * @brief Retourne la hauteur totale du widget en pixels.
     * Utile pour positionner les contrôles voisins dans MainWindow.
     */
    [[nodiscard]] static constexpr int totalHeight() { return 76; }
    // 20 (label) + 6 (gap) + 20 (barre) + 6 (gap) + 24 (cancel)

private:
    HWND m_hLabel = nullptr;  ///< Contrôle STATIC — label de phase.
    HWND m_hBar = nullptr;  ///< Contrôle PROGRESS_CLASS.
    HWND m_hCancel = nullptr;  ///< Contrôle BUTTON "Annuler".
};
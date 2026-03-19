#pragma once

/**
 * @file ProgressBar.h
 * @brief Wrapper minimaliste d'une ProgressBar Win32.
 *
 * Cette classe encapsule le contrôle natif Windows PROGRESS_CLASS
 * et fournit une API simple pour :
 *   - créer la barre
 *   - mettre à jour la progression (0–100)
 *
 * ⚠️ IMPORTANT :
 * - Les mises à jour doivent être faites depuis le thread UI
 *   (via PostMessage recommandé).
 */

class ProgressBar
{
public:

    /**
     * @brief Constructeur par défaut.
     */
    ProgressBar() : m_hwnd(nullptr) {}

    /**
     * @brief Crée la ProgressBar dans une fenêtre parent.
     *
     * @param parent Fenêtre parente (HWND).
     * @param x      Position X.
     * @param y      Position Y.
     * @param width  Largeur.
     * @param height Hauteur.
     */
    void create(HWND parent, int x, int y, int width, int height)
    {
        INITCOMMONCONTROLSEX icex{};
        icex.dwSize = sizeof(icex);
        icex.dwICC = ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&icex);

        m_hwnd = CreateWindowEx(
            0,
            PROGRESS_CLASS,
            nullptr,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            parent,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );

        if (!m_hwnd)
        {
            throw std::runtime_error("ProgressBar creation failed");
        }

        SendMessage(m_hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessage(m_hwnd, PBM_SETPOS, 0, 0);
    }

    /**
     * @brief Met à jour la progression.
     *
     * @param value Pourcentage [0–100].
     */
    void setProgress(int value)
    {
        if (m_hwnd)
        {
            SendMessage(m_hwnd, PBM_SETPOS, value, 0);
        }
    }

    /**
    * @brief Show or hide the ProgressBar
    *  @param Show true to show, false to hide
    */
    void show(bool visible)
    {
        if (m_hwnd)
            ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }

private:
    HWND m_hwnd; /**< Handle du contrôle ProgressBar. */
};
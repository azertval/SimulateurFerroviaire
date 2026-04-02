/**
 * @file  ProgressBar.cpp
 * @brief Implémentation de la ProgressBar avec label et bouton Cancel.
 *
 * @see ProgressBar
 */
#include "ProgressBar.h"
#include <commctrl.h>

void ProgressBar::create(HWND hParent, HINSTANCE hInstance,
    int x, int y, int width, int cancelId)
{
    // Label de phase — STATIC centré
    m_hLabel = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | SS_CENTER,       // pas WS_VISIBLE — masqué par défaut
        x, y, width, 20,
        hParent, nullptr, hInstance, nullptr);

    // Barre de progression
    m_hBar = CreateWindowExW(0, PROGRESS_CLASS, nullptr,
        WS_CHILD | PBS_SMOOTH,      // pas WS_VISIBLE — masqué par défaut
        x, y + 26, width, 20,
        hParent, nullptr, hInstance, nullptr);

    SendMessageW(m_hBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(m_hBar, PBM_SETPOS, 0, 0);

    // Bouton Cancel — masqué par défaut
    m_hCancel = CreateWindowExW(0, L"BUTTON", L"Annuler",
        WS_CHILD | BS_PUSHBUTTON,   // pas WS_VISIBLE
        x + width / 4, y + 52, width / 2, 24,
        hParent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(cancelId)),
        hInstance, nullptr);
}

void ProgressBar::show(bool visible)
{
    const int cmd = visible ? SW_SHOW : SW_HIDE;
    if (m_hLabel)  ShowWindow(m_hLabel, cmd);
    if (m_hBar)    ShowWindow(m_hBar, cmd);
    // m_hCancel a ses propres méthodes showCancelButton / hideCancelButton
}

void ProgressBar::setProgress(int value)
{
    if (m_hBar)
        SendMessageW(m_hBar, PBM_SETPOS, static_cast<WPARAM>(value), 0);
}

void ProgressBar::setLabel(const std::wstring& text)
{
    if (m_hLabel)
        SetWindowTextW(m_hLabel, text.c_str());
}

void ProgressBar::showCancelButton()
{
    if (m_hCancel)
        ShowWindow(m_hCancel, SW_SHOW);
}

void ProgressBar::hideCancelButton()
{
    if (m_hCancel)
        ShowWindow(m_hCancel, SW_HIDE);
}

void ProgressBar::reset()
{
    setProgress(0);
    setLabel(L"");
    hideCancelButton();
    show(false);
}
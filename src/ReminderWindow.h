#pragma once

#include <string>
#include <windows.h>

constexpr UINT WM_REMINDER_CANCEL_SHUTDOWN = WM_APP + 101;

class ReminderWindow {
public:
    bool Create(HINSTANCE hInstance, HWND parent);
    void Destroy();

    void ShowCountdownMessage(int seconds);
    void Hide();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void UpdateLabel();
    void ApplyLayout();
    void RequestCancelShutdown();
    void DrawButton(DRAWITEMSTRUCT* dis);
    void EnableDarkTitleBar();
    void UpdateCountdownFontToFit(const std::wstring& text);

    HWND parent_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND hLabel_ = nullptr;
    HWND hCancelBtn_ = nullptr;
    int remainingSeconds_ = 0;
    HFONT fontCountdown_ = nullptr;
    HFONT fontButton_ = nullptr;
    HBRUSH bgBrush_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
};

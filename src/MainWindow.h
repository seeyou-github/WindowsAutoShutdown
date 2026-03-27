#pragma once

#include "Config.h"
#include "FolderMonitor.h"
#include "ReminderWindow.h"
#include "ShutdownScheduler.h"

#include <chrono>
#include <string>
#include <windows.h>

class MainWindow {
public:
    bool Create(HINSTANCE hInstance, int nCmdShow);
    int MessageLoop();

private:
    enum class PendingAction {
        Shutdown,
        Sleep
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleSettingsMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateMainControls();
    void CreateSettingsWindow();
    void CreateSettingsControls(HWND wnd);

    void ApplyTheme(HWND wnd);
    void EnableDarkTitleBar(HWND wnd);
    void DrawButton(DRAWITEMSTRUCT* dis);

    void LoadConfig();
    void SaveConfig();
    void ApplyConfigToSettingsUI();
    bool ReadConfigFromSettingsUI(bool showError);
    void RefreshMonitorSettings();
    void WriteMonitorLogIfNeeded();
    void WriteMonitorLog() const;

    void AddManualSeconds(int seconds);
    void StartManualSchedule();
    void StartManualSleepSchedule();
    void StartMonitorSchedule(const std::wstring& reason);
    void CancelSchedule();
    void ExecuteShutdownNow();

    void UpdateMainTimeText();
    void RefreshAddButtonsText();

    void InitTrayIcon();
    void RemoveTrayIcon();
    void MinimizeToTray();
    void RestoreFromTray();
    void ShowTrayMenu();
    void ShowSettingsWindow();

    void HandleTimerTick();
    void HandleMonitorTick();

    std::wstring GetIniPath() const;
    std::wstring BrowseFolder(HWND owner);

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND settingsWnd_ = nullptr;

    HWND hKeyTimeTitle_ = nullptr;
    HWND hTimeLargeLabel_ = nullptr;
    HWND hAddBtns_[8] = {};
    HWND hStartBtn_ = nullptr;
    HWND hStartSleepBtn_ = nullptr;
    HWND hCancelBtn_ = nullptr;
    HWND hSettingsBtn_ = nullptr;

    HWND hPreventSleepCheck_ = nullptr;
    HWND hMonitorEnableCheck_ = nullptr;
    HWND hMonitorPathEdit_ = nullptr;
    HWND hBrowseBtn_ = nullptr;
    HWND hSizeRuleCheck_ = nullptr;
    HWND hSizeEdit_ = nullptr;
    HWND hStallRuleCheck_ = nullptr;
    HWND hStallEdit_ = nullptr;
    HWND hLogIntervalEdit_ = nullptr;
    HWND hSettingsSaveBtn_ = nullptr;
    HWND hSettingsCloseBtn_ = nullptr;

    HFONT fontNormal_ = nullptr;
    HFONT fontLarge_ = nullptr;
    HBRUSH bgBrush_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HICON hMainIconBig_ = nullptr;
    HICON hMainIconSmall_ = nullptr;
    HICON hSettingsIconBig_ = nullptr;
    HICON hSettingsIconSmall_ = nullptr;
    HICON hTrayIcon_ = nullptr;

    bool exiting_ = false;
    NOTIFYICONDATAW trayData_ = {};

    AppConfig config_{};
    int manualTotalSeconds_ = 0;
    PendingAction pendingAction_ = PendingAction::Shutdown;
    ShutdownScheduler scheduler_{};
    FolderMonitor monitor_{};
    ReminderWindow reminder_;
    int monitorTickCounter_ = 0;
    MonitorTickResult lastMonitorResult_{};
    std::chrono::steady_clock::time_point lastMonitorLogAt_{};
    bool monitorTriggerSuppressed_ = false;
    bool monitorLogStopped_ = false;
};

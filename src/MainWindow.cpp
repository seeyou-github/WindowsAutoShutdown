#include "MainWindow.h"

#include "AppStrings.h"
#include "..\res\resource.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <cstdio>

namespace {
constexpr wchar_t kMainWndClass[] = L"WinAutoShutdown_MainWnd";
constexpr wchar_t kSettingsWndClass[] = L"WinAutoShutdown_SettingsWnd";
constexpr UINT kTrayCallbackMessage = WM_APP + 11;
constexpr UINT_PTR kMainTimerId = 1;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

HICON LoadAppIcon(HINSTANCE hInstance, int cx, int cy) {
    return reinterpret_cast<HICON>(
        LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
}

enum ControlId {
    IDC_KEY_TIME_TITLE = 1001,
    IDC_LARGE_TIME = 1002,
    IDC_ADD_1 = 1101,
    IDC_ADD_2 = 1102,
    IDC_ADD_3 = 1103,
    IDC_ADD_4 = 1104,
    IDC_ADD_5 = 1105,
    IDC_ADD_6 = 1106,
    IDC_ADD_7 = 1107,
    IDC_ADD_8 = 1108,
    IDC_START = 1201,
    IDC_CANCEL = 1202,
    IDC_OPEN_SETTINGS = 1203,
    IDC_START_SLEEP = 1204,
    IDC_PREVENT_SLEEP = 1301,
    IDC_MONITOR_ENABLE = 1303,
    IDC_MONITOR_PATH = 1304,
    IDC_MONITOR_BROWSE = 1305,
    IDC_SIZE_RULE_ENABLE = 1306,
    IDC_SIZE_EDIT = 1307,
    IDC_STALL_RULE_ENABLE = 1308,
    IDC_STALL_EDIT = 1309,
    IDC_LOG_INTERVAL_EDIT = 1310,
    IDC_SETTINGS_SAVE = 1311,
    IDC_SETTINGS_CLOSE = 1312,
    ID_TRAY_SHOW = 1401,
    ID_TRAY_CANCEL = 1402,
    ID_TRAY_EXIT = 1403
};

bool RunShutdownCommand(const wchar_t* args) {
    std::wstring command = L"shutdown ";
    command += args;

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

std::wstring FormatHms(int sec) {
    if (sec < 0) sec = 0;
    const int h = sec / 3600;
    const int m = (sec % 3600) / 60;
    const int s = sec % 60;
    wchar_t buf[64] = {};
    wsprintfW(buf, L"%02d:%02d:%02d", h, m, s);
    return buf;
}

std::wstring FormatAddButtonText(int minutes) {
    wchar_t buf[32] = {};
    if (minutes < 60) {
        swprintf(buf, std::size(buf), LoadAppString(IDS_ADD_BUTTON_MINUTES_FORMAT).c_str(), minutes);
        return buf;
    }

    if (minutes % 60 == 0) {
        swprintf(buf, std::size(buf), LoadAppString(IDS_ADD_BUTTON_HOURS_FORMAT).c_str(), minutes / 60);
    } else {
        const double hours = static_cast<double>(minutes) / 60.0;
        swprintf(buf, std::size(buf), LoadAppString(IDS_ADD_BUTTON_HOURS_DECIMAL_FORMAT).c_str(), hours);
    }
    return buf;
}

std::wstring FormatBytes(unsigned long long bytes) {
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    wchar_t buf[64] = {};
    swprintf(buf, std::size(buf), L"%.2f %ls", value, units[unit]);
    return buf;
}

std::wstring FormatLocalTimeNow() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    wchar_t buf[64] = {};
    swprintf(buf, std::size(buf), L"%04d-%02d-%02d %02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring FormatMmSs(int sec) {
    if (sec < 0) sec = 0;
    const int m = sec / 60;
    const int s = sec % 60;
    wchar_t buf[32] = {};
    swprintf(buf, std::size(buf), L"%02d:%02d 秒", m, s);
    return buf;
}

unsigned long long ParsePositiveULL(HWND edit, unsigned long long fallback) {
    wchar_t buf[64] = {};
    GetWindowTextW(edit, buf, 64);
    const unsigned long long value = _wcstoui64(buf, nullptr, 10);
    return (value > 0) ? value : fallback;
}

int ParsePositiveInt(HWND edit, int fallback) {
    wchar_t buf[64] = {};
    GetWindowTextW(edit, buf, 64);
    const int value = _wtoi(buf);
    return (value > 0) ? value : fallback;
}
} // namespace

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    hInstance_ = hInstance;

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    wc.hIconSm = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    wc.lpszClassName = kMainWndClass;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        0, kMainWndClass, LoadAppString(IDS_APP_TITLE).c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 470,
        nullptr, nullptr, hInstance_, this);
    if (!hwnd_) return false;

    HICON hMainIconBig = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    HICON hMainIconSmall = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hMainIconBig));
    SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hMainIconSmall));

    RECT rc = {};
    GetWindowRect(hwnd_, &rc);
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int x = work.left + ((work.right - work.left) - w) / 2;
    const int y = work.top + ((work.bottom - work.top) - h) / 2;
    SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    bgBrush_ = CreateSolidBrush(RGB(52, 56, 62));
    panelBrush_ = CreateSolidBrush(RGB(68, 74, 82));
    fontNormal_ = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    fontLarge_ = CreateFontW(-72, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    CreateMainControls();
    CreateSettingsWindow();
    InitTrayIcon();
    reminder_.Create(hInstance_, hwnd_);

    EnableDarkTitleBar(hwnd_);
    ApplyTheme(hwnd_);

    LoadConfig();

    SetTimer(hwnd_, kMainTimerId, 1000, nullptr);
    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    return true;
}

int MainWindow::MessageLoop() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void MainWindow::CreateMainControls() {
    hSettingsBtn_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_SETTINGS_BUTTON).c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                    20, 18, 110, 34, hwnd_, reinterpret_cast<HMENU>(IDC_OPEN_SETTINGS), hInstance_, nullptr);

    hKeyTimeTitle_ = CreateWindowExW(0, L"STATIC", LoadAppString(IDS_MAIN_TIME_TITLE).c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER,
                                     180, 22, 400, 30, hwnd_, reinterpret_cast<HMENU>(IDC_KEY_TIME_TITLE), hInstance_, nullptr);

    hTimeLargeLabel_ = CreateWindowExW(0, L"STATIC", L"00:00:00", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                       30, 70, 690, 110, hwnd_, reinterpret_cast<HMENU>(IDC_LARGE_TIME), hInstance_, nullptr);

    const int btnW = 120;
    const int btnH = 44;
    const int gap = 14;
    const int totalW = btnW * 4 + gap * 3;
    const int startX = (760 - totalW) / 2;
    const int row1Y = 210;
    const int row2Y = 268;

    for (int i = 0; i < 8; ++i) {
        const int row = i / 4;
        const int col = i % 4;
        const int bx = startX + col * (btnW + gap);
        const int by = (row == 0) ? row1Y : row2Y;
        hAddBtns_[i] = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                       bx, by, btnW, btnH, hwnd_, reinterpret_cast<HMENU>(IDC_ADD_1 + i), hInstance_, nullptr);
        SendMessageW(hAddBtns_[i], WM_SETFONT, reinterpret_cast<WPARAM>(fontNormal_), TRUE);
    }

    hStartBtn_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_START_SHUTDOWN).c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                 200, 342, 110, 40, hwnd_, reinterpret_cast<HMENU>(IDC_START), hInstance_, nullptr);
    hStartSleepBtn_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_START_SLEEP).c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                      324, 342, 110, 40, hwnd_, reinterpret_cast<HMENU>(IDC_START_SLEEP), hInstance_, nullptr);
    hCancelBtn_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_CANCEL_SHUTDOWN).c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                  448, 342, 110, 40, hwnd_, reinterpret_cast<HMENU>(IDC_CANCEL), hInstance_, nullptr);

    HWND controls[] = {hSettingsBtn_, hKeyTimeTitle_, hTimeLargeLabel_, hStartBtn_, hStartSleepBtn_, hCancelBtn_};
    for (HWND ctrl : controls) {
        SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(fontNormal_), TRUE);
    }
    SendMessageW(hTimeLargeLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(fontLarge_), TRUE);
}

void MainWindow::CreateSettingsWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWindow::SettingsWndProc;
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    wc.hIconSm = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    wc.lpszClassName = kSettingsWndClass;
    RegisterClassExW(&wc);

    settingsWnd_ = CreateWindowExW(
        0, kSettingsWndClass, LoadAppString(IDS_SETTINGS_TITLE).c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 430,
        hwnd_, nullptr, hInstance_, this);

    if (settingsWnd_) {
        HICON hSettingsIconBig = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
        HICON hSettingsIconSmall = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
        SendMessageW(settingsWnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hSettingsIconBig));
        SendMessageW(settingsWnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hSettingsIconSmall));
        CreateSettingsControls(settingsWnd_);
        EnableDarkTitleBar(settingsWnd_);
        ApplyTheme(settingsWnd_);
    }
}
void MainWindow::CreateSettingsControls(HWND wnd) {
    CreateWindowExW(0, L"STATIC", LoadAppString(IDS_SYSTEM_SETTINGS).c_str(), WS_CHILD | WS_VISIBLE,
                    20, 16, 160, 28, wnd, nullptr, hInstance_, nullptr);

    hPreventSleepCheck_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_PREVENT_SLEEP).c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                          20, 52, 270, 30, wnd, reinterpret_cast<HMENU>(IDC_PREVENT_SLEEP), hInstance_, nullptr);

    CreateWindowExW(0, L"STATIC", LoadAppString(IDS_FOLDER_MONITOR).c_str(), WS_CHILD | WS_VISIBLE,
                    20, 98, 160, 28, wnd, nullptr, hInstance_, nullptr);

    hMonitorEnableCheck_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_ENABLE_MONITOR).c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                           20, 130, 140, 28, wnd, reinterpret_cast<HMENU>(IDC_MONITOR_ENABLE), hInstance_, nullptr);

    CreateWindowExW(0, L"STATIC", LoadAppString(IDS_MONITOR_PATH).c_str(), WS_CHILD | WS_VISIBLE,
                    20, 168, 95, 24, wnd, nullptr, hInstance_, nullptr);
    hMonitorPathEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                        115, 164, 430, 30, wnd, reinterpret_cast<HMENU>(IDC_MONITOR_PATH), hInstance_, nullptr);
    hBrowseBtn_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_BROWSE).c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                  555, 164, 110, 30, wnd, reinterpret_cast<HMENU>(IDC_MONITOR_BROWSE), hInstance_, nullptr);

    hSizeRuleCheck_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_SIZE_RULE).c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                      20, 212, 190, 28, wnd, reinterpret_cast<HMENU>(IDC_SIZE_RULE_ENABLE), hInstance_, nullptr);
    hSizeEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1024", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                 212, 210, 120, 30, wnd, reinterpret_cast<HMENU>(IDC_SIZE_EDIT), hInstance_, nullptr);

    hStallRuleCheck_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_STALL_RULE).c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       360, 212, 190, 28, wnd, reinterpret_cast<HMENU>(IDC_STALL_RULE_ENABLE), hInstance_, nullptr);
    hStallEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"10", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                  550, 210, 115, 30, wnd, reinterpret_cast<HMENU>(IDC_STALL_EDIT), hInstance_, nullptr);

    CreateWindowExW(0, L"STATIC", LoadAppString(IDS_LOG_INTERVAL).c_str(), WS_CHILD | WS_VISIBLE,
                    20, 256, 180, 28, wnd, nullptr, hInstance_, nullptr);
    hLogIntervalEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"10", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                        212, 254, 120, 30, wnd, reinterpret_cast<HMENU>(IDC_LOG_INTERVAL_EDIT), hInstance_, nullptr);

    CreateWindowExW(0, L"STATIC", LoadAppString(IDS_MONITOR_HINT).c_str(), WS_CHILD | WS_VISIBLE,
                    20, 292, 650, 28, wnd, nullptr, hInstance_, nullptr);

    hSettingsSaveBtn_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_SAVE_SETTINGS).c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                        400, 336, 120, 40, wnd, reinterpret_cast<HMENU>(IDC_SETTINGS_SAVE), hInstance_, nullptr);
    hSettingsCloseBtn_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_CLOSE).c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                         540, 336, 120, 40, wnd, reinterpret_cast<HMENU>(IDC_SETTINGS_CLOSE), hInstance_, nullptr);

    HWND controls[] = {
        hPreventSleepCheck_, hMonitorEnableCheck_, hMonitorPathEdit_, hBrowseBtn_, hSizeRuleCheck_,
        hSizeEdit_, hStallRuleCheck_, hStallEdit_, hLogIntervalEdit_, hSettingsSaveBtn_, hSettingsCloseBtn_
    };
    for (HWND ctrl : controls) {
        SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(fontNormal_), TRUE);
    }

    SetWindowTheme(hPreventSleepCheck_, L"", L"");
    SetWindowTheme(hMonitorEnableCheck_, L"", L"");
    SetWindowTheme(hSizeRuleCheck_, L"", L"");
    SetWindowTheme(hStallRuleCheck_, L"", L"");
}

void MainWindow::ApplyTheme(HWND wnd) {
    InvalidateRect(wnd, nullptr, TRUE);
    UpdateWindow(wnd);
}

void MainWindow::EnableDarkTitleBar(HWND wnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(wnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

void MainWindow::DrawButton(DRAWITEMSTRUCT* dis) {
    if (!dis) return;

    const bool selected = (dis->itemState & ODS_SELECTED) != 0;
    COLORREF bg = selected ? RGB(92, 102, 114) : RGB(106, 118, 132);
    COLORREF border = RGB(132, 146, 162);
    COLORREF text = RGB(210, 216, 224);
    const int ctrlId = GetDlgCtrlID(dis->hwndItem);

    if (ctrlId == IDC_START) {
        bg = selected ? RGB(168, 46, 46) : RGB(196, 58, 58);
        border = RGB(224, 112, 112);
        text = RGB(255, 242, 242);
    } else if (ctrlId == IDC_START_SLEEP) {
        bg = selected ? RGB(214, 180, 56) : RGB(236, 200, 68);
        border = RGB(247, 222, 133);
        text = RGB(52, 44, 18);
    }

    HBRUSH brush = CreateSolidBrush(bg);
    FillRect(dis->hDC, &dis->rcItem, brush);
    DeleteObject(brush);

    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
    HGDIOBJ oldBrush = SelectObject(dis->hDC, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
    SelectObject(dis->hDC, oldBrush);
    SelectObject(dis->hDC, oldPen);
    DeleteObject(pen);

    wchar_t caption[128] = {};
    GetWindowTextW(dis->hwndItem, caption, 128);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, text);
    DrawTextW(dis->hDC, caption, -1, const_cast<RECT*>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::LoadConfig() {
    ConfigManager::Load(GetIniPath(), config_);
    manualTotalSeconds_ = 0;
    pendingAction_ = PendingAction::Shutdown;
    ApplyConfigToSettingsUI();
    RefreshMonitorSettings();
    RefreshAddButtonsText();
    UpdateMainTimeText();
}

void MainWindow::SaveConfig() {
    ConfigManager::Save(GetIniPath(), config_);
}

void MainWindow::ApplyConfigToSettingsUI() {
    if (!settingsWnd_) return;

    SendMessageW(hPreventSleepCheck_, BM_SETCHECK, config_.preventSleep ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hMonitorEnableCheck_, BM_SETCHECK, config_.monitorEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(hMonitorPathEdit_, config_.monitorPath.c_str());
    SendMessageW(hSizeRuleCheck_, BM_SETCHECK, config_.sizeRuleEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hStallRuleCheck_, BM_SETCHECK, config_.stallRuleEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

    SetWindowTextW(hSizeEdit_, std::to_wstring(config_.sizeThresholdMb).c_str());

    wchar_t buf[64] = {};
    wsprintfW(buf, L"%d", config_.stallMinutes);
    SetWindowTextW(hStallEdit_, buf);
    wsprintfW(buf, L"%d", config_.monitorLogIntervalSeconds);
    SetWindowTextW(hLogIntervalEdit_, buf);
}

bool MainWindow::ReadConfigFromSettingsUI(bool showError) {
    if (!settingsWnd_) return true;

    AppConfig next = config_;
    next.preventSleep = (SendMessageW(hPreventSleepCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    next.monitorEnabled = (SendMessageW(hMonitorEnableCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    next.sizeRuleEnabled = (SendMessageW(hSizeRuleCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    next.stallRuleEnabled = (SendMessageW(hStallRuleCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED);

    wchar_t pathBuf[MAX_PATH] = {};
    GetWindowTextW(hMonitorPathEdit_, pathBuf, MAX_PATH);
    next.monitorPath = pathBuf;
    next.sizeThresholdMb = ParsePositiveULL(hSizeEdit_, 1024);
    next.stallMinutes = ParsePositiveInt(hStallEdit_, 10);
    next.monitorLogIntervalSeconds = ParsePositiveInt(hLogIntervalEdit_, 10);

    if (next.monitorEnabled) {
        if (next.monitorPath.empty()) {
            if (showError) MessageBoxW(settingsWnd_, LoadAppString(IDS_MONITOR_PATH_REQUIRED).c_str(), LoadAppString(IDS_PARAM_ERROR).c_str(), MB_ICONWARNING);
            return false;
        }
        if (!next.sizeRuleEnabled && !next.stallRuleEnabled) {
            if (showError) MessageBoxW(settingsWnd_, LoadAppString(IDS_MONITOR_RULE_REQUIRED).c_str(), LoadAppString(IDS_PARAM_ERROR).c_str(), MB_ICONWARNING);
            return false;
        }
        if (next.monitorLogIntervalSeconds < 1) {
            if (showError) MessageBoxW(settingsWnd_, LoadAppString(IDS_MONITOR_LOG_INTERVAL_INVALID).c_str(), LoadAppString(IDS_PARAM_ERROR).c_str(), MB_ICONWARNING);
            return false;
        }
    }

    config_ = next;
    return true;
}

void MainWindow::RefreshMonitorSettings() {
    MonitorSettings ms;
    ms.enabled = config_.monitorEnabled;
    ms.path = config_.monitorPath;
    ms.sizeRuleEnabled = config_.sizeRuleEnabled;
    ms.sizeThresholdBytes = config_.sizeThresholdMb * 1024ULL * 1024ULL;
    ms.stallRuleEnabled = config_.stallRuleEnabled;
    ms.stallSeconds = config_.stallMinutes * 60;
    monitor_.UpdateSettings(ms);

    if (!config_.monitorEnabled) {
        lastMonitorResult_ = {};
        lastMonitorLogAt_ = {};
    }
    monitorTriggerSuppressed_ = false;
    monitorLogStopped_ = false;
}

void MainWindow::RefreshAddButtonsText() {
    for (int i = 0; i < 8; ++i) {
        if (hAddBtns_[i]) {
            SetWindowTextW(hAddBtns_[i], FormatAddButtonText(config_.addMinutes[static_cast<size_t>(i)]).c_str());
        }
    }
}

void MainWindow::AddManualSeconds(int seconds) {
    if (seconds <= 0) return;

    if (scheduler_.IsRunning()) {
        scheduler_.AddSeconds(seconds);
        manualTotalSeconds_ = scheduler_.GetRemainingSeconds();
    } else {
        long long next = static_cast<long long>(manualTotalSeconds_) + seconds;
        if (next > 99LL * 3600LL) next = 99LL * 3600LL;
        manualTotalSeconds_ = static_cast<int>(next);
    }

    UpdateMainTimeText();
}

void MainWindow::StartManualSchedule() {
    if (scheduler_.IsRunning()) {
        return;
    }
    if (manualTotalSeconds_ <= 0) {
        MessageBoxW(hwnd_, LoadAppString(IDS_SET_DURATION_FIRST).c_str(), LoadAppString(IDS_PROMPT).c_str(), MB_ICONINFORMATION);
        return;
    }

    pendingAction_ = PendingAction::Shutdown;
    scheduler_.Start(manualTotalSeconds_, LoadAppString(IDS_MANUAL_SHUTDOWN_REASON));
    monitorTriggerSuppressed_ = false;
    monitorLogStopped_ = false;
    lastMonitorLogAt_ = {};
    UpdateMainTimeText();

    if (config_.preventSleep) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    }
}

void MainWindow::StartManualSleepSchedule() {
    if (scheduler_.IsRunning()) {
        return;
    }
    if (manualTotalSeconds_ <= 0) {
        MessageBoxW(hwnd_, LoadAppString(IDS_SET_DURATION_FIRST).c_str(), LoadAppString(IDS_PROMPT).c_str(), MB_ICONINFORMATION);
        return;
    }

    pendingAction_ = PendingAction::Sleep;
    scheduler_.Start(manualTotalSeconds_, LoadAppString(IDS_START_SLEEP));
    monitorTriggerSuppressed_ = false;
    monitorLogStopped_ = false;
    lastMonitorLogAt_ = {};
    UpdateMainTimeText();

    if (config_.preventSleep) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    }
}

void MainWindow::StartMonitorSchedule(const std::wstring& reason) {
    const int currentRemaining = scheduler_.IsRunning() ? scheduler_.GetRemainingSeconds() : 0;
    pendingAction_ = PendingAction::Shutdown;
    if (currentRemaining <= 0 || currentRemaining > 3 * 60) {
        scheduler_.Start(3 * 60, reason);
        manualTotalSeconds_ = 3 * 60;
        reminder_.ShowCountdownMessage(3 * 60);
    } else {
        manualTotalSeconds_ = currentRemaining;
    }
    monitorTriggerSuppressed_ = true;
    monitorLogStopped_ = true;
    UpdateMainTimeText();
    if (config_.preventSleep) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    }
}

void MainWindow::CancelSchedule() {
    scheduler_.Cancel();
    reminder_.Hide();
    RunShutdownCommand(L"/a");
    SetThreadExecutionState(ES_CONTINUOUS);
    manualTotalSeconds_ = 0;
    pendingAction_ = PendingAction::Shutdown;
    monitorTriggerSuppressed_ = true;
    monitorLogStopped_ = true;
    UpdateMainTimeText();
}

void MainWindow::ExecuteShutdownNow() {
    reminder_.Hide();
    if (pendingAction_ == PendingAction::Sleep) {
        RunShutdownCommand(L"/h");
    } else {
        RunShutdownCommand(L"/s /t 0");
    }
    scheduler_.Cancel();
    pendingAction_ = PendingAction::Shutdown;
}

void MainWindow::UpdateMainTimeText() {
    const int sec = scheduler_.IsRunning() ? scheduler_.GetRemainingSeconds() : manualTotalSeconds_;
    SetWindowTextW(hTimeLargeLabel_, FormatHms(sec).c_str());
}

void MainWindow::InitTrayIcon() {
    trayData_ = {};
    trayData_.cbSize = sizeof(trayData_);
    trayData_.hWnd = hwnd_;
    trayData_.uID = 1;
    trayData_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayData_.uCallbackMessage = kTrayCallbackMessage;
    trayData_.hIcon = LoadAppIcon(hInstance_, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    lstrcpynW(trayData_.szTip, LoadAppString(IDS_APP_TITLE).c_str(), ARRAYSIZE(trayData_.szTip));
    Shell_NotifyIconW(NIM_ADD, &trayData_);
}

void MainWindow::RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &trayData_);
}

void MainWindow::MinimizeToTray() {
    ShowWindow(hwnd_, SW_HIDE);
}

void MainWindow::RestoreFromTray() {
    ShowWindow(hwnd_, SW_SHOW);
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
}

void MainWindow::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, LoadAppString(IDS_TRAY_SHOW).c_str());
    AppendMenuW(menu, MF_STRING, ID_TRAY_CANCEL, LoadAppString(IDS_TRAY_CANCEL).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, LoadAppString(IDS_TRAY_EXIT).c_str());

    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::ShowSettingsWindow() {
    if (!settingsWnd_) return;
    ApplyConfigToSettingsUI();
    RECT rc = {};
    GetWindowRect(settingsWnd_, &rc);
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int x = work.left + ((work.right - work.left) - w) / 2;
    const int y = work.top + ((work.bottom - work.top) - h) / 2;
    SetWindowPos(settingsWnd_, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    ShowWindow(settingsWnd_, SW_SHOW);
    ShowWindow(settingsWnd_, SW_RESTORE);
    SetForegroundWindow(settingsWnd_);
}

void MainWindow::HandleTimerTick() {
    const auto tick = scheduler_.Tick();

    if (tick.showThreeMinuteReminder) {
        reminder_.ShowCountdownMessage(tick.remainingSeconds);
    }
    if (tick.shouldShutdownNow) {
        ExecuteShutdownNow();
    }

    UpdateMainTimeText();
    WriteMonitorLogIfNeeded();

    if (!scheduler_.IsRunning()) {
        return;
    }

    monitorTickCounter_++;
    if (monitorTickCounter_ >= 5) {
        monitorTickCounter_ = 0;
        HandleMonitorTick();
    }
}

void MainWindow::HandleMonitorTick() {
    if (!config_.monitorEnabled || !scheduler_.IsRunning()) {
        return;
    }

    auto result = monitor_.Tick();
    lastMonitorResult_ = result;
    if (!result.triggered) {
        monitorTriggerSuppressed_ = false;
        if (config_.monitorEnabled) {
            monitorLogStopped_ = false;
        }
    }
    if (result.triggered && !monitorTriggerSuppressed_) {
        if (!config_.monitorPath.empty()) {
            WriteMonitorLog();
            lastMonitorLogAt_ = std::chrono::steady_clock::now();
        }
        StartMonitorSchedule(result.reason);
        monitorLogStopped_ = true;
    } else if (result.triggered) {
        monitorLogStopped_ = true;
    }
}

void MainWindow::WriteMonitorLogIfNeeded() {
    if (!scheduler_.IsRunning() || !config_.monitorEnabled || config_.monitorPath.empty() ||
        monitorLogStopped_ || lastMonitorResult_.triggered) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (lastMonitorLogAt_.time_since_epoch().count() == 0) {
        lastMonitorLogAt_ = now - std::chrono::seconds(config_.monitorLogIntervalSeconds);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastMonitorLogAt_).count();
    if (elapsed < config_.monitorLogIntervalSeconds) {
        return;
    }

    WriteMonitorLog();
    lastMonitorLogAt_ = now;
}

void MainWindow::WriteMonitorLog() const {
    std::wstring logPath = GetIniPath();
    const size_t pos = logPath.find_last_of(L".");
    if (pos != std::wstring::npos) {
        logPath = logPath.substr(0, pos);
    }
    logPath += L".log";

    FILE* file = _wfopen(logPath.c_str(), L"a+, ccs=UTF-8");
    if (!file) {
        return;
    }

    const std::wstring sizeText = FormatBytes(lastMonitorResult_.currentSizeBytes);
    const std::wstring triggered = LoadAppString(lastMonitorResult_.triggered ? IDS_MONITOR_YES : IDS_MONITOR_NO);
    std::wstring reason = L"-";
    switch (lastMonitorResult_.triggerType) {
    case MonitorTriggerType::size_threshold:
        reason = LoadAppString(IDS_MONITOR_REASON_SIZE_REACHED);
        break;
    case MonitorTriggerType::stall_timeout:
        reason = LoadAppString(IDS_MONITOR_REASON_TIME_REACHED);
        break;
    case MonitorTriggerType::none:
        if (lastMonitorResult_.triggered) {
            reason = LoadAppString(IDS_MONITOR_REASON_TRIGGERED);
        }
        break;
    }

    std::wstring thresholdText = config_.sizeRuleEnabled
        ? FormatBytes(config_.sizeThresholdMb * 1024ULL * 1024ULL)
        : LoadAppString(IDS_MONITOR_DISABLED);

    std::wstring stallTriggerText = LoadAppString(IDS_MONITOR_DISABLED);
    if (config_.stallRuleEnabled) {
        if (config_.stallMinutes >= 60 && config_.stallMinutes % 60 == 0) {
            wchar_t buf[64] = {};
            swprintf(buf, std::size(buf), LoadAppString(IDS_STALL_HOURS_FORMAT).c_str(), config_.stallMinutes / 60);
            stallTriggerText = buf;
        } else {
            wchar_t buf[64] = {};
            swprintf(buf, std::size(buf), LoadAppString(IDS_STALL_MINUTES_FORMAT).c_str(), config_.stallMinutes);
            stallTriggerText = buf;
        }
    }

    const std::wstring stableDurationText = FormatMmSs(lastMonitorResult_.stableSeconds);

    const std::wstring format = LoadAppString(IDS_MONITOR_LOG_FORMAT);
    fwprintf(file, format.c_str(),
             FormatLocalTimeNow().c_str(),
             sizeText.c_str(),
             thresholdText.c_str(),
             stableDurationText.c_str(),
             stallTriggerText.c_str(),
             triggered.c_str(),
             reason.c_str(),
             config_.monitorPath.c_str());
    fclose(file);
}
std::wstring MainWindow::GetIniPath() const {
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::wstring path = modulePath;
    const size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        path = path.substr(0, pos + 1);
    } else {
        path.clear();
    }
    path += L"WindowsAutoShutdown.ini";
    return path;
}

std::wstring MainWindow::BrowseFolder(HWND owner) {
    BROWSEINFOW bi = {};
    bi.hwndOwner = owner;
    const std::wstring title = LoadAppString(IDS_SELECT_MONITOR_FOLDER);
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return L"";

    wchar_t path[MAX_PATH] = {};
    if (!SHGetPathFromIDListW(pidl, path)) {
        CoTaskMemFree(pidl);
        return L"";
    }
    CoTaskMemFree(pidl);
    return path;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
        return TRUE;
    }

    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);
    return self->HandleMessage(msg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }

    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);
    return self->HandleSettingsMessage(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (code == BN_CLICKED) {
            if (id >= IDC_ADD_1 && id <= IDC_ADD_8) {
                const int idx = id - IDC_ADD_1;
                AddManualSeconds(config_.addMinutes[static_cast<size_t>(idx)] * 60);
                return 0;
            }
            if (id == IDC_START) { StartManualSchedule(); return 0; }
            if (id == IDC_START_SLEEP) { StartManualSleepSchedule(); return 0; }
            if (id == IDC_CANCEL) { CancelSchedule(); return 0; }
            if (id == IDC_OPEN_SETTINGS) { ShowSettingsWindow(); return 0; }
        }

        if (id == ID_TRAY_SHOW) { RestoreFromTray(); return 0; }
        if (id == ID_TRAY_CANCEL) { CancelSchedule(); return 0; }
        if (id == ID_TRAY_EXIT) {
            exiting_ = true;
            DestroyWindow(hwnd_);
            return 0;
        }
        break;
    }
    case WM_DRAWITEM:
        DrawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_TIMER:
        if (wParam == kMainTimerId) {
            HandleTimerTick();
            return 0;
        }
        break;
    case WM_REMINDER_CANCEL_SHUTDOWN:
        CancelSchedule();
        return 0;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            MinimizeToTray();
            return 0;
        }
        break;
    case WM_CLOSE:
        if (scheduler_.IsRunning()) {
            MinimizeToTray();
            return 0;
        }
        exiting_ = true;
        DestroyWindow(hwnd_);
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, RGB(206, 212, 220));
        SetBkColor(hdc, RGB(52, 56, 62));
        return reinterpret_cast<LRESULT>(bgBrush_);
    }
    case WM_ERASEBKGND: {
        RECT rc = {};
        GetClientRect(hwnd_, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, bgBrush_);
        return TRUE;
    }
    case WM_DESTROY:
        SaveConfig();
        reminder_.Destroy();
        RemoveTrayIcon();
        if (settingsWnd_) DestroyWindow(settingsWnd_);
        if (fontNormal_) DeleteObject(fontNormal_);
        if (fontLarge_) DeleteObject(fontLarge_);
        if (bgBrush_) DeleteObject(bgBrush_);
        if (panelBrush_) DeleteObject(panelBrush_);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    if (msg == kTrayCallbackMessage) {
        if (lParam == WM_LBUTTONDBLCLK) {
            RestoreFromTray();
            return 0;
        }
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu();
            return 0;
        }
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

LRESULT MainWindow::HandleSettingsMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == IDC_MONITOR_BROWSE && code == BN_CLICKED) {
            const std::wstring path = BrowseFolder(hwnd);
            if (!path.empty()) SetWindowTextW(hMonitorPathEdit_, path.c_str());
            return 0;
        }
        if (id == IDC_SETTINGS_SAVE && code == BN_CLICKED) {
            if (!ReadConfigFromSettingsUI(true)) return 0;
            SaveConfig();
            RefreshMonitorSettings();
            RefreshAddButtonsText();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        if (id == IDC_SETTINGS_CLOSE && code == BN_CLICKED) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;
    }
    case WM_DRAWITEM:
        DrawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, RGB(206, 212, 220));
        SetBkColor(hdc, RGB(52, 56, 62));
        return reinterpret_cast<LRESULT>(bgBrush_);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, RGB(206, 212, 220));
        SetBkColor(hdc, RGB(68, 74, 82));
        return reinterpret_cast<LRESULT>(panelBrush_);
    }
    case WM_ERASEBKGND: {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, bgBrush_);
        return TRUE;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

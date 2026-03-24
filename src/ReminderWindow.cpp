#include "ReminderWindow.h"

#include "AppStrings.h"
#include "..\res\resource.h"

#include <dwmapi.h>
#include <algorithm>

namespace {
constexpr wchar_t kReminderWndClass[] = L"WinAutoShutdown_ReminderWnd";
constexpr UINT_PTR kReminderTimer = 1001;
constexpr int kCancelButtonId = 2002;
constexpr int kCardWidth = 460;
constexpr int kCardHeight = 620;
constexpr int kScreenMargin = 20;
constexpr COLORREF kBgColor = RGB(38, 42, 48);
constexpr COLORREF kPanelColor = RGB(52, 58, 66);
constexpr COLORREF kBorderColor = RGB(92, 102, 114);
constexpr COLORREF kTextColor = RGB(235, 240, 248);

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
}

bool ReminderWindow::Create(HINSTANCE hInstance, HWND parent) {
    parent_ = parent;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ReminderWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kReminderWndClass;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kReminderWndClass,
        LoadAppString(IDS_REMINDER_WINDOW_TITLE).c_str(),
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, kCardWidth, kCardHeight,
        parent, nullptr, hInstance, this);

    if (!hwnd_) return false;

    EnableDarkTitleBar();
    bgBrush_ = CreateSolidBrush(kBgColor);
    panelBrush_ = CreateSolidBrush(kPanelColor);
    fontCountdown_ = CreateFontW(-56, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    fontButton_ = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    hLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                              24, 20, 412, 130, hwnd_, nullptr, hInstance, nullptr);
    hCancelBtn_ = CreateWindowExW(0, L"BUTTON", LoadAppString(IDS_REMINDER_CANCEL).c_str(),
                                  WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                  130, 170, 200, 42, hwnd_, reinterpret_cast<HMENU>(kCancelButtonId), hInstance, nullptr);

    if (!hLabel_ || !hCancelBtn_) {
        return false;
    }

    SendMessageW(hLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(fontCountdown_), TRUE);
    SendMessageW(hCancelBtn_, WM_SETFONT, reinterpret_cast<WPARAM>(fontButton_), TRUE);
    return true;
}

void ReminderWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (fontCountdown_) DeleteObject(fontCountdown_);
    if (fontButton_) DeleteObject(fontButton_);
    if (bgBrush_) DeleteObject(bgBrush_);
    if (panelBrush_) DeleteObject(panelBrush_);
    fontCountdown_ = nullptr;
    fontButton_ = nullptr;
    bgBrush_ = nullptr;
    panelBrush_ = nullptr;
}

void ReminderWindow::ShowCountdownMessage(int seconds) {
    if (!hwnd_) return;
    remainingSeconds_ = (seconds < 1) ? 1 : seconds;
    ApplyLayout();
    UpdateLabel();
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    SetTimer(hwnd_, kReminderTimer, 1000, nullptr);
}

void ReminderWindow::Hide() {
    if (!hwnd_) return;
    KillTimer(hwnd_, kReminderTimer);
    ShowWindow(hwnd_, SW_HIDE);
}

void ReminderWindow::UpdateLabel() {
    if (!hLabel_) return;

    const int minutes = remainingSeconds_ / 60;
    const int seconds = remainingSeconds_ % 60;
    wchar_t text[64] = {};
    swprintf(text, std::size(text), L"%d分: %02d秒\n后关机", minutes, seconds);
    UpdateCountdownFontToFit(text);
    SetWindowTextW(hLabel_, text);
}

void ReminderWindow::ApplyLayout() {
    if (!hwnd_ || !hLabel_ || !hCancelBtn_) return;

    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int width = kCardWidth;
    const int height = kCardHeight;
    const int x = workArea.right - width - kScreenMargin;
    const int y = workArea.bottom - height - kScreenMargin;

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
    SetWindowPos(hLabel_, nullptr, 18, 82, width - 36, height - 152, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(hCancelBtn_, nullptr, 50, height - 64, width - 100, 40, SWP_NOZORDER | SWP_NOACTIVATE);
}

void ReminderWindow::RequestCancelShutdown() {
    if (!parent_) return;
    SendMessageW(parent_, WM_REMINDER_CANCEL_SHUTDOWN, 0, 0);
}

void ReminderWindow::DrawButton(DRAWITEMSTRUCT* dis) {
    if (!dis) return;

    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const bool isCancel = (GetDlgCtrlID(dis->hwndItem) == kCancelButtonId);
    const COLORREF fill = isCancel
        ? (pressed ? RGB(146, 72, 66) : RGB(170, 82, 74))
        : (pressed ? RGB(78, 88, 100) : RGB(92, 102, 114));

    HBRUSH fillBrush = CreateSolidBrush(fill);
    FillRect(dis->hDC, &dis->rcItem, fillBrush);
    DeleteObject(fillBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, kBorderColor);
    HGDIOBJ oldPen = SelectObject(dis->hDC, borderPen);
    HGDIOBJ oldBrush = SelectObject(dis->hDC, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
    SelectObject(dis->hDC, oldBrush);
    SelectObject(dis->hDC, oldPen);
    DeleteObject(borderPen);

    wchar_t caption[64] = {};
    GetWindowTextW(dis->hwndItem, caption, 64);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, kTextColor);
    DrawTextW(dis->hDC, caption, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void ReminderWindow::EnableDarkTitleBar() {
    if (!hwnd_) return;
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

void ReminderWindow::UpdateCountdownFontToFit(const std::wstring& text) {
    if (!hwnd_ || !hLabel_) return;

    RECT rc = {};
    GetClientRect(hLabel_, &rc);
    const int targetWidth = (std::max)(1, static_cast<int>(rc.right - rc.left - 8));
    const int targetHeight = (std::max)(1, static_cast<int>(rc.bottom - rc.top - 8));
    HFONT bestFont = nullptr;

    HDC hdc = GetDC(hLabel_);
    if (!hdc) return;

    for (int size = 72; size >= 18; --size) {
        HFONT testFont = CreateFontW(-size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        HGDIOBJ oldFont = SelectObject(hdc, testFont);
        RECT measure = {0, 0, targetWidth, targetHeight};
        DrawTextW(hdc, text.c_str(), -1, &measure, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_CALCRECT);
        SelectObject(hdc, oldFont);

        if ((measure.right - measure.left) <= targetWidth && (measure.bottom - measure.top) <= targetHeight) {
            bestFont = testFont;
            break;
        }
        DeleteObject(testFont);
    }

    ReleaseDC(hLabel_, hdc);

    if (!bestFont) {
        bestFont = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    }

    if (fontCountdown_) {
        DeleteObject(fontCountdown_);
    }
    fontCountdown_ = bestFont;
    SendMessageW(hLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(fontCountdown_), TRUE);
}

LRESULT CALLBACK ReminderWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = reinterpret_cast<ReminderWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }
    auto* self = reinterpret_cast<ReminderWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleMessage(msg, wParam, lParam);
}

LRESULT ReminderWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED) {
            if (LOWORD(wParam) == kCancelButtonId) {
                RequestCancelShutdown();
                return 0;
            }
        }
        break;
    case WM_DRAWITEM:
        DrawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, kTextColor);
        return reinterpret_cast<LRESULT>(panelBrush_);
    }
    case WM_ERASEBKGND: {
        RECT rc = {};
        GetClientRect(hwnd_, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, bgBrush_);
        RECT panel = {16, 16, rc.right - 16, rc.bottom - 16};
        FillRect(reinterpret_cast<HDC>(wParam), &panel, panelBrush_);
        FrameRect(reinterpret_cast<HDC>(wParam), &panel, bgBrush_);
        return TRUE;
    }
    case WM_TIMER:
        if (wParam == kReminderTimer) {
            --remainingSeconds_;
            if (remainingSeconds_ <= 0) {
                remainingSeconds_ = 0;
                return 0;
            }
            UpdateLabel();
            return 0;
        }
        break;
    case WM_CLOSE:
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

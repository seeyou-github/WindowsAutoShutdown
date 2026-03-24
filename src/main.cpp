#include "MainWindow.h"

#include "AppStrings.h"
#include "..\res\resource.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HANDLE singleInstanceMutex = CreateMutexW(nullptr, FALSE, L"Global\\WindowsAutoShutdown_SingleInstance_Mutex");
    if (!singleInstanceMutex) {
        MessageBoxW(nullptr, LoadAppString(IDS_SINGLE_INSTANCE_CREATE_FAILED).c_str(),
                    LoadAppString(IDS_ERROR).c_str(), MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, LoadAppString(IDS_ALREADY_RUNNING).c_str(),
                    LoadAppString(IDS_PROMPT).c_str(), MB_ICONINFORMATION);
        CloseHandle(singleInstanceMutex);
        CoUninitialize();
        return 0;
    }

    MainWindow app;
    if (!app.Create(hInstance, nCmdShow)) {
        MessageBoxW(nullptr, LoadAppString(IDS_APP_INIT_FAILED).c_str(),
                    LoadAppString(IDS_ERROR).c_str(), MB_ICONERROR);
        CloseHandle(singleInstanceMutex);
        CoUninitialize();
        return 1;
    }

    const int code = app.MessageLoop();
    CloseHandle(singleInstanceMutex);
    CoUninitialize();
    return code;
}

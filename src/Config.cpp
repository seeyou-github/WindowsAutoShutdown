#include "Config.h"

#include "AppStrings.h"
#include "..\res\resource.h"

#include <array>
#include <windows.h>

namespace {
bool ReadBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool defValue) {
    return GetPrivateProfileIntW(section, key, defValue ? 1 : 0, path.c_str()) != 0;
}

int ReadInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int defValue) {
    return GetPrivateProfileIntW(section, key, defValue, path.c_str());
}

unsigned long long ReadULL(const std::wstring& path, const wchar_t* section, const wchar_t* key, unsigned long long defValue) {
    wchar_t buffer[64] = {};
    GetPrivateProfileStringW(section, key, L"0", buffer, 64, path.c_str());
    if (buffer[0] == L'\0') {
        return defValue;
    }
    return _wcstoui64(buffer, nullptr, 10);
}

void WriteBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool value) {
    WritePrivateProfileStringW(section, key, value ? L"1" : L"0", path.c_str());
}

void WriteInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int value) {
    wchar_t buf[32] = {};
    wsprintfW(buf, L"%d", value);
    WritePrivateProfileStringW(section, key, buf, path.c_str());
}

void WriteULL(const std::wstring& path, const wchar_t* section, const wchar_t* key, unsigned long long value) {
    const std::wstring text = std::to_wstring(value);
    WritePrivateProfileStringW(section, key, text.c_str(), path.c_str());
}

void WriteDescription(const std::wstring& path, const wchar_t* key, UINT stringId) {
    const std::wstring section = LoadAppString(IDS_INI_SECTION_DESCRIPTION);
    const std::wstring value = LoadAppString(stringId);
    WritePrivateProfileStringW(section.c_str(), key, value.c_str(), path.c_str());
}
} 

bool ConfigManager::Load(const std::wstring& iniPath, AppConfig& outConfig) {
    outConfig.preventSleep = ReadBool(iniPath, L"Timer", L"PreventSleep", false);
    for (int i = 0; i < 8; ++i) {
        wchar_t key[32] = {};
        wsprintfW(key, L"AddBtn%dMin", i + 1);
        int v = ReadInt(iniPath, L"Timer", key, outConfig.addMinutes[static_cast<size_t>(i)]);
        if (v < 1) v = outConfig.addMinutes[static_cast<size_t>(i)];
        if (v > 24 * 60) v = 24 * 60;
        outConfig.addMinutes[static_cast<size_t>(i)] = v;
    }

    outConfig.monitorEnabled = ReadBool(iniPath, L"Monitor", L"Enabled", false);
    outConfig.sizeRuleEnabled = ReadBool(iniPath, L"Monitor", L"SizeRuleEnabled", false);
    outConfig.sizeThresholdMb = ReadULL(iniPath, L"Monitor", L"SizeThresholdMb", 1024);
    outConfig.stallRuleEnabled = ReadBool(iniPath, L"Monitor", L"StallRuleEnabled", false);
    outConfig.stallMinutes = ReadInt(iniPath, L"Monitor", L"StallMinutes", 10);
    outConfig.monitorLogIntervalSeconds = ReadInt(iniPath, L"Monitor", L"LogIntervalSeconds", 10);

    wchar_t pathBuf[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Monitor", L"Path", L"", pathBuf, MAX_PATH, iniPath.c_str());
    outConfig.monitorPath = pathBuf;

    if (outConfig.stallMinutes < 1) outConfig.stallMinutes = 10;
    if (outConfig.sizeThresholdMb < 1) outConfig.sizeThresholdMb = 1024;
    if (outConfig.monitorLogIntervalSeconds < 1) outConfig.monitorLogIntervalSeconds = 10;
    if (outConfig.monitorLogIntervalSeconds > 3600) outConfig.monitorLogIntervalSeconds = 3600;

    return true;
}

bool ConfigManager::Save(const std::wstring& iniPath, const AppConfig& config) {
    WriteBool(iniPath, L"Timer", L"PreventSleep", config.preventSleep);
    for (int i = 0; i < 8; ++i) {
        wchar_t key[32] = {};
        wsprintfW(key, L"AddBtn%dMin", i + 1);
        WriteInt(iniPath, L"Timer", key, config.addMinutes[static_cast<size_t>(i)]);
    }

    WriteBool(iniPath, L"Monitor", L"Enabled", config.monitorEnabled);
    WriteBool(iniPath, L"Monitor", L"SizeRuleEnabled", config.sizeRuleEnabled);
    WriteULL(iniPath, L"Monitor", L"SizeThresholdMb", config.sizeThresholdMb);
    WriteBool(iniPath, L"Monitor", L"StallRuleEnabled", config.stallRuleEnabled);
    WriteInt(iniPath, L"Monitor", L"StallMinutes", config.stallMinutes);
    WriteInt(iniPath, L"Monitor", L"LogIntervalSeconds", config.monitorLogIntervalSeconds);
    WritePrivateProfileStringW(L"Monitor", L"Path", config.monitorPath.c_str(), iniPath.c_str());

    const std::array<UINT, 8> addButtonDescriptionIds = {
        IDS_INI_DESC_ADD_BTN1,
        IDS_INI_DESC_ADD_BTN2,
        IDS_INI_DESC_ADD_BTN3,
        IDS_INI_DESC_ADD_BTN4,
        IDS_INI_DESC_ADD_BTN5,
        IDS_INI_DESC_ADD_BTN6,
        IDS_INI_DESC_ADD_BTN7,
        IDS_INI_DESC_ADD_BTN8,
    };

    WriteDescription(iniPath, L"PreventSleep", IDS_INI_DESC_PREVENT_SLEEP);
    for (int i = 0; i < 8; ++i) {
        wchar_t key[32] = {};
        wsprintfW(key, L"AddBtn%dMin", i + 1);
        WriteDescription(iniPath, key, addButtonDescriptionIds[static_cast<size_t>(i)]);
    }
    WriteDescription(iniPath, L"Monitor.Enabled", IDS_INI_DESC_MONITOR_ENABLED);
    WriteDescription(iniPath, L"Monitor.Path", IDS_INI_DESC_MONITOR_PATH);
    WriteDescription(iniPath, L"SizeRuleEnabled", IDS_INI_DESC_SIZE_RULE);
    WriteDescription(iniPath, L"SizeThresholdMb", IDS_INI_DESC_SIZE_THRESHOLD);
    WriteDescription(iniPath, L"StallRuleEnabled", IDS_INI_DESC_STALL_RULE);
    WriteDescription(iniPath, L"StallMinutes", IDS_INI_DESC_STALL_MINUTES);
    WriteDescription(iniPath, L"LogIntervalSeconds", IDS_INI_DESC_LOG_INTERVAL);
    return true;
}

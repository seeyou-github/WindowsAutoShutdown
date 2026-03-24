#include "Config.h"

#include "AppStrings.h"
#include "..\res\resource.h"

#include <array>
#include <windows.h>

namespace {
std::wstring ReadString(const std::wstring& path, const wchar_t* section, const wchar_t* key) {
    wchar_t buffer[512] = {};
    GetPrivateProfileStringW(section, key, L"", buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
    return buffer;
}

bool WriteStringIfChanged(const std::wstring& path, const wchar_t* section, const wchar_t* key, const std::wstring& value) {
    if (ReadString(path, section, key) == value) {
        return true;
    }
    return WritePrivateProfileStringW(section, key, value.c_str(), path.c_str()) != FALSE;
}

bool ReadBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool defValue) {
    return GetPrivateProfileIntW(section, key, defValue ? 1 : 0, path.c_str()) != 0;
}

int ReadInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int defValue) {
    return GetPrivateProfileIntW(section, key, defValue, path.c_str());
}

void WriteBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool value) {
    WriteStringIfChanged(path, section, key, value ? L"1" : L"0");
}

void WriteInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int value) {
    wchar_t buf[32] = {};
    wsprintfW(buf, L"%d", value);
    WriteStringIfChanged(path, section, key, buf);
}

void WriteDescription(const std::wstring& path, const wchar_t* key, UINT stringId) {
    const std::wstring section = LoadAppString(IDS_INI_SECTION_DESCRIPTION);
    const std::wstring value = LoadAppString(stringId);
    WriteStringIfChanged(path, section.c_str(), key, value);
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

    return true;
}

bool ConfigManager::Save(const std::wstring& iniPath, const AppConfig& config) {
    WriteBool(iniPath, L"Timer", L"PreventSleep", config.preventSleep);
    for (int i = 0; i < 8; ++i) {
        wchar_t key[32] = {};
        wsprintfW(key, L"AddBtn%dMin", i + 1);
        WriteInt(iniPath, L"Timer", key, config.addMinutes[static_cast<size_t>(i)]);
    }

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
    return true;
}

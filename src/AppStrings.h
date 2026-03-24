#pragma once

#include <string>
#include <windows.h>
#include <cstdarg>

inline std::wstring LoadAppString(UINT id) {
    wchar_t buffer[1024] = {};
    const int length = LoadStringW(GetModuleHandleW(nullptr), id, buffer, static_cast<int>(std::size(buffer)));
    return std::wstring(buffer, buffer + length);
}

inline std::wstring FormatAppString(UINT id, ...) {
    const std::wstring pattern = LoadAppString(id);
    wchar_t buffer[2048] = {};
    va_list args;
    va_start(args, id);
    _vsnwprintf_s(buffer, std::size(buffer), _TRUNCATE, pattern.c_str(), args);
    va_end(args);
    return buffer;
}

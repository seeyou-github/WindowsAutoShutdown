#pragma once

#include <array>
#include <string>

struct AppConfig {
    bool preventSleep = false;
    std::array<int, 8> addMinutes = {1, 5, 10, 30, 60, 120, 300, 600};
    bool monitorEnabled = false;
    std::wstring monitorPath;
    bool sizeRuleEnabled = false;
    unsigned long long sizeThresholdMb = 1024;
    bool stallRuleEnabled = false;
    int stallMinutes = 10;
    int monitorLogIntervalSeconds = 10;
};

class ConfigManager {
public:
    static bool Load(const std::wstring& iniPath, AppConfig& outConfig);
    static bool Save(const std::wstring& iniPath, const AppConfig& config);
};

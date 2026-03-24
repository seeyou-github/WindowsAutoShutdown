#pragma once

#include <chrono>
#include <string>

enum class MonitorTriggerType {
    none,
    size_threshold,
    stall_timeout,
};

struct MonitorSettings {
    bool enabled = false;
    std::wstring path;
    bool sizeRuleEnabled = false;
    unsigned long long sizeThresholdBytes = 0;
    bool stallRuleEnabled = false;
    int stallSeconds = 0;
};

struct MonitorTickResult {
    bool triggered = false;
    MonitorTriggerType triggerType = MonitorTriggerType::none;
    std::wstring reason;
    unsigned long long currentSizeBytes = 0;
};

class FolderMonitor {
public:
    void UpdateSettings(const MonitorSettings& settings);
    void ResetState();
    MonitorTickResult Tick();

private:
    unsigned long long CalculateFolderSize(const std::wstring& folderPath) const;

    MonitorSettings settings_{};
    bool hasLastSample_ = false;
    unsigned long long lastSizeBytes_ = 0;
    std::chrono::steady_clock::time_point lastChangeAt_{};
};

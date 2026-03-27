#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

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
    int stableSeconds = 0;
};

class FolderMonitor {
public:
    FolderMonitor();
    ~FolderMonitor();

    FolderMonitor(const FolderMonitor&) = delete;
    FolderMonitor& operator=(const FolderMonitor&) = delete;

    void UpdateSettings(const MonitorSettings& settings);
    void ResetState();
    MonitorTickResult Tick();

private:
    void WorkerLoop();
    unsigned long long CalculateFolderSize(const std::wstring& folderPath) const;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stopWorker_ = false;
    bool settingsDirty_ = false;
    MonitorSettings settings_{};
    bool hasLastSample_ = false;
    unsigned long long lastSizeBytes_ = 0;
    std::chrono::steady_clock::time_point lastChangeAt_{};
    MonitorTickResult latestResult_{};
};

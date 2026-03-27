#include "FolderMonitor.h"

#include "AppStrings.h"
#include "..\res\resource.h"

#include <filesystem>
#include <system_error>

FolderMonitor::FolderMonitor() : worker_(&FolderMonitor::WorkerLoop, this) {}

FolderMonitor::~FolderMonitor() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopWorker_ = true;
        settingsDirty_ = true;
    }
    cv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void FolderMonitor::UpdateSettings(const MonitorSettings& settings) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool pathChanged = settings_.path != settings.path;
        settings_ = settings;
        if (pathChanged) {
            hasLastSample_ = false;
            lastSizeBytes_ = 0;
            lastChangeAt_ = {};
            latestResult_ = {};
        }
        settingsDirty_ = true;
    }
    cv_.notify_one();
}

void FolderMonitor::ResetState() {
    std::lock_guard<std::mutex> lock(mutex_);
    hasLastSample_ = false;
    lastSizeBytes_ = 0;
    lastChangeAt_ = {};
    latestResult_ = {};
}

MonitorTickResult FolderMonitor::Tick() {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestResult_;
}

void FolderMonitor::WorkerLoop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopWorker_) {
        cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
            return stopWorker_ || settingsDirty_;
        });
        if (stopWorker_) {
            break;
        }

        settingsDirty_ = false;
        const MonitorSettings settings = settings_;
        const bool hasLastSample = hasLastSample_;
        const unsigned long long lastSizeBytes = lastSizeBytes_;
        const auto lastChangeAt = lastChangeAt_;
        lock.unlock();

        MonitorTickResult result;
        bool nextHasLastSample = hasLastSample;
        unsigned long long nextLastSizeBytes = lastSizeBytes;
        auto nextLastChangeAt = lastChangeAt;

        if (settings.enabled && !settings.path.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(settings.path, ec) && std::filesystem::is_directory(settings.path, ec)) {
                result.currentSizeBytes = CalculateFolderSize(settings.path);

                const auto now = std::chrono::steady_clock::now();
                if (!nextHasLastSample) {
                    nextHasLastSample = true;
                    nextLastSizeBytes = result.currentSizeBytes;
                    nextLastChangeAt = now;
                } else if (result.currentSizeBytes != nextLastSizeBytes) {
                    nextLastSizeBytes = result.currentSizeBytes;
                    nextLastChangeAt = now;
                }

                if (nextHasLastSample) {
                    result.stableSeconds = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::seconds>(now - nextLastChangeAt).count());
                    if (result.stableSeconds < 0) {
                        result.stableSeconds = 0;
                    }
                }

                if (settings.sizeRuleEnabled && settings.sizeThresholdBytes > 0 &&
                    result.currentSizeBytes >= settings.sizeThresholdBytes) {
                    result.triggered = true;
                    result.triggerType = MonitorTriggerType::size_threshold;
                    result.reason = LoadAppString(IDS_MONITOR_SIZE_TRIGGER_REASON);
                } else if (settings.stallRuleEnabled && settings.stallSeconds > 0 && nextHasLastSample &&
                           result.stableSeconds >= settings.stallSeconds) {
                    result.triggered = true;
                    result.triggerType = MonitorTriggerType::stall_timeout;
                    result.reason = LoadAppString(IDS_MONITOR_STALL_TRIGGER_REASON);
                }
            }
        }

        lock.lock();
        if (stopWorker_) {
            break;
        }
        hasLastSample_ = nextHasLastSample;
        lastSizeBytes_ = nextLastSizeBytes;
        lastChangeAt_ = nextLastChangeAt;
        latestResult_ = result;
    }
}

unsigned long long FolderMonitor::CalculateFolderSize(const std::wstring& folderPath) const {
    unsigned long long total = 0;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(
        folderPath, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;

    while (it != end) {
        if (!ec && it->is_regular_file(ec)) {
            total += static_cast<unsigned long long>(it->file_size(ec));
        }
        it.increment(ec);
        ec.clear();
    }
    return total;
}

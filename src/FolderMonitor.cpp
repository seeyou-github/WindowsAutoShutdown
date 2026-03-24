#include "FolderMonitor.h"

#include "AppStrings.h"
#include "..\res\resource.h"

#include <filesystem>
#include <system_error>

void FolderMonitor::UpdateSettings(const MonitorSettings& settings) {
    const bool pathChanged = settings_.path != settings.path;
    settings_ = settings;
    if (pathChanged) {
        ResetState();
    }
}

void FolderMonitor::ResetState() {
    hasLastSample_ = false;
    lastSizeBytes_ = 0;
    lastChangeAt_ = {};
}

MonitorTickResult FolderMonitor::Tick() {
    MonitorTickResult result;
    if (!settings_.enabled || settings_.path.empty()) {
        return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(settings_.path, ec) || !std::filesystem::is_directory(settings_.path, ec)) {
        return result;
    }

    result.currentSizeBytes = CalculateFolderSize(settings_.path);

    const auto now = std::chrono::steady_clock::now();
    if (!hasLastSample_) {
        hasLastSample_ = true;
        lastSizeBytes_ = result.currentSizeBytes;
        lastChangeAt_ = now;
    } else if (result.currentSizeBytes != lastSizeBytes_) {
        lastSizeBytes_ = result.currentSizeBytes;
        lastChangeAt_ = now;
    }

    if (settings_.sizeRuleEnabled && settings_.sizeThresholdBytes > 0 &&
        result.currentSizeBytes >= settings_.sizeThresholdBytes) {
        result.triggered = true;
        result.triggerType = MonitorTriggerType::size_threshold;
        result.reason = LoadAppString(IDS_MONITOR_SIZE_TRIGGER_REASON);
        return result;
    }

    if (settings_.stallRuleEnabled && settings_.stallSeconds > 0 && hasLastSample_) {
        const auto stableSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(now - lastChangeAt_).count();
        if (stableSeconds >= settings_.stallSeconds) {
            result.triggered = true;
            result.triggerType = MonitorTriggerType::stall_timeout;
            result.reason = LoadAppString(IDS_MONITOR_STALL_TRIGGER_REASON);
            return result;
        }
    }

    return result;
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

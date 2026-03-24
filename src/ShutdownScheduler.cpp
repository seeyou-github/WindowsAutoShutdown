#include "ShutdownScheduler.h"

void ShutdownScheduler::Start(int totalSeconds, const std::wstring& reasonText) {
    if (totalSeconds < 1) {
        totalSeconds = 1;
    }
    running_ = true;
    reason_ = reasonText;
    deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(totalSeconds);
    shown3_ = false;
}

void ShutdownScheduler::Cancel() {
    running_ = false;
    reason_.clear();
    shown3_ = false;
}

void ShutdownScheduler::AddSeconds(int seconds) {
    if (!running_ || seconds <= 0) {
        return;
    }
    deadline_ += std::chrono::seconds(seconds);
}

int ShutdownScheduler::GetRemainingSeconds() const {
    if (!running_) {
        return 0;
    }
    auto now = std::chrono::steady_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(deadline_ - now).count();
    if (sec < 0) return 0;
    return static_cast<int>(sec);
}

ScheduleTickResult ShutdownScheduler::Tick() {
    ScheduleTickResult result;
    result.running = running_;
    if (!running_) {
        return result;
    }

    result.remainingSeconds = GetRemainingSeconds();

    if (!shown3_ && result.remainingSeconds <= 180 && result.remainingSeconds > 120) {
        result.showThreeMinuteReminder = true;
        shown3_ = true;
    }
    if (result.remainingSeconds <= 0) {
        result.shouldShutdownNow = true;
    }
    return result;
}

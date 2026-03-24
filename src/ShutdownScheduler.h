#pragma once

#include <chrono>
#include <string>

struct ScheduleTickResult {
    bool running = false;
    int remainingSeconds = 0;
    bool showThreeMinuteReminder = false;
    bool shouldShutdownNow = false;
};

class ShutdownScheduler {
public:
    void Start(int totalSeconds, const std::wstring& reasonText);
    void Cancel();
    void AddSeconds(int seconds);
    bool IsRunning() const { return running_; }
    int GetRemainingSeconds() const;
    std::wstring GetReason() const { return reason_; }
    ScheduleTickResult Tick();

private:
    bool running_ = false;
    std::chrono::steady_clock::time_point deadline_{};
    std::wstring reason_;
    bool shown3_ = false;
};

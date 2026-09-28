#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

class WindowsNetworkTracker {
public:
    WindowsNetworkTracker();
    ~WindowsNetworkTracker();

    WindowsNetworkTracker(const WindowsNetworkTracker&) = delete;
    WindowsNetworkTracker& operator=(const WindowsNetworkTracker&) = delete;

    bool available() const;
    unsigned long startError() const;
    std::unordered_map<std::uint32_t, std::uint64_t> takeRates();
    void onEvent(void* record);

private:
    struct ByteCount {
        std::uint64_t send = 0;
        std::uint64_t recv = 0;
    };

    void stop();
    void traceLoop();

    mutable std::mutex mutex_;
    std::unordered_map<std::uint32_t, ByteCount> bytes_;
    std::chrono::steady_clock::time_point lastTake_{};
    std::thread thread_;
    std::uint64_t session_ = 0;
    std::uint64_t trace_ = static_cast<std::uint64_t>(-1);
    std::atomic<unsigned long> startError_{0};
    std::atomic<bool> available_{false};
    std::wstring sessionName_;
};

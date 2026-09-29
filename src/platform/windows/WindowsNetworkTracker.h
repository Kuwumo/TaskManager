#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// 按 PID 统计网络收发字节。
//
// Windows 没有稳定的「每进程网络字节」普通 API，任务管理器本身用 ETW。
// 这里开一个实时会话，订阅 Microsoft-Windows-Kernel-Network，
// 在回调里按事件里的 PID 和 size 累加，sample() 再换成字节/秒。
//
// 启动会话通常需要管理员权限。失败时 available() 为 false，
// 进程表的网络列显示「—」，整机网络曲线仍由 PDH 提供。
//
// 头文件故意不包含 windows.h，避免被 Qt 源文件包含时发生 byte 等宏冲突。
// 会话句柄用 uint64 保存，实际类型是 TRACEHANDLE。
class WindowsNetworkTracker {
public:
    WindowsNetworkTracker();
    ~WindowsNetworkTracker();

    WindowsNetworkTracker(const WindowsNetworkTracker&) = delete;
    WindowsNetworkTracker& operator=(const WindowsNetworkTracker&) = delete;

    // 会话已建立且跟踪线程在跑。回调线程和采样线程都会读它，所以用 atomic。
    bool available() const;
    // StartTrace / EnableTraceEx2 / OpenTrace 失败时的 Win32 错误码，成功为 0。
    unsigned long startError() const;
    // 取出自上次调用以来每个 PID 的收发字节/秒，并清零累计值。
    std::unordered_map<std::uint32_t, std::uint64_t> takeRates();
    // ETW 回调入口。record 实际是 PEVENT_RECORD，由独立跟踪线程调用。
    void onEvent(void* record);

private:
    struct ByteCount {
        std::uint64_t send = 0;
        std::uint64_t recv = 0;
    };

    void stop();
    void traceLoop();

    // 只保护 bytes_。available_ 和 startError_ 用 atomic，避免回调和 stop() 互等。
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

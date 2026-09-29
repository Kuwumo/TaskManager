#pragma once

#include "core/ISystemMonitor.h"

#include <chrono>
#include <memory>
#include <unordered_map>

class WindowsNetworkTracker;

// Windows 采集实现。
//
// 整机 CPU、磁盘、网络走 PDH（英文计数器路径，不随系统语言变化）。
// 物理内存走 GlobalMemoryStatusEx。
// 每个进程的 CPU 用 GetProcessTimes 与 GetSystemTimes 的增量比。
// 每个进程的磁盘用 GetProcessIoCounters 的读写字节增量。
// 每个进程的网络交给 WindowsNetworkTracker（ETW）。
//
// 速率都依赖上一拍，所以第一拍的 CPU 和磁盘会标记为不可用。
class WindowsSystemMonitor final : public ISystemMonitor {
public:
    WindowsSystemMonitor();
    ~WindowsSystemMonitor() override;

    SystemSnapshot sample() override;
    bool terminateProcess(quint32 pid, QString* error) override;

private:
    // 上一拍某个进程的计数，用来和这一拍做差。
    // create 是进程创建时间。PID 被系统复用时创建时间会变，必须丢掉旧增量。
    struct PrevProc {
        quint64 create = 0;
        quint64 cpu = 0;      // 内核时间 + 用户时间，单位 100 纳秒
        quint64 ioBytes = 0;  // 读传输 + 写传输
        bool hasBaseline = false;
    };

    // PDH 查询句柄。定义放在 cpp 里，避免这个头文件包含 windows.h。
    struct PdhState;

    void sampleSystem(SystemSnapshot* snapshot);
    void sampleProcesses(SystemSnapshot* snapshot);

    std::unique_ptr<WindowsNetworkTracker> network_;
    std::unique_ptr<PdhState> pdh_;
    // 上一拍仍存在的进程。这一拍没再出现的会被丢弃，避免表无限增长。
    std::unordered_map<quint32, PrevProc> previous_;
    // GetSystemTimes 的上一拍。内核时间已经包含空闲时间，总时间 = 内核 + 用户。
    quint64 prevSystemTotal_ = 0;
    quint64 prevIdle_ = 0;
    bool hasSystemBaseline_ = false;
    bool hasLastSample_ = false;
    // 用墙钟计算磁盘字节/秒。CPU 百分比用的是系统时间增量，不用这只钟。
    std::chrono::steady_clock::time_point lastSample_{};
};

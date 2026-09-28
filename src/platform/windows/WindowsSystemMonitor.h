#pragma once

#include "core/ISystemMonitor.h"

#include <chrono>
#include <memory>
#include <unordered_map>

class WindowsNetworkTracker;

class WindowsSystemMonitor final : public ISystemMonitor {
public:
    WindowsSystemMonitor();
    ~WindowsSystemMonitor() override;

    SystemSnapshot sample() override;
    bool terminateProcess(quint32 pid, QString* error) override;

private:
    struct PrevProc {
        quint64 create = 0;
        quint64 cpu = 0;
        quint64 ioBytes = 0;
        bool hasBaseline = false;
    };

    struct PdhState;

    void sampleSystem(SystemSnapshot* snapshot);
    void sampleProcesses(SystemSnapshot* snapshot);

    std::unique_ptr<WindowsNetworkTracker> network_;
    std::unique_ptr<PdhState> pdh_;
    std::unordered_map<quint32, PrevProc> previous_;
    quint64 prevSystemTotal_ = 0;
    quint64 prevIdle_ = 0;
    bool hasSystemBaseline_ = false;
    bool hasLastSample_ = false;
    std::chrono::steady_clock::time_point lastSample_{};
};

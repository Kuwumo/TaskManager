#pragma once

#include "core/ISystemMonitor.h"

// Linux 占位实现。读取 /proc 的逻辑以后加在这里。
// 本文件没有加入 CMake，当前 Windows 构建不会编译它。
class LinuxSystemMonitor final : public ISystemMonitor {
public:
    SystemSnapshot sample() override;
    bool terminateProcess(quint32 pid, QString* error) override;
};

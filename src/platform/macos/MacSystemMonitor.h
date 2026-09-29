#pragma once

#include "core/ISystemMonitor.h"

// macOS 占位实现。以后可以用 libproc / sysctl 填充。
// 本文件没有加入 CMake，当前 Windows 构建不会编译它。
class MacSystemMonitor final : public ISystemMonitor {
public:
    SystemSnapshot sample() override;
    bool terminateProcess(quint32 pid, QString* error) override;
};

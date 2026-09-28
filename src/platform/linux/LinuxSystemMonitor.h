#pragma once

#include "core/ISystemMonitor.h"

class LinuxSystemMonitor final : public ISystemMonitor {
public:
    SystemSnapshot sample() override;
    bool terminateProcess(quint32 pid, QString* error) override;
};

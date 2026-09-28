#pragma once

#include "core/ISystemMonitor.h"

class MacSystemMonitor final : public ISystemMonitor {
public:
    SystemSnapshot sample() override;
    bool terminateProcess(quint32 pid, QString* error) override;
};

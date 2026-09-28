#pragma once

#include "core/Types.h"

#include <QString>

class ISystemMonitor {
public:
    virtual ~ISystemMonitor() = default;

    virtual SystemSnapshot sample() = 0;
    virtual bool terminateProcess(quint32 pid, QString* error) = 0;
};

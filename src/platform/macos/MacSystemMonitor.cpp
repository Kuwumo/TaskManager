#include "platform/macos/MacSystemMonitor.h"

SystemSnapshot MacSystemMonitor::sample()
{
    SystemSnapshot snapshot;
    snapshot.statusMessage = QStringLiteral("macOS 采集尚未实现。");
    return snapshot;
}

bool MacSystemMonitor::terminateProcess(quint32, QString* error)
{
    if (error != nullptr) {
        *error = QStringLiteral("macOS 采集尚未实现。");
    }
    return false;
}

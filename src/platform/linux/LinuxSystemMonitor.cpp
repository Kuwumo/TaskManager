// 占位。接入时优先读 /proc/stat、/proc/meminfo 和 /proc/<pid>/stat。
#include "platform/linux/LinuxSystemMonitor.h"

SystemSnapshot LinuxSystemMonitor::sample()
{
    SystemSnapshot snapshot;
    snapshot.statusMessage = QStringLiteral("Linux 采集尚未实现。");
    return snapshot;
}

bool LinuxSystemMonitor::terminateProcess(quint32, QString* error)
{
    if (error != nullptr) {
        *error = QStringLiteral("Linux 采集尚未实现。");
    }
    return false;
}

#include "core/SystemMonitorFactory.h"

#include "core/ISystemMonitor.h"

#ifdef Q_OS_WIN
#include "platform/windows/WindowsSystemMonitor.h"
#endif

std::unique_ptr<ISystemMonitor> createSystemMonitor()
{
#ifdef Q_OS_WIN
    return std::make_unique<WindowsSystemMonitor>();
#else
    // Linux 与 macOS 的采集类放在 src/platform/，本版不加入编译。
    return nullptr;
#endif
}

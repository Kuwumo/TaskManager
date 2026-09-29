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
    // LinuxSystemMonitor / MacSystemMonitor 已写好空实现，但 CMake 未加入它们。
    // 返回空指针后，采样线程会显示「当前平台尚未实现系统监控」。
    return nullptr;
#endif
}

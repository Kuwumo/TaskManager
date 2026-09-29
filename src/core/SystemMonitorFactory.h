#pragma once

#include <memory>

class ISystemMonitor;

// 按当前操作系统创建采集实现。
// Windows 返回 WindowsSystemMonitor。其他系统本版返回空指针，
// 采样线程会据此在状态栏提示「尚未实现」。
// Linux / macOS 的源文件在 src/platform/ 下，CMake 没有把它们编进来。
std::unique_ptr<ISystemMonitor> createSystemMonitor();

# 任务管理器

类似于 Windows 的任务管理器。界面和采集接口按跨平台来拆，当前先完成 Windows；Linux 以后接到同一套接口，macOS 视以后是否需要再做。

界面使用 Qt 6 Widgets，构建使用 CMake。Linux 与 macOS 的类放在 `src/platform/`，尚未加入编译。

## 功能

- 进程列表：名称、PID、CPU、内存（工作集）、磁盘读写速率、网络收发速率。可排序，可按名称筛选。
- 结束任务：确认后结束所选进程。权限不足时给出原因。不能结束系统空闲进程、System 和本程序自身。
- 性能：CPU（总占用和每个逻辑核心）、内存、磁盘、网络，各保留约 60 秒曲线。
- 默认每秒刷新，工具栏可以暂停。

## 依赖

- Windows 10 或更高版本
- Visual Studio 2022（含 MSVC x64 和 Windows SDK）
- CMake 3.21 或更高版本
- Qt 6.11（或兼容的 Qt 6）MSVC 2022 64-bit 套件，需要 Widgets

本仓库的 `CMakePresets.json` 把 Qt 前缀设为 `C:/Qt/6.11.2/msvc2022_64`。如果 Qt 装在其他位置，改这个路径。

## 编译

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc --config Release
```

可执行文件在 `build/Release/TaskManager.exe`。

在没有把 Qt 的 `bin` 加入 PATH 时，用 windeployqt 把运行库放到可执行文件旁边：

```powershell
& "C:\Qt\6.11.2\msvc2022_64\bin\windeployqt.exe" --release build\Release\TaskManager.exe
```

## 计数说明

- 每进程 CPU 是相对整机容量的占用，范围 0–100%，和当前 Windows 任务管理器一致。
- 每进程「磁盘」来自 `GetProcessIoCounters` 的读写字节增量。它包含文件 I/O，不是任务管理器里基于 ETW 的磁盘活动时间。
- 每进程网络使用 ETW 提供者 `Microsoft-Windows-Kernel-Network`。启动跟踪会话通常需要管理员权限。权限不足时该列显示「—」，状态栏会说明原因。整机网络曲线使用 PDH，不依赖这次跟踪。
- 打不开句柄的受保护进程仍会列出名称和 PID，CPU、内存和磁盘显示为「—」。

## 以后扩展

`SystemMonitorFactory` 按系统选择 `ISystemMonitor`。Windows 实现在 `src/platform/windows/`。`src/platform/linux/` 和 `src/platform/macos/` 目前是未编译的占位实现。

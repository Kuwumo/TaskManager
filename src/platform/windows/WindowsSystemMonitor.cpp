#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <tlhelp32.h>

#ifdef byte
#undef byte
#endif

#include "platform/windows/WindowsSystemMonitor.h"

#include "platform/windows/WindowsNetworkTracker.h"

#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace {

// 离开作用域时关闭 HANDLE。快照和进程句柄数量多，用它避免中途 return 漏关。
struct ScopeHandle {
    HANDLE handle = nullptr;
    ~ScopeHandle()
    {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
};

// FILETIME 是两个 32 位字段，拼成 100 纳秒为单位的 64 位计数。
quint64 fileTimeToU64(const FILETIME& value)
{
    ULARGE_INTEGER integer{};
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return integer.QuadPart;
}

QString systemMessage(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);
    QString text = length != 0 ? QString::fromWCharArray(buffer).trimmed()
                               : QStringLiteral("错误码 %1").arg(error);
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return text;
}

double clampPercent(double value)
{
    return std::clamp(value, 0.0, 100.0);
}

bool counterValid(DWORD status)
{
    return status == PDH_CSTATUS_VALID_DATA || status == PDH_CSTATUS_NEW_DATA;
}

} // namespace

struct WindowsSystemMonitor::PdhState {
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER cpu = nullptr;
    PDH_HCOUNTER diskTime = nullptr;
    PDH_HCOUNTER diskBytes = nullptr;
    PDH_HCOUNTER netRecv = nullptr;
    PDH_HCOUNTER netSend = nullptr;

    ~PdhState()
    {
        if (query != nullptr) {
            PdhCloseQuery(query);
        }
    }
};

WindowsSystemMonitor::WindowsSystemMonitor()
    : network_(std::make_unique<WindowsNetworkTracker>())
    , pdh_(std::make_unique<PdhState>())
{
    // 英文计数器路径。中文 Windows 上本地化路径会变，PdhAddEnglishCounter 不受影响。
    // 构造末尾先 Collect 一次，PDH 的速率计数器要有上一拍才有值。
    if (PdhOpenQueryW(nullptr, 0, &pdh_->query) != ERROR_SUCCESS) {
        pdh_.reset();
        return;
    }

    const auto add = [this](const wchar_t* path, PDH_HCOUNTER* counter) {
        if (PdhAddEnglishCounterW(pdh_->query, path, 0, counter) != ERROR_SUCCESS) {
            *counter = nullptr;
        }
    };
    add(L"\\Processor(*)\\% Processor Time", &pdh_->cpu);
    add(L"\\PhysicalDisk(_Total)\\% Disk Time", &pdh_->diskTime);
    add(L"\\PhysicalDisk(_Total)\\Disk Bytes/sec", &pdh_->diskBytes);
    add(L"\\Network Interface(*)\\Bytes Received/sec", &pdh_->netRecv);
    add(L"\\Network Interface(*)\\Bytes Sent/sec", &pdh_->netSend);
    PdhCollectQueryData(pdh_->query);
}

WindowsSystemMonitor::~WindowsSystemMonitor() = default;

SystemSnapshot WindowsSystemMonitor::sample()
{
    SystemSnapshot snapshot;
    sampleSystem(&snapshot);
    sampleProcesses(&snapshot);

    if (!network_->available()) {
        const unsigned long code = network_->startError();
        if (code == ERROR_ACCESS_DENIED) {
            snapshot.statusMessage = QStringLiteral(
                "每进程网络不可用：需要管理员权限才能跟踪网络。系统网络曲线仍可使用。");
        } else {
            snapshot.statusMessage = QStringLiteral(
                                         "每进程网络不可用（错误 %1）。系统网络曲线仍可使用。")
                                         .arg(code);
        }
    } else {
        snapshot.statusMessage = QStringLiteral("正在监控");
    }
    return snapshot;
}

bool WindowsSystemMonitor::terminateProcess(quint32 pid, QString* error)
{
    // PID 0 是空闲进程，PID 4 是 System。两者都不能结束。
    // 结束自身会把监控线程一起杀掉，所以直接拒绝。
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (pid == 0 || pid == 4) {
        return fail(QStringLiteral("不能结束系统进程。"));
    }
    if (pid == GetCurrentProcessId()) {
        return fail(QStringLiteral("不能结束任务管理器自身。"));
    }

    ScopeHandle process;
    process.handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process.handle == nullptr) {
        const DWORD code = GetLastError();
        if (code == ERROR_ACCESS_DENIED) {
            return fail(QStringLiteral("权限不足，无法结束该进程。%1").arg(systemMessage(code)));
        }
        return fail(QStringLiteral("无法打开进程。%1").arg(systemMessage(code)));
    }

    if (!TerminateProcess(process.handle, 1)) {
        const DWORD code = GetLastError();
        if (code == ERROR_ACCESS_DENIED) {
            return fail(QStringLiteral("权限不足，无法结束该进程。%1").arg(systemMessage(code)));
        }
        return fail(QStringLiteral("结束进程失败。%1").arg(systemMessage(code)));
    }
    return true;
}

void WindowsSystemMonitor::sampleSystem(SystemSnapshot* snapshot)
{
    snapshot->cpu.logicalProcessors = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        snapshot->memory.available = true;
        snapshot->memory.totalBytes = memory.ullTotalPhys;
        snapshot->memory.usedBytes = memory.ullTotalPhys - memory.ullAvailPhys;
    }

    if (!pdh_) {
        return;
    }
    if (PdhCollectQueryData(pdh_->query) != ERROR_SUCCESS) {
        return;
    }

    if (pdh_->cpu != nullptr) {
        DWORD bufferSize = 0;
        DWORD count = 0;
        const PDH_STATUS sizing = PdhGetFormattedCounterArrayW(
            pdh_->cpu, PDH_FMT_DOUBLE, &bufferSize, &count, nullptr);
        if (sizing == PDH_MORE_DATA && bufferSize > 0) {
            std::vector<BYTE> buffer(bufferSize);
            auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
            if (PdhGetFormattedCounterArrayW(pdh_->cpu, PDH_FMT_DOUBLE, &bufferSize, &count, items) == ERROR_SUCCESS) {
                struct CoreReading {
                    int order = 0;
                    double value = 0;
                };
                std::vector<CoreReading> cores;
                bool haveTotal = false;
                for (DWORD index = 0; index < count; ++index) {
                    if (!counterValid(items[index].FmtValue.CStatus) || items[index].szName == nullptr) {
                        continue;
                    }
                    const QString name = QString::fromWCharArray(items[index].szName);
                    const double value = clampPercent(items[index].FmtValue.doubleValue);
                    if (name.compare(QStringLiteral("_Total"), Qt::CaseInsensitive) == 0) {
                        snapshot->cpu.totalPercent = value;
                        haveTotal = true;
                        continue;
                    }
                    bool numeric = false;
                    const int order = name.toInt(&numeric);
                    cores.push_back(CoreReading{numeric ? order : 100000 + static_cast<int>(cores.size()), value});
                }
                std::sort(cores.begin(), cores.end(), [](const CoreReading& left, const CoreReading& right) {
                    return left.order < right.order;
                });
                snapshot->cpu.perCore.reserve(static_cast<int>(cores.size()));
                double sum = 0;
                for (const CoreReading& core : cores) {
                    snapshot->cpu.perCore.push_back(core.value);
                    sum += core.value;
                }
                if (!haveTotal && !cores.empty()) {
                    snapshot->cpu.totalPercent = clampPercent(sum / static_cast<double>(cores.size()));
                }
                snapshot->cpu.available = haveTotal || !cores.empty();
            }
        }
    }

    const auto readSingle = [](PDH_HCOUNTER counter, double* value) {
        if (counter == nullptr) {
            return false;
        }
        PDH_FMT_COUNTERVALUE formatted{};
        if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &formatted) != ERROR_SUCCESS) {
            return false;
        }
        if (!counterValid(formatted.CStatus) || !std::isfinite(formatted.doubleValue)) {
            return false;
        }
        *value = formatted.doubleValue;
        return true;
    };

    double diskTime = 0;
    double diskBytes = 0;
    const bool haveDiskTime = readSingle(pdh_->diskTime, &diskTime);
    const bool haveDiskBytes = readSingle(pdh_->diskBytes, &diskBytes);
    if (haveDiskTime || haveDiskBytes) {
        snapshot->disk.available = true;
        snapshot->disk.activePercent = haveDiskTime ? std::max(0.0, diskTime) : 0;
        snapshot->disk.bytesPerSec = haveDiskBytes && diskBytes > 0 ? static_cast<quint64>(diskBytes) : 0;
    }

    // 通配符计数器每个网卡一项。回环流量不是对外网络，不计入整机曲线。
    auto sumInterfaces = [](PDH_HCOUNTER counter, QStringList* names) {
        double total = 0;
        bool any = false;
        if (counter == nullptr) {
            return std::pair<bool, double>{false, 0};
        }
        DWORD bufferSize = 0;
        DWORD count = 0;
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bufferSize, &count, nullptr) != PDH_MORE_DATA
            || bufferSize == 0) {
            return std::pair<bool, double>{false, 0};
        }
        std::vector<BYTE> buffer(bufferSize);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bufferSize, &count, items) != ERROR_SUCCESS) {
            return std::pair<bool, double>{false, 0};
        }
        for (DWORD index = 0; index < count; ++index) {
            if (!counterValid(items[index].FmtValue.CStatus) || items[index].szName == nullptr) {
                continue;
            }
            const QString name = QString::fromWCharArray(items[index].szName);
            if (name.contains(QStringLiteral("Loopback"), Qt::CaseInsensitive)) {
                continue;
            }
            if (std::isfinite(items[index].FmtValue.doubleValue) && items[index].FmtValue.doubleValue > 0) {
                total += items[index].FmtValue.doubleValue;
            }
            any = true;
            if (names != nullptr && !names->contains(name)) {
                names->push_back(name);
            }
        }
        return std::pair<bool, double>{any, total};
    };

    QStringList adapters;
    const auto received = sumInterfaces(pdh_->netRecv, &adapters);
    const auto sent = sumInterfaces(pdh_->netSend, nullptr);
    if (received.first || sent.first) {
        snapshot->net.available = true;
        snapshot->net.recvBytesPerSec = received.second > 0 ? static_cast<quint64>(received.second) : 0;
        snapshot->net.sendBytesPerSec = sent.second > 0 ? static_cast<quint64>(sent.second) : 0;
        snapshot->net.adapterSummary = adapters.isEmpty() ? QStringLiteral("所有适配器") : adapters.join(QStringLiteral("、"));
    }
}

void WindowsSystemMonitor::sampleProcesses(SystemSnapshot* snapshot)
{
    // GetSystemTimes：内核时间已包含空闲时间，整机总时间 = 内核 + 用户。
    // 该值是所有逻辑处理器之和。进程 CPU = 进程时间增量 / 整机时间增量 × 100，
    // 因此结果是「占整机的百分比」，不是单核百分比。
    FILETIME idleFt{};
    FILETIME kernelFt{};
    FILETIME userFt{};
    quint64 deltaTotal = 0;
    quint64 deltaIdle = 0;
    bool haveSystemDelta = false;
    if (GetSystemTimes(&idleFt, &kernelFt, &userFt)) {
        const quint64 idle = fileTimeToU64(idleFt);
        const quint64 total = fileTimeToU64(kernelFt) + fileTimeToU64(userFt);
        if (hasSystemBaseline_ && total >= prevSystemTotal_) {
            deltaTotal = total - prevSystemTotal_;
            deltaIdle = idle >= prevIdle_ ? idle - prevIdle_ : 0;
            haveSystemDelta = deltaTotal > 0;
        }
        prevSystemTotal_ = total;
        prevIdle_ = idle;
        hasSystemBaseline_ = true;
    }

    const auto now = std::chrono::steady_clock::now();
    double seconds = 0;
    if (hasLastSample_) {
        seconds = std::chrono::duration<double>(now - lastSample_).count();
    }
    lastSample_ = now;
    hasLastSample_ = true;

    const auto rates = network_->takeRates();
    const bool networkAvailable = network_->available();

    ScopeHandle snap;
    snap.handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    std::unordered_map<quint32, PrevProc> nextPrevious;
    bool foundIdle = false;

    if (snap.handle != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snap.handle, &entry)) {
            do {
                ProcessSnapshot process;
                process.pid = entry.th32ProcessID;
                process.name = QString::fromWCharArray(entry.szExeFile).trimmed();
                if (process.name.isEmpty()) {
                    process.name = QStringLiteral("进程 %1").arg(process.pid);
                }
                process.netAvailable = networkAvailable;
                const auto rate = rates.find(process.pid);
                if (rate != rates.end()) {
                    process.netBytesPerSec = rate->second;
                }

                // 工具帮助快照里 PID 0 的映像名是「[System Process]」。
                // 它的 CPU 用系统空闲时间，而不是 OpenProcess（PID 0 打不开）。
                if (process.pid == 0) {
                    foundIdle = true;
                    process.name = QStringLiteral("System Idle Process");
                    process.cpuAvailable = haveSystemDelta;
                    if (haveSystemDelta) {
                        process.cpuPercent = clampPercent(static_cast<double>(deltaIdle) * 100.0
                                                           / static_cast<double>(deltaTotal));
                    }
                    snapshot->processes.push_back(process);
                    continue;
                }

                ScopeHandle handle;
                handle.handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.pid);
                // 受保护进程会拒绝打开。名称和 PID 仍然列出，CPU、内存、磁盘保持不可用。
                // 网络不依赖进程句柄，上面已经按 PID 填过。
                if (handle.handle == nullptr) {
                    snapshot->processes.push_back(process);
                    continue;
                }

                FILETIME created{};
                FILETIME exited{};
                FILETIME procKernel{};
                FILETIME procUser{};
                const bool haveTimes = GetProcessTimes(handle.handle, &created, &exited, &procKernel, &procUser);

                PROCESS_MEMORY_COUNTERS memory{};
                memory.cb = sizeof(memory);
                if (GetProcessMemoryInfo(handle.handle, &memory, sizeof(memory))) {
                    process.memoryAvailable = true;
                    process.workingSetBytes = memory.WorkingSetSize;
                }

                IO_COUNTERS io{};
                const bool haveIo = GetProcessIoCounters(handle.handle, &io);
                const quint64 ioBytes = haveIo ? io.ReadTransferCount + io.WriteTransferCount : 0;

                if (haveTimes) {
                    const quint64 create = fileTimeToU64(created);
                    const quint64 cpu = fileTimeToU64(procKernel) + fileTimeToU64(procUser);
                    const auto previous = previous_.find(process.pid);
                    if (previous != previous_.end() && previous->second.hasBaseline && previous->second.create == create
                        && haveSystemDelta) {
                        const quint64 deltaCpu = cpu >= previous->second.cpu ? cpu - previous->second.cpu : 0;
                        process.cpuAvailable = true;
                        process.cpuPercent = clampPercent(static_cast<double>(deltaCpu) * 100.0
                                                           / static_cast<double>(deltaTotal));
                    }
                    // 间隔太短时字节/秒会抖。0.2 秒以下本拍不报磁盘速率。
                    // 只加读写传输，不加 OtherTransferCount，避免把非文件 I/O 算进去。
                    if (previous != previous_.end() && previous->second.hasBaseline && previous->second.create == create
                        && haveIo && seconds >= 0.2) {
                        const quint64 deltaIo = ioBytes >= previous->second.ioBytes ? ioBytes - previous->second.ioBytes : 0;
                        process.diskAvailable = true;
                        process.diskBytesPerSec = static_cast<quint64>(static_cast<double>(deltaIo) / seconds);
                    }

                    PrevProc stored;
                    stored.create = create;
                    stored.cpu = cpu;
                    stored.ioBytes = ioBytes;
                    stored.hasBaseline = true;
                    nextPrevious.emplace(process.pid, stored);
                }

                snapshot->processes.push_back(process);
            } while (Process32NextW(snap.handle, &entry));
        }
    }

    if (!foundIdle) {
        ProcessSnapshot idle;
        idle.pid = 0;
        idle.name = QStringLiteral("System Idle Process");
        idle.cpuAvailable = haveSystemDelta;
        idle.netAvailable = networkAvailable;
        if (haveSystemDelta) {
            idle.cpuPercent = clampPercent(static_cast<double>(deltaIdle) * 100.0 / static_cast<double>(deltaTotal));
        }
        snapshot->processes.push_front(idle);
    }

    previous_ = std::move(nextPrevious);
}

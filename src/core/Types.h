#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

// 采样层交给界面的数据。这里只用 Qt 基础类型，不能出现 Win32 头文件，
// 这样进程表和性能曲线不需要知道当前是 Windows、Linux 还是 macOS。

// 某一个进程在一次采样中的指标。
// available 为 false 表示这一项此刻不可用（例如受保护进程打不开句柄，
// 或还没有上一拍数据，无法计算增量）。界面应显示「—」，不要当成 0。
struct ProcessSnapshot {
    quint32 pid = 0;
    QString name;
    // 占整机容量的百分比，范围 0–100。单核跑满在 8 核机器上大约是 12.5%，
    // 与当前 Windows 任务管理器的默认算法一致，不是「单核 100%」。
    double cpuPercent = 0;
    bool cpuAvailable = false;
    // 工作集，单位字节。这是进程当前占用的物理内存，不是提交大小。
    quint64 workingSetBytes = 0;
    bool memoryAvailable = false;
    // 读写字节速率。来自进程 I/O 计数器，包含文件读写，
    // 不等于任务管理器里基于 ETW 的「磁盘活动时间」。
    quint64 diskBytesPerSec = 0;
    bool diskAvailable = false;
    // 发送与接收字节速率之和。ETW 会话打不开时 netAvailable 为 false。
    quint64 netBytesPerSec = 0;
    bool netAvailable = false;
};

// 整机 CPU。perCore 的下标对应逻辑处理器，每一项都是该核心自己的 0–100%。
struct CpuSample {
    double totalPercent = 0;
    QVector<double> perCore;
    int logicalProcessors = 0;
    bool available = false;
};

// 物理内存。usedBytes = 总量 - 可用量，不包含待机列表的细分。
struct MemorySample {
    quint64 totalBytes = 0;
    quint64 usedBytes = 0;
    bool available = false;
};

// 整机磁盘。activePercent 是 PDH 的 % Disk Time，bytesPerSec 是吞吐。
struct DiskSample {
    double activePercent = 0;
    quint64 bytesPerSec = 0;
    bool available = false;
};

// 整机网络。回环适配器已排除，多块网卡的收发分别相加。
struct NetSample {
    quint64 sendBytesPerSec = 0;
    quint64 recvBytesPerSec = 0;
    QString adapterSummary;
    bool available = false;
};

// 一次完整采样。采样线程按值发出，Qt 会在跨线程投递时再复制一份，
// 因此界面可以长期保存历史，不必担心采样线程随后改掉这块数据。
struct SystemSnapshot {
    CpuSample cpu;
    MemorySample memory;
    DiskSample disk;
    NetSample net;
    QVector<ProcessSnapshot> processes;
    // 给状态栏的说明，例如 ETW 因权限失败，或「正在监控」。
    QString statusMessage;
};

// 跨线程信号要携带 SystemSnapshot，必须注册成 Qt 元类型。
Q_DECLARE_METATYPE(SystemSnapshot)

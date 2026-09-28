#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct ProcessSnapshot {
    quint32 pid = 0;
    QString name;
    double cpuPercent = 0;
    bool cpuAvailable = false;
    quint64 workingSetBytes = 0;
    bool memoryAvailable = false;
    quint64 diskBytesPerSec = 0;
    bool diskAvailable = false;
    quint64 netBytesPerSec = 0;
    bool netAvailable = false;
};

struct CpuSample {
    double totalPercent = 0;
    QVector<double> perCore;
    int logicalProcessors = 0;
    bool available = false;
};

struct MemorySample {
    quint64 totalBytes = 0;
    quint64 usedBytes = 0;
    bool available = false;
};

struct DiskSample {
    double activePercent = 0;
    quint64 bytesPerSec = 0;
    bool available = false;
};

struct NetSample {
    quint64 sendBytesPerSec = 0;
    quint64 recvBytesPerSec = 0;
    QString adapterSummary;
    bool available = false;
};

struct SystemSnapshot {
    CpuSample cpu;
    MemorySample memory;
    DiskSample disk;
    NetSample net;
    QVector<ProcessSnapshot> processes;
    QString statusMessage;
};

Q_DECLARE_METATYPE(SystemSnapshot)

#pragma once

#include "core/Types.h"

#include <QString>

// 系统采集接口。界面和采样线程只依赖这一层。
// Windows / Linux / macOS 各自实现，工厂按当前系统返回具体对象。
class ISystemMonitor {
public:
    virtual ~ISystemMonitor() = default;

    // 采集一拍整机指标和进程列表。调用方大约每秒调一次。
    // 实现内部要保留上一拍的计数器，才能算出 CPU 和速率。
    virtual SystemSnapshot sample() = 0;

    // 结束指定进程。成功返回 true。
    // 失败时如果 error 非空，写入可直接显示的原因（权限不足、系统进程等）。
    // 该函数可能和 sample() 同时发生在不同线程，实现不得依赖「只在采样线程调用」。
    virtual bool terminateProcess(quint32 pid, QString* error) = 0;
};

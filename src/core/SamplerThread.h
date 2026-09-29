#pragma once

#include "core/ISystemMonitor.h"

#include <QThread>

#include <condition_variable>
#include <memory>
#include <mutex>

// 后台采样线程。
//
// 界面不能直接在主线程里枚举进程或查询 PDH，否则窗口会卡住。
// 这个类在自己的 run() 里创建 ISystemMonitor，大约每秒采一拍，
// 再通过 snapshotReady 把副本投递回界面线程。
//
// 暂停只是不再发信号。采样循环仍然继续，这样 CPU、磁盘和网络的增量
// 不会把暂停期间的总量算进恢复后的第一拍。
//
// 结束进程从界面线程调用。monitor_ 用 shared_ptr 持有，
// 调用前先复制一份指针，避免采样线程正在退出时把对象析构掉。
class SamplerThread : public QThread {
    Q_OBJECT

public:
    explicit SamplerThread(QObject* parent = nullptr);
    ~SamplerThread() override;

    // 通知循环退出并等待线程结束。析构函数也会调用它，可以重复调用。
    void stop();
    // 暂停或恢复向界面发送快照。
    void setPaused(bool paused);
    // 在监控对象已创建后结束进程。监控尚未就绪时返回 false 并写明原因。
    bool terminateProcess(quint32 pid, QString* error);

signals:
    // 跨线程排队连接。SystemSnapshot 必须先用 qRegisterMetaType 注册。
    void snapshotReady(const SystemSnapshot& snapshot);

protected:
    void run() override;

private:
    // 保护 stop_、paused_ 和 monitor_。采样本身不持有这把锁。
    std::mutex mutex_;
    // stop() 用它立刻叫醒正在等待下一拍的循环。
    std::condition_variable cv_;
    bool stop_ = false;
    bool paused_ = false;
    // 由 run() 创建，供界面线程的 terminateProcess 使用。
    std::shared_ptr<ISystemMonitor> monitor_;
};

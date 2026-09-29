#include "core/SamplerThread.h"

#include "core/SystemMonitorFactory.h"

#include <chrono>
#include <exception>

SamplerThread::SamplerThread(QObject* parent)
    : QThread(parent)
{
    // 必须在跨线程 connect 之前注册，否则 snapshotReady 到不了界面。
    qRegisterMetaType<SystemSnapshot>();
}

SamplerThread::~SamplerThread()
{
    stop();
}

void SamplerThread::stop()
{
    // 先置标志再唤醒。wait_until 的谓词看到 stop_ 就会返回，不必干等满 1 秒。
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (isRunning()) {
        wait();
    }
}

void SamplerThread::setPaused(bool paused)
{
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = paused;
}

bool SamplerThread::terminateProcess(quint32 pid, QString* error)
{
    // 复制 shared_ptr 后再调用，锁内不做 Win32 调用。
    // 这样 run() 退出时可以 reset，正在结束进程的界面仍持有对象直到调用返回。
    std::shared_ptr<ISystemMonitor> monitor;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        monitor = monitor_;
    }
    if (!monitor) {
        if (error) {
            *error = QStringLiteral("监控尚未就绪。");
        }
        return false;
    }
    return monitor->terminateProcess(pid, error);
}

void SamplerThread::run()
{
    // 监控对象创建在采样线程上：PDH 查询和 ETW 会话都跟这条线程的生命周期走。
    auto monitor = createSystemMonitor();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        monitor_ = std::shared_ptr<ISystemMonitor>(std::move(monitor));
    }

    auto next = std::chrono::steady_clock::now();
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_) {
                break;
            }
            cv_.wait_until(lock, next, [this] { return stop_; });
            if (stop_) {
                break;
            }
        }

        std::shared_ptr<ISystemMonitor> monitor;
        bool paused = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            monitor = monitor_;
            paused = paused_;
        }

        SystemSnapshot snapshot;
        if (!monitor) {
            snapshot.statusMessage = QStringLiteral("当前平台尚未实现系统监控。");
        } else {
            try {
                snapshot = monitor->sample();
            } catch (const std::exception& ex) {
                snapshot.statusMessage = QStringLiteral("采样失败：%1").arg(QString::fromUtf8(ex.what()));
            } catch (...) {
                snapshot.statusMessage = QStringLiteral("采样失败。");
            }
        }

        // 暂停时仍然 sample()，只是不发信号。增量基线留在监控对象里，恢复后的第一拍仍是约 1 秒。
        if (!paused) {
            emit snapshotReady(snapshot);
        }

        // 从这一拍结束时再等 1 秒，而不是从循环开始算，避免采样本身的耗时把间隔越拉越长。
        next = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        monitor_.reset();
    }
}

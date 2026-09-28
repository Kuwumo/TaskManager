#include "core/SamplerThread.h"

#include "core/SystemMonitorFactory.h"

#include <chrono>
#include <exception>

SamplerThread::SamplerThread(QObject* parent)
    : QThread(parent)
{
    qRegisterMetaType<SystemSnapshot>();
}

SamplerThread::~SamplerThread()
{
    stop();
}

void SamplerThread::stop()
{
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

        if (!paused) {
            emit snapshotReady(snapshot);
        }

        next = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        monitor_.reset();
    }
}

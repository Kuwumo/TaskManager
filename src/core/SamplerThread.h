#pragma once

#include "core/ISystemMonitor.h"

#include <QThread>

#include <condition_variable>
#include <memory>
#include <mutex>

class SamplerThread : public QThread {
    Q_OBJECT

public:
    explicit SamplerThread(QObject* parent = nullptr);
    ~SamplerThread() override;

    void stop();
    void setPaused(bool paused);
    bool terminateProcess(quint32 pid, QString* error);

signals:
    void snapshotReady(const SystemSnapshot& snapshot);

protected:
    void run() override;

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    bool paused_ = false;
    std::shared_ptr<ISystemMonitor> monitor_;
};

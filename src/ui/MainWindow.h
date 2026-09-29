#pragma once

#include "core/Types.h"

#include <QMainWindow>

class PerformancePage;
class ProcessPage;
class SamplerThread;

// 主窗口：工具栏、进程页、性能页和状态栏。
// 采样线程是子对象，析构时先 stop()，再让 Qt 销毁子控件，
// 避免窗口已经拆掉还收到快照。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    // 把一拍快照分给两个页，并在状态栏附上进程数和总 CPU。
    void onSnapshot(const SystemSnapshot& snapshot);
    // 暂停时界面自己改状态栏。采样线程仍在采样，只是不再发信号。
    void onPauseToggled(bool paused);

    SamplerThread* sampler_ = nullptr;
    ProcessPage* processPage_ = nullptr;
    PerformancePage* performancePage_ = nullptr;
    bool paused_ = false;
};

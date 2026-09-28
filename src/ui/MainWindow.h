#pragma once

#include "core/Types.h"

#include <QMainWindow>

class PerformancePage;
class ProcessPage;
class SamplerThread;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void onSnapshot(const SystemSnapshot& snapshot);
    void onPauseToggled(bool paused);

    SamplerThread* sampler_ = nullptr;
    ProcessPage* processPage_ = nullptr;
    PerformancePage* performancePage_ = nullptr;
    bool paused_ = false;
};

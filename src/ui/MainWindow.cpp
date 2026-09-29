#include "ui/MainWindow.h"

#include "core/Format.h"
#include "core/SamplerThread.h"
#include "ui/PerformancePage.h"
#include "ui/ProcessPage.h"

#include <QAction>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , sampler_(new SamplerThread(this))
    , processPage_(new ProcessPage(this))
    , performancePage_(new PerformancePage(this))
{
    setWindowTitle(QStringLiteral("任务管理器"));
    setAccessibleName(QStringLiteral("任务管理器"));
    resize(1100, 720);

    auto* pause = new QAction(QStringLiteral("暂停刷新"), this);
    pause->setCheckable(true);
    auto* toolbar = addToolBar(QStringLiteral("主工具栏"));
    toolbar->setMovable(false);
    toolbar->addAction(pause);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(processPage_, QStringLiteral("进程"));
    tabs->addTab(performancePage_, QStringLiteral("性能"));
    setCentralWidget(tabs);
    statusBar()->showMessage(QStringLiteral("正在启动监控…"));

    connect(pause, &QAction::toggled, this, &MainWindow::onPauseToggled);
    connect(processPage_, &ProcessPage::terminateRequested, this, [this](quint32 pid) {
        QString error;
        if (!sampler_->terminateProcess(pid, &error)) {
            QMessageBox::warning(this,
                                 QStringLiteral("结束任务"),
                                 error.isEmpty() ? QStringLiteral("无法结束该进程。") : error);
        }
    });
    // 明确排队到界面线程。采样在 QThread::run 里 emit，不能在那里直接改控件。
    connect(sampler_, &SamplerThread::snapshotReady, this, &MainWindow::onSnapshot, Qt::QueuedConnection);
    sampler_->start();
}

MainWindow::~MainWindow()
{
    sampler_->stop();
}

void MainWindow::onSnapshot(const SystemSnapshot& snapshot)
{
    if (paused_) {
        return;
    }
    processPage_->setProcesses(snapshot.processes);
    performancePage_->applySnapshot(snapshot);
    QString message = snapshot.statusMessage;
    if (!snapshot.processes.isEmpty()) {
        message += QStringLiteral(" · %1 个进程").arg(snapshot.processes.size());
    }
    if (snapshot.cpu.available) {
        message += QStringLiteral(" · CPU %1").arg(format::percent(snapshot.cpu.totalPercent));
    }
    statusBar()->showMessage(message);
}

void MainWindow::onPauseToggled(bool paused)
{
    paused_ = paused;
    sampler_->setPaused(paused);
    statusBar()->showMessage(paused ? QStringLiteral("已暂停") : QStringLiteral("正在监控"));
}

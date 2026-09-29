#pragma once

#include "core/Types.h"

#include <QWidget>

class HistoryChart;
class QLabel;
class QListWidget;

// 性能页。左侧四项，右侧是当前值和约 60 秒曲线。
// 历史保存在这一页，不从进程表累加。暂停时主窗口不再调用 applySnapshot，曲线就停住。
class PerformancePage : public QWidget {
    Q_OBJECT

public:
    explicit PerformancePage(QWidget* parent = nullptr);

    void applySnapshot(const SystemSnapshot& snapshot);

private:
    // 按左侧当前项，把 history_ 画成对应曲线。
    // CPU 和内存用 0–100%。磁盘和网络按近期最大值自动缩放。
    void rebuild();

    QListWidget* list_ = nullptr;
    QLabel* valueLabel_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    HistoryChart* chart_ = nullptr;
    // 最近约 60 次采样，一次一秒。
    QVector<SystemSnapshot> history_;
};

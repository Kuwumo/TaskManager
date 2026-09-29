#pragma once

#include "core/Types.h"

#include <QWidget>

#include <optional>

class ProcessTableModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;

// 进程页：名称筛选、可排序表格、结束任务。
// 筛选和排序都做在代理模型上，源模型只负责换数据。
// 第一次拿到非空列表时按 CPU 降序排一次，之后保留用户点过的列。
class ProcessPage : public QWidget {
    Q_OBJECT

public:
    explicit ProcessPage(QWidget* parent = nullptr);

    void setProcesses(const QVector<ProcessSnapshot>& processes);

signals:
    // 用户已在确认框里选择「是」。真正结束进程由主窗口调用采样线程。
    void terminateRequested(quint32 pid);

private:
    void endSelectedProcess();
    // 没有当前行时返回空。PID 0 是空闲进程，也是合法选择，所以不能用 0 表示「没选中」。
    std::optional<quint32> selectedPid() const;
    // 刷新后按 PID 找回原来的行。筛选把该行藏起来时就不再选中。
    void selectPid(quint32 pid);

    ProcessTableModel* model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;
    QTableView* view_ = nullptr;
    QLineEdit* filter_ = nullptr;
    bool sortedOnce_ = false;
};

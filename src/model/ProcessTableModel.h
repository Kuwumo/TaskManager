#pragma once

#include "core/Types.h"

#include <QAbstractTableModel>

// 进程表的数据模型。视图通过代理模型排序和筛选，这里只保存原始快照。
//
// DisplayRole 给用户看格式化文本（「12.5%」「83.7 MB」）。
// UserRole 给排序用原始数值。不可用的指标用 -1，降序时会排到后面。
// 名称列两种角色都是字符串。
class ProcessTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        Name = 0,
        Pid,
        Cpu,
        Memory,
        Disk,
        Net,
        ColumnCount
    };

    explicit ProcessTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    // 整表替换。调用方要先记住选中的 PID，替换后再选回去，否则刷新会丢掉选择。
    void setProcesses(const QVector<ProcessSnapshot>& processes);

private:
    QVector<ProcessSnapshot> processes_;
};

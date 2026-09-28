#pragma once

#include "core/Types.h"

#include <QAbstractTableModel>

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
    void setProcesses(const QVector<ProcessSnapshot>& processes);

private:
    QVector<ProcessSnapshot> processes_;
};

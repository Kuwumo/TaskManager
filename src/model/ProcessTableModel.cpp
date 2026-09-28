#include "model/ProcessTableModel.h"

#include "core/Format.h"

ProcessTableModel::ProcessTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int ProcessTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : processes_.size();
}

int ProcessTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ProcessTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= processes_.size()) {
        return {};
    }
    const ProcessSnapshot& process = processes_.at(index.row());

    if (role == Qt::TextAlignmentRole && index.column() != Name) {
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    if (role != Qt::DisplayRole && role != Qt::UserRole) {
        return {};
    }

    const bool display = role == Qt::DisplayRole;
    switch (index.column()) {
    case Name:
        return process.name;
    case Pid:
        return display ? QVariant(process.pid) : QVariant(process.pid);
    case Cpu:
        if (!process.cpuAvailable) {
            return display ? QVariant(QStringLiteral("—")) : QVariant(-1.0);
        }
        return display ? QVariant(format::percent(process.cpuPercent)) : QVariant(process.cpuPercent);
    case Memory:
        if (!process.memoryAvailable) {
            return display ? QVariant(QStringLiteral("—")) : QVariant(-1.0);
        }
        return display ? QVariant(format::bytes(process.workingSetBytes))
                       : QVariant(static_cast<qulonglong>(process.workingSetBytes));
    case Disk:
        if (!process.diskAvailable) {
            return display ? QVariant(QStringLiteral("—")) : QVariant(-1.0);
        }
        return display ? QVariant(format::rate(process.diskBytesPerSec))
                       : QVariant(static_cast<qulonglong>(process.diskBytesPerSec));
    case Net:
        if (!process.netAvailable) {
            return display ? QVariant(QStringLiteral("—")) : QVariant(-1.0);
        }
        return display ? QVariant(format::rate(process.netBytesPerSec))
                       : QVariant(static_cast<qulonglong>(process.netBytesPerSec));
    default:
        return {};
    }
}

QVariant ProcessTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) {
        return {};
    }
    if (role == Qt::ToolTipRole) {
        if (section == Disk) {
            return QStringLiteral("进程读写字节速率，包含文件 I/O，不是任务管理器的磁盘活动时间。");
        }
        if (section == Net) {
            return QStringLiteral("进程网络收发速率。未以管理员身份运行时显示为不可用。");
        }
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case Name:
        return QStringLiteral("名称");
    case Pid:
        return QStringLiteral("PID");
    case Cpu:
        return QStringLiteral("CPU");
    case Memory:
        return QStringLiteral("内存");
    case Disk:
        return QStringLiteral("磁盘");
    case Net:
        return QStringLiteral("网络");
    default:
        return {};
    }
}

void ProcessTableModel::setProcesses(const QVector<ProcessSnapshot>& processes)
{
    beginResetModel();
    processes_ = processes;
    endResetModel();
}

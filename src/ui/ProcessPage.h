#pragma once

#include "core/Types.h"

#include <QWidget>

#include <optional>

class ProcessTableModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;

class ProcessPage : public QWidget {
    Q_OBJECT

public:
    explicit ProcessPage(QWidget* parent = nullptr);

    void setProcesses(const QVector<ProcessSnapshot>& processes);

signals:
    void terminateRequested(quint32 pid);

private:
    void endSelectedProcess();
    std::optional<quint32> selectedPid() const;
    void selectPid(quint32 pid);

    ProcessTableModel* model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;
    QTableView* view_ = nullptr;
    QLineEdit* filter_ = nullptr;
    bool sortedOnce_ = false;
};

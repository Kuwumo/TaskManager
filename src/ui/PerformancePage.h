#pragma once

#include "core/Types.h"

#include <QWidget>

class HistoryChart;
class QLabel;
class QListWidget;

class PerformancePage : public QWidget {
    Q_OBJECT

public:
    explicit PerformancePage(QWidget* parent = nullptr);

    void applySnapshot(const SystemSnapshot& snapshot);

private:
    void rebuild();

    QListWidget* list_ = nullptr;
    QLabel* valueLabel_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    HistoryChart* chart_ = nullptr;
    QVector<SystemSnapshot> history_;
};

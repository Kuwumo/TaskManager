#pragma once

#include "core/Types.h"

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

enum class AxisFormat {
    Number,
    Percent,
    Rate
};

struct ChartSeries {
    QString name;
    QColor color;
    QVector<double> values;
    qreal width = 1.5;
};

class HistoryChart : public QWidget {
public:
    explicit HistoryChart(QWidget* parent = nullptr);

    void setYRange(double minimum, double maximum, AxisFormat format);
    void setSeries(const QVector<ChartSeries>& series);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString formatTick(double value) const;

    QVector<ChartSeries> series_;
    double yMin_ = 0;
    double yMax_ = 100;
    bool autoScale_ = false;
    AxisFormat format_ = AxisFormat::Number;
};

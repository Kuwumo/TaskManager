#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

// 纵轴刻度怎么写。百分比和速率交给 format 命名空间，避免图表自己再算一遍单位。
enum class AxisFormat {
    Number,
    Percent,
    Rate
};

// 一条折线。values 与其他序列等长，下标 0 是最旧的点。
struct ChartSeries {
    QString name;
    QColor color;
    QVector<double> values;
    qreal width = 1.5;
};

// 自绘的 60 秒历史曲线，不依赖 Qt Charts。
// setYRange 的 maximum <= minimum 时改为自动缩放，从 0 到数据最大值的 1.1 倍。
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

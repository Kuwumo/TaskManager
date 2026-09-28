#include "ui/HistoryChart.h"

#include "core/Format.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

HistoryChart::HistoryChart(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setAutoFillBackground(false);
}

void HistoryChart::setYRange(double minimum, double maximum, AxisFormat format)
{
    format_ = format;
    if (maximum <= minimum) {
        autoScale_ = true;
        yMin_ = 0;
        yMax_ = 1;
    } else {
        autoScale_ = false;
        yMin_ = minimum;
        yMax_ = maximum;
    }
    update();
}

void HistoryChart::setSeries(const QVector<ChartSeries>& series)
{
    series_ = series;
    update();
}

QSize HistoryChart::sizeHint() const
{
    return {640, 280};
}

QString HistoryChart::formatTick(double value) const
{
    if (format_ == AxisFormat::Percent) {
        return format::percent(value);
    }
    if (format_ == AxisFormat::Rate) {
        return format::rate(value < 0 ? 0 : static_cast<quint64>(value));
    }
    return QString::number(value, 'f', value >= 100 ? 0 : 1);
}

void HistoryChart::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().base());

    const QRectF plot = QRectF(rect()).adjusted(64, 16, -16, -28);
    double yMin = yMin_;
    double yMax = yMax_;
    if (autoScale_) {
        yMin = 0;
        yMax = 0;
        for (const ChartSeries& series : series_) {
            for (double value : series.values) {
                yMax = std::max(yMax, value);
            }
        }
        yMax = yMax <= 0 ? 1 : yMax * 1.1;
    }
    if (yMax <= yMin) {
        yMax = yMin + 1;
    }

    painter.setPen(palette().mid().color());
    painter.drawRect(plot);
    painter.setPen(palette().midlight().color());
    for (int step = 1; step <= 3; ++step) {
        const qreal y = plot.top() + plot.height() * step / 4.0;
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    painter.setPen(palette().windowText().color());
    const QFontMetrics metrics(painter.font());
    const double ticks[] = {yMax, (yMax + yMin) / 2.0, yMin};
    const qreal tickY[] = {plot.top(), plot.center().y(), plot.bottom()};
    for (int index = 0; index < 3; ++index) {
        const QString label = formatTick(ticks[index]);
        const QRectF labelRect(4, tickY[index] - metrics.height() / 2.0, plot.left() - 8, metrics.height());
        painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, label);
    }
    painter.drawText(QRectF(plot.left(), plot.bottom() + 4, plot.width(), 20),
                     Qt::AlignRight | Qt::AlignTop,
                     QStringLiteral("60 秒"));

    const auto yFor = [&](double value) {
        const double ratio = (value - yMin) / (yMax - yMin);
        const double clamped = std::clamp(ratio, 0.0, 1.0);
        return plot.bottom() - plot.height() * clamped;
    };

    int legendX = static_cast<int>(plot.right()) - 8;
    const bool showLegend = series_.size() > 1 && series_.size() <= 4;
    if (showLegend) {
        for (int index = series_.size() - 1; index >= 0; --index) {
            const QString name = series_.at(index).name;
            const int textWidth = metrics.horizontalAdvance(name);
            legendX -= textWidth + 18;
        }
    }
    int legendCursor = legendX;

    for (const ChartSeries& series : series_) {
        if (series.values.isEmpty()) {
            continue;
        }
        QPainterPath path;
        const int count = series.values.size();
        for (int index = 0; index < count; ++index) {
            const qreal x = count == 1 ? plot.left()
                                       : plot.left() + plot.width() * index / static_cast<qreal>(count - 1);
            const QPointF point(x, yFor(series.values.at(index)));
            if (index == 0) {
                path.moveTo(point);
            } else {
                path.lineTo(point);
            }
        }
        QPen pen(series.color, series.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawPath(path);

        if (showLegend) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(series.color);
            painter.drawRoundedRect(QRectF(legendCursor, plot.top() + 6, 10, 10), 2, 2);
            painter.setPen(palette().windowText().color());
            painter.drawText(QRectF(legendCursor + 14, plot.top() + 2, metrics.horizontalAdvance(series.name) + 8, 18),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             series.name);
            legendCursor += metrics.horizontalAdvance(series.name) + 18;
        }
    }
}

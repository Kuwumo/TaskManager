#include "ui/PerformancePage.h"

#include "core/Format.h"
#include "ui/HistoryChart.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace {

QColor coreColor(int index)
{
    return QColor::fromHsv((index * 37) % 360, 140, 210);
}

} // namespace

PerformancePage::PerformancePage(QWidget* parent)
    : QWidget(parent)
    , list_(new QListWidget(this))
    , valueLabel_(new QLabel(this))
    , detailLabel_(new QLabel(this))
    , chart_(new HistoryChart(this))
{
    list_->addItems({QStringLiteral("CPU"), QStringLiteral("内存"), QStringLiteral("磁盘"), QStringLiteral("网络")});
    list_->setFixedWidth(140);
    list_->setCurrentRow(0);

    QFont valueFont = valueLabel_->font();
    valueFont.setPointSize(28);
    valueFont.setBold(true);
    valueLabel_->setFont(valueFont);
    valueLabel_->setText(QStringLiteral("—"));
    valueLabel_->setAccessibleName(QStringLiteral("性能数值"));
    detailLabel_->setWordWrap(true);

    auto* textLayout = new QVBoxLayout;
    textLayout->addWidget(valueLabel_);
    textLayout->addWidget(detailLabel_);
    textLayout->addWidget(chart_, 1);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(list_);
    layout->addLayout(textLayout, 1);

    connect(list_, &QListWidget::currentRowChanged, this, [this](int) { rebuild(); });
}

void PerformancePage::applySnapshot(const SystemSnapshot& snapshot)
{
    // 保留约 60 秒。更早的点从队头丢掉，曲线满宽后会向左滚动。
    history_.push_back(snapshot);
    while (history_.size() > 60) {
        history_.removeFirst();
    }
    rebuild();
}

void PerformancePage::rebuild()
{
    const int metric = list_->currentRow();
    if (history_.isEmpty()) {
        valueLabel_->setText(QStringLiteral("—"));
        detailLabel_->clear();
        chart_->setSeries({});
        return;
    }

    const SystemSnapshot& latest = history_.constLast();
    QVector<ChartSeries> series;

    if (metric == 0) {
        valueLabel_->setText(latest.cpu.available ? format::percent(latest.cpu.totalPercent) : QStringLiteral("—"));
        detailLabel_->setText(QStringLiteral("%1 个逻辑处理器").arg(latest.cpu.logicalProcessors));
        ChartSeries total;
        total.name = QStringLiteral("总计");
        total.color = QColor(0, 120, 212);
        total.width = 2.2;
        for (int index = 0; index < latest.cpu.perCore.size(); ++index) {
            ChartSeries core;
            core.name = QStringLiteral("CPU %1").arg(index);
            core.color = coreColor(index);
            core.width = 1.1;
            series.push_back(core);
        }
        series.push_back(total);
        for (const SystemSnapshot& sample : history_) {
            for (int index = 0; index < series.size() - 1; ++index) {
                const double value = index < sample.cpu.perCore.size() ? sample.cpu.perCore.at(index) : 0;
                series[index].values.push_back(value);
            }
            series.last().values.push_back(sample.cpu.available ? sample.cpu.totalPercent : 0);
        }
        chart_->setYRange(0, 100, AxisFormat::Percent);
    } else if (metric == 1) {
        if (latest.memory.available && latest.memory.totalBytes > 0) {
            const double percent = static_cast<double>(latest.memory.usedBytes) * 100.0
                                   / static_cast<double>(latest.memory.totalBytes);
            valueLabel_->setText(format::percent(percent));
            detailLabel_->setText(QStringLiteral("%1 / %2")
                                      .arg(format::bytes(latest.memory.usedBytes),
                                           format::bytes(latest.memory.totalBytes)));
        } else {
            valueLabel_->setText(QStringLiteral("—"));
            detailLabel_->clear();
        }
        ChartSeries used;
        used.name = QStringLiteral("已用");
        used.color = QColor(16, 124, 16);
        used.width = 2.0;
        for (const SystemSnapshot& sample : history_) {
            double percent = 0;
            if (sample.memory.available && sample.memory.totalBytes > 0) {
                percent = static_cast<double>(sample.memory.usedBytes) * 100.0
                          / static_cast<double>(sample.memory.totalBytes);
            }
            used.values.push_back(percent);
        }
        series.push_back(used);
        chart_->setYRange(0, 100, AxisFormat::Percent);
    } else if (metric == 2) {
        valueLabel_->setText(latest.disk.available ? format::rate(latest.disk.bytesPerSec) : QStringLiteral("—"));
        detailLabel_->setText(latest.disk.available
                                  ? QStringLiteral("活动时间 %1").arg(format::percent(latest.disk.activePercent))
                                  : QString());
        ChartSeries throughput;
        throughput.name = QStringLiteral("吞吐量");
        throughput.color = QColor(135, 100, 184);
        throughput.width = 2.0;
        for (const SystemSnapshot& sample : history_) {
            throughput.values.push_back(sample.disk.available ? static_cast<double>(sample.disk.bytesPerSec) : 0);
        }
        series.push_back(throughput);
        chart_->setYRange(0, 0, AxisFormat::Rate);
    } else {
        if (latest.net.available) {
            valueLabel_->setText(QStringLiteral("接收 %1").arg(format::rate(latest.net.recvBytesPerSec)));
            detailLabel_->setText(QStringLiteral("发送 %1    %2")
                                      .arg(format::rate(latest.net.sendBytesPerSec),
                                           latest.net.adapterSummary));
        } else {
            valueLabel_->setText(QStringLiteral("—"));
            detailLabel_->clear();
        }
        ChartSeries recv;
        recv.name = QStringLiteral("接收");
        recv.color = QColor(0, 120, 212);
        recv.width = 2.0;
        ChartSeries send;
        send.name = QStringLiteral("发送");
        send.color = QColor(232, 17, 35);
        send.width = 2.0;
        for (const SystemSnapshot& sample : history_) {
            recv.values.push_back(sample.net.available ? static_cast<double>(sample.net.recvBytesPerSec) : 0);
            send.values.push_back(sample.net.available ? static_cast<double>(sample.net.sendBytesPerSec) : 0);
        }
        series.push_back(recv);
        series.push_back(send);
        chart_->setYRange(0, 0, AxisFormat::Rate);
    }

    chart_->setSeries(series);
}

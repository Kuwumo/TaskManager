#include "ui/ProcessPage.h"

#include "model/ProcessTableModel.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

ProcessPage::ProcessPage(QWidget* parent)
    : QWidget(parent)
    , model_(new ProcessTableModel(this))
    , proxy_(new QSortFilterProxyModel(this))
    , view_(new QTableView(this))
    , filter_(new QLineEdit(this))
{
    proxy_->setSourceModel(model_);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy_->setFilterKeyColumn(ProcessTableModel::Name);
    proxy_->setSortRole(Qt::UserRole);
    proxy_->setDynamicSortFilter(true);

    filter_->setPlaceholderText(QStringLiteral("筛选进程名称"));
    filter_->setClearButtonEnabled(true);

    auto* endButton = new QPushButton(QStringLiteral("结束任务"), this);
    view_->setModel(proxy_);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setAlternatingRowColors(true);
    view_->setSortingEnabled(true);
    view_->verticalHeader()->setVisible(false);
    view_->setShowGrid(false);
    view_->setWordWrap(false);
    view_->setAccessibleName(QStringLiteral("进程表"));

    auto* header = view_->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(ProcessTableModel::Name, QHeaderView::Stretch);
    for (int column = ProcessTableModel::Pid; column < ProcessTableModel::ColumnCount; ++column) {
        header->setSectionResizeMode(column, QHeaderView::Interactive);
    }
    view_->setColumnWidth(ProcessTableModel::Pid, 80);
    view_->setColumnWidth(ProcessTableModel::Cpu, 90);
    view_->setColumnWidth(ProcessTableModel::Memory, 110);
    view_->setColumnWidth(ProcessTableModel::Disk, 130);
    view_->setColumnWidth(ProcessTableModel::Net, 130);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(filter_, 1);
    toolbar->addWidget(endButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(view_, 1);

    connect(filter_, &QLineEdit::textChanged, proxy_, &QSortFilterProxyModel::setFilterFixedString);
    connect(endButton, &QPushButton::clicked, this, &ProcessPage::endSelectedProcess);
    connect(view_, &QTableView::doubleClicked, this, [this](const QModelIndex&) { endSelectedProcess(); });
}

void ProcessPage::setProcesses(const QVector<ProcessSnapshot>& processes)
{
    const auto selected = selectedPid();
    model_->setProcesses(processes);
    if (!sortedOnce_ && !processes.isEmpty()) {
        view_->sortByColumn(ProcessTableModel::Cpu, Qt::DescendingOrder);
        sortedOnce_ = true;
    }
    if (selected.has_value()) {
        selectPid(*selected);
    }
}

void ProcessPage::endSelectedProcess()
{
    const auto pid = selectedPid();
    if (!pid.has_value()) {
        QMessageBox::information(this, QStringLiteral("结束任务"), QStringLiteral("请先选择一个进程。"));
        return;
    }

    const QModelIndex current = view_->currentIndex();
    const QString name = current.siblingAtColumn(ProcessTableModel::Name).data(Qt::DisplayRole).toString();
    const auto answer = QMessageBox::warning(
        this,
        QStringLiteral("结束任务"),
        QStringLiteral("确定要结束「%1」(PID %2) 吗？未保存的数据会丢失。").arg(name).arg(*pid),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    emit terminateRequested(*pid);
}

std::optional<quint32> ProcessPage::selectedPid() const
{
    const QModelIndex current = view_->currentIndex();
    if (!current.isValid()) {
        return std::nullopt;
    }
    return current.siblingAtColumn(ProcessTableModel::Pid).data(Qt::UserRole).toUInt();
}

void ProcessPage::selectPid(quint32 pid)
{
    for (int row = 0; row < proxy_->rowCount(); ++row) {
        const QModelIndex index = proxy_->index(row, ProcessTableModel::Pid);
        if (index.data(Qt::UserRole).toUInt() == pid) {
            view_->selectRow(row);
            view_->scrollTo(index);
            return;
        }
    }
}

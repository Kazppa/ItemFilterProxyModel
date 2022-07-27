#include "ExampleWidget.h"

#include <QAbstractItemModelTester>
#include <QBoxLayout>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLoggingCategory>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>

#include "ExampleItemModel.h"
#include "ExampleItemFilterProxyModel.h"
#include "ExampleTreeView.h"

ExampleWidget::ExampleWidget(QWidget *parent) : QWidget(parent)
{
    _sourceModel = new ExampleItemModel(this);
    _proxyModel = new ExampleItemFilterProxyModel(this);
    _proxyModel->setSourceModel(_sourceModel);
#ifdef QT_DEBUG
    QLoggingCategory cat("qt.modeltest");
    cat.setEnabled(QtMsgType::QtDebugMsg, true);
    cat.setEnabled(QtMsgType::QtWarningMsg, true);
    cat.setEnabled(QtMsgType::QtCriticalMsg, true);
    cat.setEnabled(QtMsgType::QtFatalMsg, true);
    cat.setEnabled(QtMsgType::QtInfoMsg, true);
    auto sourceTester = new QAbstractItemModelTester(_sourceModel, QAbstractItemModelTester::FailureReportingMode::Fatal, this);
    auto proxyTester = new QAbstractItemModelTester(_proxyModel, QAbstractItemModelTester::FailureReportingMode::Fatal, this);
#endif

    _sourceTreeView = new ExampleTreeView();
    _sourceTreeView->setModel(_sourceModel);
    _sourceTreeView->expandAll();

    _proxyTreeView = new ExampleTreeView();
    _proxyTreeView->setModel(_proxyModel);
    _proxyTreeView->expandAll();

    _syncViewsCheckBox = new QCheckBox(tr("Activate QTreeViews synchronization"));

    auto titleLayout = new QHBoxLayout;
    titleLayout->addWidget(new QLabel(QStringLiteral("Source model")), 0, Qt::AlignHCenter);
    titleLayout->addWidget(new QLabel(QStringLiteral("Filtered model")), 0, Qt::AlignHCenter);

    auto splitter = new QSplitter;
    splitter->addWidget(_sourceTreeView);
    splitter->addWidget(_proxyTreeView);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(splitter, 100);
    mainLayout->addWidget(_syncViewsCheckBox);

    setMinimumSize(800, 600);

    connect(_syncViewsCheckBox, &QCheckBox::toggled, this, &ExampleWidget::onSyncViewsCheckBox);
    _syncViewsCheckBox->setChecked(true);

    QTimer::singleShot(1, [this] {
        _proxyTreeView->resizeColumnsToContents();
        _sourceTreeView->resizeColumnsToContents();
    });
}

void ExampleWidget::onSyncViewsCheckBox(bool isChecked)
{
    if (isChecked)
    {
        connect(_sourceTreeView, &ExampleTreeView::currentIndexChanged, this, &ExampleWidget::onViewIndexChanged);
        connect(_proxyTreeView, &ExampleTreeView::currentIndexChanged, this, &ExampleWidget::onViewIndexChanged);

        connect(_sourceTreeView->verticalScrollBar(), &QScrollBar::sliderPressed, this, &ExampleWidget::onSliderPressed);
        connect(_sourceTreeView->verticalScrollBar(), &QScrollBar::sliderReleased, this, &ExampleWidget::onSliderReleased);
        connect(_proxyTreeView->verticalScrollBar(), &QScrollBar::sliderPressed, this, &ExampleWidget::onSliderPressed);
        connect(_proxyTreeView->verticalScrollBar(), &QScrollBar::sliderReleased, this, &ExampleWidget::onSliderReleased);
    }
    else
    {
        disconnect(_sourceTreeView->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(_proxyTreeView->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(_sourceTreeView, nullptr, this, nullptr);
        disconnect(_proxyTreeView, nullptr, this, nullptr);
    }
}

void ExampleWidget::onSliderPressed()
{
    auto scrollBar = static_cast<QScrollBar *>(sender());
    connect(scrollBar, &QScrollBar::valueChanged, this, &ExampleWidget::onViewScrolled);
    
}

void ExampleWidget::onSliderReleased()
{
    auto scrollBar = static_cast<QScrollBar *>(sender());
    disconnect(scrollBar, &QScrollBar::valueChanged, this, &ExampleWidget::onViewScrolled);
}

void ExampleWidget::onViewScrolled(int newValue)
{
    const auto basicViewScrollBar = _sourceTreeView->verticalScrollBar();
    const auto restructuredViewScrollBar = _proxyTreeView->verticalScrollBar();
    const auto fromScrollBar = sender() == basicViewScrollBar ? basicViewScrollBar : restructuredViewScrollBar;
    const auto toScrollBar = fromScrollBar == basicViewScrollBar ? restructuredViewScrollBar : basicViewScrollBar;

    // Calculate the relative value to get a percent of the range
    const auto fromMin = fromScrollBar->minimum();
    const auto fromMax = fromScrollBar->maximum();
    const auto relative = ((newValue - fromMin) * 100.0) / (fromMax - fromMin);

    const auto toMin = toScrollBar->minimum();
    const auto toMax = toScrollBar->maximum();
    const auto toValue = ((relative * (toMax - toMin)) / 100) + toMin;
    toScrollBar->setValue((int) std::round(toValue));
}

void ExampleWidget::onViewIndexChanged(const QModelIndex &newIndex)
{
    const auto fromView = sender() == _sourceTreeView ? _sourceTreeView : _proxyTreeView;
    const auto toView = fromView == _sourceTreeView ? _proxyTreeView : _sourceTreeView;
    if (!fromView->model() || !toView->model()) {
        return;
    }

    QModelIndex toViewIndex;
    if (_proxyTreeView->model() == _proxyModel) {
        if (toView == _proxyTreeView) {
            toViewIndex = _proxyModel->mapFromSource(newIndex);
        }
        else {
            toViewIndex = _proxyModel->mapToSource(newIndex);
        }
    }
    else {
        toViewIndex = newIndex;
    }

     QTimer::singleShot(1, [=]() {
        toView->setCurrentIndex(toViewIndex);
    });
}

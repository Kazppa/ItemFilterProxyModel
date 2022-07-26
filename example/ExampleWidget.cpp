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
    // auto tester = new QAbstractItemModelTester(_proxyModel, QAbstractItemModelTester::FailureReportingMode::Fatal, this);
#endif

    _basicTreeView = new ExampleTreeView();
    _basicTreeView->setModel(_sourceModel);
    _basicTreeView->expandAll();

    _restructuredTreeView = new ExampleTreeView();
    _restructuredTreeView->setModel(_proxyModel);
    _restructuredTreeView->expandAll();

    _syncViewsCheckBox = new QCheckBox(tr("Activate QTreeViews synchronization"));

    auto titleLayout = new QHBoxLayout;
    titleLayout->addWidget(new QLabel(QStringLiteral("Source model")), 0, Qt::AlignHCenter);
    titleLayout->addWidget(new QLabel(QStringLiteral("Filtered model")), 0, Qt::AlignHCenter);

    auto splitter = new QSplitter;
    splitter->addWidget(_basicTreeView);
    splitter->addWidget(_restructuredTreeView);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(splitter, 100);
    mainLayout->addWidget(_syncViewsCheckBox);

    setMinimumSize(800, 600);

    connect(_syncViewsCheckBox, &QCheckBox::toggled, this, &ExampleWidget::onSyncViewsCheckBox);
    _syncViewsCheckBox->setChecked(true);
    _basicTreeView->resizeColumnsToContents();
    _restructuredTreeView->resizeColumnsToContents();
}

void ExampleWidget::onSyncViewsCheckBox(bool isChecked)
{
    if (isChecked)
    {
        connect(_basicTreeView, &ExampleTreeView::currentIndexChanged, this, &ExampleWidget::onViewIndexChanged);
        connect(_restructuredTreeView, &ExampleTreeView::currentIndexChanged, this, &ExampleWidget::onViewIndexChanged);

        connect(_basicTreeView->verticalScrollBar(), &QScrollBar::sliderPressed, this, &ExampleWidget::onSliderPressed);
        connect(_basicTreeView->verticalScrollBar(), &QScrollBar::sliderReleased, this, &ExampleWidget::onSliderReleased);
        connect(_restructuredTreeView->verticalScrollBar(), &QScrollBar::sliderPressed, this, &ExampleWidget::onSliderPressed);
        connect(_restructuredTreeView->verticalScrollBar(), &QScrollBar::sliderReleased, this, &ExampleWidget::onSliderReleased);
    }
    else
    {
        disconnect(_basicTreeView->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(_restructuredTreeView->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(_basicTreeView, nullptr, this, nullptr);
        disconnect(_restructuredTreeView, nullptr, this, nullptr);
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
    const auto basicViewScrollBar = _basicTreeView->verticalScrollBar();
    const auto restructuredViewScrollBar = _restructuredTreeView->verticalScrollBar();
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
    const auto fromView = sender() == _basicTreeView ? _basicTreeView : _restructuredTreeView;
    const auto toView = fromView == _basicTreeView ? _restructuredTreeView : _basicTreeView;

    QModelIndex toViewIndex;
    if (_restructuredTreeView->model() == _proxyModel) {
        if (toView == _restructuredTreeView) {
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


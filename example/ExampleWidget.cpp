#include "ExampleWidget.h"

#include <stack>

#include <QBoxLayout>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTimer>

#include "structure_proxy_model/StructureProxyModel.h"

void ExampleTreeView::currentChanged(const QModelIndex &newIdx, const QModelIndex& prevIdx) 
{
    QTreeView::currentChanged(newIdx, prevIdx);
    Q_EMIT currentIndexChanged(newIdx);
}

////////////////////////

ExampleWidget::ExampleWidget(QWidget* parent) :
    QWidget(parent)
{
    _sourceModel = new QStandardItemModel(this);
    fillSourceModel();

    _basicTreeView = createView();
    _restructuredTreeView = createView();

    _syncViewsheckBox = new QCheckBox(tr("Activate QTreeView's synchronization"));

    auto titleLayout = new QHBoxLayout;
    titleLayout->addWidget(new QLabel(QStringLiteral("Source model")), 0, Qt::AlignHCenter);
    titleLayout->addWidget(new QLabel(QStringLiteral("Restructured model")), 0, Qt::AlignHCenter);

    auto viewLayout = new QHBoxLayout;
    viewLayout->addWidget(_basicTreeView);
    viewLayout->addWidget(_restructuredTreeView);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(titleLayout);
    mainLayout->addLayout(viewLayout);
    mainLayout->addWidget(_syncViewsheckBox);

    setMinimumSize(600, 500);

    connect(_syncViewsheckBox, &QCheckBox::toggled, this, &ExampleWidget::onSyncViewsCheckBox);
    _syncViewsheckBox->setChecked(true);
}

ExampleTreeView* ExampleWidget::createView()
{
    auto view = new ExampleTreeView;
    view->setHeaderHidden(true);
    view->verticalScrollBar()->setTracking(true);
    view->setModel(_sourceModel);
    view->expandAll();
    return view;
}

void ExampleWidget::fillSourceModel()
{
    QRandomGenerator rand;
    rand.seed(415);
    auto nameGenerator = [r = 1]() mutable -> QString {
        return QString::number(r++);
    };

    const auto rootNodeCount = rand.bounded(4, 10);
    std::stack<std::pair<QStandardItem *, int>> stack;
    for (int i = 0; i < rootNodeCount; ++i) {
        auto rootItem = new QStandardItem;
        rootItem->setText(nameGenerator());
        _sourceModel->appendRow(rootItem);

        const auto subLevelCount = rand.bounded(0, 6);
        if (subLevelCount != 0) {
            stack.push(std::make_pair(rootItem, subLevelCount));
        }
    }

    while (!stack.empty()) {
        auto [parentItem, subLevelCount] = stack.top();
        stack.pop();

        const auto childCount = rand.bounded(1, 4);
        for (int i = 0; i < childCount; ++i) {
            auto childItem = new QStandardItem;
            childItem->setText(nameGenerator());
            parentItem->appendRow(childItem);
            if (--subLevelCount > 0) {
                stack.push(std::make_pair(childItem, subLevelCount));
            }
        }
    }
}

void ExampleWidget::onSyncViewsCheckBox(bool isChecked)
{
    if (isChecked) {
        connect(_basicTreeView->verticalScrollBar(), &QScrollBar::valueChanged, this, &ExampleWidget::onViewScrolled);
        connect(_restructuredTreeView->verticalScrollBar(), &QScrollBar::valueChanged, this, &ExampleWidget::onViewScrolled);
        connect(_basicTreeView, &ExampleTreeView::currentIndexChanged, this, &ExampleWidget::onViewIndexChanged);
        connect(_restructuredTreeView, &ExampleTreeView::currentIndexChanged, this, &ExampleWidget::onViewIndexChanged);
    }
    else {
        disconnect(_basicTreeView->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(_restructuredTreeView->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(_basicTreeView, nullptr, this, nullptr);
        disconnect(_restructuredTreeView, nullptr, this, nullptr);
    }
}

void ExampleWidget::onViewScrolled(int newValue)
{
    const auto basicViewScrollBar = _basicTreeView->verticalScrollBar();
    const auto restructuredViewScrollBar = _restructuredTreeView->verticalScrollBar();
    const auto fromScrollBar = sender() == basicViewScrollBar ? basicViewScrollBar : restructuredViewScrollBar;
    const auto toScrollBar = fromScrollBar == basicViewScrollBar ? restructuredViewScrollBar : basicViewScrollBar;

    toScrollBar->setValue(newValue);
}

void ExampleWidget::onViewIndexChanged(const QModelIndex &newIndex)
{
    const auto fromView = sender() == _basicTreeView ? _basicTreeView : _restructuredTreeView;
    const auto toView = fromView == _basicTreeView ? _restructuredTreeView : _basicTreeView;

    QTimer::singleShot(1, [=]() {
        toView->setCurrentIndex(newIndex);
    });
}
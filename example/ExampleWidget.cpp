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

ExampleTreeView::ExampleTreeView(QWidget *parent)
{
    setHeaderHidden(true);
    verticalScrollBar()->setTracking(true);
}

void ExampleTreeView::currentChanged(const QModelIndex &newIdx, const QModelIndex &prevIdx)
{
    QTreeView::currentChanged(newIdx, prevIdx);
    Q_EMIT currentIndexChanged(newIdx);
}

////////////////////////

ExampleWidget::ExampleWidget(QWidget *parent) : QWidget(parent)
{
    _sourceModel = new QStandardItemModel(this);
    fillSourceModel();

    _basicTreeView = new ExampleTreeView();
    _basicTreeView->setModel(_sourceModel);
    _basicTreeView->expandAll();
    _restructuredTreeView = new ExampleTreeView();
    _restructuredTreeView->setModel(_sourceModel);
    _restructuredTreeView->expandAll();

    _syncViewsheckBox = new QCheckBox(tr("Activate QTreeViews synchronization"));

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

void ExampleWidget::fillSourceModel()
{
    QRandomGenerator rand;
    rand.seed(415);

    const auto rootNodeCount = rand.bounded(4, 10);
    std::stack<std::pair<QStandardItem *, int>> stack;
    QChar c = u'A';
    for (int i = 0; i < rootNodeCount; ++i)
    {
        auto rootItem = new QStandardItem;
        rootItem->setText(c);
        c.unicode() += 1;
        _sourceModel->appendRow(rootItem);

        const auto subLevelCount = rand.bounded(0, 6);
        if (subLevelCount != 0)
        {
            stack.push(std::make_pair(rootItem, subLevelCount));
        }
    }

    while (!stack.empty())
    {
        auto [parentItem, subLevelCount] = stack.top();
        stack.pop();
        const auto parentName = parentItem->text();
        const auto parentLetter = parentName[0];
        auto number = QStringView(parentName.begin() + 1, parentName.end()).toInt();

        const auto childCount = rand.bounded(1, 4);
        for (int i = 0; i < childCount; ++i)
        {
            const auto childName = parentLetter + QString::number(++number);
            auto childItem = new QStandardItem;
            childItem->setText(childName);
            parentItem->appendRow(childItem);
            if (--subLevelCount > 0)
            {
                stack.push(std::make_pair(childItem, subLevelCount));
            }
        }
    }
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

    const auto fromMin = fromScrollBar->minimum();
    const auto fromMax = fromScrollBar->maximum();
    const auto relative = ((newValue - fromMin) * 100.0) / (fromMax - fromMin);

    const auto toMin = toScrollBar->minimum();
    const auto toMax = toScrollBar->maximum();
    const auto toValue = ((relative * (toMax - toMin)) / 100) + toMin;

    toScrollBar->setValue((int)toValue);
}

void ExampleWidget::onViewIndexChanged(const QModelIndex &newIndex)
{
    const auto fromView = sender() == _basicTreeView ? _basicTreeView : _restructuredTreeView;
    const auto toView = fromView == _basicTreeView ? _restructuredTreeView : _basicTreeView;

    QTimer::singleShot(1, [=]() {
        toView->setCurrentIndex(newIndex);
    });
}

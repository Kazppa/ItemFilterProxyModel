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
#include <QLineEdit>

#include "ExampleItemModel.h"
#include "ExampleItemFilterProxyModel.h"
#include "ExampleTreeView.h"

ExampleWidget::ExampleWidget(QWidget *parent) : QWidget(parent)
{
    m_sourceModel = new ExampleItemModel(this);
    m_proxyModel = new ExampleItemFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_sourceModel);

    QLoggingCategory cat("qt.modeltest");
    for (const auto msgType : { QtDebugMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg, QtInfoMsg }) {
        cat.setEnabled(msgType, true);
    }
    auto proxyTester = new QAbstractItemModelTester(m_proxyModel, QAbstractItemModelTester::FailureReportingMode::Fatal, this);

    m_sourceTreeView = new ExampleTreeView();
    m_sourceTreeView->setModel(m_sourceModel);
    m_sourceTreeView->expandAll();

    m_proxyTreeView = new ExampleTreeView();
    m_proxyTreeView->setModel(m_proxyModel);
    m_proxyTreeView->expandAll();

    m_syncViewsCheckBox = new QCheckBox(tr("Activate QTreeViews synchronization"));
    m_filterLineEdit = new QLineEdit;
    m_filterLineEdit->setPlaceholderText(QStringLiteral("A, A1, F5"));

    auto titleLayout = new QHBoxLayout;
    titleLayout->addWidget(new QLabel(tr("Source model")), 0, Qt::AlignHCenter);
    titleLayout->addWidget(new QLabel(tr("Filtered model")), 0, Qt::AlignHCenter);

    auto splitter = new QSplitter;
    splitter->addWidget(m_sourceTreeView);
    splitter->addWidget(m_proxyTreeView);

    auto filterLayout = new QHBoxLayout;
    filterLayout->addWidget(m_syncViewsCheckBox, 50);
    filterLayout->addWidget(new QLabel(tr("Filters")), 0, Qt::AlignLeft);
    filterLayout->addWidget(m_filterLineEdit);
    m_filterLineEdit->setMinimumWidth(120);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(splitter, 100);
    mainLayout->addLayout(filterLayout, 100);

    setMinimumSize(800, 600);

    connect(m_filterLineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        static QRegularExpression splitRegExp(QStringLiteral(",\\s*"));
        const auto filters = text.split(splitRegExp, Qt::SkipEmptyParts);
        m_proxyModel->setFilteredNames(std::move(filters));
        m_proxyTreeView->expandAll();
    });
    m_filterLineEdit->setText(QStringLiteral("A14, A17, A10, D"));

    connect(m_syncViewsCheckBox, &QCheckBox::toggled, this, &ExampleWidget::onSyncViewsCheckBox);
    m_syncViewsCheckBox->setChecked(true);

    QTimer::singleShot(1, [this] {
        m_proxyTreeView->resizeColumnsToContents();
        m_sourceTreeView->resizeColumnsToContents();

        const auto idx = m_sourceTreeView->searchTextIndex(QStringLiteral("A17"));
        //m_sourceModel->setData(idx, QStringLiteral("A17_"), Qt::DisplayRole);
    });
}

void ExampleWidget::onSyncViewsCheckBox(bool isChecked)
{
    if (isChecked)
    {
        connect(m_sourceTreeView, &ExampleTreeView::currentIndexChanged, this, &ExampleWidget::onViewIndexChanged);
        connect(m_proxyTreeView, &ExampleTreeView::currentIndexChanged, this, &ExampleWidget::onViewIndexChanged);
        connect(m_sourceTreeView->verticalScrollBar(), &QScrollBar::actionTriggered, this, &ExampleWidget::onViewScrollActionTriggered);
        connect(m_proxyTreeView->verticalScrollBar(), &QScrollBar::actionTriggered, this, &ExampleWidget::onViewScrollActionTriggered);
    }
    else
    {
        disconnect(m_sourceTreeView->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(m_proxyTreeView->verticalScrollBar(), nullptr, this, nullptr);
        disconnect(m_sourceTreeView, nullptr, this, nullptr);
        disconnect(m_proxyTreeView, nullptr, this, nullptr);
    }
}

void ExampleWidget::onViewScrollActionTriggered(int action)
{
    if (action != QAbstractSlider::SliderMove) {
        return;
    }
    
    const auto basicViewScrollBar = m_sourceTreeView->verticalScrollBar();
    const auto restructuredViewScrollBar = m_proxyTreeView->verticalScrollBar();
    const auto fromScrollBar = sender() == basicViewScrollBar ? basicViewScrollBar : restructuredViewScrollBar;
    const auto toScrollBar = fromScrollBar == basicViewScrollBar ? restructuredViewScrollBar : basicViewScrollBar;
    const auto newValue = fromScrollBar->value();

    // Calculate the relative value to get a percentage of the range
    const auto fromMin = fromScrollBar->minimum();
    const auto fromMax = fromScrollBar->maximum();
    const auto relative = ((newValue - fromMin) * 100.0) / (fromMax - fromMin);

    const auto toMin = toScrollBar->minimum();
    const auto toMax = toScrollBar->maximum();
    const auto toValue = ((relative * (toMax - toMin)) / 100) + toMin;
    toScrollBar->setValue(static_cast<int>(std::round(toValue)));
}

void ExampleWidget::onViewIndexChanged(const QModelIndex &newIndex)
{
    const auto fromView = sender() == m_sourceTreeView ? m_sourceTreeView : m_proxyTreeView;
    const auto toView = fromView == m_sourceTreeView ? m_proxyTreeView : m_sourceTreeView;
    if (!fromView->model() || !toView->model()) {
        return;
    }

    QModelIndex toViewIndex;
    if (m_proxyTreeView->model() == m_proxyModel) {
        if (toView == m_proxyTreeView) {
            toViewIndex = m_proxyModel->mapFromSource(newIndex);
        }
        else {
            toViewIndex = m_proxyModel->mapToSource(newIndex);
        }
    }
    else {
        toViewIndex = newIndex;
    }

     QTimer::singleShot(1, [=]() {
        toView->setCurrentIndex(toViewIndex);
    });
}

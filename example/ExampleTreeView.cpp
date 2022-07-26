#include "ExampleTreeView.h"

#include <QMenu>
#include <QScrollBar>

ExampleTreeView::ExampleTreeView(QWidget *parent) :
        QTreeView(parent)
{
    setHeaderHidden(true);
    verticalScrollBar()->setTracking(true);

    setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    auto contextMenu = new QMenu(this);
    auto expandAction = contextMenu->addAction(tr("Expand"));
    auto collapseAction = contextMenu->addAction(tr("Collapse"));
    contextMenu->addSeparator();
    auto expandAllAction = contextMenu->addAction(tr("Expand all"));
    auto collapseAllAction = contextMenu->addAction(tr("Collapse all"));

    connect(expandAllAction, &QAction::triggered, this, &QTreeView::expandAll);
    connect(collapseAllAction, &QAction::triggered, this, &QTreeView::collapseAll);
    connect(expandAction, &QAction::triggered, this, [this]() {
        expandRecursively(currentIndex());
    });
    connect(collapseAction, &QAction::triggered, this, [this]() {
        collapse(currentIndex());
    });
    connect(this, &QTreeView::customContextMenuRequested, this, [=](const QPoint& pos) {
        const auto index = currentIndex();
        const auto isValid = index.isValid();
        const auto indexExpanded = isValid && isExpanded(index);
        expandAction->setVisible(isValid && !indexExpanded);
        collapseAction->setVisible(isValid && indexExpanded);
        contextMenu->popup(mapToGlobal(pos));
    });
}

void ExampleTreeView::resizeColumnsToContents()
{
    const auto columnCount = model()->columnCount();
    for (int col = 0; col < columnCount; ++col) {
        resizeColumnToContents(col);
    }
}

void ExampleTreeView::currentChanged(const QModelIndex &newIdx, const QModelIndex &prevIdx)
{
    QTreeView::currentChanged(newIdx, prevIdx);
    Q_EMIT currentIndexChanged(newIdx);
}

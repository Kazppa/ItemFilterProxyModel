#include "ExampleTreeView.h"

#include <QMenu>
#include <QScrollBar>

ExampleTreeView::ExampleTreeView(QWidget *parent) :
        QTreeView(parent)
{
    verticalScrollBar()->setTracking(true);
    setSelectionMode(SelectionMode::ExtendedSelection);
    setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    setDragDropMode(DragDropMode::InternalMove);

    m_menu = new QMenu(this);
    m_expandAction = m_menu->addAction(tr("Expand"));
    m_collapseAction = m_menu->addAction(tr("Collapse"));
    m_menu->addSeparator();
    m_expandAllAction = m_menu->addAction(tr("Expand all"));
    m_collapseAllAction = m_menu->addAction(tr("Collapse all"));
    m_removeAction = m_menu->addAction(tr("Delete item"));

    connect(m_expandAllAction, &QAction::triggered, this, &QTreeView::expandAll);
    connect(m_collapseAllAction, &QAction::triggered, this, &QTreeView::collapseAll);
    connect(m_expandAction, &QAction::triggered, this, [this]() { expandRecursively(currentIndex()); });
    connect(m_collapseAction, &QAction::triggered, this, [this]() { collapse(currentIndex()); });
    connect(m_removeAction, &QAction::triggered, this, &ExampleTreeView::onRemoveAction);
    connect(this, &QTreeView::customContextMenuRequested, this, &ExampleTreeView::onContextMenuRequested);
}

void ExampleTreeView::resizeColumnsToContents()
{
    auto model = QTreeView::model();
    if (!model) {
        return;
    }
    const auto columnCount = model->columnCount();
    for (int col = 0; col < columnCount; ++col) {
        resizeColumnToContents(col);
    }
}

void ExampleTreeView::onContextMenuRequested(const QPoint& pos)
{
    const auto index = currentIndex();
    const auto isValid = index.isValid();
    auto indexExpanded = isValid && isExpanded(index);
    m_expandAction->setVisible(isValid && !indexExpanded);
    m_collapseAction->setVisible(isValid && indexExpanded);
    m_removeAction->setVisible(!selectedIndexes().empty());

    m_menu->popup(mapToGlobal(pos));
}

void ExampleTreeView::currentChanged(const QModelIndex &newIdx, const QModelIndex &prevIdx)
{
    QTreeView::currentChanged(newIdx, prevIdx);
    Q_EMIT currentIndexChanged(newIdx);
}


void ExampleTreeView::onRemoveAction()
{
    const auto model = QTreeView::model();
    const auto indexesToRemove = selectedIndexes();
    for (const auto& index : indexesToRemove) {
        model->removeRow(index.row(), index.parent());
    }
}


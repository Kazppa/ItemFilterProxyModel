#include "ExampleTreeView.h"

#include <QMenu>
#include <QScrollBar>
#include <QStack>

ExampleTreeView::ExampleTreeView(QWidget *parent) :
        QTreeView(parent)
{
    verticalScrollBar()->setTracking(true);
    setSelectionMode(SelectionMode::ExtendedSelection);
    setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    viewport()->setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(DragDropMode::DragDrop);
    setDefaultDropAction(Qt::MoveAction);

    m_menu = new QMenu(this);
    m_expandAction = m_menu->addAction(tr("Expand"));
    m_collapseAction = m_menu->addAction(tr("Collapse"));
    m_removeAction = m_menu->addAction(tr("Delete item"));
    m_menu->addSeparator();
    m_expandAllAction = m_menu->addAction(tr("Expand all"));
    m_collapseAllAction = m_menu->addAction(tr("Collapse all"));

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

QModelIndex ExampleTreeView::searchTextIndex(const QString &searchedText) const
{
    auto sourceModel = model();
    Q_ASSERT(sourceModel);
    QStack<QModelIndex> stack;

    const auto rootRowCount = sourceModel->rowCount();
    const auto rootColumnCount = sourceModel->columnCount();
    for (auto row = 0; row < rootRowCount; ++row) {
        for (auto col = 0; col < rootColumnCount; ++col) {
            const auto index = sourceModel->index(row, col);
            const auto txt = index.data().toString();
            if (txt == searchedText) {
                return  index;
            }
            stack.push_back(index);
        }
    }

    while(!stack.empty()) {
        const auto parentIndex = stack.pop();

        const auto rowCount = sourceModel->rowCount(parentIndex);
        const auto colCount = sourceModel->columnCount(parentIndex);
        for (auto row = 0; row < rowCount; ++row) {
            for (auto col = 0; col < colCount; ++col) {
                const auto index = sourceModel->index(row, col, parentIndex);
                const auto txt = index.data().toString();
                if (txt == searchedText) {
                    return  index;
                }
                stack.push_back(index);
            }
        }
    }
    return {};
}

void ExampleTreeView::currentChanged(const QModelIndex &newIdx, const QModelIndex &prevIdx)
{
    QTreeView::currentChanged(newIdx, prevIdx);
    Q_EMIT currentIndexChanged(newIdx);
}


void ExampleTreeView::onRemoveAction()
{
    const auto model = QTreeView::model();
    const auto index = currentIndex();
    const auto text = index.data().toString();
    model->removeRow(index.row(), index.parent());
}


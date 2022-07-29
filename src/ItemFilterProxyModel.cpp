#include "ItemFilterProxyModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QStack>

#include "ItemFilterProxyModelPrivate.h"

using namespace kaz;    // fine in a cpp file

namespace
{
    struct SourceModelIndexLessComparator
    {
        mutable QMap<QModelIndex, QStack<QModelIndex>> m_parentStack;

        QStack<QModelIndex>& getSourceIndexParents(const QModelIndex &sourceIndex) const
        {
            Q_ASSERT(sourceIndex.isValid());
            auto& parents = m_parentStack[sourceIndex];
            if (parents.empty()) {
                QModelIndex parentIndex = sourceIndex;
                do {
                    parentIndex = parentIndex.parent();
                    parents.push(parentIndex);
                }
                while (parentIndex.isValid());
            }
            return parents;
        }

        bool operator()(const QModelIndex& left, const QModelIndex &right) const
        {
            Q_ASSERT(left.isValid() && right.isValid());
            Q_ASSERT(left.column() == right.column());
            if (left.parent() == right.parent()) {
                return left.row() < right.row();
            }

            const auto& leftParents = getSourceIndexParents(left);
            const auto& rightParents = getSourceIndexParents(right);
            
            auto leftP = left.data().toString();
            for (const auto& p : leftParents) {
                leftP += QStringLiteral(" -> ") + p.data().toString();
            }
            auto rightP = right.data().toString();
            for (const auto& p : rightParents) {
                rightP += QStringLiteral(" -> ") + p.data().toString();
            }

            const auto it = std::mismatch(leftParents.rbegin(), leftParents.rend(), rightParents.rbegin(), rightParents.rend());
            const auto firstText = it.first->data().toString();
            const auto dummy = it.first->model()->index(0, left.column(), *it.second);
            const auto txt = dummy.data().toString();
            Q_ASSERT(it.first->parent() == it .second->parent());
            return it.first->row() < it.second->row();
        }

        bool operator()(const QModelIndex &left, const std::shared_ptr<ProxyIndexInfo>& right)
        {
            return this->operator()(left, right->m_source);
        }

        bool operator()(const std::shared_ptr<ProxyIndexInfo>& left, const QModelIndex &right)
        {
            return this->operator()(left->m_source, right);
        }
    };

    struct InsertInfo
    {
        int m_row;
        ProxyIndexInfo::ChildrenList::const_iterator m_it;
    };

    // Return at which row (with an iterator to the specific children) the given sourceIndex should be inserted     
    InsertInfo searchInsertableRow(const std::shared_ptr<ProxyIndexInfo> &proxyInfo, const QModelIndex &sourceIndex)
    {
        Q_ASSERT(sourceIndex.isValid());
        auto& proxyChildren = proxyInfo->m_children;
        if (proxyChildren.empty()) {
            return { 0, proxyChildren.begin() };
        }
        Q_ASSERT(sourceIndex.model() == proxyChildren.at(0)->m_source.model());

        const auto column = sourceIndex.column();
        const auto [ columnBegin, columnEnd ] = proxyInfo->getColumnBeginEnd(column);
        const auto it = std::lower_bound(columnBegin, columnEnd, sourceIndex, SourceModelIndexLessComparator{});
        return {
            (int) std::distance(columnBegin, it),
            it
        };
    }

}

//////////////////////////

ItemFilterProxyModel::ItemFilterProxyModel(QObject *parent) :
    QAbstractProxyModel(parent)
{
    m_d.reset(new ItemFilterProxyModelPrivate(this));
}

void ItemFilterProxyModel::setSourceModel(QAbstractItemModel *newSourceModel)
{
    auto previousSourceModel = sourceModel();
    if (newSourceModel == previousSourceModel) {
        return;
    }

    beginResetModel();
    if (previousSourceModel) {
        disconnect(previousSourceModel, nullptr, this, nullptr);
    }
    disconnect(newSourceModel, nullptr, this, nullptr);
    QAbstractProxyModel::setSourceModel(newSourceModel);

    if (!newSourceModel) {
        return;
    }
    connect(newSourceModel, &QAbstractItemModel::dataChanged, this, &ItemFilterProxyModel::sourceDataChanged);
    connect(newSourceModel, &QAbstractItemModel::modelAboutToBeReset, this, &ItemFilterProxyModel::beginResetModel);
    connect(newSourceModel, &QAbstractItemModel::modelReset, this, [this]() {
        resetProxyIndexes();
        endResetModel();
    });
    connect(newSourceModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &ItemFilterProxyModel::layoutAboutToBeChanged);
    connect(newSourceModel, &QAbstractItemModel::layoutChanged, this, [this]() {
        resetProxyIndexes();
        layoutChanged();
    });
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this, &ItemFilterProxyModel::onRowsAboutToBeInserted);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ItemFilterProxyModel::onRowsAboutToBeRemoved);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeMoved, this, &ItemFilterProxyModel::onRowsAboutToBeMoved);

    resetProxyIndexes();
    endResetModel();
}

QModelIndex ItemFilterProxyModel::index(int row,int column, const QModelIndex &parent) const 
{
    return m_d->index(row, column, parent);
}

QModelIndex ItemFilterProxyModel::parent(const QModelIndex &childIndex) const
{
    return m_d->parent(childIndex);
}

int ItemFilterProxyModel::rowCount(const QModelIndex &parentIndex) const
{
    return m_d->rowCount(parentIndex);
}

int ItemFilterProxyModel::columnCount(const QModelIndex &parentIndex) const
{
    return m_d->columnCount(parentIndex);
}

bool ItemFilterProxyModel::hasChildren(const QModelIndex &parentIndex) const
{
    return m_d->hasChildren(parentIndex);
}

QModelIndex ItemFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!sourceModel()) {
        return {};
    }
    
    return m_d->mapToSource(proxyIndex);
}

QModelIndex ItemFilterProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceModel()) {
        return {};
    }

    return m_d->mapFromSource(sourceIndex);
}

void ItemFilterProxyModel::invalidateRowsFilter()
{
    // TODO implement this without a model reset : by deleting, inserting and moving rows around, just like onSourceDataChanged
    beginResetModel();
    resetProxyIndexes();
    endResetModel();
}

void ItemFilterProxyModel::sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles)
{
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        return;
    }

    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    Q_ASSERT(sourceLeft.model() == sourceModel);
    Q_ASSERT(sourceRight.model() == sourceModel);
    Q_ASSERT(sourceLeft.parent() == sourceRight.parent());
    const auto sourceParent = sourceLeft.parent();
    const auto sourceLeftRow = sourceLeft.row();
    for (int sourceRow = sourceRight.row(); sourceRow >= sourceLeftRow; --sourceRow) {
        auto sourceIndex = sourceModel->index(sourceRow, 0, sourceParent);
        const auto isNewValueVisible = filterAcceptsRow(sourceRow, sourceParent);
        const auto proxyInfo = m_d->m_sourceIndexHash.value(sourceIndex);
        if (isNewValueVisible) {
            if (proxyInfo) {
                // Proxy index is still valid and visible
                Q_EMIT dataChanged(proxyInfo->m_index, proxyInfo->m_index, roles);
                continue;
            }

            // Create a new proxy index because the source index became visible with this data update
            const auto proxyParent = m_d->getProxyNearestParentIndex(sourceIndex);
            const auto [row, insertIt] = searchInsertableRow(proxyParent, sourceIndex);
            beginInsertRows(proxyParent->m_index, row, row);
            const auto newProxyIndex = createIndex(row, sourceIndex.column(), sourceIndex.internalId());
            auto newProxyIndexInfo = std::make_shared<ProxyIndexInfo>(sourceIndex, newProxyIndex, proxyParent); 
            m_d->m_sourceIndexHash[sourceIndex] = newProxyIndexInfo;
            m_d->m_proxyIndexHash[newProxyIndex] = newProxyIndexInfo;
            proxyParent->m_children.insert(insertIt, newProxyIndexInfo);
            endInsertRows();

            // TODO move source index's children to the newly created proxy index

        }
        else {
            const auto proxyRow = proxyInfo->row();
            if (proxyInfo) {
                // The source index is not visible anymore, the proxy index must be deleted
                if (!proxyInfo->m_children.empty()) {
                    // move proxy's children to proxy's parent
                    const auto proxyLastRow = proxyInfo->rowCount() - 1;
                    const auto& parentIndex = proxyInfo->parentIndex();
                    beginMoveRows(proxyInfo->m_index, 0, proxyLastRow, parentIndex, proxyRow + 1);
                    m_d->moveRowsImpl(proxyInfo, 0, proxyLastRow, proxyInfo->m_parent, proxyRow + 1);
                    endMoveRows();
                }
                // Remove the proxy index
                beginRemoveRows(proxyInfo->parentIndex(), proxyRow, proxyRow);
                eraseRows(proxyInfo->m_parent, proxyRow, proxyRow);
                endResetModel();
            }
        }
    }
}


std::shared_ptr<ProxyIndexInfo> ItemFilterProxyModel::appendSourceIndexInSortedIndexes(const QModelIndex& sourceIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParent)
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(proxyParent);
    
    const auto column = sourceIndex.column();
    const auto [ columnBegin, columnEnd ] = proxyParent->getColumnBeginEnd(column);
    const auto proxyRow = std::distance(columnBegin, columnEnd);
    const auto proxyIndex = createIndex(proxyRow, column, sourceIndex.internalId());
    auto child = std::make_shared<ProxyIndexInfo>(sourceIndex, proxyIndex, proxyParent);
    proxyParent->m_children.insert(columnEnd, child);

    m_d->m_sourceIndexHash.emplace(sourceIndex, child);
    m_d->m_proxyIndexHash.emplace(proxyIndex, child);
    return child;
}

// TODO Fixme
void ItemFilterProxyModel::eraseRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow)
{
    Q_ASSERT(!parentProxyInfo->m_index.isValid() || parentProxyInfo->m_index.model() == this);
    
    QStack<std::shared_ptr<ProxyIndexInfo>> stack;
    auto& parentChildren = parentProxyInfo->m_children;
    const auto end = parentChildren.end();
    parentChildren.erase(std::remove_if(parentChildren.begin(), end, [&](const std::shared_ptr<ProxyIndexInfo>& proxyInfo) {
        const auto row = proxyInfo->m_index.row();
        if (row < firstRow || row > lastRow) {
            return false;
        }
        m_d->m_sourceIndexHash.remove(proxyInfo->m_source);
        m_d->m_proxyIndexHash.erase(proxyInfo->m_index);
        for (auto& child : proxyInfo->m_children) {
            stack.push(std::move(child));
        }
        return true;
    }), end);

    while (!stack.empty()) {
        const auto parentInfo = std::move(stack.top());
        stack.pop();

        m_d->m_sourceIndexHash.remove(parentInfo->m_source);
        m_d->m_proxyIndexHash.erase(parentInfo->m_index);
        for (auto& child : parentInfo->m_children) {
            stack.push(std::move(child));
        }
        parentInfo->m_children.clear();
    }
    updateChildrenRows(parentProxyInfo);
}

void ItemFilterProxyModel::updateChildrenRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo)
{
    auto& parentChildren = parentProxyInfo->m_children;
    if (parentChildren.empty()) {
        return;
    }

    int expectedRow = 0, col = 0;
    for (auto& child : parentChildren) {
        auto& childIndex = child->m_index;
        const auto childRow = childIndex.row();
        const auto childColumn = childIndex.column();
        bool updatedRequired = false;
        if (childColumn != col) {
            // Found the next column
            Q_ASSERT(childColumn == col + 1);
            col = childColumn;
            // First row of the column
            expectedRow = 0;
        }
        if (childRow != expectedRow) {
            updatedRequired = true;
        }

        if (updatedRequired) {
            m_d->m_proxyIndexHash.erase(childIndex);
            // New proxy mapping
            childIndex = createIndex(expectedRow, col, childIndex.internalId());
#ifdef QT_DEBUG
            const auto it = m_d->m_proxyIndexHash.emplace(childIndex, child);
            Q_ASSERT(it.second);
#else
            m_proxyIndexHash.emplace(childIndex, child);
#endif
        }
        ++expectedRow;
    }
}

void ItemFilterProxyModel::onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    const auto proxyRange = m_d->mapFromSourceRange(sourceParent, sourceFirst, sourceLast);
    for (const auto& [first, last] : proxyRange) {
        Q_ASSERT(first.isValid() && last.isValid());
        Q_ASSERT(first.parent() == last.parent());
        Q_ASSERT(first.model() == this && last.model() == this);
        const auto proxyParent = first.parent();
        const auto firstRow = first.row();
        const auto lastRow = last.row();
        auto proxyParentInfo = m_d->m_proxyIndexHash.at(proxyParent);

        Q_EMIT beginRemoveRows(proxyParent, firstRow, lastRow);
        eraseRows(proxyParentInfo, firstRow, lastRow);
        Q_EMIT endRemoveRows();
    }
}

void ItemFilterProxyModel::onRowsAboutToBeInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
   // TODO
    Q_ASSERT(false);
}

void ItemFilterProxyModel::onRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
    const QModelIndex &destinationParent, int destinationRow)
{
    // TODO
    Q_ASSERT(false);
}

void ItemFilterProxyModel::resetProxyIndexes()
{
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    Q_ASSERT(sourceModel);
    
    m_d->m_sourceIndexHash.clear();
    m_d->m_proxyIndexHash.clear();

    // Parent of all root nodes
    auto hiddenRootIndex = std::make_shared<ProxyIndexInfo>(QModelIndex(), QModelIndex(), nullptr);
    m_d->m_proxyIndexHash.emplace(QModelIndex(), hiddenRootIndex);
    const auto rootRowCount = sourceModel->rowCount();
    const auto rootColumnCount = sourceModel->columnCount();
    for (int sourceRow = 0; sourceRow < rootRowCount; ++sourceRow) {
        const auto isRootNodeVisible = filterAcceptsRow(sourceRow, QModelIndex());
        for (int col = 0; col < rootColumnCount; ++col) {
            const auto sourceIndex = sourceModel->index(sourceRow, col);
            Q_ASSERT(sourceIndex.isValid());

            if (isRootNodeVisible) {
                const auto proxyIndexInfo = appendSourceIndexInSortedIndexes(sourceIndex, hiddenRootIndex);
                fillChildrenIndexesRecursively(sourceIndex, proxyIndexInfo);
            }
            else {
                fillChildrenIndexesRecursively(sourceIndex, hiddenRootIndex);
            }
        }
    }
}

void ItemFilterProxyModel::fillChildrenIndexesRecursively(const QModelIndex &sourceParent, const std::shared_ptr<ProxyIndexInfo>& parentInfo)
{
    Q_ASSERT(sourceParent.isValid());
    Q_ASSERT(sourceParent.model() == sourceModel());
    Q_ASSERT(parentInfo);

    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    const auto& proxyParent = parentInfo->m_index;
    const auto rowCount = sourceModel->rowCount(sourceParent);
    const auto columnCount = sourceModel->columnCount(sourceParent);
    Q_ASSERT(!proxyParent.isValid() ||  proxyParent.model() == this);

    for (auto sourceRow = 0; sourceRow < rowCount; ++sourceRow) {
        const auto isChildVisible = filterAcceptsRow(sourceRow, sourceParent);

        for (int col = 0; col < columnCount; ++col) {
            const auto sourceIndex = sourceModel->index(sourceRow, col, sourceParent);
            Q_ASSERT(sourceIndex.isValid());

            if (isChildVisible) {
                const auto proxyIndexInfo = appendSourceIndexInSortedIndexes(sourceIndex, parentInfo);
                fillChildrenIndexesRecursively(sourceIndex, proxyIndexInfo);
            }
            else {
                fillChildrenIndexesRecursively(sourceIndex, parentInfo);
            }
        }
    }
}
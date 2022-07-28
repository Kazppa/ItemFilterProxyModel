#include "ItemFilterProxyModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QStack>

using namespace kaz;    // fine in a cpp file
using ProxyIndexInfo = ItemFilterProxyModel::ProxyIndexInfo;

namespace
{
    // Helper to use multiple lambdas as a comparator
    template<typename ...Callables>
    struct make_visitor : Callables... { using Callables::operator()...; };

    template<typename ...Callables>
    make_visitor(Callables...) -> make_visitor<Callables...>;

    struct ProxyIndexInfoLessComparator
    {
        bool operator()(const std::shared_ptr<ProxyIndexInfo>& left, const std::shared_ptr<ProxyIndexInfo>& right) const
        {
            const auto leftColumn = left->m_index.column();
            const auto rightColumn = right->m_index.column();
            if (leftColumn < rightColumn) {
                // Order by column
                return true;
            }
            else if (leftColumn == rightColumn) {
                // In the same column, order by row
                return left->m_index.row() < right->m_index.row();
            }
            return false;
        }
    };

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

ProxyIndexInfo::ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyIndex,
    const std::shared_ptr<ProxyIndexInfo> &proxyParentIndex) :
    m_source(sourceIndex),
    m_index(proxyIndex),
    m_parent(proxyParentIndex)
{
#ifdef QT_DEBUG
    if (sourceIndex.isValid()) {
        Q_ASSERT(sourceIndex.model() != proxyIndex.model());
    }
    if (proxyIndex.isValid() && proxyParentIndex && proxyParentIndex->m_index.isValid()) {
        Q_ASSERT(proxyIndex.model() == proxyParentIndex->m_index.model());
    }
#endif
}

std::pair<ProxyIndexInfo::ChildrenList::const_iterator, ProxyIndexInfo::ChildrenList::const_iterator> ProxyIndexInfo::getColumnBeginEnd(int column) const
{
    Q_ASSERT(column >= 0);
    return std::equal_range(m_children.begin(), m_children.end(), column, make_visitor {
        [](const std::shared_ptr<ProxyIndexInfo>& child, int column) { return child->m_index.column() < column; },
        [](int column, const std::shared_ptr<ProxyIndexInfo>& child) { return column < child->m_index.column(); }
    });
}

ProxyIndexInfo::ChildrenList::const_iterator ProxyIndexInfo::getColumnBegin(int column) const
{
    return std::lower_bound(m_children.begin(), m_children.end(), column, [](const auto& child, int column) {
        return child->m_index.column() < column;
    });
}

std::shared_ptr<ProxyIndexInfo> ProxyIndexInfo::childAt(int row, int column) const
{
    const auto columnIt = getColumnBegin(column);
    if (columnIt == m_children.end()) {
        Q_ASSERT(false);
        return {};
    }

    const auto dist = std::distance(m_children.begin(), columnIt);
    const auto child = *(columnIt + row);
    Q_ASSERT(child->m_index.row() == row && child->m_index.column() == column);
    return child;
}

int ProxyIndexInfo::rowCount(int column) const
{
    const auto [first, last] = getColumnBeginEnd(column);
    return std::distance(first, last);
}

//////////////////////////

ItemFilterProxyModel::ItemFilterProxyModel(QObject *parent) :
    QAbstractProxyModel(parent)
{
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
    const auto it = m_proxyIndexHash.find(parent);
    if (it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
        return {};
    }
    
    const auto child = it->second->childAt(row, column);
    if (!child) {
        return {};
    }
    return child->m_index;
}

QModelIndex ItemFilterProxyModel::parent(const QModelIndex &childIndex) const
{
    if (!childIndex.isValid()) {
        return {};
    }

    const auto it = m_proxyIndexHash.find(childIndex);
    if(it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
        return {};
    }
    
    return it->second->m_parent->m_index;
}

int ItemFilterProxyModel::rowCount(const QModelIndex &parentIndex) const
{
    if (!sourceModel()) {
        return 0;
    }

    const auto it = m_proxyIndexHash.find(parentIndex);
    if (it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
        return 0;
    }

    const auto column = parentIndex.column() >= 0 ? parentIndex.column() : 0;
    const auto text = it->second->m_index.data().toString();
    if (text == u"A25") {
        qDebug();
    }
    return it->second->rowCount(column);
}

int ItemFilterProxyModel::columnCount(const QModelIndex &parentIndex) const
{
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    if (!sourceModel) {
        return 0;
    }

    const auto sourceIndex = mapToSource(parentIndex);
    Q_ASSERT(sourceIndex.isValid() || !parentIndex.isValid());
    return sourceModel->columnCount(sourceIndex);
}

bool ItemFilterProxyModel::hasChildren(const QModelIndex &parentIndex) const
{
    const auto it = m_proxyIndexHash.find(parentIndex);
    if (it == m_proxyIndexHash.end()) {
        return false;
    }
    return !it->second->m_children.empty();
}

QModelIndex ItemFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!sourceModel() || !proxyIndex.isValid()) {
        return {};
    }

    Q_ASSERT(proxyIndex.model() == this);
    const auto it = m_proxyIndexHash.find(proxyIndex);
    if (it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
        return {};
    }
    return it->second->m_source;
}

QModelIndex ItemFilterProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceModel() || !sourceIndex.isValid()) {
        return {};
    }

    Q_ASSERT(sourceIndex.model() == sourceModel());
    const auto it = m_sourceIndexHash.find(sourceIndex);
    return it == m_sourceIndexHash.end() ? QModelIndex() : (*it)->m_index;
}

std::vector<std::pair<QModelIndex, QModelIndex>> ItemFilterProxyModel::mapFromSourceRange(const QModelIndex &sourceLeft,
    const QModelIndex& sourceRight, const SelectionParameters parameters) const
{
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    Q_ASSERT(sourceLeft.model() == sourceModel);
    Q_ASSERT(sourceRight.model() == sourceModel);
    Q_ASSERT(sourceLeft.parent() == sourceRight.parent());
    const auto sourceParent = sourceLeft.parent();

    const auto sourceLeftRow = sourceLeft.row();
    const auto sourceRightRow = sourceRight.row();
    const auto column = sourceLeft.column();
    
    QModelIndex proxyIndexEndRange, proxyIndexBeginRange = mapFromSource(sourceLeft);
    std::vector<std::pair<QModelIndex, QModelIndex>> ranges;
    const auto appendIndexToRange = [&](const QModelIndex &proxyIndex) {
        if (!proxyIndexBeginRange.isValid()) {
            // New range
            proxyIndexBeginRange = proxyIndex;
            return;
        }

        if (proxyIndex.parent() == proxyIndexBeginRange.parent()) {
            // Enlarge the current range
            proxyIndexEndRange = proxyIndex;
        }
        else {
            // End of the range, insert in the results
            ranges.emplace_back(proxyIndexBeginRange, proxyIndexEndRange.isValid() ? proxyIndexEndRange : proxyIndexBeginRange);
            proxyIndexBeginRange = proxyIndex;
            proxyIndexEndRange = QModelIndex();
        }
    };

    for (int sourceRow = sourceLeftRow + 1; sourceRow < sourceRightRow; ++sourceRow) {
        const auto sourceIndex = sourceModel->index(sourceRow, column, sourceParent);
        const auto proxyIndex = mapFromSource(sourceIndex);
        if (proxyIndex.isValid()) {
            appendIndexToRange(proxyIndex);
        }
        else if (parameters & IncludeChildrenIfInvisible) {
            // Source index is invisble, select his "direct" visible children instead (check recursively until finding a visible child index)
            const auto visibleChildren = getProxyChildrenIndexes(sourceIndex);
            for (const auto& proxyIndex : visibleChildren) {
                appendIndexToRange(proxyIndex);
            }
        }
    }

    if (proxyIndexBeginRange.isValid()) {
        ranges.emplace_back(proxyIndexBeginRange, proxyIndexEndRange.isValid() ? proxyIndexEndRange : proxyIndexBeginRange);
    }
    return ranges;
}

std::vector<std::pair<QModelIndex, QModelIndex>> ItemFilterProxyModel::mapFromSourceRange(
        const QModelIndex& sourceParent, int sourceFirst, int sourceLast, const SelectionParameters parameters) const
{
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    const auto column = sourceParent.column() >= 0 ? sourceParent.column() : 0;
    const auto sourceLeft = sourceModel->index(sourceFirst, column, sourceParent);
    const auto sourceRight = sourceModel->index(sourceLast, column, sourceParent);
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        return {};
    }
    return mapFromSourceRange(sourceLeft, sourceRight, parameters);
}

QModelIndexList ItemFilterProxyModel::getProxyChildrenIndexes(const QModelIndex &sourceIndex) const
{
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    const auto childrenCount = sourceModel->rowCount(sourceIndex);
    const auto column = sourceIndex.column() >= 0 ? sourceIndex.column() : 0;

    QModelIndexList indexes;
    indexes.reserve(childrenCount);
    for (int sourceRow = 0; sourceRow < childrenCount; ++sourceRow) {
        const auto childSourceIndex = sourceModel->index(sourceRow, column, sourceIndex);
        const auto proxyIndex = mapFromSource(childSourceIndex);
        if (proxyIndex.isValid()) {
            // Visible child
            indexes.push_back(proxyIndex);
        }
        else {
            // Non visible index, search recursively a visible child
            auto childVisibleIndexes = getProxyChildrenIndexes(childSourceIndex);
            indexes.append(std::move(childVisibleIndexes));
        }
    }
    return indexes;
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
        const auto proxyInfo = m_sourceIndexHash.value(sourceIndex);
        if (isNewValueVisible) {
            if (proxyInfo) {
                // Proxy index is still valid and visible
                Q_EMIT dataChanged(proxyInfo->m_index, proxyInfo->m_index, roles);
                continue;
            }

            // Create a new proxy index because the source index became visible with this data update
            const auto proxyParent = getProxyNearestParentIndex(sourceIndex);
            const auto [row, insertIt] = searchInsertableRow(proxyParent, sourceIndex);
            beginInsertRows(proxyParent->m_index, row, row);
            const auto newProxyIndex = createIndex(row, sourceIndex.column(), sourceIndex.internalId());
            auto newProxyIndexInfo = std::make_shared<ProxyIndexInfo>(sourceIndex, newProxyIndex, proxyParent); 
            m_sourceIndexHash[sourceIndex] = newProxyIndexInfo;
            m_proxyIndexHash[newProxyIndex] = newProxyIndexInfo;
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
                    const auto proxyLastRow = proxyInfo->rowCount(0) - 1;
                    const auto& parentIndex = proxyInfo->parentIndex();
                    beginMoveRows(proxyInfo->m_index, 0, proxyLastRow, parentIndex, proxyRow + 1);
                    moveRowsImpl(proxyInfo, 0, proxyLastRow, proxyInfo->m_parent, proxyRow + 1);
                    endMoveRows();
                }
                // Remove the proxy index
                beginRemoveRows(proxyInfo->parentIndex(), proxyRow, proxyRow);
                removeRowsImpl(proxyInfo->m_parent, proxyRow, proxyRow);
                endResetModel();
            }
        }
    }
}

std::shared_ptr<ProxyIndexInfo> ItemFilterProxyModel::getProxyNearestParentIndex(const QModelIndex &sourceIndex) const
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(sourceIndex.model() == sourceModel());

    auto sourceParentIndex = sourceIndex.parent();
    QModelIndex matchingProxyParentIndex = mapFromSource(sourceParentIndex);
    while (!matchingProxyParentIndex.isValid()) {
        sourceParentIndex = sourceParentIndex.parent();
        matchingProxyParentIndex = mapFromSource(sourceParentIndex);
        if (!sourceParentIndex.isValid()) {
            break;
        }
    }
    return m_proxyIndexHash.at(matchingProxyParentIndex);
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

    m_sourceIndexHash.emplace(sourceIndex, child);
    m_proxyIndexHash.emplace(proxyIndex, child);
    return child;
}

// TODO Fixme
void ItemFilterProxyModel::removeRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow)
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
        m_sourceIndexHash.remove(proxyInfo->m_source);
        m_proxyIndexHash.erase(proxyInfo->m_index);
        for (auto& child : proxyInfo->m_children) {
            stack.push(std::move(child));
        }
        return true;
    }), end);

    while (!stack.empty()) {
        const auto parentInfo = std::move(stack.top());
        stack.pop();

        m_sourceIndexHash.remove(parentInfo->m_source);
        m_proxyIndexHash.erase(parentInfo->m_index);
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
            m_proxyIndexHash.erase(childIndex);
            // New proxy mapping
            childIndex = createIndex(expectedRow, col, childIndex.internalId());
#ifdef QT_DEBUG
            const auto it = m_proxyIndexHash.emplace(childIndex, child);
            Q_ASSERT(it.second);
#else
            m_proxyIndexHash.emplace(childIndex, child);
#endif
        }
        ++expectedRow;
    }
}

void ItemFilterProxyModel::moveRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow,
        const std::shared_ptr<ProxyIndexInfo>& destinationInfo, int destinationRow)
{
    // TODO
}

void ItemFilterProxyModel::onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    const auto proxyRange = mapFromSourceRange(sourceParent, sourceFirst, sourceLast);
    for (const auto& [first, last] : proxyRange) {
        Q_ASSERT(first.isValid() && last.isValid());
        Q_ASSERT(first.parent() == last.parent());
        Q_ASSERT(first.model() == this && last.model() == this);
        const auto proxyParent = first.parent();
        const auto firstRow = first.row();
        const auto lastRow = last.row();
        auto proxyParentInfo = m_proxyIndexHash.at(proxyParent);

        Q_EMIT beginRemoveRows(proxyParent, firstRow, lastRow);
        removeRowsImpl(proxyParentInfo, firstRow, lastRow);
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
    m_sourceIndexHash.clear();
    m_proxyIndexHash.clear();

    // Parent of all root nodes
    auto hiddenRootIndex = std::make_shared<ProxyIndexInfo>(QModelIndex(), QModelIndex(), nullptr);
    m_proxyIndexHash.emplace(QModelIndex(), hiddenRootIndex);
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
#include "ItemFilterProxyModel.h"

#include <QtCore/QDateTime>

using namespace kaz;    // fine in a cpp file
using ProxyIndexInfo = ItemFilterProxyModel::ProxyIndexInfo;

namespace
{
    // Helper to use multiple lambdas as a comparator
    template<typename ...Callables>
    struct make_visitor : Callables... { using Callables::operator()...; };

    template<typename ...Callables>
    make_visitor(Callables...) -> make_visitor<Callables...>;
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
        [=](const std::shared_ptr<ProxyIndexInfo>& child, int column) { return child->m_index.column() < column; },
        [=](int column, const std::shared_ptr<ProxyIndexInfo>& child) { return column < child->m_index.column(); }
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
        updateProxyIndexes();
        endResetModel();    // associated beginResetModel() called in the above connect
    });
    connect(newSourceModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &ItemFilterProxyModel::layoutAboutToBeChanged);
    connect(newSourceModel, &QAbstractItemModel::layoutChanged, this, [this]() {
        updateProxyIndexes();
        layoutChanged();    // associated beginResetModel() called in the above connect
    });
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this, &ItemFilterProxyModel::onRowsAboutToBeInserted);
    connect(newSourceModel, &QAbstractItemModel::rowsInserted, this, &ItemFilterProxyModel::onRowsInserted);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ItemFilterProxyModel::onRowsAboutToBeRemoved);
    connect(newSourceModel, &QAbstractItemModel::rowsRemoved, this, &ItemFilterProxyModel::rowsRemoved);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeMoved, this, &ItemFilterProxyModel::rowsAboutToBeMoved);
    connect(newSourceModel, &QAbstractItemModel::rowsMoved, this, &ItemFilterProxyModel::rowsMoved);

    updateProxyIndexes();
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
    return child ? child->m_index : QModelIndex();
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

    const auto begin = it->second->m_children.begin();
    const auto end = it->second->m_children.end();
    const auto column = parentIndex.column() >= 0 ? parentIndex.column() : 0;
    return std::count_if(begin, end, [=](const auto& child) {
        return child->m_index.column() == column;
    });
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
    return m_sourceIndexHash.value(sourceIndex);
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

    const auto proxyRange = mapFromSourceRange(sourceLeft, sourceRight);
    for (const auto& range : proxyRange) {
        Q_EMIT dataChanged(range.first, range.second, roles);
    }
    // TODO update visible / hidden items
}

QModelIndex ItemFilterProxyModel::getProxyNearestParentIndex(const QModelIndex &sourceIndex) const
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(sourceIndex.model() == this);

    QModelIndex matchingProxyParentIndex;
    auto sourceParentIndex = sourceIndex;
    do {
        matchingProxyParentIndex = mapFromSource(sourceParentIndex);
    }
    while (!matchingProxyParentIndex.isValid() && (sourceParentIndex = sourceParentIndex.parent()).isValid());
    return matchingProxyParentIndex;
}

std::shared_ptr<ProxyIndexInfo> ItemFilterProxyModel::appendIndex(const QModelIndex& sourceIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParent)
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(proxyParent);
    const auto column = sourceIndex.column();
    const auto [ columnBegin, columnEnd ] = proxyParent->getColumnBeginEnd(column);
    const auto proxyRow = std::distance(columnBegin, columnEnd);
    const auto proxyIndex = createIndex(proxyRow, column, sourceIndex.internalId());
    auto child = std::make_shared<ProxyIndexInfo>(sourceIndex, proxyIndex, proxyParent);
    proxyParent->m_children.insert(columnEnd, child);

    m_sourceIndexHash.emplace(sourceIndex, proxyIndex);
    m_proxyIndexHash.emplace(proxyIndex, child);
    return child;
}

void ItemFilterProxyModel::onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    const auto proxyRange = mapFromSourceRange(sourceParent, sourceFirst, sourceLast);
    for (const auto& [first, last] : proxyRange) {
        Q_ASSERT(first.isValid() && last.isValid());
        Q_ASSERT(first.parent() == last.parent());
        Q_ASSERT(first.model() == this && last.model() == this);
        Q_EMIT beginRemoveRows(first.parent(), first.row(), last.row());
    }
}

void ItemFilterProxyModel::onRowsRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    const auto proxyRange = mapFromSourceRange(sourceParent, sourceFirst, sourceLast);
    for (const auto& [first, last] : proxyRange) {
        Q_ASSERT(first.isValid() && last.isValid());
        Q_ASSERT(first.parent() == last.parent());
        Q_ASSERT(first.model() == this && last.model() == this);
        
        // TODO

    }
    Q_EMIT endRemoveRows();
}

void ItemFilterProxyModel::onRowsAboutToBeInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
   // TODO
    Q_ASSERT(false);
}

void ItemFilterProxyModel::onRowsInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    // TODO
    Q_ASSERT(false);
}

void ItemFilterProxyModel::onRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent)
{
    // TODO
    Q_ASSERT(false);
}

void ItemFilterProxyModel::onRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent)
{
    // TODO
    Q_ASSERT(false);
}

void ItemFilterProxyModel::updateProxyIndexes()
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
                const auto proxyIndexInfo = appendIndex(sourceIndex, hiddenRootIndex);
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
                const auto proxyIndexInfo = appendIndex(sourceIndex, parentInfo);
                fillChildrenIndexesRecursively(sourceIndex, proxyIndexInfo);
            }
            else {
                fillChildrenIndexesRecursively(sourceIndex, parentInfo);
            }
        }
    }
}
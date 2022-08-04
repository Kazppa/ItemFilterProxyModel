#include "ItemFilterProxyModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QStack>

#include "ItemFilterProxyModelPrivate.h"

using namespace kaz;


ItemFilterProxyModel::ItemFilterProxyModel(QObject *parent) :
    QAbstractProxyModel(parent)
{
    m_impl = new ItemFilterProxyModelPrivate(this);
}

ItemFilterProxyModel::~ItemFilterProxyModel()
{
    delete m_impl;
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
        m_impl->resetProxyIndexes();
        endResetModel();
    });
    connect(newSourceModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &ItemFilterProxyModel::layoutAboutToBeChanged);
    connect(newSourceModel, &QAbstractItemModel::layoutChanged, this, [this]() {
        m_impl->resetProxyIndexes();
        layoutChanged();
    });
    connect(newSourceModel, &QAbstractItemModel::rowsInserted, this, &ItemFilterProxyModel::onRowsInserted);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ItemFilterProxyModel::onRowsAboutToBeRemoved);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeMoved, this, &ItemFilterProxyModel::onRowsAboutToBeMoved);

    m_impl->resetProxyIndexes();
    endResetModel();
}

QModelIndex ItemFilterProxyModel::index(int row,int column, const QModelIndex &parent) const 
{
    return m_impl->index(row, column, parent);
}

QModelIndex ItemFilterProxyModel::parent(const QModelIndex &childIndex) const
{
    return m_impl->parent(childIndex);
}

int ItemFilterProxyModel::rowCount(const QModelIndex &parentIndex) const
{
    return m_impl->rowCount(parentIndex);
}

int ItemFilterProxyModel::columnCount(const QModelIndex &parentIndex) const
{
    return m_impl->columnCount(parentIndex);
}

bool ItemFilterProxyModel::hasChildren(const QModelIndex &parentIndex) const
{
    return m_impl->hasChildren(parentIndex);
}

QModelIndex ItemFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!sourceModel()) {
        return {};
    }
    return m_impl->mapToSource(proxyIndex);
}

QModelIndex ItemFilterProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceModel()) {
        return {};
    }
    return m_impl->mapFromSource(sourceIndex);
}

void ItemFilterProxyModel::invalidateRowsFilter()
{
    // TODO implement this without a model reset : by deleting, inserting and moving rows around, just like onSourceDataChanged
    beginResetModel();
    m_impl->resetProxyIndexes();
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
        const auto isNewDataVisible = filterAcceptsRow(sourceRow, sourceParent);
        const auto proxyInfo = m_impl->m_sourceIndexHash.value(sourceIndex);
        if (isNewDataVisible) {
            if (proxyInfo) {
                // Proxy index is still valid and visible
                Q_EMIT dataChanged(proxyInfo->m_index, proxyInfo->m_index, roles);
                continue;
            }
            
            // Create a new proxy index because the source index became visible with this data changes
            const auto proxyParent = m_impl->getProxyParentIndex(sourceIndex);
            const auto [row, insertIt] = m_impl->searchInsertableRow(proxyParent, sourceIndex);
            beginInsertRows(proxyParent->m_index, row, row);
            const auto newProxyIndex = createIndex(row, sourceIndex.column(), sourceIndex.internalId());
            auto newProxyIndexInfo = std::make_shared<ProxyIndexInfo>(sourceIndex, newProxyIndex, proxyParent); 
            m_impl->m_sourceIndexHash[sourceIndex] = newProxyIndexInfo;
            m_impl->m_proxyIndexHash[newProxyIndex] = newProxyIndexInfo;
            proxyParent->m_children.insert(insertIt, newProxyIndexInfo);
            endInsertRows();

            const auto childProxyIndexes = m_impl->getProxyChildrenIndexes(sourceIndex);
            // TODO move children by "range" instead of one by one
            for (const auto& childProxyIndex : childProxyIndexes) {
                const auto sourceRow = childProxyIndex->row();
                const auto destRow = newProxyIndexInfo->rowCount();
                beginMoveRows(childProxyIndex->parentIndex(), sourceRow, sourceRow, newProxyIndex, destRow);
                m_impl->moveRowsImpl(childProxyIndex->m_parent, sourceRow, sourceRow, newProxyIndexInfo, destRow);
                endMoveRows();
            }
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
                    m_impl->moveRowsImpl(proxyInfo, 0, proxyLastRow, proxyInfo->m_parent, proxyRow + 1);
                    endMoveRows();
                }
                // Remove the proxy index
                beginRemoveRows(proxyInfo->parentIndex(), proxyRow, proxyRow);
                m_impl->eraseRowsImpl(proxyInfo->m_parent, proxyRow, proxyRow);
                endResetModel();
            }
        }
    }
}

void ItemFilterProxyModel::onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    const auto proxyRange = m_impl->mapFromSourceRange(sourceParent, sourceFirst, sourceLast);
    for (const auto& [firstInfo, lastInfo] : proxyRange) {
        const auto& first = firstInfo->m_index;
        const auto& last = lastInfo->m_index;
        Q_ASSERT(first.isValid() && last.isValid());
        Q_ASSERT(first.parent() == last.parent());
        Q_ASSERT(first.model() == this && last.model() == this);
        const auto firstRow = first.row();
        const auto lastRow = last.row();
        const auto& proxyParentInfo = firstInfo->m_parent;

        Q_EMIT beginRemoveRows(proxyParentInfo->m_index, firstRow, lastRow);
        m_impl->eraseRowsImpl(proxyParentInfo, firstRow, lastRow);
        Q_EMIT endRemoveRows();
    }
}

void ItemFilterProxyModel::onRowsInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    auto sourceModel = ItemFilterProxyModel::sourceModel();
    Q_ASSERT(sourceParent.model() == sourceModel);

    const auto sourceVisibleIndexes = m_impl->getSourceVisibleChildren(sourceParent, sourceFirst, sourceLast);
    const auto childrenCount = sourceVisibleIndexes.size();
    if (childrenCount == 0) {
        return;
    }

    const auto proxyParent = m_impl->getProxyNearestIndex(sourceParent);
    const auto columnCount = proxyParent->columnCount();
    auto [proxyRow, childIt] = m_impl->searchInsertableRow(proxyParent, sourceVisibleIndexes.front());

    beginInsertRows(proxyParent->m_index, proxyRow, proxyRow + childrenCount - 1);
    auto& parentChildren = proxyParent->m_children;

    {
        // Update previous indexes
        int newRow = proxyRow + childrenCount;
        const auto childrenEnd = parentChildren.end();
        for (auto it = childIt; it != childrenEnd; ++it) {
            auto& proxyInfo = *it;
            const auto previousIndex = proxyInfo->m_index;
            m_impl->m_proxyIndexHash.erase(previousIndex);

            proxyInfo->m_index = createIndex(newRow++, previousIndex.column(), previousIndex.internalId());
            m_impl->m_sourceIndexHash[proxyInfo->m_source] = proxyInfo;
            m_impl->m_proxyIndexHash.emplace(proxyInfo->m_index, proxyInfo);
        }
    }

    // Insert new rows
    const int dummyDist = std::distance(parentChildren.cbegin(), childIt);
    parentChildren.resize(parentChildren.size() + childrenCount * columnCount);
    auto proxyIt = parentChildren.begin() + dummyDist;
    // Move previous rows to the end of the vector, to free space for the new coming rows
    std::move_backward(proxyIt, proxyIt + childrenCount, parentChildren.end() - 1);

    for (const auto &idx : sourceVisibleIndexes) {
        for (int col = 0; col < columnCount; ++col) {
            const auto sourceIndex = sourceModel->index(idx.row(), col, idx.parent());
            auto proxyIndex = createIndex(proxyRow++, col, sourceIndex.internalId());
            auto proxyInfo = std::make_shared<ProxyIndexInfo>(sourceIndex, proxyIndex, proxyParent);
            m_impl->m_sourceIndexHash.emplace(sourceIndex, proxyInfo);
            m_impl->m_proxyIndexHash.emplace(proxyIndex, proxyInfo);
            *proxyIt = proxyInfo;
            ++proxyIt;

            // Insert visible source indexes aswell
            m_impl->fillChildrenIndexesRecursively(sourceIndex, proxyInfo);
        }
    }

    endInsertRows();
}

void ItemFilterProxyModel::onRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
    const QModelIndex &destinationParent, int destinationRow)
{
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    Q_ASSERT(sourceParent.model() == sourceModel);
    Q_ASSERT(destinationParent.model() == sourceModel);
    const auto destinationInfo = m_impl->getProxyNearestIndex(destinationParent);
    const auto destinationSourceIndex = sourceModel->index(destinationRow, 0, destinationParent);
    auto [destRow, _] = m_impl->searchInsertableRow(destinationInfo, destinationSourceIndex);

    const auto proxyIndexes = m_impl->mapFromSourceRange(sourceParent, sourceStart, sourceEnd, ItemFilterProxyModelPrivate::IncludeChildrenIfInvisible);
    for (const auto& [firstInfo, lastInfo] : proxyIndexes) {
        const auto& parentInfo = firstInfo->m_parent;
        const auto firstRow = firstInfo->row();
        const auto lastRow = lastInfo->row();
        Q_ASSERT(firstRow <= lastRow);

        beginMoveRows(parentInfo->m_index, firstRow, lastRow, destinationInfo->m_index, destRow);
        m_impl->moveRowsImpl(parentInfo, firstRow, lastRow, destinationInfo, destRow);
        endMoveRows();
        destRow += lastRow - firstRow;
    }
}

QModelIndex ItemFilterProxyModel::createIndexImpl(int row, int col, quintptr internalId) const
{
    return createIndex(row, col, internalId);
}
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
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this, &ItemFilterProxyModel::onRowsAboutToBeInserted);
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
        const auto isNewValueVisible = filterAcceptsRow(sourceRow, sourceParent);
        const auto proxyInfo = m_impl->m_sourceIndexHash.value(sourceIndex);
        if (isNewValueVisible) {
            if (proxyInfo) {
                // Proxy index is still valid and visible
                Q_EMIT dataChanged(proxyInfo->m_index, proxyInfo->m_index, roles);
                continue;
            }

            // Create a new proxy index because the source index became visible with this data update
            const auto proxyParent = m_impl->getProxyNearestParentIndex(sourceIndex);
            const auto [row, insertIt] = m_impl->searchInsertableRow(proxyParent, sourceIndex);
            beginInsertRows(proxyParent->m_index, row, row);
            const auto newProxyIndex = createIndex(row, sourceIndex.column(), sourceIndex.internalId());
            auto newProxyIndexInfo = std::make_shared<ProxyIndexInfo>(sourceIndex, newProxyIndex, proxyParent); 
            m_impl->m_sourceIndexHash[sourceIndex] = newProxyIndexInfo;
            m_impl->m_proxyIndexHash[newProxyIndex] = newProxyIndexInfo;
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
                    m_impl->moveRowsImpl(proxyInfo, 0, proxyLastRow, proxyInfo->m_parent, proxyRow + 1);
                    endMoveRows();
                }
                // Remove the proxy index
                beginRemoveRows(proxyInfo->parentIndex(), proxyRow, proxyRow);
                m_impl->eraseRows(proxyInfo->m_parent, proxyRow, proxyRow);
                endResetModel();
            }
        }
    }
}

void ItemFilterProxyModel::onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    const auto proxyRange = m_impl->mapFromSourceRange(sourceParent, sourceFirst, sourceLast);
    for (const auto& [first, last] : proxyRange) {
        Q_ASSERT(first.isValid() && last.isValid());
        Q_ASSERT(first.parent() == last.parent());
        Q_ASSERT(first.model() == this && last.model() == this);
        const auto proxyParent = first.parent();
        const auto firstRow = first.row();
        const auto lastRow = last.row();
        auto proxyParentInfo = m_impl->m_proxyIndexHash.at(proxyParent);

        Q_EMIT beginRemoveRows(proxyParent, firstRow, lastRow);
        m_impl->eraseRows(proxyParentInfo, firstRow, lastRow);
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

#include "ItemFilterProxyModel.h"

#include <QtCore/QDateTime>

ItemFilterProxyModel::ProxyIndexInfo::ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyIndex, const QModelIndex &proxyParentIndex) :
    m_source(sourceIndex),
    m_index(proxyIndex),
    m_parent(proxyParentIndex)
{

}

QModelIndexList::const_iterator ItemFilterProxyModel::ProxyIndexInfo::lowerBoundSourceRow(int sourceRow) const
{
    return std::lower_bound(m_children.begin(), m_children.end(), sourceRow, [](const QModelIndex &idx, int sourceRow) {
        return false; // TODO idx.row() < row;
    });
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
        refreshProxyIndexes();
        endResetModel();    // associated beginResetModel() called in the above connect
    });
    connect(newSourceModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &ItemFilterProxyModel::layoutAboutToBeChanged);
    connect(newSourceModel, &QAbstractItemModel::layoutChanged, this, [this]() {
        refreshProxyIndexes();
        layoutChanged();    // associated beginResetModel() called in the above connect
    });
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this, &ItemFilterProxyModel::onRowsAboutToBeInserted);
    connect(newSourceModel, &QAbstractItemModel::rowsInserted, this, &ItemFilterProxyModel::onRowsInserted);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ItemFilterProxyModel::onRowsAboutToBeRemoved);
    connect(newSourceModel, &QAbstractItemModel::rowsRemoved, this, &ItemFilterProxyModel::rowsRemoved);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeMoved, this, &ItemFilterProxyModel::rowsAboutToBeMoved);
    connect(newSourceModel, &QAbstractItemModel::rowsMoved, this, &ItemFilterProxyModel::rowsMoved);

    refreshProxyIndexes();
    endResetModel();
}

QModelIndex ItemFilterProxyModel::index(int row,int column, const QModelIndex &parent) const 
{
    const auto it = m_proxyIndexHash.find(parent);
    if (it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
        return {};
    }

    return it->second->m_children.at(row);
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
    return it->second->m_parent;
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
    return (int) it->second->m_children.size();
}

int ItemFilterProxyModel::columnCount(const QModelIndex &parentIndex) const
{
    if (!sourceModel()) {
        return 0;
    }
    const auto sourceIndex = mapToSource(parentIndex);
    Q_ASSERT(sourceIndex.isValid() || !parentIndex.isValid());
    return sourceModel()->columnCount(sourceIndex);
}

bool ItemFilterProxyModel::hasChildren(const QModelIndex &parentIndex) const
{
    const auto it = m_proxyIndexHash.find(parentIndex);
    if (it == m_proxyIndexHash.end()) {
        return false;
    }
    return !it->second->m_children.isEmpty();
}

QModelIndex ItemFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
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
    if (!sourceIndex.isValid()) {
        return {};
    }
    Q_ASSERT(sourceIndex.model() == sourceModel());
    return m_sourceIndexHash.value(sourceIndex);
}

std::pair<QModelIndex, QModelIndex> ItemFilterProxyModel::mapToSourceRange(const QModelIndex &sourceLeft, const QModelIndex& sourceRight) const
{
    Q_ASSERT(sourceLeft.isValid() && sourceRight.isValid());
    Q_ASSERT(sourceLeft.parent() == sourceRight.parent());
 
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    Q_ASSERT(sourceLeft.model() == sourceModel);
    Q_ASSERT(sourceRight.model() == sourceModel);

    const auto sourceParent = sourceLeft.parent();
    const auto topRow = sourceLeft.row();
    const auto bottomRow = sourceRight.row();
    const auto sourceColumn = sourceLeft.column();

    auto proxyLeft = mapFromSource(sourceLeft);
    auto proxyRight = mapFromSource(sourceRight);
    const auto isBaseProxyRightValid = proxyRight.isValid();

    if (proxyLeft.isValid() && isBaseProxyRightValid) {
        return std::make_pair(proxyLeft, proxyRight);
    }

    for (int row = topRow + 1; row < bottomRow; ++row) {
        const auto sourceIndex = sourceModel->index(row, sourceColumn, sourceParent);
        const auto proxyIndex = mapFromSource(sourceIndex);
        if (!proxyIndex.isValid()) {
            // Hidden index
            continue;
        }

        if (!proxyLeft.isValid()) {
            if (isBaseProxyRightValid) {
                // Range already found
                return std::make_pair(proxyIndex, proxyRight);
            }
            proxyLeft = proxyIndex;
        }

        if (!isBaseProxyRightValid) {
            proxyRight = proxyIndex;
        }
    }

    if (!proxyLeft.isValid()) {
        return std::make_pair(proxyRight, proxyRight);
    }
    else if (!proxyRight.isValid()) {
        return std::make_pair(proxyLeft, proxyLeft);
    }
    else {
        return std::make_pair(proxyLeft, proxyRight);
    }
}

void ItemFilterProxyModel::sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles)
{
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        return;
    }

    const auto proxyRange = mapToSourceRange(sourceLeft, sourceRight);
    if (proxyRange.first.isValid()) {
        Q_EMIT dataChanged(proxyRange.first, proxyRange.second, roles);
    }
}

QModelIndex ItemFilterProxyModel::getProxyNearestParentIndex(const QModelIndex &sourceIndex) const
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(sourceIndex.model() == this);

    QModelIndex matchingProxyParentIndex;
    auto sourceParentIndex = sourceIndex.parent();
    do {
        matchingProxyParentIndex = mapFromSource(sourceParentIndex);
    }
    while (!matchingProxyParentIndex.isValid() && (sourceParentIndex = sourceParentIndex.parent()).isValid());
    return matchingProxyParentIndex;
}

void ItemFilterProxyModel::onRowsAboutToBeInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    auto model = sourceModel();
    const auto column = sourceParent.column() >= 0 ? sourceParent.column() : 0;
    const auto sourceFirstIndex = model->index(sourceFirst, column, sourceParent);
    const auto sourceLastIndex = model->index(sourceLast, column, sourceParent);
    auto proxyFirstIndex = mapFromSource(sourceFirstIndex);
    if (!proxyFirstIndex.isValid()) {
        const auto proxyParent = getProxyNearestParentIndex(sourceFirstIndex);
        if (!proxyParent.isValid()) {
            return;
        }
        // TODO
    }
    auto proxyLastIndex = mapFromSource(sourceLastIndex);
    if (!proxyLastIndex.isValid()) {
        // TODO
    }
    beginInsertRows(proxyFirstIndex.parent(), proxyFirstIndex.row(), proxyLastIndex.row());
}

void ItemFilterProxyModel::onRowsInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    // TODO
    Q_ASSERT(false);
}

void ItemFilterProxyModel::onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
{
    // TODO
    Q_ASSERT(false);
}

void ItemFilterProxyModel::onRowsRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast)
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

void ItemFilterProxyModel::refreshProxyIndexes()
{
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    Q_ASSERT(sourceModel);
    m_sourceIndexHash.clear();
    m_proxyIndexHash.clear();

    // Parent of all root nodes
    auto hiddenRootIndex = new ProxyIndexInfo(QModelIndex(), QModelIndex());
    m_proxyIndexHash.emplace(QModelIndex(), hiddenRootIndex);
    const auto rootRowCount = sourceModel->rowCount();
    const auto rootColumnCount = sourceModel->columnCount();
    for (int sourceRow = 0; sourceRow < rootRowCount; ++sourceRow) {
        const auto isRootNodeVisible = filterAcceptsRow(sourceRow, QModelIndex());
        for (int col = 0; col < rootColumnCount; ++col) {
            const auto sourceIndex = sourceModel->index(sourceRow, col);
            Q_ASSERT(sourceIndex.isValid());

            if (isRootNodeVisible) {
                const auto proxyRow = hiddenRootIndex->m_children.size();
                const auto proxyIndex = createIndex(proxyRow, col, sourceIndex.internalId());
                m_sourceIndexHash.emplace(sourceIndex, proxyIndex);
                hiddenRootIndex->m_children.push_back(proxyIndex);
                auto proxyIndexInfo = new ProxyIndexInfo(sourceIndex, proxyIndex);
                m_proxyIndexHash.emplace(proxyIndex, proxyIndexInfo);

                fillChildrenIndexes(sourceIndex, proxyIndex, proxyIndexInfo);
            }
            else {
                fillChildrenIndexes(sourceIndex, QModelIndex(), hiddenRootIndex);
            }
        }
    }
}

void ItemFilterProxyModel::fillChildrenIndexes(const QModelIndex &sourceParent, const QModelIndex &proxyParent, ProxyIndexInfo* parentInfo)
{
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    const auto rowCount = sourceModel->rowCount(sourceParent);
    const auto columnCount = sourceModel->columnCount(sourceParent);

    for (auto sourceRow = 0; sourceRow < rowCount; ++sourceRow) {
        const auto isChildVisible = filterAcceptsRow(sourceRow, sourceParent);
        for (auto col = 0; col < columnCount; ++col) {
            const auto sourceIndex = sourceModel->index(sourceRow, col, sourceParent);
            Q_ASSERT(sourceIndex.isValid());

            if (isChildVisible) {
                const auto proxyRow = parentInfo->m_children.size();
                const auto proxyIndex = createIndex(proxyRow, col, sourceIndex.internalId());
                m_sourceIndexHash.emplace(sourceIndex, proxyIndex);
                parentInfo->m_children.push_back(proxyIndex);

                auto proxyIndexInfo = new ProxyIndexInfo(sourceIndex, proxyIndex, proxyParent);
                m_proxyIndexHash.emplace(proxyIndex, proxyIndexInfo);
                fillChildrenIndexes(sourceIndex, proxyIndex, proxyIndexInfo);
            }
            else {
                fillChildrenIndexes(sourceIndex, proxyParent, parentInfo);
            }
        }
    }
}
#include "ItemFilterProxyModel.h"


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

    connect(newSourceModel, &QAbstractItemModel::dataChanged, this, &ItemFilterProxyModel::sourceDataChanged);
    connect(newSourceModel, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
        beginResetModel();
        m_sourceIndexHash.clear();
        m_proxyIndexHash.clear();
    });
    connect(newSourceModel, &QAbstractItemModel::modelReset, this, &ItemFilterProxyModel::endResetModel);
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this, [this](const QModelIndex& sourceParent, int sourceRow, int sourceLast) {
        // TODO
        Q_ASSERT(false);
    });
    connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex& sourceParent, int sourceRow, int sourceLast) {
        // TODO
        Q_ASSERT(false);
    });
        connect(newSourceModel, &QAbstractItemModel::rowsAboutToBeMoved, this, [this](const QModelIndex& sourceParent, int sourceRow, int sourceLast) {
        // TODO
        Q_ASSERT(false);
    });
    endResetModel();

}

QModelIndex ItemFilterProxyModel::index(int row,int column, const QModelIndex &parent) const 
{
    // TODO
     return {};  
}

QModelIndex ItemFilterProxyModel::parent(const QModelIndex &childIndex) const
{
    if (!childIndex.isValid()) {
        return {};
    }
    
    const auto& indexInfos = getProxyIndexInfo(childIndex);
    return indexInfos._parent;
}

int ItemFilterProxyModel::rowCount(const QModelIndex &parentIndex) const
{
    if (!parentIndex.isValid()) {
        auto sourceModel = ItemFilterProxyModel::sourceModel();
        const auto sourceRowCount = sourceModel->rowCount();
        for (int i = 0; i < sourceRowCount; ++i) {
            const auto sourceIndex = sourceModel->index(i, 0);
            const auto proxyIndex = mapFromSource(sourceIndex);
        }
    }
    return {};
}

int ItemFilterProxyModel::columnCount(const QModelIndex &parentIndex) const
{
    if (!parentIndex.isValid()) {
        return sourceModel()->rowCount();
    }
    const auto sourceIndex = mapToSource(parentIndex);
    return sourceModel()->columnCount(sourceIndex);
}

QModelIndex ItemFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
        return {};
    }
    Q_ASSERT(proxyIndex.model() == this);

    return m_proxyIndexHash.value(proxyIndex)._sourceIndex;
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
    Q_ASSERT(sourceLeft.model() == sourceModel());
    Q_ASSERT(sourceRight.model() == sourceModel());

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

    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    for (int i = topRow + 1; i < bottomRow; ++i) {
        const auto sourceIndex = sourceModel->index(i, sourceColumn, sourceParent);
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

const ItemFilterProxyModel::ModelIndexInfo& ItemFilterProxyModel::getProxyIndexInfo(const QModelIndex &proxyIndex) const
{
    Q_ASSERT(proxyIndex.model() == this);
    Q_ASSERT(proxyIndex.isValid());
    auto it = m_proxyIndexHash.find(proxyIndex);
    if (it == m_proxyIndexHash.end() || !it.key().isValid()) {
        auto sourceIndex = mapToSource(proxyIndex);
        auto proxyParent = findProxyParentIndex(proxyIndex);
        const auto isVisible = filterAcceptsRow(sourceIndex.row(), sourceIndex.parent());
        it = m_proxyIndexHash.insert(proxyIndex, { std::move(proxyParent), findProxyChildrenIndexes(proxyIndex), isVisible });
    }

    return it.value();
}

QModelIndex ItemFilterProxyModel::findProxyParentIndex(const QModelIndex &proxyIndex) const
{
    // TODO
    return {};
}

QVector<QPersistentModelIndex> ItemFilterProxyModel::findProxyChildrenIndexes(const QModelIndex &proxyIndex) const
{
    // TODO
    return {};
}
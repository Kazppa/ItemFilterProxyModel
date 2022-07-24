#include "ItemFilterProxyModel.h"

#include <QtCore/QStack>
#include <QtCore/QDateTime>


ItemFilterProxyModel::ProxyIndex::ProxyIndex(const QModelIndex& sourceIndex, const QModelIndex &index,
    const QModelIndex& parent, QModelIndexList children) :
    m_source(sourceIndex),
    m_index(index),
    m_parent(parent),
    m_children(std::move(children))
{
    Q_ASSERT((!index.isValid() || !parent.isValid()) || index.model() == parent.model());
    Q_ASSERT(!sourceIndex.isValid() || (sourceIndex.model() != index.model()));
}

///////////////////////////////

 ItemFilterProxyModel::ItemFilterProxyModel(QObject *parent) :
    QAbstractProxyModel(parent)
{
}

void ItemFilterProxyModel::setSourceModel(QAbstractItemModel *newSourceModel)
{
    auto previousModel = sourceModel();
    if (newSourceModel == previousModel) {
        return;
    }

    beginResetModel();
    if (previousModel) {
        disconnect(previousModel, nullptr, this, nullptr);
    }
    QAbstractProxyModel::setSourceModel(newSourceModel);
    disconnect(newSourceModel, nullptr, this, nullptr);

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
    refreshProxyIndexes();
    endResetModel();
}

QVariant ItemFilterProxyModel::data(const QModelIndex &idx, int role) const 
{
    if (!idx.isValid()) {
        return {};
    }
    Q_ASSERT(idx.model() == this);
    return sourceModel()->data(mapToSource(idx), role);
}

void ItemFilterProxyModel::multiData(const QModelIndex& idx, QModelRoleDataSpan roleDataSpan) const 
{
    Q_ASSERT(idx.isValid());
    Q_ASSERT(idx.model() == this);
    return sourceModel()->multiData(mapToSource(idx), roleDataSpan);
}

QModelIndex ItemFilterProxyModel::index(int row,int column, const QModelIndex &parentIndex) const 
{
    Q_ASSERT(row >= 0);
    Q_ASSERT(!parentIndex.isValid() || parentIndex.model() == this);
    if (parentIndex.isValid()) {
        const auto begin = m_indexes.begin();
        const auto end = m_indexes.end();
        const auto parentIt = std::find_if(begin, end, [&](const auto &modelIndex) {
            return modelIndex->m_index == parentIndex;
        });
        if (parentIt == end) {
            Q_ASSERT(false);
            return {};
        }
        Q_ASSERT(row >= 0 && row < (*parentIt)->m_children.size());
        return (*parentIt)->m_children.at(row);
        
    }
    else {
        const QModelIndex parent;
        int currentRow = 0;
        for (const auto& modelIndex : m_indexes) {
            if (modelIndex->m_parent != parent) {
                continue;
            }
            if (currentRow++ == row) {
                return modelIndex->m_index;
            }
        }
    }

    Q_ASSERT(false);
    return {};
}

QModelIndex ItemFilterProxyModel::parent(const QModelIndex &childIndex) const
{
    if (!childIndex.isValid()) {
        return {};
    }
    Q_ASSERT(childIndex.model() == this);

    const auto it = std::find_if(m_indexes.cbegin(), m_indexes.cend(), [&](const auto& modelIndex) {
        return modelIndex->m_index == childIndex;
    });
    if (it == m_indexes.end()) {
        return {};
    }
    return (*it)->m_parent;
}

int ItemFilterProxyModel::rowCount(const QModelIndex &parentIndex) const
{
    if (!sourceModel()) {
        return 0;
    }
    Q_ASSERT(!parentIndex.isValid() || parentIndex.model() == this);
    
    if (parentIndex.isValid()) {
        const auto it = std::find_if(m_indexes.cbegin(), m_indexes.cend(), [&](const auto& proxyIndex) {
            return proxyIndex->m_index == parentIndex;
        });
        return it == m_indexes.end() ? 0 : (*it)->m_children.size();
    }
    else {
        return std::count_if(m_indexes.cbegin(), m_indexes.cend(), [parent = QModelIndex()](const auto& proxyIndex) {
            return proxyIndex->m_parent == parent;
        });
    }
}

int ItemFilterProxyModel::columnCount(const QModelIndex &parentIndex) const
{
    auto baseModel = sourceModel();
    if (!baseModel) {
        return 0;
    }
    Q_ASSERT(!parentIndex.isValid() || parentIndex.model() == this);
    
    const auto sourceIndex = mapToSource(parentIndex);
    return baseModel->columnCount(sourceIndex);
}

QModelIndex ItemFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
        return {};
    }
    Q_ASSERT(proxyIndex.model() == this);
    static int counter = 0;
    ++counter;
    const auto it = std::find_if(m_indexes.begin(), m_indexes.end(), [&](const auto& modelIndex) {
        return modelIndex->m_index == proxyIndex;
    });
    if (it == m_indexes.end()) {
        return {};
    }
    return (*it)->m_source;
}

QModelIndex ItemFilterProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid()) {
        return {};
    }
    Q_ASSERT(sourceIndex.model() == sourceModel());
    
    const auto it = std::find_if(m_indexes.begin(), m_indexes.end(), [&](const auto& modelIndex) {
        return modelIndex->m_source == sourceIndex;
    });
    if (it == m_indexes.end()) {
        return {};
    }
    return (*it)->m_index;
}

void ItemFilterProxyModel::sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles)
{
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        return;
    }

    // TODO
}

void ItemFilterProxyModel::refreshProxyIndexes()
{
    m_indexes.clear();
    m_indexes.reserve(200);
    m_idCount = 0;

    auto baseModel = sourceModel();
    Q_ASSERT(baseModel);
    QStack<ProxyIndex *> stack;

    for (int rowCount = baseModel->rowCount(), row = 0; row < rowCount; ++row) {
        const auto columnCount = baseModel->columnCount();
        for (auto col = 0; col < columnCount; ++col) {
            const auto sourceIndex = baseModel->index(row, col);
            m_indexes.push_back(std::make_unique<ProxyIndex>(sourceIndex, createIndex(row, col, m_idCount++)));
            stack.push(m_indexes.back().get());
        }
    }

    while (!stack.isEmpty()) {
        auto& parent = *stack.pop();
        const auto rowCount = baseModel->rowCount(parent.m_source);
        const auto columnCount = baseModel->columnCount(parent.m_source);

        for (auto row = 0; row < rowCount; ++row) {
            for (auto col = 0; col < columnCount; ++col) {
                const auto currentIndex = createIndex(row, col, m_idCount++);
                const auto sourceIndex = baseModel->index(row, col, parent.m_source);
                m_indexes.push_back(std::make_unique<ProxyIndex>(sourceIndex, currentIndex, parent.m_index));
                parent.m_children.push_back(currentIndex);
                stack.push(m_indexes.back().get());
            }
        }
    }
}

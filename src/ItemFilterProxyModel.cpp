#include "ItemFilterProxyModel.h"

#include <QtCore/QStack>
#include <QtCore/QDateTime>

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

QModelIndex ItemFilterProxyModel::index(int row,int column, const QModelIndex &parent) const 
{
     return createIndex(row, column);  
}

QModelIndex ItemFilterProxyModel::parent(const QModelIndex &childIndex) const
{
    if (!childIndex.isValid()) {
        return {};
    }

    const auto it = m_proxyIndexHash.find(childIndex);
    if(it == m_proxyIndexHash.end()) {
        return {};
    }
    return it->_parent;
}

int ItemFilterProxyModel::rowCount(const QModelIndex &parentIndex) const
{
    if (!sourceModel()) {
        return 0;
    }
    // Replace invalid index by default one
    const auto it = m_proxyChildrenHash.find(parentIndex.isValid() ? parentIndex : QModelIndex());
    return it == m_proxyChildrenHash.end() ? 0 : it.value().size();
}

int ItemFilterProxyModel::columnCount(const QModelIndex &parentIndex) const
{
    if (!sourceModel()) {
        return 0;
    }
    const auto sourceIndex = mapToSource(parentIndex);
    // Q_ASSERT(sourceIndex.isValid() || !parentIndex.isValid());
    return sourceModel()->columnCount(sourceIndex);
}

QModelIndex ItemFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
        return {};
    }
    Q_ASSERT(proxyIndex.model() == this);

    const auto it = m_proxyIndexHash.find(proxyIndex);
    if (it == m_proxyIndexHash.end()) {
        for (auto it = m_proxyIndexHash.begin(); it != m_proxyIndexHash.end(); ++it) {
            qDebug() << it.key();
        }
        return {};
    }
    return it->_sourceIndex;        
}

QModelIndex ItemFilterProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid()) {
        return {};
    }
    Q_ASSERT(sourceIndex.model() == sourceModel());

    const auto it = m_sourceIndexHash.find(sourceIndex);
    if (it == m_sourceIndexHash.end()) {
        return {};
    }
    return it.value();
}

bool ItemFilterProxyModel::lessThan(const QModelIndex &leftIndex, const QModelIndex &rightIndex) const
{
    const auto left = leftIndex.data(Qt::DisplayRole);
    const auto right = rightIndex.data(Qt::DisplayRole);

    if (left.userType() == QVariant::Invalid)
        return false;
    if (right.userType() == QVariant::Invalid)
        return true;
    switch (left.userType()) {
    case QVariant::Int:
        return left.toInt() < right.toInt();
    case QVariant::UInt:
        return left.toUInt() < right.toUInt();
    case QVariant::LongLong:
        return left.toLongLong() < right.toLongLong();
    case QVariant::ULongLong:
        return left.toULongLong() < right.toULongLong();
    case QMetaType::Float:
        return left.toFloat() < right.toFloat();
    case QVariant::Double:
        return left.toDouble() < right.toDouble();
    case QVariant::Char:
        return left.toChar() < right.toChar();
    case QVariant::Date:
        return left.toDate() < right.toDate();
    case QVariant::Time:
        return left.toTime() < right.toTime();
    case QVariant::DateTime:
        return left.toDateTime() < right.toDateTime();

    case QVariant::String:
    default:
        return left.toString().compare(right.toString()) < 0;
    }
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

namespace
{
    struct ModelIndexDetails
    { 
        QPersistentModelIndex _proxyParent;
        QPersistentModelIndex _sourceParent;
    };
}

void ItemFilterProxyModel::refreshProxyIndexes() const
{
    m_sourceIndexHash.clear();
    m_proxyIndexHash.clear();
    m_proxyChildrenHash.clear();
    
    QStack<ModelIndexDetails> stack;
    const auto sourceModel = ItemFilterProxyModel::sourceModel();
    const auto rootRowCount = sourceModel->rowCount();
    const auto rootColumnCount = sourceModel->columnCount();
    for (int row = 0; row < rootRowCount; ++row) {
        const auto isRootNodeVisible = filterAcceptsRow(row);
        for (int col = 0; col < rootColumnCount; ++col) {
            const auto sourceIndex = sourceModel->index(row, col);
            Q_ASSERT(sourceIndex.isValid());
            const auto proxyIndex = isRootNodeVisible ? createIndex(row, col) : QModelIndex();
            if (isRootNodeVisible) {
                m_sourceIndexHash.insert(sourceIndex, proxyIndex);
                m_proxyIndexHash.emplace(proxyIndex, sourceIndex, QModelIndex());
                m_proxyChildrenHash[QModelIndex()].push_back(proxyIndex);
            }
            stack.push({ proxyIndex, sourceIndex });
        }
    }

    while (!stack.isEmpty()) {
        const auto [ proxyParent, sourceParent ] = stack.pop();
        const auto childCount = sourceModel->rowCount(sourceParent);
        const auto columnCount = sourceModel->columnCount(sourceParent);
        int filterRow = 0;
        for (auto childSourceRow = 0; childSourceRow < childCount; ++childSourceRow) {
            const auto isChildVisible = filterAcceptsRow(childSourceRow, sourceParent);
            for (auto childSourceCol = 0; childSourceCol < columnCount; ++childSourceCol) {
                const auto childSourceIndex = sourceModel->index(childSourceRow, childSourceCol, sourceParent);
                Q_ASSERT(childSourceIndex.isValid());

                if (isChildVisible) {
                    const auto childProxyIndex = createIndex(filterRow++, childSourceCol);
                    m_sourceIndexHash.insert(childSourceIndex, childProxyIndex);
                    m_proxyIndexHash.insert(childProxyIndex, { childSourceIndex, proxyParent });
                    m_proxyChildrenHash[proxyParent].push_back(childProxyIndex);
                    stack.push({ childProxyIndex, childSourceIndex });
                }
                else {
                    stack.push({ proxyParent, childSourceIndex });
                }
            }
        }
    }
}

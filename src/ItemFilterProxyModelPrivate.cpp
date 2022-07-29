#include "ItemFilterProxyModelPrivate.h"

using namespace kaz;

ItemFilterProxyModelPrivate::ItemFilterProxyModelPrivate(ItemFilterProxyModel *proxyModel) :
    m_q(proxyModel)
{
}


QModelIndex ItemFilterProxyModelPrivate::index(int row,int column, const QModelIndex &parent) const 
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

QModelIndex ItemFilterProxyModelPrivate::parent(const QModelIndex &childIndex) const
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

int ItemFilterProxyModelPrivate::rowCount(const QModelIndex &parentIndex) const
{
    const auto it = m_proxyIndexHash.find(parentIndex);
    if (it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
        return 0;
    }
    return it->second->rowCount();
}

int ItemFilterProxyModelPrivate::columnCount(const QModelIndex &parentIndex) const
{
    const auto sourceModel = m_q->sourceModel();
    if (!sourceModel) {
        return 0;
    }

    const auto sourceIndex = mapToSource(parentIndex);
    Q_ASSERT(sourceIndex.isValid() || !parentIndex.isValid());
    return sourceModel->columnCount(sourceIndex);
}

bool ItemFilterProxyModelPrivate::hasChildren(const QModelIndex &parentIndex) const
{
    const auto it = m_proxyIndexHash.find(parentIndex);
    if (it == m_proxyIndexHash.end()) {
        return false;
    }
    return !it->second->m_children.empty();
}

QModelIndex ItemFilterProxyModelPrivate::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
        return {};
    }
    
    Q_ASSERT(proxyIndex.model() == m_q);
    const auto it = m_proxyIndexHash.find(proxyIndex);
    if (it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
        return {};
    }
    return it->second->m_source;
}

QModelIndex ItemFilterProxyModelPrivate::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid()) {
        return {};
    }
    
    Q_ASSERT(sourceIndex.model() == m_q->sourceModel());
    const auto it = m_sourceIndexHash.find(sourceIndex);
    return it == m_sourceIndexHash.end() ? QModelIndex() : (*it)->m_index;
}

std::vector<std::pair<QModelIndex, QModelIndex>> ItemFilterProxyModelPrivate::mapFromSourceRange(const QModelIndex &sourceLeft,
    const QModelIndex& sourceRight, const SelectionParameters parameters) const
{
    const auto sourceModel = m_q->sourceModel();
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

std::vector<std::pair<QModelIndex, QModelIndex>> ItemFilterProxyModelPrivate::mapFromSourceRange(
        const QModelIndex& sourceParent, int sourceFirst, int sourceLast, const SelectionParameters parameters) const
{
    const auto sourceModel = m_q->sourceModel();
    const auto column = sourceParent.column() >= 0 ? sourceParent.column() : 0;
    const auto sourceLeft = sourceModel->index(sourceFirst, column, sourceParent);
    const auto sourceRight = sourceModel->index(sourceLast, column, sourceParent);
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        return {};
    }
    return mapFromSourceRange(sourceLeft, sourceRight, parameters);
}

QModelIndexList ItemFilterProxyModelPrivate::getProxyChildrenIndexes(const QModelIndex &sourceIndex) const
{
    const auto sourceModel = m_q->sourceModel();
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

std::shared_ptr<ProxyIndexInfo> ItemFilterProxyModelPrivate::getProxyNearestParentIndex(const QModelIndex &sourceIndex) const
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(sourceIndex.model() == m_q->sourceModel());
    

    auto sourceParentIndex = sourceIndex.parent();
    QModelIndex matchingProxyParentIndex = m_q->mapFromSource(sourceParentIndex);
    while (!matchingProxyParentIndex.isValid()) {
        sourceParentIndex = sourceParentIndex.parent();
        matchingProxyParentIndex = m_q->mapFromSource(sourceParentIndex);
        if (!sourceParentIndex.isValid()) {
            break;
        }
    }
    return m_proxyIndexHash.at(matchingProxyParentIndex);
}

std::vector<std::shared_ptr<ProxyIndexInfo>> ItemFilterProxyModelPrivate::getProxyNearestChildrenIndexes(const QModelIndex &parentIndex) const
{
    const auto sourceModel = m_q->sourceModel();
    const auto column = parentIndex.column() >= 0 ? parentIndex.column() : 0;
    const auto sourceMappingEnd = m_sourceIndexHash.end();

    std::vector<std::shared_ptr<ProxyIndexInfo>> visibleChildren;
    const auto childrenCount = sourceModel->rowCount(parentIndex);
    for (int row = 0; row < childrenCount; ++row) {
        const auto childSourceIndex = sourceModel->index(row, column, parentIndex);
        const auto it = m_sourceIndexHash.find(childSourceIndex);
        if (it == sourceMappingEnd) {
            // Child is not visible, search recursively in child's children for any visible index
            auto recursiveVisibleChildren = getProxyNearestChildrenIndexes(childSourceIndex);
            if (recursiveVisibleChildren.empty()) {
                continue;
            }
            if (visibleChildren.empty()) {
                visibleChildren = std::move(recursiveVisibleChildren);
            }
            else {
                visibleChildren.insert(visibleChildren.end(), recursiveVisibleChildren.begin(), recursiveVisibleChildren.end());
            }
        }
        else {
            // Child is visible
            visibleChildren.push_back(it.value());
        }
    } 
    return visibleChildren;
}

void ItemFilterProxyModelPrivate::moveRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow,
        const std::shared_ptr<ProxyIndexInfo>& destinationInfo, int destinationRow)
{
    Q_ASSERT(parentProxyInfo);
    Q_ASSERT(firstRow >= 0 && firstRow <= lastRow);
    Q_ASSERT(destinationRow >= 0);
    Q_ASSERT(destinationInfo);
    auto& source = parentProxyInfo->m_children;
    auto& destination = destinationInfo->m_children;

    const auto columnCount = parentProxyInfo->columnCount();
    for(int col = 0; col < columnCount; ++col) {
        const auto sourceIt = parentProxyInfo->getColumnBegin(col);
        const auto destIt = destination.begin() + destinationRow;
    } 
}

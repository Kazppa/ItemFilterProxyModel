#include "ItemFilterProxyModelPrivate.h"

#include <QStack>

using namespace kaz;

QStack<QModelIndex>& SourceModelIndexLessComparator::getSourceIndexParents(const QModelIndex &sourceIndex) const
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

bool SourceModelIndexLessComparator::operator()(const QModelIndex& left, const QModelIndex &right) const
{
    Q_ASSERT(left.isValid() && right.isValid());
    Q_ASSERT(left.column() == right.column());
    if (left.parent() == right.parent()) {
        return left.row() < right.row();
    }

    const auto& leftParents = getSourceIndexParents(left);
    const auto& rightParents = getSourceIndexParents(right);
    
    const auto it = std::mismatch(leftParents.rbegin(), leftParents.rend(), rightParents.rbegin(), rightParents.rend());
    const auto firstText = it.first->data().toString();
    const auto dummy = it.first->model()->index(0, left.column(), *it.second);
    const auto txt = dummy.data().toString();
    Q_ASSERT(it.first->parent() == it .second->parent());
    return it.first->row() < it.second->row();
}

bool SourceModelIndexLessComparator::operator()(const QModelIndex &left, const std::shared_ptr<ProxyIndexInfo>& right) const
{
    return this->operator()(left, right->m_source);
}

bool SourceModelIndexLessComparator::operator()(const std::shared_ptr<ProxyIndexInfo>& left, const QModelIndex &right) const
{
    return this->operator()(left->m_source, right);
}

/////////////////////


ItemFilterProxyModelPrivate::ItemFilterProxyModelPrivate(ItemFilterProxyModel *proxyModel) :
    m_proxyModel(proxyModel)
{
}

QModelIndex ItemFilterProxyModelPrivate::ItemFilterProxyModelPrivate::createIndex(int row, int col, quintptr internalId) const
{
    return m_proxyModel->createIndex(row, col, internalId);
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
    const auto sourceModel = m_proxyModel->sourceModel();
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
    
    Q_ASSERT(proxyIndex.model() == m_proxyModel);
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
    
    Q_ASSERT(sourceIndex.model() == m_proxyModel->sourceModel());
    const auto it = m_sourceIndexHash.find(sourceIndex);
    return it == m_sourceIndexHash.end() ? QModelIndex() : (*it)->m_index;
}

std::vector<std::pair<QModelIndex, QModelIndex>> ItemFilterProxyModelPrivate::mapFromSourceRange(const QModelIndex &sourceLeft,
    const QModelIndex& sourceRight, const SelectionParameters parameters) const
{
    const auto sourceModel = m_proxyModel->sourceModel();
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
            const auto proxyIndexes = getProxyNearestChildrenIndexes(sourceIndex);
            for (const auto& proxyIndexInfo : proxyIndexes) {
                appendIndexToRange(proxyIndexInfo->m_index);
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
    const auto sourceModel = m_proxyModel->sourceModel();
    const auto column = sourceParent.column() >= 0 ? sourceParent.column() : 0;
    const auto sourceLeft = sourceModel->index(sourceFirst, column, sourceParent);
    const auto sourceRight = sourceModel->index(sourceLast, column, sourceParent);
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        return {};
    }
    return mapFromSourceRange(sourceLeft, sourceRight, parameters);
}

std::shared_ptr<ProxyIndexInfo> ItemFilterProxyModelPrivate::getProxyNearestParentIndex(const QModelIndex &sourceIndex) const
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(sourceIndex.model() == m_proxyModel->sourceModel());
    

    auto sourceParentIndex = sourceIndex.parent();
    QModelIndex matchingProxyParentIndex = m_proxyModel->mapFromSource(sourceParentIndex);
    while (!matchingProxyParentIndex.isValid()) {
        sourceParentIndex = sourceParentIndex.parent();
        matchingProxyParentIndex = m_proxyModel->mapFromSource(sourceParentIndex);
        if (!sourceParentIndex.isValid()) {
            break;
        }
    }
    return m_proxyIndexHash.at(matchingProxyParentIndex);
}

std::vector<std::shared_ptr<ProxyIndexInfo>> ItemFilterProxyModelPrivate::getProxyNearestChildrenIndexes(const QModelIndex &parentIndex) const
{
    const auto sourceModel = m_proxyModel->sourceModel();
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

ItemFilterProxyModelPrivate::InsertInfo ItemFilterProxyModelPrivate::searchInsertableRow(const std::shared_ptr<ProxyIndexInfo> &proxyInfo, const QModelIndex &sourceIndex)
{
    Q_ASSERT(sourceIndex.isValid());
    auto& proxyChildren = proxyInfo->m_children;
    if (proxyChildren.empty()) {
        return { 0, proxyChildren.begin() };
    }
    Q_ASSERT(sourceIndex.model() == proxyChildren.at(0)->m_source.model());

    const auto beginIt = proxyInfo->columnBegin();
    const auto endIt = proxyInfo->columnEnd();
    for (auto d = beginIt; d != endIt; ++d) {
        qDebug() << "oh";
    }
    const auto it = std::lower_bound(beginIt, endIt, sourceIndex, SourceModelIndexLessComparator{});
    return InsertInfo {
        it == endIt ? proxyInfo->rowCount() : (*it)->row(),
        it
    };
}

void ItemFilterProxyModelPrivate::updateChildrenRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo)
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

std::shared_ptr<ProxyIndexInfo> ItemFilterProxyModelPrivate::appendSourceIndexInSortedIndexes(const QModelIndex& sourceIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParent)
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(proxyParent);
    
    int proxyRow;
    if (proxyParent->hasChildren()) {
        const auto& lastProxyRow = proxyParent->m_children.back();
        if (lastProxyRow->m_source.row() == sourceIndex.row() && lastProxyRow->m_source.parent() == sourceIndex.parent()) {
            proxyRow = lastProxyRow->row();
        }
        else {
            proxyRow = proxyParent->rowCount();
        }
    }
    else {
        proxyRow = 0;
    }

    const auto column = sourceIndex.column();
    const auto proxyIndex = createIndex(proxyRow, column, sourceIndex.internalId());
    auto child = std::make_shared<ProxyIndexInfo>(sourceIndex, proxyIndex, proxyParent);
    proxyParent->m_children.push_back(child);

    m_sourceIndexHash.emplace(sourceIndex, child);
    m_proxyIndexHash.emplace(proxyIndex, child);
    return child;
}

// TODO Fixme
void ItemFilterProxyModelPrivate::eraseRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow)
{
    Q_ASSERT(!parentProxyInfo->m_index.isValid() || parentProxyInfo->m_index.model() == m_proxyModel);
    
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

void ItemFilterProxyModelPrivate::moveRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow,
        const std::shared_ptr<ProxyIndexInfo>& destinationInfo, int destinationRow)
{
    Q_ASSERT(parentProxyInfo);
    Q_ASSERT(firstRow >= 0 && firstRow <= lastRow);
    Q_ASSERT(destinationRow >= 0);
    Q_ASSERT(destinationInfo);
    auto& source = parentProxyInfo->m_children;
    auto& destination = destinationInfo->m_children;
    const auto& destinationIndex = destinationInfo->m_index;

    const auto columnCount = parentProxyInfo->columnCount();
    for(int col = 0; col < columnCount; ++col) {
        const auto sourceColumnIt = parentProxyInfo->columnBegin(col);
        const auto sourceFirstRowIt = sourceColumnIt + firstRow;
        const auto sourceLastRowIt = sourceColumnIt + lastRow;
        const auto destIt = destination.begin() + destinationRow;

        const auto insertedChildren = destinationInfo->m_children.insert(destIt,
        std::make_move_iterator(sourceFirstRowIt), std::make_move_iterator(sourceLastRowIt + 1));
        // TODO finish
    } 
    updateChildrenRows(parentProxyInfo);
    updateChildrenRows(destinationInfo);
}


void ItemFilterProxyModelPrivate::resetProxyIndexes()
{
    const auto sourceModel = m_proxyModel->sourceModel();
    Q_ASSERT(sourceModel);
    
    m_sourceIndexHash.clear();
    m_proxyIndexHash.clear();

    // Parent of all root nodes
    auto hiddenRootIndex = std::make_shared<ProxyIndexInfo>(QModelIndex(), QModelIndex(), nullptr);
    m_proxyIndexHash.emplace(QModelIndex(), hiddenRootIndex);
    const auto rootRowCount = sourceModel->rowCount();
    const auto rootColumnCount = sourceModel->columnCount();
    for (int sourceRow = 0; sourceRow < rootRowCount; ++sourceRow) {
        const auto isRootNodeVisible = m_proxyModel->filterAcceptsRow(sourceRow, QModelIndex());
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

void ItemFilterProxyModelPrivate::fillChildrenIndexesRecursively(const QModelIndex &sourceParent, const std::shared_ptr<ProxyIndexInfo>& parentInfo)
{
    Q_ASSERT(sourceParent.isValid());
    Q_ASSERT(sourceParent.model() == m_proxyModel->sourceModel());
    Q_ASSERT(parentInfo);

    const auto sourceModel = m_proxyModel->sourceModel();
    const auto& proxyParent = parentInfo->m_index;
    const auto rowCount = sourceModel->rowCount(sourceParent);
    const auto columnCount = sourceModel->columnCount(sourceParent);
    Q_ASSERT(!proxyParent.isValid() ||  proxyParent.model() == m_proxyModel);

    for (auto sourceRow = 0; sourceRow < rowCount; ++sourceRow) {
        const auto isChildVisible = m_proxyModel->filterAcceptsRow(sourceRow, sourceParent);

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
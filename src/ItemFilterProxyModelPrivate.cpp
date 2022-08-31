#include "ItemFilterProxyModelPrivate.h"

#include <QStack>

using namespace kaz;

namespace
{
    template<typename Predicate>
    std::optional<std::shared_ptr<ProxyIndexInfo>> find_if_recursive(Predicate&& predicate, const ItemFilterProxyModelPrivate *proxyModel)
    {
        QStack<std::shared_ptr<ProxyIndexInfo>> stack;
        const auto& rootInfo = proxyModel->m_proxyIndexHash.at(QModelIndex());
        for (const auto& proxyInfo : rootInfo->m_children) {
            if (predicate(proxyInfo)) {
                return proxyInfo;
            }
            stack.push(proxyInfo);
        }

        while (!stack.empty()) {
            const auto parentInfo = stack.pop();
            for (const auto& proxyInfo : parentInfo->m_children) {
                if (predicate(proxyInfo)) {
                    return proxyInfo;
                }
                stack.push(proxyInfo);
            }
        }
        return {};
    }
}

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
    ProxyIndexInfo::ChildrenLessComparator indexComparator;
    if (left.parent() == right.parent()) {
        return indexComparator(left, right);
    }

    const auto& leftParents = getSourceIndexParents(left);
    const auto& rightParents = getSourceIndexParents(right);
    const auto leftParentsEnd = leftParents.rend();
    const auto rightParentsEnd = rightParents.rend();
    
    const auto [ leftLastIndex, rightLastIndex ] = std::mismatch(leftParents.rbegin(), leftParentsEnd, rightParents.rbegin(), rightParentsEnd);
    if (leftLastIndex == leftParentsEnd) {
        return true;
    }
    else if (rightLastIndex == rightLastIndex) {
        return false;
    }

    Q_ASSERT(*leftLastIndex != *rightLastIndex);
    Q_ASSERT(leftLastIndex->parent() == rightLastIndex->parent());
    return indexComparator(*leftLastIndex, *rightLastIndex);
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

#ifdef KAZ_DEBUG
        const auto parentTxt = parent.data().toString();
#endif

    const auto& parentInfo = it->second;
    const auto child = parentInfo->childAt(row, column);
    if (!child) {
#ifdef KAZ_DEBUG
        Q_ASSERT(false);
#endif
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
 #ifdef KAZ_DEBUG
        const auto opt = find_if_recursive([=](const auto& proxyInfo) {
            return proxyInfo->m_index == childIndex;
        }, this);
        const auto proxyInfo = opt.value();
        const auto proxyTxt = proxyInfo->m_source.data().toString();
#endif
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
    const auto it = m_proxyIndexHash.find(parentIndex);
    if (it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
        return 0;
    }
    return it->second->columnCount();
}

bool ItemFilterProxyModelPrivate::hasChildren(const QModelIndex &parentIndex) const
{
    const auto it = m_proxyIndexHash.find(parentIndex);
    if (it == m_proxyIndexHash.end()) {
        Q_ASSERT(false);
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

std::vector<std::pair<std::shared_ptr<ProxyIndexInfo>, std::shared_ptr<ProxyIndexInfo>>> ItemFilterProxyModelPrivate::mapFromSourceRange(const QModelIndex &sourceLeft,
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
    
    std::shared_ptr<ProxyIndexInfo> proxyIndexBeginRange, proxyIndexEndRange;
    std::vector<std::pair<std::shared_ptr<ProxyIndexInfo>, std::shared_ptr<ProxyIndexInfo>>> ranges;
    const auto appendIndexToRange = [&](const std::shared_ptr<ProxyIndexInfo> &proxyInfo) {
        if (!proxyIndexBeginRange) {
            // New range
            proxyIndexBeginRange = proxyInfo;
            return;
        }

        if (proxyInfo->m_parent == proxyIndexBeginRange->m_parent) {
            // Enlarge the current range
            proxyIndexEndRange = proxyInfo;
        }
        else {
            // End of the range, insert in the results
            ranges.emplace_back(proxyIndexBeginRange, proxyIndexEndRange ? proxyIndexEndRange : proxyIndexBeginRange);
            proxyIndexBeginRange = proxyInfo;
            proxyIndexEndRange.reset();
        }
    };

    for (int sourceRow = sourceLeftRow; sourceRow <= sourceRightRow; ++sourceRow) {
        const auto sourceIndex = sourceModel->index(sourceRow, column, sourceParent);
        const auto proxyInfo = m_sourceIndexHash.value(sourceIndex);
        if (proxyInfo) {
            appendIndexToRange(proxyInfo);
        }
        else if (parameters & IncludeChildrenIfInvisible) {
            // Source index is invisble, select his "direct" visible children instead (check recursively until finding a visible child index)
            const auto proxyIndexes = getProxyChildrenIndexes(sourceIndex);
            for (const auto& proxyIndexInfo : proxyIndexes) {
                appendIndexToRange(proxyIndexInfo);
            }
        }
    }

    if (proxyIndexBeginRange) {
        ranges.emplace_back(proxyIndexBeginRange, proxyIndexEndRange ? proxyIndexEndRange : proxyIndexBeginRange);
    }
    return ranges;
}

std::vector<std::pair<std::shared_ptr<ProxyIndexInfo>, std::shared_ptr<ProxyIndexInfo>>> ItemFilterProxyModelPrivate::mapFromSourceRange(
        const QModelIndex& sourceParent, int sourceFirst, int sourceLast, const SelectionParameters parameters) const
{
    const auto sourceModel = m_proxyModel->sourceModel();
    const auto column = sourceParent.column() >= 0 ? sourceParent.column() : 0;
    const auto sourceLeft = sourceModel->index(sourceFirst, column, sourceParent);
    const auto sourceRight = sourceModel->index(sourceLast, column, sourceParent);
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        Q_ASSERT(false);
        return {};
    }
    return mapFromSourceRange(sourceLeft, sourceRight, parameters);
}

std::shared_ptr<ProxyIndexInfo> ItemFilterProxyModelPrivate::getProxyParentIndex(const QModelIndex &sourceIndex) const
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(sourceIndex.model() == m_proxyModel->sourceModel());
    return getProxyNearestIndex(sourceIndex.parent());
}

std::shared_ptr<ProxyIndexInfo> ItemFilterProxyModelPrivate::getProxyNearestIndex(QModelIndex sourceIndex) const
{
    Q_ASSERT(sourceIndex.isValid());
    Q_ASSERT(sourceIndex.model() == m_proxyModel->sourceModel());
    const auto end = m_sourceIndexHash.end();

    auto it = m_sourceIndexHash.find(sourceIndex);
    while (it == end && sourceIndex.isValid()) {
        sourceIndex = sourceIndex.parent();
        it = m_sourceIndexHash.find(sourceIndex);
    }
    Q_ASSERT(it != end);
    return *it;
}

std::vector<std::shared_ptr<ProxyIndexInfo>> ItemFilterProxyModelPrivate::getProxyChildrenIndexes(const QModelIndex &sourceParentIndex) const
{
    const auto sourceModel = m_proxyModel->sourceModel();
    Q_ASSERT(sourceParentIndex.model() == sourceModel);
    const auto column = sourceParentIndex.column() >= 0 ? sourceParentIndex.column() : 0;
    const auto sourceMappingEnd = m_sourceIndexHash.end();

    std::vector<std::shared_ptr<ProxyIndexInfo>> visibleChildren;
    const auto childrenCount = sourceModel->rowCount(sourceParentIndex);
    for (int row = 0; row < childrenCount; ++row) {
        const auto childSourceIndex = sourceModel->index(row, column, sourceParentIndex);
        const auto it = m_sourceIndexHash.find(childSourceIndex);
        if (it == sourceMappingEnd) {
            // Child is not visible, search recursively in child's children for any visible index
            auto recursiveVisibleChildren = getProxyChildrenIndexes(childSourceIndex);
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
#ifdef KAZ_DEBUG
        const auto txt = child->m_source.data().toString();
#endif
        auto& childIndex = child->m_index;
        const auto childRow = childIndex.row();
        const auto childColumn = childIndex.column();
        bool updatedRequired = false;
        if (childColumn != col) {
            Q_ASSERT(childColumn == 0);
            // Found the next row
            col = childColumn;
            ++expectedRow;
        }
        if (childRow != expectedRow) {
            updatedRequired = true;
        }

        if (updatedRequired) {
            auto it = m_proxyIndexHash.find(childIndex);
            if (it != m_proxyIndexHash.end() && it->second == child) {
                // Remove previous mapping
                m_proxyIndexHash.erase(it);
            }
            childIndex = createIndex(expectedRow, col, childIndex.internalId());
            m_proxyIndexHash[childIndex] = child;
        }
        ++col;
    }

    Q_ASSERT(parentProxyInfo->assertChildrenAreValid(true));
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

void ItemFilterProxyModelPrivate::eraseRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow)
{
    Q_ASSERT(!parentProxyInfo->m_index.isValid() || parentProxyInfo->m_index.model() == m_proxyModel);
    
    const auto [childBegin, childEnd] = parentProxyInfo->childRange(firstRow, lastRow);
    QStack<std::shared_ptr<ProxyIndexInfo>> stack;
    std::for_each(childBegin, childEnd, [&, this](const std::shared_ptr<ProxyIndexInfo>& proxyInfo) {
        m_sourceIndexHash.remove(proxyInfo->m_source);
        m_proxyIndexHash.erase(proxyInfo->m_index);
        for (auto& child : proxyInfo->m_children) {
            child->m_parent.reset();
            stack.push(std::move(child));
        }
    });

    while (!stack.empty()) {
        const auto parentInfo = std::move(stack.pop());

        m_sourceIndexHash.remove(parentInfo->m_source);
        m_proxyIndexHash.erase(parentInfo->m_index);
        for (auto& child : parentInfo->m_children) {
            child->m_parent.reset();
            stack.push(std::move(child));
        }
        parentInfo->m_children.clear();
    }

    parentProxyInfo->m_children.erase(childBegin, childEnd);
    updateChildrenRows(parentProxyInfo);
}

void ItemFilterProxyModelPrivate::moveRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow,
        const std::shared_ptr<ProxyIndexInfo>& destinationInfo, int destinationRow)
{
    Q_ASSERT(parentProxyInfo);
    Q_ASSERT(firstRow >= 0 && firstRow <= lastRow);
    Q_ASSERT(destinationRow >= 0);
    Q_ASSERT(destinationInfo);

    auto parentChildrenBegin = parentProxyInfo->m_children.begin();
    const auto [ rowBeginIt, rowEndIt ] = parentProxyInfo->childRange(firstRow, lastRow);
    const auto movedIndexCount = std::distance(rowBeginIt, rowEndIt);
    auto destIt = destinationInfo->childIt(destinationRow);
    destIt = destinationInfo->m_children.insert(destIt, std::make_move_iterator(rowBeginIt), std::make_move_iterator(rowEndIt));
    parentProxyInfo->m_children.erase(rowBeginIt, rowEndIt);
    // Assign new parent
    for (int i = 0; i < movedIndexCount; ++i) {
        (**(destIt + i)).m_parent = destinationInfo;
    }

    
    updateChildrenRows(destinationInfo);
    updateChildrenRows(parentProxyInfo);
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

std::vector<QModelIndex> ItemFilterProxyModelPrivate::getSourceVisibleChildren(const QModelIndex &sourceParentIndex) const
{
    const auto rowCount = m_proxyModel->sourceModel()->rowCount(sourceParentIndex);
    if (rowCount == 0) {
        return {};
    }

    return getSourceVisibleChildren(sourceParentIndex, 0, rowCount - 1);
}

std::vector<QModelIndex> ItemFilterProxyModelPrivate::getSourceVisibleChildren(const QModelIndex &sourceParentIndex, int firstRow, int lastRow) const
{
    auto sourceModel = m_proxyModel->sourceModel();
    Q_ASSERT(!sourceParentIndex.isValid() || sourceParentIndex.model() == sourceModel);

    std::vector<QModelIndex> visibleIndexes;
    visibleIndexes.reserve(lastRow - firstRow);
    for (int row = firstRow; row <= lastRow; ++row) {
        const auto childIndex = sourceModel->index(row, 0, sourceParentIndex);
        const auto isChildVisible = m_proxyModel->filterAcceptsRow(row, sourceParentIndex);
        if (isChildVisible) {
            visibleIndexes.push_back(childIndex);
        }   
        else {
            // Search recursively in children
            auto visibleChildren = getSourceVisibleChildren(childIndex);
            if (!visibleChildren.empty()) {
                if (visibleIndexes.empty()) {
                    visibleIndexes = std::move(visibleChildren);
                }
                else {
                    visibleIndexes.insert(visibleIndexes.end(), visibleChildren.begin(), visibleChildren.end());
                }
            }
        }
    }
    return visibleIndexes;
}

std::vector<std::pair<std::shared_ptr<ProxyIndexInfo>, std::shared_ptr<ProxyIndexInfo>>>
ItemFilterProxyModelPrivate::splitInRowRanges(ProxyIndexInfo::ChildrenList&& indexes) const
{
    if (indexes.empty()) {
        return {};
    }

    std::sort(indexes.begin(), indexes.end(), [](const std::shared_ptr<ProxyIndexInfo>& left, const std::shared_ptr<ProxyIndexInfo>& right) {
        const auto& leftParent = left->m_parent;
        const auto& rightParent = right->m_parent;
        if (leftParent == rightParent) {
            // same parent, compare rows
            return left->row() < right->row();
        }
        // Different parent, arbitrary ordering based on pointer's addresses
        return leftParent < rightParent;
    });

    std::vector<std::pair<std::shared_ptr<ProxyIndexInfo>, std::shared_ptr<ProxyIndexInfo>>> ranges;
    std::shared_ptr<ProxyIndexInfo> beginRange = std::move(indexes.front());
    auto previousIndex = beginRange;
    for (std::size_t size = indexes.size(), i = 1; i < size; ++i) {
        const auto& index = indexes[i];
        int expectedRow = previousIndex->row() + 1;
        if (index->m_parent == beginRange->m_parent && index->row() == expectedRow) {
            // Keep going in the same range
            previousIndex = std::move(index);
            continue;
        }
        else {
            // End of current range
            ranges.push_back(std::make_pair(std::move(beginRange), std::move(previousIndex)));
            beginRange = index;
            previousIndex = std::move(index);
        }
    }
    ranges.push_back(std::make_pair(std::move(beginRange), std::move(previousIndex)));
    return ranges;
}
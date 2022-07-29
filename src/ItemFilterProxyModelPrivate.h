#ifndef __ITEMFILTERPROXYMODELPRIVATE_H__
#define __ITEMFILTERPROXYMODELPRIVATE_H__

#include "ItemFilterProxyModel.h"
#include "ProxyIndexInfo.h"

#include <QtCore/qglobal.h>
#include <QModelIndex>

namespace kaz
{

class ItemFilterProxyModel;


struct SourceModelIndexLessComparator
{
    mutable QMap<QModelIndex, QStack<QModelIndex>> m_parentStack;

    QStack<QModelIndex>& getSourceIndexParents(const QModelIndex &sourceIndex) const;

    bool operator()(const QModelIndex& left, const QModelIndex &right) const;

    bool operator()(const QModelIndex &left, const std::shared_ptr<ProxyIndexInfo>& right) const;

    bool operator()(const std::shared_ptr<ProxyIndexInfo>& left, const QModelIndex &right) const;
};

class ItemFilterProxyModelPrivate
{
public:
    struct ProxyIndexHash
    {
        std::size_t operator()(const QModelIndex &idx) const noexcept
        {
            return qHash(idx);
        }
    };

    enum SelectionParameter
    {
        None = 0,
        IncludeChildrenIfInvisible = 1
    };
    Q_DECLARE_FLAGS(SelectionParameters, SelectionParameter);

public:
    explicit ItemFilterProxyModelPrivate(ItemFilterProxyModel *proxyModel);

    QModelIndex createIndex(int row, int col, quintptr internalId) const;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;

    QModelIndex parent(const QModelIndex &childIndex) const;

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const;

    int columnCount(const QModelIndex &parentIndex = QModelIndex()) const;

    bool hasChildren(const QModelIndex &parentIndex = QModelIndex()) const;

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const;

    std::vector<std::pair<QModelIndex, QModelIndex>> mapFromSourceRange(
        const QModelIndex &sourceLeft, const QModelIndex& sourceRight, const SelectionParameters parameters = None) const;

    std::vector<std::pair<QModelIndex, QModelIndex>> mapFromSourceRange(
        const QModelIndex& sourceParent, int sourceFirst, int sourceLast, const SelectionParameters parameters = None) const;

    // For each source child index, return the first visible index (check recursively down in the tree)
    QModelIndexList getProxyChildrenIndexes(const QModelIndex &sourceIndex) const;

    // Return the proxy index matching the sourceIndex : if the latter is not visible, search recursively a visible parent
    std::shared_ptr<ProxyIndexInfo> getProxyNearestParentIndex(const QModelIndex &sourceIndex) const;

    // Return the proxy index matching for each sourceIndex's children : if the a child indexed is not visible, search recursively all his visible children
    std::vector<std::shared_ptr<ProxyIndexInfo>> getProxyNearestChildrenIndexes(const QModelIndex &parentIndex) const;

    struct InsertInfo
    {
        int m_row;
        ProxyIndexInfo::ChildrenList::const_iterator m_it;
    };

    // Return at which row (with an iterator to the specific children) the given sourceIndex should be inserted     
    InsertInfo searchInsertableRow(const std::shared_ptr<ProxyIndexInfo> &proxyInfo, const QModelIndex &sourceIndex);

    // Insert a new proxy index for the given source index, as a child of proxyParent
    std::shared_ptr<ProxyIndexInfo> appendSourceIndexInSortedIndexes(const QModelIndex& sourceIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParent);

    void eraseRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow);

    void moveRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow,
        const std::shared_ptr<ProxyIndexInfo>& destinationInfo, int destinationRow);

    // Update children's rows (used after an insertion or a suppression of a child)
    void updateChildrenRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo);

    // Recalculate the proxy mapping (used when a source model reset happens)
    void resetProxyIndexes();

    // Recursively insert proxy indexes mapping based on sourceModel()'s indexes
    void fillChildrenIndexesRecursively(const QModelIndex &sourceParent, const std::shared_ptr<ProxyIndexInfo>& parentInfo);

    // Mapping source index -> proxy index
    QHash<QPersistentModelIndex, std::shared_ptr<ProxyIndexInfo>> m_sourceIndexHash;
    // Mapping proxy index
    std::unordered_map<QModelIndex, std::shared_ptr<ProxyIndexInfo>, ProxyIndexHash> m_proxyIndexHash;

    friend ItemFilterProxyModel;

    ItemFilterProxyModel *m_proxyModel;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ItemFilterProxyModelPrivate::SelectionParameters);

}

#endif // __ITEMFILTERPROXYMODELPRIVATE_H__
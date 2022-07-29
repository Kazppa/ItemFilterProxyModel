#ifndef __ITEMFILTERPROXYMODELPRIVATE_H__
#define __ITEMFILTERPROXYMODELPRIVATE_H__

#include "ItemFilterProxyModel.h"
#include "ProxyIndexInfo.h"

#include <QtCore/qglobal.h>
#include <QModelIndex>

namespace kaz
{

class ItemFilterProxyModel;

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

    explicit ItemFilterProxyModelPrivate(ItemFilterProxyModel *proxyModel);

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

    void moveRowsImpl(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow,
        const std::shared_ptr<ProxyIndexInfo>& destinationInfo, int destinationRow);

    // Mapping source index -> proxy index
    QHash<QPersistentModelIndex, std::shared_ptr<ProxyIndexInfo>> m_sourceIndexHash;
    // Mapping proxy index
    std::unordered_map<QModelIndex, std::shared_ptr<ProxyIndexInfo>, ProxyIndexHash> m_proxyIndexHash;

    ItemFilterProxyModel *m_q;
    friend ItemFilterProxyModel;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ItemFilterProxyModelPrivate::SelectionParameters);

}

#endif // __ITEMFILTERPROXYMODELPRIVATE_H__
#ifndef __KAZ_PROXYINDEXINFO_H__
#define __KAZ_PROXYINDEXINFO_H__

#include <QModelIndex>

#include <memory>
#include <vector>

namespace kaz
{
    class ProxyIndexInfo
    {
    public:
        // Children are sorted by column and then by row
        using ChildrenList = typename std::vector<std::shared_ptr<ProxyIndexInfo>>;

        ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParentIndex);

        // Return the column's rows range as a pair : [first, second[
        std::pair<ChildrenList::const_iterator, ChildrenList::const_iterator> getColumnBeginEnd(int column) const;
        
        // Return an iterator to the first row of the given column
        ChildrenList::const_iterator getColumnBegin(int column) const;

        // Return the child index at the given row and column
        std::shared_ptr<ProxyIndexInfo> childAt(int row, int column) const;

        // Return the number of rows
        int rowCount() const;

        int columnCount() const;

        const QModelIndex& parentIndex() const { return m_parent->m_index; }

        int row() const noexcept { return m_index.row(); }

        int column() const noexcept { return m_index.column(); }

        QPersistentModelIndex m_source;
        QModelIndex m_index;
        std::shared_ptr<ProxyIndexInfo> m_parent;
        ChildrenList m_children{};
    };

}

#endif
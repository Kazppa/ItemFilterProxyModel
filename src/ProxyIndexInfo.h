#ifndef __KAZ_PROXYINDEXINFO_H__
#define __KAZ_PROXYINDEXINFO_H__

#include <QModelIndex>

#include <vector>

namespace kaz
{
    class ProxyIndexInfo
    {
    public:
        struct ChildrenLessComparator
        {
            bool operator()(const QModelIndex &left, const QModelIndex &right) const
            {
                const auto leftRow = left.row();
                const auto rightRow = right.row();
                if (leftRow < rightRow) {
                    return true;
                }
                if (leftRow == rightRow) {
                    return left.column() < right.column();
                }
                return false;
            }

            bool operator()(const ProxyIndexInfo* left, const std::pair<int, int> &right) const
            {
                const auto leftRow = left->row();
                if (leftRow < right.first) {
                    return true;
                }
                if (leftRow == right.first) {
                    return left->column() < right.second;
                }
                return false;
            }
        };

        // Children are sorted by indexes (by row and then by column)
        // for example : [ {0,0}, {0,1}, {0,2}, {1,0}, {1,1}, {1,2} ]
        using ChildrenList = typename std::vector<ProxyIndexInfo *>;

        // Iterate through all children's index of a given column
        struct ColumnIterator final
        {
            using iterator_category = std::random_access_iterator_tag;
            using difference_type = int;
            using value_type = ProxyIndexInfo *;
            using reference = ProxyIndexInfo&;
            using pointer = ChildrenList::const_iterator;

            value_type operator*() const { return *m_it; };
            value_type operator->() const { return *m_it; };

            ColumnIterator operator+(int row) const;
            ColumnIterator operator-(int row) const;
            ColumnIterator& operator+=(int row);
            ColumnIterator& operator-=(int row);
            ColumnIterator& operator++();
            ColumnIterator& operator--();
            ColumnIterator operator++(int);
            ColumnIterator operator--(int);

            difference_type operator-(const ColumnIterator& other) const;

            bool operator==(const ColumnIterator& other) const;
            bool operator!=(const ColumnIterator& other) const;
            bool operator<(const ColumnIterator& other) const;
            bool operator>(const ColumnIterator& other) const;
            bool operator<=(const ColumnIterator& other) const;
            bool operator>=(const ColumnIterator& other) const;

            pointer base() const noexcept {return m_it; }

            operator pointer() const noexcept { return m_it; }

            friend ProxyIndexInfo;

        private:
            explicit ColumnIterator(const ChildrenList* childrenList, const ChildrenList::const_iterator& it);

            int columnCount() const;

            int column() const;

            const ChildrenList* m_childrenList;
            ChildrenList::const_iterator m_it;
        };

        ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyIndex, ProxyIndexInfo *proxyParentIndex);

        // Return an iterator to the first row of the given column
        ColumnIterator columnBegin(int column = 0) const;
        
        // Return an iterator to end
        ColumnIterator columnEnd() const;

        // Return the column's rows range as a pair : [first, second[
        std::pair<ColumnIterator, ColumnIterator> columnBeginEnd(int column) const;

        // Return the child index at the given row and column
        ChildrenList::const_iterator childIt(int row, int column = 0) const;
        ProxyIndexInfo * childAt(int row, int column) const;
        ProxyIndexInfo * childAt(const QModelIndex &idx) const;

        // return a pair of iterator [begin, end[
        std::pair<ChildrenList::iterator, ChildrenList::iterator> childRange(const int firstRow, const int lastRow);

        // Return the number of rows
        int rowCount() const;

        int columnCount() const;

        std::pair<int, int> rowColCount() const;

        // Debugging purpose only
        bool assertChildrenAreValid(bool recursively = false) const;  

        const QModelIndex& parentIndex() const { return m_parent->m_index; }

        int row() const noexcept { return m_index.row(); }

        int column() const noexcept { return m_index.column(); }

        bool hasChildren() const noexcept { return !m_children.empty(); }

        QPersistentModelIndex m_source;
        QModelIndex m_index;
        ProxyIndexInfo * m_parent = nullptr;
        ChildrenList m_children{};
    };


/////////////////////////////////

inline ProxyIndexInfo::ColumnIterator::ColumnIterator(const ProxyIndexInfo::ChildrenList* childrenList, const ProxyIndexInfo::ChildrenList::const_iterator& it) :
    m_childrenList(childrenList),
    m_it(it) 
{
}

inline ProxyIndexInfo::ColumnIterator ProxyIndexInfo::ColumnIterator::operator+(int row) const
{
    const auto increment = columnCount() * row;
    const auto end = m_childrenList->end();
    const auto endDist = std::distance(m_it, end);
    if (increment > endDist) {
        return ColumnIterator(m_childrenList, end);
    }
    return ColumnIterator(m_childrenList, m_it + increment);
}

inline ProxyIndexInfo::ColumnIterator ProxyIndexInfo::ColumnIterator::operator-(int row) const
{
    const auto decrement = columnCount() * row;
    const auto col = column();
    const auto begin = m_childrenList->begin() + col;
    const auto beginDist = std::distance(begin, m_it);
    if (decrement > beginDist) {
        return ColumnIterator(m_childrenList, begin);
    }
    return ColumnIterator(m_childrenList, m_it - decrement);
}

inline ProxyIndexInfo::ColumnIterator& ProxyIndexInfo::ColumnIterator::operator+=(int row)
{
    const auto increment = columnCount() * row;
    const auto end = m_childrenList->end();
    const auto endDist = std::distance(m_it, end);
    if (increment > endDist) {
        m_it = end;
    }
    else {
        m_it += increment;
    }
    return *this;
}

inline ProxyIndexInfo::ColumnIterator& ProxyIndexInfo::ColumnIterator::operator-=(int row)
{
    const auto decrement = columnCount() * row;
    const auto col = column();
    const auto begin = m_childrenList->begin() + col;
    const auto beginDist = std::distance(begin, m_it);
    if (decrement > beginDist) {
        m_it = begin;
    }
    else {
        m_it -= decrement;
    }
    return *this;
}

inline ProxyIndexInfo::ColumnIterator& ProxyIndexInfo::ColumnIterator::operator++()
{
    const auto increment = columnCount();
    const auto end = m_childrenList->end();
    const auto endDist = std::distance(m_it, end);
    if (increment > endDist) {
        m_it = end;
    }
    else {
        m_it += increment;
    }
    return *this;
}

inline ProxyIndexInfo::ColumnIterator& ProxyIndexInfo::ColumnIterator::operator--()
{
    const auto decrement = columnCount();
    const auto col = column();
    const auto begin = m_childrenList->begin() + col;
    const auto beginDist = std::distance(begin, m_it);
    if (decrement > beginDist) {
        m_it = begin;
    }
    else {
        m_it -= decrement;
    }
    return *this;
}

inline ProxyIndexInfo::ColumnIterator ProxyIndexInfo::ColumnIterator::operator++(int)
{
    auto tmp = m_it;
    const auto increment = columnCount();
    const auto end = m_childrenList->end();
    const auto endDist = std::distance(m_it, end);
    if (increment > endDist) {
        m_it = end;
    }
    else {
        m_it += increment;
    }
    return ColumnIterator(m_childrenList, tmp);
}

inline ProxyIndexInfo::ColumnIterator ProxyIndexInfo::ColumnIterator::operator--(int)
{
    auto tmp = m_it;
    const auto decrement = columnCount();
    const auto col = column();
    const auto begin = m_childrenList->begin() + col;
    const auto beginDist = std::distance(begin, m_it);
    if (decrement > beginDist) {
        m_it = begin;
    }
    else {
        m_it -= decrement;
    }
    return ColumnIterator(m_childrenList, tmp);
}

inline ProxyIndexInfo::ColumnIterator::difference_type ProxyIndexInfo::ColumnIterator::operator-(const ColumnIterator& other) const
{
    const auto dist = m_it - other;
    return dist == 0
        ? 0
        : dist / columnCount();
}

inline bool ProxyIndexInfo::ColumnIterator::operator==(const ColumnIterator& other) const
{
    return m_it == other.m_it;
}

inline bool ProxyIndexInfo::ColumnIterator::operator!=(const ColumnIterator& other) const
{
    return m_it != other.m_it;
}

inline bool ProxyIndexInfo::ColumnIterator::operator<(const ColumnIterator& other) const
{
    return m_it < other.m_it;
}

inline bool ProxyIndexInfo::ColumnIterator::operator>(const ColumnIterator& other) const
{
    return m_it > other.m_it;
}

inline bool ProxyIndexInfo::ColumnIterator::operator<=(const ColumnIterator& other) const
{
    return m_it <= other.m_it;
}

inline bool ProxyIndexInfo::ColumnIterator::operator>=(const ColumnIterator& other) const
{
    return m_it >= other.m_it;
}

inline int ProxyIndexInfo::ColumnIterator::columnCount() const
{
    if (m_childrenList->empty()) {
        return 0;
    }
    return m_childrenList->back()->column() + 1;
}

inline int ProxyIndexInfo::ColumnIterator::column() const
{
    return (*m_it)->column();
}

} // end namespace kaz

#endif
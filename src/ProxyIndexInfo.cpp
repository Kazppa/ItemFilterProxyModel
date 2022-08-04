#include "ProxyIndexInfo.h"

using namespace kaz;

namespace
{
    // Helper to use multiple lambdas as a comparator
    template<typename ...Callables>
    struct make_visitor : Callables... { using Callables::operator()...; };

    template<typename ...Callables>
    make_visitor(Callables...) -> make_visitor<Callables...>;
}

ProxyIndexInfo::ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyIndex,
    const std::shared_ptr<ProxyIndexInfo> &proxyParentIndex) :
    m_source(sourceIndex),
    m_index(proxyIndex),
    m_parent(proxyParentIndex)
{
#ifdef QT_DEBUG
    if (sourceIndex.isValid()) {
        Q_ASSERT(sourceIndex.model() != proxyIndex.model());
    }
    if (proxyIndex.isValid() && proxyParentIndex && proxyParentIndex->m_index.isValid()) {
        Q_ASSERT(proxyIndex.model() == proxyParentIndex->m_index.model());
    }
#endif
}

ProxyIndexInfo::ColumnIterator ProxyIndexInfo::columnBegin(int column) const
{
    return ColumnIterator(&m_children, m_children.begin() + (rowCount() * column));
}

ProxyIndexInfo::ColumnIterator ProxyIndexInfo::columnEnd() const
{
    return ColumnIterator(&m_children, m_children.end());
}

std::pair<ProxyIndexInfo::ColumnIterator, ProxyIndexInfo::ColumnIterator> ProxyIndexInfo::columnBeginEnd(int column) const
{
    return std::make_pair(
        ColumnIterator(&m_children, m_children.begin() + column),
        ColumnIterator(&m_children, m_children.end())
    );
}

std::shared_ptr<ProxyIndexInfo> ProxyIndexInfo::childAt(int row, int column) const
{
    const auto it = std::lower_bound(m_children.begin(), m_children.end(), std::make_pair(row, column), ChildrenLessComparator{});
    Q_ASSERT(it != m_children.end());
    Q_ASSERT((*it)->row() == row && (*it)->column() == column);
    
    return *it;
}

std::shared_ptr<ProxyIndexInfo> ProxyIndexInfo::childAt(const QModelIndex &idx) const
{
    Q_ASSERT(idx.parent() == m_index);
    return childAt(idx.row(), idx.column());
}

int ProxyIndexInfo::rowCount() const
{
    if (m_children.empty()) {
         return 0;
    }
    return m_children.back()->row() + 1;
}

int ProxyIndexInfo::columnCount() const
{
    if (m_children.empty()) {
        return 0;
    }
    return m_children.back()->column() + 1;
}

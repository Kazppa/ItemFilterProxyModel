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

std::pair<ProxyIndexInfo::ChildrenList::const_iterator, ProxyIndexInfo::ChildrenList::const_iterator> ProxyIndexInfo::getColumnBeginEnd(int column) const
{
    Q_ASSERT(column >= 0);
    return std::equal_range(m_children.begin(), m_children.end(), column, make_visitor {
        [](const std::shared_ptr<ProxyIndexInfo>& child, int column) { return child->m_index.column() < column; },
        [](int column, const std::shared_ptr<ProxyIndexInfo>& child) { return column < child->m_index.column(); }
    });
}

ProxyIndexInfo::ChildrenList::const_iterator ProxyIndexInfo::getColumnBegin(int column) const
{
    return std::lower_bound(m_children.begin(), m_children.end(), column, [](const auto& child, int column) {
        return child->m_index.column() < column;
    });
}

std::shared_ptr<ProxyIndexInfo> ProxyIndexInfo::childAt(int row, int column) const
{
    const auto columnIt = getColumnBegin(column);
    if (columnIt == m_children.end()) {
        Q_ASSERT(false);
        return {};
    }

    const auto dist = std::distance(m_children.begin(), columnIt);
    const auto child = *(columnIt + row);
    Q_ASSERT(child->m_index.row() == row && child->m_index.column() == column);
    return child;
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

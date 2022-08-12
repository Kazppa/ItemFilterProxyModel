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

ProxyIndexInfo::ChildrenList::const_iterator ProxyIndexInfo::childIt(int row, int column) const
{
#ifdef QT_DEBUG
    const auto [ rowCount, colCount ] = rowColCount();
    Q_ASSERT(row >= 0 && row < rowCount);
    Q_ASSERT(column >= 0 && column < colCount);
#else
    const auto colCount = columnCount();
#endif

    return m_children.begin() + (row * colCount) + column;
}

std::shared_ptr<ProxyIndexInfo> ProxyIndexInfo::childAt(int row, int column) const
{
#ifdef QT_DEBUG
    const auto [ rowCount, colCount ] = rowColCount();
    if (row >= rowCount || column >= colCount) {
        return {};
    } 
    Q_ASSERT(row >= 0 && row < rowCount);
    Q_ASSERT(column >= 0 && column < colCount);
#else
    const auto colCount = columnCount();
#endif

    return *(m_children.begin() + (row * colCount) + column);
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

std::pair<ProxyIndexInfo::ChildrenList::const_iterator, ProxyIndexInfo::ChildrenList::const_iterator>
ProxyIndexInfo::childRange(const int firstRow, const int lastRow) const
{
#ifdef QT_DEBUG
    const auto [ rowCount, colCount ] = rowColCount();
    Q_ASSERT(firstRow >= 0 && firstRow <= lastRow);
    Q_ASSERT(lastRow < rowCount);
#else
    const auto colCount = columnCount();
#endif

    const auto begin = m_children.begin();
    return std::make_pair(
        begin + (firstRow * colCount),
        begin + ((lastRow + 1) * colCount)
    );
}

int ProxyIndexInfo::columnCount() const
{
    if (m_children.empty()) {
        return 0;
    }
    return m_children.back()->column() + 1;
}

std::pair<int, int> ProxyIndexInfo::rowColCount() const
{
    if (m_children.empty()) {
        return std::make_pair(0, 0);
    }
    
    const auto& idx = m_children.back()->m_index;
    return std::make_pair(idx.row() + 1, idx.column() + 1);
}

bool ProxyIndexInfo::assertChildrenAreValid(bool recursively) const
{
    if (m_children.empty()) {
        return true;
    }

    const auto [rowCount, colCount] = rowColCount();
    int expectedRow = 0, expectedCol = 0;
    for (const auto& child : m_children) {
        if(child->row() != expectedRow) {
            Q_ASSERT(false);
            return false;
        }
        if (child->column() != expectedCol) {
            Q_ASSERT(false);
            return false;
        }
        if (recursively) {
            if (!child->assertChildrenAreValid(true)) {
                return false;
            }
        }

        expectedCol = (expectedCol + 1) % colCount;
        if (expectedCol == 0) {
            // next row
            expectedRow = (expectedRow + 1) % rowCount;
        }
    }

    return true;
}

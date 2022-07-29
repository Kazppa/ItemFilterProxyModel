#include "ExampleItemFilterProxyModel.h"

namespace
{
    template<typename String, typename ...Values>
    constexpr bool anyOf(const String &str, const Values& ...values)
    {
        return ((str == values) || ...);
    }

    template<typename String, typename ...Values>
    constexpr bool noneOf(const String &str, const Values& ...values)
    {
        return ((str != values) && ...);
    }
}

void ExampleItemFilterProxyModel::setFilteredNames(QStringList names)
{
    m_filteredNames = std::move(names);
    invalidateRowsFilter();
}

bool ExampleItemFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto text = sourceModel()->data(sourceIndex, Qt::DisplayRole).toString();
    return !m_filteredNames.contains(text, Qt::CaseInsensitive);
    // return noneOf(text, u"A14", u"A17", u"A10", u"D");
}

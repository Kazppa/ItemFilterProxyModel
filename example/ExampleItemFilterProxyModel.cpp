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

bool ExampleItemFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto text = sourceModel()->data(sourceIndex, Qt::DisplayRole).toString();
    return noneOf(text, u"A17", u"A10", u"D");
}

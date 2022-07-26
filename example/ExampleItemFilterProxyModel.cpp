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
    return noneOf(text, u"F7"); //u"A17");
    const auto size = text.size();
    if (size < 2) {
        return true;
    }

    const auto number = QStringView(text.data() + 1, text.data() + size).toInt();
    return number % 2 == 0;
}

#ifndef ITEMFILTERPROXYMODEL_EXAMPLEFILTERPROXYMODEL_H
#define ITEMFILTERPROXYMODEL_EXAMPLEFILTERPROXYMODEL_H

#include "ItemFilterProxyModel/ItemFilterProxyModel.h"

class ExampleItemFilterProxyModel : public kaz::ItemFilterProxyModel
{
public:
    using ItemFilterProxyModel::ItemFilterProxyModel;

    void setFilteredNames(QStringList names);

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QStringList m_filteredNames;
};


#endif

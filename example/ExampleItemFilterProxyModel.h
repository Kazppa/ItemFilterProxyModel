#ifndef ITEMFILTERPROXYMODEL_EXAMPLEFILTERPROXYMODEL_H
#define ITEMFILTERPROXYMODEL_EXAMPLEFILTERPROXYMODEL_H

#include "ItemFilterProxyModel.h"

class ExampleItemFilterProxyModel : public ItemFilterProxyModel
{
public:
    using ItemFilterProxyModel::ItemFilterProxyModel;

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};


#endif

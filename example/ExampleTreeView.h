#ifndef ITEMFILTERPROXYMODEL_EXAMPLETREEVIEW_H
#define ITEMFILTERPROXYMODEL_EXAMPLETREEVIEW_H

#include <QtWidgets/QTreeView>

class ExampleTreeView : public QTreeView
{
    Q_OBJECT
public:
    ExampleTreeView(QWidget *parent = nullptr);

    Q_SIGNAL void currentIndexChanged(const QModelIndex &newIndex);

protected:
    void currentChanged(const QModelIndex &, const QModelIndex&) override;

};

#endif //ITEMFILTERPROXYMODEL_EXAMPLETREEVIEW_H

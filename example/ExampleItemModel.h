#ifndef __EXAMPLEITEMMODEL_H__
#define __EXAMPLEITEMMODEL_H__

#include <QtGui/qstandarditemmodel.h>


class ExampleItemModel : public QStandardItemModel
{
public:
    ExampleItemModel(QObject *parent);

    QVariant data(const QModelIndex &idx, int role) const override;

private:
    void generateExampleData();
};


#endif // __EXAMPLEITEMMODEL_H__
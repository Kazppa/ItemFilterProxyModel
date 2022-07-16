#ifndef __STRUCTUREPROXYMODEL_H__
#define __STRUCTUREPROXYMODEL_H__

#include <QtCore/QAbstractProxyModel>

class StructureProxyModelPrivate;

class StructureProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

public:
    StructureProxyModel(QObject *parent);

private:
    std::unique_ptr<StructureProxyModelPrivate> m_ptr;
};

#endif // __STRUCTUREPROXYMODEL_H__
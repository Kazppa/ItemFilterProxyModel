#ifndef __STRUCTUREPROXYMODEL_H__
#define __STRUCTUREPROXYMODEL_H__

#include <QtCore/QAbstractProxyModel>


namespace kaz
{

class ProxyIndexInfo;
class ItemFilterProxyModelPrivate;


class ItemFilterProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

public:
    explicit ItemFilterProxyModel(QObject *parent);

    virtual ~ItemFilterProxyModel();

    Q_DISABLE_COPY(ItemFilterProxyModel);

    void setSourceModel(QAbstractItemModel *newSourceModel) override;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex &childIndex) const override;

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;

    int columnCount(const QModelIndex &parentIndex = QModelIndex()) const override;

    bool hasChildren(const QModelIndex &parentIndex = QModelIndex()) const override;

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;

    // Return false if the row must be hidden by the proxy model
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const = 0;

protected:        
    void invalidateRowsFilter();

private:

    /*
    * Callbacks to handle source model's modifications
    */
    void sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles = {});
    void onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
        const QModelIndex &destinationParent, int destinationRow);
    void onRowsInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);

    QModelIndex createIndexImpl(int row, int col, quintptr internalId) const { return createIndex(row, col, internalId); };

    friend ItemFilterProxyModelPrivate;

    ItemFilterProxyModelPrivate * m_impl;
};

} // end namespace kaz   

#endif // __STRUCTUREPROXYMODEL_H__
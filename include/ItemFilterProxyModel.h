#ifndef __STRUCTUREPROXYMODEL_H__
#define __STRUCTUREPROXYMODEL_H__

#include <unordered_map>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QVector>


namespace kaz
{

class ProxyIndexInfo;
class ItemFilterProxyModelPrivate;


class ItemFilterProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

public:
    explicit ItemFilterProxyModel(QObject *parent);

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

    void sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles = {});

    /*
    * Bunch of callbacks to handle source model modifications
    */
    void onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsAboutToBeInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
        const QModelIndex &destinationParent, int destinationRow);

    // Insert a new proxy index for the given source index, as a child of proxyParent
    std::shared_ptr<ProxyIndexInfo> appendSourceIndexInSortedIndexes(const QModelIndex& sourceIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParent);

    void eraseRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow);

    // Update children's rows (used after an insertion or a suppression of a child)
    void updateChildrenRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo);

    // Recalculate the proxy mapping (used when a source model reset happens)
    void resetProxyIndexes();

    // Recursively insert proxy indexes mapping based on sourceModel()'s indexes
    void fillChildrenIndexesRecursively(const QModelIndex &sourceParent, const std::shared_ptr<ProxyIndexInfo>& parentInfo);


    std::unique_ptr<ItemFilterProxyModelPrivate> m_d;
};


} // end namespace kaz   

#endif // __STRUCTUREPROXYMODEL_H__
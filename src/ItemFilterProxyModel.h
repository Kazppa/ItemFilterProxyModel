#ifndef __STRUCTUREPROXYMODEL_H__
#define __STRUCTUREPROXYMODEL_H__

#include <unordered_map>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QVector>


class ItemFilterProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

public:
    struct ProxyIndexHash
    {
        std::size_t operator()(const QModelIndex &idx) const noexcept
        {
            return qHash(idx);
        }
    };

    explicit ItemFilterProxyModel(QObject *parent);

    void setSourceModel(QAbstractItemModel *newSourceModel) override;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex &childIndex) const override;

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;

    int columnCount(const QModelIndex &parentIndex = QModelIndex()) const override;

    bool hasChildren(const QModelIndex &parentIndex = QModelIndex()) const override;

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const = 0;

    // Return the mapped range for this proxyModel, in worst case the second value est equal to the first (both being invalid)
    std::pair<QModelIndex, QModelIndex> mapToSourceRange(const QModelIndex &sourceLeft, const QModelIndex& sourceRight) const;

private:
    void sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles = {});

    // Return the proxy index matching the sourceIndex : if the latter is not visible, search recursively a visible parent
    QModelIndex getProxyNearestParentIndex(const QModelIndex &sourceIndex) const;

    void onRowsAboutToBeInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);

    void onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);

    void onRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent);
    void onRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent);

    void refreshProxyIndexes();

    class ProxyIndexInfo
    {
    public:
        ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyIndex, const QModelIndex &proxyParentIndex = {});
    
        // Return the closest inferior or equal child for the given source row
        QModelIndexList::const_iterator lowerBoundSourceRow(int sourceRow) const;

        QModelIndex m_source;
        QModelIndex m_index;
        QModelIndex m_parent;
        QModelIndexList m_children{};
    };

    // Recursively insert indexes based on sourceModel()'s indexes
    void fillChildrenIndexes(const QModelIndex &sourceParent, const QModelIndex &proxyParent, ProxyIndexInfo* parentInfo);

    // SourceIndex -> ProxyIndex
    mutable QHash<QModelIndex, QModelIndex> m_sourceIndexHash;
    // ProxyIndex -> infos
    mutable std::unordered_map<QModelIndex, std::shared_ptr<ProxyIndexInfo>, ProxyIndexHash> m_proxyIndexHash;
};

#endif // __STRUCTUREPROXYMODEL_H__
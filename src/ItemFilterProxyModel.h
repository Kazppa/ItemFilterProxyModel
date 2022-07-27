#ifndef __STRUCTUREPROXYMODEL_H__
#define __STRUCTUREPROXYMODEL_H__

#include <unordered_map>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QVector>


class ItemFilterProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

public:
    class ProxyIndexInfo
    {
    public:
        using ChildrenList = typename std::vector<std::shared_ptr<ProxyIndexInfo>>;

        ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParentIndex);

        // Return the column's rows range as a pair : [first, second[
        std::pair<ChildrenList::const_iterator, ChildrenList::const_iterator> getColumnBeginEnd(int column) const;

        ChildrenList::const_iterator getColumnBegin(int column) const;

        std::shared_ptr<ProxyIndexInfo> childAt(int row, int column) const;

        // Return the closest inferior or equal child for the given source row
        // ChildrenList::const_iterator lowerBoundSourceRow(int sourceRow) const;

        QModelIndex m_source;
        QModelIndex m_index;
        std::shared_ptr<ProxyIndexInfo> m_parent;
        ChildrenList m_children{};
    };

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

    std::shared_ptr<ProxyIndexInfo> appendIndex(const QModelIndex& sourceIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParent);

    void onRowsAboutToBeInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);

    void onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);

    void onRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent);
    void onRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent);

    void updateProxyIndexes();

    // Recursively insert indexes based on sourceModel()'s indexes
    void fillChildrenIndexesRecursively(const QModelIndex &sourceParent, const std::shared_ptr<ProxyIndexInfo>& parentInfo);

    // SourceIndex -> ProxyIndex
    mutable QHash<QModelIndex, QModelIndex> m_sourceIndexHash;
    // ProxyIndex -> infos
    mutable std::unordered_map<QModelIndex, std::shared_ptr<ProxyIndexInfo>, ProxyIndexHash> m_proxyIndexHash;
};

#endif // __STRUCTUREPROXYMODEL_H__
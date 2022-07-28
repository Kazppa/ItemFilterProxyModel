#ifndef __STRUCTUREPROXYMODEL_H__
#define __STRUCTUREPROXYMODEL_H__

#include <unordered_map>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QVector>


namespace kaz
{

class ItemFilterProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

public:
    class ProxyIndexInfo
    {
    public:
        // Children are sorted by column and then by row
        using ChildrenList = typename std::vector<std::shared_ptr<ProxyIndexInfo>>;

        ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParentIndex);

        // Return the column's rows range as a pair : [first, second[
        std::pair<ChildrenList::const_iterator, ChildrenList::const_iterator> getColumnBeginEnd(int column) const;
        
        // Return an iterator to the first row of the given column
        ChildrenList::const_iterator getColumnBegin(int column) const;

        // Return the child index at the given row and column
        std::shared_ptr<ProxyIndexInfo> childAt(int row, int column) const;

        // Return the number of rows for the given column
        int rowCount(int column) const;

        QPersistentModelIndex m_source;
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
    
    // Return false if the row must be hidden by the proxy model
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const = 0;

    enum SelectionParameter
    {
        None = 0,
        IncludeChildrenIfInvisible = 1
    };
    Q_DECLARE_FLAGS(SelectionParameters, SelectionParameter);

    std::vector<std::pair<QModelIndex, QModelIndex>> mapFromSourceRange(
        const QModelIndex &sourceLeft, const QModelIndex& sourceRight, const SelectionParameters parameters = None) const;

    std::vector<std::pair<QModelIndex, QModelIndex>> mapFromSourceRange(
        const QModelIndex& sourceParent, int sourceFirst, int sourceLast, const SelectionParameters parameters = None) const;

    // For each source child index, return the first visible index (check recursively down in the tree)
    QModelIndexList getProxyChildrenIndexes(const QModelIndex &sourceIndex) const;

protected:
    void sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles = {});

    // Return the proxy index matching the sourceIndex : if the latter is not visible, search recursively a visible parent
    QModelIndex getProxyNearestParentIndex(const QModelIndex &sourceIndex) const;

    // Insert a new proxy index for the given source index, as a child of proxyParent
    std::shared_ptr<ProxyIndexInfo> appendIndex(const QModelIndex& sourceIndex, const std::shared_ptr<ProxyIndexInfo> &proxyParent);

    void eraseRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo, int firstRow, int lastRow);

    // Update children's rows (used after an insertion or a suppression of a child)
    void updateChildrenRows(const std::shared_ptr<ProxyIndexInfo>& parentProxyInfo);

    /*
    * Bunch of callbacks to handle source model modifications
    */
    void onRowsAboutToBeRemoved(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsAboutToBeInserted(const QModelIndex& sourceParent, int sourceFirst, int sourceLast);
    void onRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent);

    // Recalculate the proxy mapping (used when a source model reset happens)
    void updateProxyIndexes();

    // Recursively insert proxy indexes mapping based on sourceModel()'s indexes
    void fillChildrenIndexesRecursively(const QModelIndex &sourceParent, const std::shared_ptr<ProxyIndexInfo>& parentInfo);

    // Mapping source index -> proxy index
    QHash<QPersistentModelIndex, QModelIndex> m_sourceIndexHash;
    // Mapping proxy index -> ProxyIndexInfo
    std::unordered_map<QModelIndex, std::shared_ptr<ProxyIndexInfo>, ProxyIndexHash> m_proxyIndexHash;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ItemFilterProxyModel::SelectionParameters);

} // end namespace kaz   

#endif // __STRUCTUREPROXYMODEL_H__
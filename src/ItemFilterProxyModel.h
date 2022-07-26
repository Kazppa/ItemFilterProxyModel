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
        explicit ProxyIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyParentIndex = QModelIndex()) :
            m_source(sourceIndex), m_parent(proxyParentIndex) {}
    
        QModelIndex m_source;
        QModelIndex m_parent;
        QModelIndexList m_children{};
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
    
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const = 0;

    // Return the mapped range for this proxyModel, in worst case the second value est equal to the first (both being invalid)
    std::pair<QModelIndex, QModelIndex> mapToSourceRange(const QModelIndex &sourceLeft, const QModelIndex& sourceRight) const;

private:
    void sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles = {});

    void refreshProxyIndexes();

    struct ProxyIndexHash
    {
        std::size_t operator()(const QModelIndex &idx) const noexcept
        {
            return qHash(idx);
        }
    };

    // SourceIndex -> ProxyIndex
    mutable QHash<QModelIndex, QModelIndex> m_sourceIndexHash;
    // ProxyIndex -> infos
    mutable std::unordered_map<QModelIndex, std::unique_ptr<ProxyIndexInfo>, ProxyIndexHash> m_proxyIndexHash;
};

#endif // __STRUCTUREPROXYMODEL_H__
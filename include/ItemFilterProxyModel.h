#ifndef __STRUCTUREPROXYMODEL_H__
#define __STRUCTUREPROXYMODEL_H__

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QVector>


class ItemFilterProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

    struct ModelIndexInfo
    {
        QPersistentModelIndex _parent;
        QVector<QPersistentModelIndex> _children;
        bool _isVisible;
    };

public:
    ItemFilterProxyModel(QObject *parent);

    void setSourceModel(QAbstractItemModel *newSourceModel) override;

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;

    QModelIndex parent(const QModelIndex &childIndex) const override;

    int rowCount(const QModelIndex &parentIndex = {}) const override;

    int columnCount(const QModelIndex &parentIndex = {}) const override;

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent = {}) const = 0;

    // Returned the mapped range for this proxyModel, in worst case the second value est equal to the first (both being invalid)
    std::pair<QModelIndex, QModelIndex> mapToSourceRange(const QModelIndex &sourceLeft, const QModelIndex& sourceRight) const;

private:
    void sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles = {});

    bool isSourceIndexVisible(const QModelIndex &sourceIndex) const;

    const ModelIndexInfo& getProxyIndexInfo(const QModelIndex &proxyIndex) const;

    QModelIndex findProxyParentIndex(const QModelIndex &proxyIndex) const;

    QVector<QPersistentModelIndex> findProxyChildrenIndexes(const QModelIndex &proxyIndex) const;

    // SourceIndex -> ProxyIndex
    mutable QHash<QPersistentModelIndex, QPersistentModelIndex> m_sourceIndexHash;
    // ProxyIndex -> infos
    mutable QHash<QPersistentModelIndex, ModelIndexInfo> m_proxyIndexHash;
};

#endif // __STRUCTUREPROXYMODEL_H__
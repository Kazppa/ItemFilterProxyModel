#ifndef __STRUCTUREPROXYMODEL_H__
#define __STRUCTUREPROXYMODEL_H__

#include <unordered_map>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QVector>

namespace std
{
    template <>
    struct hash<QModelIndex>
    {
        std::size_t operator()(const QModelIndex &idx) const noexcept
        {
            return qHash(idx);
        }
    };
}

class ItemFilterProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

public:
    ItemFilterProxyModel(QObject *parent);

    void setSourceModel(QAbstractItemModel *newSourceModel) override;

    QVariant data(const QModelIndex &idx, int role) const override;

    void multiData(const QModelIndex& idx, QModelRoleDataSpan roleDataSpan) const override;

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;

    QModelIndex parent(const QModelIndex &childIndex) const override;

    int rowCount(const QModelIndex &parentIndex = {}) const override;

    int columnCount(const QModelIndex &parentIndex = {}) const override;

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent = {}) const = 0;

    // Returned the mapped range for this proxyModel, in worst case the second value est equal to the first (both being invalid)
    std::pair<QModelIndex, QModelIndex> mapToSourceRange(const QModelIndex &sourceLeft, const QModelIndex& sourceRight) const;

private:
    void sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles = {});

    bool isSourceIndexVisible(const QModelIndex &sourceIndex) const;

    void refreshProxyIndexes() const;

    struct ProxyModelIndexInfo
    {
        ProxyModelIndexInfo(const QModelIndex &sourceIndex, const QModelIndex &proxyParent) :
            _sourceIndex(sourceIndex), _parent(proxyParent) {}

        QModelIndex _sourceIndex;
        QModelIndex _parent;
    };

    // SourceIndex -> ProxyIndex
    mutable std::unordered_map<QModelIndex, QModelIndex> m_sourceIndexHash;
    // ProxyIndex -> infos
    mutable std::unordered_map<QModelIndex, ProxyModelIndexInfo> m_proxyIndexHash;
    // ProxtIndex -> ProxyIndex[]
    mutable std::unordered_map<QModelIndex, QVector<QModelIndex>> m_proxyChildrenHash;
};

#endif // __STRUCTUREPROXYMODEL_H__
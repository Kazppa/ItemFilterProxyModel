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

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent = {}) const = 0;

private:
    void sourceDataChanged(const QModelIndex &sourceLeft, const QModelIndex &sourceRight, const QList<int>& roles = {});

    bool isSourceIndexVisible(const QModelIndex &sourceIndex) const;

    void refreshProxyIndexes();

    struct ProxyIndex
    {
        ProxyIndex(const QModelIndex& sourceIndex, const QModelIndex &index, const QModelIndex& parent = {}, QModelIndexList children = {});
        
        QModelIndex m_source;
        QModelIndex m_index;
        QModelIndex m_parent{};
        QModelIndexList m_children{};
    };

    int m_idCount = 0;
    std::vector<std::unique_ptr<ProxyIndex>> m_indexes;
};

#endif // __STRUCTUREPROXYMODEL_H__
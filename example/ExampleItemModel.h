#ifndef __EXAMPLEITEMMODEL_H__
#define __EXAMPLEITEMMODEL_H__

#include <memory>
#include <vector>

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/QVector>


class ExampleItemModel : public QAbstractItemModel
{
    struct Node
    {
        Node(const QString &text) : m_text(text) {}

        ~Node();

        Node* m_parent = nullptr;
        QString m_text;
        QVector<Node*> m_children{};
    };
public:
    ExampleItemModel(QObject *parent);

    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex & parent) const override;
    QVariant data(const QModelIndex &idx, int role) const override;

    Node* nodeFromIndex(const QModelIndex &idx) const;

private:
    int nodeRow(const Node* node) const noexcept;

    void generateExampleData();

    std::vector<std::unique_ptr<Node>> m_rootNodes;
};


#endif // __EXAMPLEITEMMODEL_H__
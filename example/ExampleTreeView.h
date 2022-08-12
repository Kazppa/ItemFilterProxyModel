#ifndef ITEMFILTERPROXYMODEL_EXAMPLETREEVIEW_H
#define ITEMFILTERPROXYMODEL_EXAMPLETREEVIEW_H

#include <QtWidgets/QTreeView>

class QAction;

class ExampleTreeView : public QTreeView
{
    Q_OBJECT
public:
    ExampleTreeView(QWidget *parent = nullptr);

    Q_SIGNAL void currentIndexChanged(const QModelIndex &newIndex);

    void resizeColumnsToContents();

    void onContextMenuRequested(const QPoint& pos);

    QModelIndex searchTextIndex(const QString &text) const;

protected:
    void currentChanged(const QModelIndex &, const QModelIndex&) override;

    void onRemoveAction();

    QMenu *m_menu;
    QAction *m_expandAction;
    QAction *m_collapseAction;
    QAction *m_expandAllAction;
    QAction *m_collapseAllAction;
    QAction *m_removeAction;
};

#endif //ITEMFILTERPROXYMODEL_EXAMPLETREEVIEW_H

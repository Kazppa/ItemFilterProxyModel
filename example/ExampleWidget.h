#ifndef __EXAMPLEWIDGET_H__
#define __EXAMPLEWIDGET_H__

#include <QTreeView>
#include <QWidget>

class QCheckBox;
class QStandardItem;
class QStandardItemModel;
class StructureProxyModel;

class ExampleTreeView : public QTreeView
{
    Q_OBJECT
public:
    Q_SIGNAL void currentIndexChanged(const QModelIndex &newIndex);

    ExampleTreeView(QWidget *parent = nullptr) : QTreeView(parent) {}

protected:
    void currentChanged(const QModelIndex &, const QModelIndex&) override;

};


class ExampleWidget : public QWidget
{
public:
    ExampleWidget(QWidget* parent = nullptr);

private:
    ExampleTreeView* createView();

    void fillSourceModel();

    void onSyncViewsCheckBox(bool isChecked);

    void onViewScrolled(int newValue);

    void onViewIndexChanged(const QModelIndex &newIndex);

    QStandardItemModel * _sourceModel;
    StructureProxyModel* _proxyModel;
    ExampleTreeView *_basicTreeView;
    ExampleTreeView *_restructuredTreeView;
    QCheckBox *_syncViewsheckBox;
};

#endif // __EXAMPLEWIDGET_H__
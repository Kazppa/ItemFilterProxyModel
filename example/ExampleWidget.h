#ifndef __EXAMPLEWIDGET_H__
#define __EXAMPLEWIDGET_H__

#include <QTreeView>
#include <QWidget>

class QCheckBox;
class QStandardItem;
class QStandardItemModel;
class ExampleItemModel;

#include "ItemFilterProxyModel.h"


class ExampleItemFilterProxyModel : public ItemFilterProxyModel
{
public:
    using ItemFilterProxyModel::ItemFilterProxyModel;

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent = {}) const override;
};


class ExampleTreeView : public QTreeView
{
    Q_OBJECT
public:
    Q_SIGNAL void currentIndexChanged(const QModelIndex &newIndex);

    ExampleTreeView(QWidget *parent = nullptr);

protected:
    void currentChanged(const QModelIndex &, const QModelIndex&) override;

};


class ExampleWidget : public QWidget
{
public:
    ExampleWidget(QWidget* parent = nullptr);

private:
    void onSyncViewsCheckBox(bool isChecked);

    void onSliderPressed();

    void onSliderReleased();

    void onViewScrolled(int newValue);

    void onViewIndexChanged(const QModelIndex &newIndex);

    ExampleItemModel * _sourceModel;
    ExampleItemFilterProxyModel* _proxyModel;
    ExampleTreeView *_basicTreeView;
    ExampleTreeView *_restructuredTreeView;
    QCheckBox *_syncViewsheckBox;
};

#endif // __EXAMPLEWIDGET_H__
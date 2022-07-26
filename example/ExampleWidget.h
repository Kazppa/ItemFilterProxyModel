#ifndef __EXAMPLEWIDGET_H__
#define __EXAMPLEWIDGET_H__

#include <QTreeView>
#include <QWidget>

class QCheckBox;
class QStandardItem;
class ExampleItemModel;
class ExampleItemFilterProxyModel;
class ExampleTreeView;


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
    QCheckBox *_syncViewsCheckBox;
};

#endif // __EXAMPLEWIDGET_H__
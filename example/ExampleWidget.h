#ifndef __EXAMPLEWIDGET_H__
#define __EXAMPLEWIDGET_H__

#include <QTreeView>
#include <QWidget>

class QCheckBox;
class QStandardItem;
class ExampleItemModel;
class ExampleItemFilterProxyModel;
class ExampleTreeView;
class QLineEdit;

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

    ExampleItemModel * m_sourceModel;
    ExampleItemFilterProxyModel* m_proxyModel;
    ExampleTreeView *m_sourceTreeView;
    ExampleTreeView *m_proxyTreeView;
    QLineEdit *m_filterLineEdit;
    QCheckBox *m_syncViewsCheckBox;
};

#endif // __EXAMPLEWIDGET_H__
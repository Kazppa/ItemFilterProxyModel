# ItemFilterProxyModel

## Presentation
A Qt item proxy model which allow to hide an item but keeps its parent and children visible. To do so the proxy model has a different tree structure layout.  

## Example
Taking a source model with the following tree :

```
├── A
│   ├── A1
│   └── A2
├── B
│   ├── B1
│   │   └── B3
│   └── B2
└── C
    ├── C1
    └── C2
        └── C3
```

If we hide **A1**, **B1** and **C** using *ItemFilterProxyModel* as a proxy model, we obtain this tree :
```
├── A
│   └── A2
├── B
│   ├── B3
│   └── B2
├── C1
└── C2
    └── C3
```

## How to use it
This class works the same way as Qt's *QSortFilterProxyModel* : you have to inherit this proxy model and overload the virtual function
```cpp
bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
```
  
Here is a simple implementation for the example above :

```cpp
bool MyCustomProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto sourceText = sourceModel()->data(sourceIndex).toString();

    if (sourceText == u"A1" || sourceText == u"B1" || sourceText == "C") {
        return false;
    }
    return true;
}
```  
<br/>

You specify the source model using the *QAbstractProxyModel* 's virutal function :  

```cpp
void QAbstractProxyModel::setSourceModel(QAbstractItemModel *newSourceModel);
```
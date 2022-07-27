# ItemFilterProxyModel

## Presentation
A Qt item proxy model which allow to hide an item but keeps its parent and children visible. To do so the proxy model has a different tree structure layout.

### Example
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
This class works the same way as Qt's *QSortFilterProxyModel* : you have to inherit this proxy model and overload the following virtual function :
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

Like any proxy model, you can specify the source model using the *QAbstractProxyModel* 's virtual function :  

```cpp
void QAbstractProxyModel::setSourceModel(QAbstractItemModel *newSourceModel);
```

A trivial example :
```cpp
auto itemModel = new QStandardItemModel(parent);
auto proxyModel = new ItemFilterProxyModel(parent);
proxyModel->setSourceModel(itemModel);

auto treeView = new QTreeView(parent);
treeView->setModel(proxyModel);
```

## Installation
Copy paste both file from the **src** folder into your project and you're good to go ! 

**C++17** (or higher) is required.  

<details><summary>The only dependencies are the C++ Standard Templated Library and Qt</summary>
<pre>
QtCore  
QtGui  
QtWidgets  
</pre>
</details>
This class was made with (and for) Qt6 but might works with Qt5 (i didn't test it though).  

## Contributing
If this project happens to be useful in one of your project and you do make some changes / improvements, feel free to create a pull request to help the next guy coming by this repo.

## License
The whole repository is licensed under `Apache-2.0`.  
It means you can use it for more or less whatever you want, read [License.md](License.md) for the full license.
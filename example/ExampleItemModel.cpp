#include "ExampleItemModel.h"

#include <queue>

#include <QRandomGenerator>

namespace
{
    QStandardItem * createItem(const QString &text)
    {
        auto item = new QStandardItem(text);
        // item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        return item;
    }
}

ExampleItemModel::ExampleItemModel(QObject *parent) :
    QStandardItemModel(parent)
{
    setHorizontalHeaderLabels({ tr("Item name"), tr("Item value") });
    generateExampleData();
}

QVariant ExampleItemModel::data(const QModelIndex &idx, int role) const 
{
    if (role == Qt::TextAlignmentRole && idx.column() == 1) {
        return Qt::AlignCenter;
    }
    return QStandardItemModel::data(idx, role);
}


void ExampleItemModel::generateExampleData()
{
#ifdef QT_DEBUG
    QRandomGenerator rand(415);
#else
    auto rand = QRandomGenerator::global();
#endif
    const auto columnCount = QStandardItemModel::columnCount();
    const auto rootNodeCount = rand.bounded(4, 10);
    std::queue<std::pair<QStandardItem *, int>> queue;
    QChar c = u'A';
    for (int row = 0; row < rootNodeCount; ++row) {
        auto node = createItem(c);
        auto colItem = createItem(QStringLiteral("%1 col 1").arg(c));
        appendRow({ node, colItem });

        c.unicode() += 1;
        const auto subLevelCount = rand.bounded(0, 6);
        if (subLevelCount != 0) {
            queue.push(std::make_pair(node, subLevelCount));
        }
    }

    QMap<QChar, int> numbers;
    while (!queue.empty())
    {
        auto [parentNode, subLevelCount] = queue.front();
        queue.pop();
        const auto& parentName = parentNode->text();
        const auto parentLetter = parentName[0];
        auto& number = numbers[parentLetter];

        const auto childCount = rand.bounded(1, 4);
        for (int row = 0; row < childCount; ++row) {
            const auto childName = parentLetter + QString::number(++number);
            auto childItem = createItem(childName);
            auto colItem = createItem(QStringLiteral("%1 col 1").arg(childName));
            parentNode->appendRow({ childItem, colItem });

            if (--subLevelCount > 0) {
                queue.push(std::make_pair(childItem, subLevelCount));
            }
        }
    }
}

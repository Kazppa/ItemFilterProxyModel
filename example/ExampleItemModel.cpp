#include "ExampleItemModel.h"

#include <queue>

#include <QRandomGenerator>


ExampleItemModel::ExampleItemModel(QObject *parent) :
    QStandardItemModel(parent)
{
    generateExampleData();
}


void ExampleItemModel::generateExampleData()
{
    QRandomGenerator rand;
    rand.seed(415);

    const auto rootNodeCount = rand.bounded(4, 10);
    std::queue<std::pair<QStandardItem *, int>> queue;
    QChar c = u'A';
    for (int i = 0; i < rootNodeCount; ++i) {
        auto node = new QStandardItem(c);
        c.unicode() += 1;
        appendRow(node);

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
        for (int i = 0; i < childCount; ++i) {
            const auto childName = parentLetter + QString::number(++number);
            auto childItem = new QStandardItem(childName);
            parentNode->appendRow(childItem);
            if (--subLevelCount > 0) {
                queue.push(std::make_pair(childItem, subLevelCount));
            }
        }
    }
}

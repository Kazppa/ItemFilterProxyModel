#include "ExampleItemModel.h"

#include <queue>

#include <QRandomGenerator>


ExampleItemModel::Node::~Node()
{
    if (m_parent) {
        const auto parentEnd = m_parent->m_children.end();
        m_parent->m_children.erase(std::find(m_parent->m_children.begin(), parentEnd, this), parentEnd);
    }
    qDeleteAll(m_children); 
}

//////////////////

ExampleItemModel::ExampleItemModel(QObject *parent) :
    QAbstractItemModel(parent)
{
    generateExampleData();
}

QModelIndex ExampleItemModel::index(int row, int column, const QModelIndex &parent) const 
{
    if (parent.isValid()) {
        const auto parentNode = nodeFromIndex(parent);
        Q_ASSERT(row >= 0 && row < parentNode->m_children.size());
        return createIndex(row, column, parentNode->m_children[row]);
    }

    // Root node
    Q_ASSERT(row >= 0 && row < m_rootNodes.size());
    return createIndex(row, column, m_rootNodes[row].get());
}

QModelIndex ExampleItemModel::parent(const QModelIndex &child) const 
{
    if (!child.isValid()) {
        return {};
    }

    const auto childNode = nodeFromIndex(child);
    const auto parentNode = childNode->m_parent;
    if (parentNode == nullptr) {
        return {};
    }

    return createIndex(nodeRow(parentNode), child.column(), parentNode);
}

int ExampleItemModel::rowCount(const QModelIndex &parent) const 
{
    if (parent.isValid()) {
        const auto parentNode = nodeFromIndex(parent);
        return parentNode->m_children.size();
    }
    return m_rootNodes.size();
}

int ExampleItemModel::columnCount(const QModelIndex & parent) const 
{
    return 1;
}

QVariant ExampleItemModel::data(const QModelIndex &idx, int role) const 
{
    if (!idx.isValid()) {
        return {};
    }

    if (role == Qt::DisplayRole) {
        const auto node = nodeFromIndex(idx);
        return node->m_text;
    }
    return {};
}

ExampleItemModel::Node* ExampleItemModel::nodeFromIndex(const QModelIndex &idx) const
{
    Q_ASSERT(idx.isValid());
    Q_ASSERT(idx.model() == this);
    return static_cast<Node *>(idx.internalPointer());
}

int ExampleItemModel::nodeRow(const Node* node) const noexcept
{
    const auto parentNode = node->m_parent;
    if (parentNode == nullptr) {
        // Root node
        const auto begin = m_rootNodes.begin();
        return std::distance(begin, std::find_if(begin, m_rootNodes.end(), [=](const std::unique_ptr<Node>& rootNode) {
            return rootNode.get() == node;
        }));
    }

    const auto parentBegin = parentNode->m_children.begin();
    return std::distance(parentBegin, std::find(parentBegin, parentNode->m_children.end(), node));
}


void ExampleItemModel::generateExampleData()
{
    m_rootNodes.clear();

    QRandomGenerator rand;
    rand.seed(415);

    const auto rootNodeCount = rand.bounded(4, 10);
    m_rootNodes.reserve(rootNodeCount);
    std::queue<std::pair<Node *, int>> queue;
    QChar c = u'A';
    for (int i = 0; i < rootNodeCount; ++i) {
        m_rootNodes.push_back(std::make_unique<Node>(c));
        c.unicode() += 1;

        const auto subLevelCount = rand.bounded(0, 6);
        if (subLevelCount != 0) {
            auto rootNode = m_rootNodes.back().get();
            rootNode->m_children.reserve(subLevelCount);
            queue.push(std::make_pair(rootNode, subLevelCount));
        }
    }

    QMap<QChar, int> numbers;
    while (!queue.empty())
    {
        auto [parentNode, subLevelCount] = queue.front();
        queue.pop();
        const auto& parentName = parentNode->m_text;
        const auto parentLetter = parentName[0];
        auto& number = numbers[parentLetter];

        const auto childCount = rand.bounded(1, 4);
        parentNode->m_children.reserve(childCount);
        for (int i = 0; i < childCount; ++i) {
            const auto childName = parentLetter + QString::number(++number);
            auto childItem = new Node(childName);
            childItem->m_parent = parentNode;
            parentNode->m_children.push_back(childItem);
            if (--subLevelCount > 0) {
                queue.push(std::make_pair(childItem, subLevelCount));
            }
        }
    }
}

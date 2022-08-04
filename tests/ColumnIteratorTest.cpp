#include "ColumnIteratorTest.h"

#include <QIdentityProxyModel>
#include <QStandardItemModel>

#include "ProxyIndexInfo.h"

using namespace kaz;

void ColumnIteratorTest::testOperators_data()
{
    QTest::addColumn<QStandardItemModel *>("sourceModel");

    auto itemModel = new QStandardItemModel(this);
    QList<QStandardItem *> items;
    for (auto row = 0; row < 3; ++row) {
        const QChar letter(u'A' + row);
        for (auto col = 0; col < 4; ++col) {
            items.push_back(new QStandardItem(QStringLiteral("%1%2").arg(letter, QString::number(col))));
        }
        itemModel->appendRow(items);
        items.clear();
    }
    QTest::newRow("standard item model") << itemModel;
}

void ColumnIteratorTest::testOperators()
{
    QFETCH(QStandardItemModel *, sourceModel);
    const auto rowCount = sourceModel->rowCount();
    const auto columnCount = sourceModel->columnCount();

    auto proxyModel = new QIdentityProxyModel(this);
    proxyModel->setSourceModel(sourceModel);

    ProxyIndexInfo proxyInfo({}, {}, {});
    for (int row = 0; row < rowCount; ++row) {
        for (int col = 0; col < columnCount; ++col) {
            const auto sourceIndex = sourceModel->index(row, col);
            const auto proxyIndex = proxyModel->index(row, col);
            auto childProxy = std::make_shared<ProxyIndexInfo>(sourceIndex, proxyIndex, nullptr);
            proxyInfo.m_children.push_back(childProxy);
        }
    }
    
    for (int col = 0; col < columnCount; ++col) {
        int expectedRow = 0;
        QChar letter(u'A');
        const auto [ beginIt, endIt ] = proxyInfo.columnBeginEnd(col);
        QVERIFY(beginIt != endIt || rowCount == 0);
        for (auto it = beginIt; it != endIt; ++it) {
            const auto row = (*it)->row();
            const auto colIt = (*it)->column();
            QVERIFY(col == colIt);
            QVERIFY(row == expectedRow);

            const auto index = proxyModel->index(row, col);
            const auto txt = index.data().toString();
            QVERIFY(txt == QStringLiteral("%1%2").arg(letter, QString::number(col)));
            ++expectedRow;
            ++letter.unicode();
        }
    }
}

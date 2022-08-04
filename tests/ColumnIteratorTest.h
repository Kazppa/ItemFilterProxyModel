#ifndef __COLUMNITERATORTEST_H__
#define __COLUMNITERATORTEST_H__

#include <QTest>

namespace kaz
{

class ColumnIteratorTest : public QObject
{
    Q_OBJECT

private slots:

    void testOperators_data();

    void testOperators();
};

}

#endif // __COLUMNITERATORTEST_H__
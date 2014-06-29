#include "sqlstoragetest.h"

#include <QTest>

SqlStorageTest::SqlStorageTest(QObject *parent) :
    QObject(parent)
{
}

QTEST_MAIN(SqlStorageTest)

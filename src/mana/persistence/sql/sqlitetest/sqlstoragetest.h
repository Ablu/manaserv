#ifndef SQLSTORAGETEST_H
#define SQLSTORAGETEST_H

#include <QObject>

class SqlStorage;

class SqlStorageTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void characterSaveAndGetTest();

private:
    SqlStorage *mStorage = nullptr;
};

#endif // SQLSTORAGETEST_H

#include "sqlstoragetest.h"

#include "mana/entities/account.h"
#include "mana/entities/character.h"

#include "mana/persistence/sql/sqlstorage.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTest>
#include <QTextStream>

void executeAllFromFile(QFile &file, QSqlDatabase &db)
{
    QVERIFY2(file.open(QFile::ReadOnly | QFile::Text),
             file.errorString().toStdString().c_str());

    QTextStream input(&file);
    QString sql = input.readAll();
    QStringList queries = sql.split(";", QString::SkipEmptyParts);

    for (QString query : queries)
    {
        db.exec(query);
    }
}

void SqlStorageTest::init()
{
    QVERIFY2(mStorage == nullptr, "Storage was already initialized!");

    auto database = QSqlDatabase::addDatabase("QSQLITE", "testDb");
    database.setDatabaseName(":memory:");
    QVERIFY2(database.open(), "Opening the database failed!");

    QFile createScript(QCoreApplication::applicationDirPath()
                       + "/../../share/manaserv/sql/sqlite/createTables.sql");

    executeAllFromFile(createScript, database);

    mStorage = new SqlStorage;
    mStorage->setDatabase(database);
    mStorage->open();
}

void SqlStorageTest::cleanup()
{
    QVERIFY2(mStorage != nullptr, "Storage was already deleted!");

    delete mStorage;
    mStorage = nullptr;
    QSqlDatabase::removeDatabase("testDb");
}


void SqlStorageTest::characterSaveAndGetTest()
{
    std::unique_ptr<CharacterData> character(new CharacterData("Test"));

    Account account;
    account.setName("test");
    account.setEmail("test");
    account.setPassword("test");
    mStorage->addAccount(account);

    account.addCharacter(std::move(character));
    mStorage->flush(account);

    auto characterFromStorage = mStorage->getCharacter("Test");
    QVERIFY2(characterFromStorage, "The character just stored was unable to retreive!");
    QCOMPARE(characterFromStorage->getDatabaseID(), 1);
}

QTEST_MAIN(SqlStorageTest)

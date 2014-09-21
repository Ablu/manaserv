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


static std::unique_ptr<Account> createTestAccount()
{
    std::unique_ptr<Account> account(new Account);
    account->setName("test");
    account->setEmail("mailtest");
    account->setPassword("secretpw");

    return account;
}

static void verifyAccount(const std::unique_ptr<Account> &account)
{
    QVERIFY2(account, "The account was unable to retreive!");
    QCOMPARE(account->getName(), QString("test"));
    QCOMPARE(account->getEmail(), QString("mailtest"));
    QCOMPARE(account->getPassword(), QString("secretpw"));
}


static std::unique_ptr<CharacterData> createTestCharacter()
{
    std::unique_ptr<CharacterData> character(new CharacterData("Test"));
    return character;
}

static void verifyCharacter(const std::unique_ptr<CharacterData> &character)
{
    QVERIFY2(character, "The character was unable to retreive!");
    QCOMPARE(character->getName(), QString("Test"));
}


void SqlStorageTest::characterSaveAndGetTest()
{
    auto account = createTestAccount();
    mStorage->addAccount(*account);

    auto character = createTestCharacter();
    account->addCharacter(std::move(character));
    mStorage->flush(*account);
    int characterId = account->getCharacters()[0]->getDatabaseId();

    verifyAccount(mStorage->getAccount(account->getId()));
    verifyAccount(mStorage->getAccount("test"));

    verifyCharacter(mStorage->getCharacter("Test"));
    verifyCharacter(mStorage->getCharacter(characterId, nullptr));

    QCOMPARE(mStorage->getCharacterId("Test"), (unsigned)characterId);
}

void SqlStorageTest::accountDeletion()
{
    auto account = createTestAccount();
    mStorage->addAccount(*account);
    mStorage->delAccount(*account);

    QVERIFY2(!mStorage->getAccount("test"), "Account is still there!");
}

void SqlStorageTest::accountDeletionWithCharacters()
{
    auto account = createTestAccount();
    mStorage->addAccount(*account);

    auto character = createTestCharacter();
    account->addCharacter(std::move(character));
    mStorage->flush(*account);

    mStorage->delAccount(*account);

    QVERIFY2(!mStorage->getAccount("test"), "Account is still there!");
    QVERIFY2(!mStorage->getCharacter("Test"), "Character is still there!");
}

void SqlStorageTest::characterDeletion()
{
    auto account = createTestAccount();
    mStorage->addAccount(*account);

    auto character = createTestCharacter();
    account->addCharacter(std::move(character));
    mStorage->flush(*account);
    int characterId = account->getCharacters()[0]->getDatabaseId();

    mStorage->delCharacter(characterId);
    QVERIFY2(!mStorage->getCharacter("Test"), "Character is still there!");
}

QTEST_MAIN(SqlStorageTest)

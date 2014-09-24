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

#include <memory>

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
    QDateTime registrationDate(QDate(2005, 3, 6), QTime(22, 6, 58));
    account->setLastLogin(registrationDate);
    account->setRegistrationDate(registrationDate);

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
    character->setAttribute(100, 1.0);
    character->setModAttribute(100, 5.0);
    return character;
}

static void verifyCharacter(const std::unique_ptr<CharacterData> &character)
{
    QVERIFY2(character, "The character was unable to retreive!");
    QCOMPARE(character->getName(), QString("Test"));
    auto &attributes = character->getAttributes();
    QCOMPARE(attributes.size(), 1ul);
    QCOMPARE(attributes.at(100).base, 1.0);
    QCOMPARE(attributes.at(100).modified, 5.0);
}


void SqlStorageTest::characterSaveAndGetTest()
{
    auto account = provideTestAccountWithCharacter();
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
    auto account = provideTestAccountWithCharacter();
    mStorage->delAccount(*account);

    QVERIFY2(!mStorage->getAccount("test"), "Account is still there!");
    QVERIFY2(!mStorage->getCharacter("Test"), "Character is still there!");
}

void SqlStorageTest::characterDeletion()
{
    auto account = provideTestAccountWithCharacter();
    int characterId = account->getCharacters()[0]->getDatabaseId();

    mStorage->delCharacter(characterId);
    QVERIFY2(!mStorage->getCharacter("Test"), "Character is still there!");
}

void SqlStorageTest::updateLastLogin()
{
    auto account = createTestAccount();
    mStorage->addAccount(*account);
    QDateTime date(QDate(2014, 9, 21), QTime(12, 10, 1));
    account->setLastLogin(date);
    mStorage->updateLastLogin(*account);

    auto accountFromStorage = mStorage->getAccount("test");
    QCOMPARE(accountFromStorage->getLastLogin(), date);
}

void SqlStorageTest::updateCharacterPoints()
{
    auto account = provideTestAccountWithCharacter();
    int characterId = account->getCharacters()[0]->getDatabaseId();

    int newCharacterPoints = 10;
    int newCorrectionPoints = 5;
    mStorage->updateCharacterPoints(characterId, newCharacterPoints,
                                    newCorrectionPoints);
    auto characterFromStorage = mStorage->getCharacter("Test");
    QCOMPARE(characterFromStorage->getAttributePoints(), newCharacterPoints);
    QCOMPARE(characterFromStorage->getCorrectionPoints(), newCorrectionPoints);
}

void SqlStorageTest::updateAttribute()
{
    auto account = provideTestAccountWithCharacter();
    int characterId = account->getCharacters()[0]->getDatabaseId();

    int attributeId = 100;
    double value = 1.3;
    double modifier = 5.0;
    mStorage->updateAttribute(characterId, attributeId, value, modifier);

    auto characterFromDatabase = mStorage->getCharacter("Test");
    auto &attributes = characterFromDatabase->getAttributes();
    auto it = attributes.find(attributeId);
    QVERIFY2(it != attributes.end(), "Attribute was not found!");
    const AttributeValue &attributeValue = it->second;
    QCOMPARE(attributeValue.base, value);
    QCOMPARE(attributeValue.modified, modifier);
}

void SqlStorageTest::insertStatusEffect()
{
    auto account = provideTestAccountWithCharacter();
    int characterId = account->getCharacters()[0]->getDatabaseId();

    const int statusId = 100;
    const unsigned ticks = 1000;
    mStorage->insertStatusEffect(characterId, statusId, ticks);

    auto characterFromDatabase = mStorage->getCharacter("Test");
    auto &statusEffects = characterFromDatabase->getStatusEffects();
    auto it = statusEffects.find(statusId);
    QVERIFY2(it != statusEffects.end(), "Status effect did not exist!");
    const Status &status = it->second;
    QCOMPARE(status.time, ticks);
}

void SqlStorageTest::banCharacter()
{
    auto account = provideTestAccountWithCharacter();
    int characterId = account->getCharacters()[0]->getDatabaseId();
    QDateTime banEnd = QDateTime::currentDateTime();
    mStorage->banCharacter(characterId, banEnd);

    {
        auto accountFromStorage = mStorage->getAccount("test");
        QCOMPARE((AccessLevel)accountFromStorage->getLevel(), AL_BANNED);
    }

    mStorage->checkBannedAccounts();

    {
        auto accountFromStorage = mStorage->getAccount("test");
        QCOMPARE((AccessLevel)accountFromStorage->getLevel(), AL_PLAYER);
    }
}

std::unique_ptr<Account> SqlStorageTest::provideTestAccountWithCharacter()
{
    auto account = createTestAccount();
    mStorage->addAccount(*account);

    auto character = createTestCharacter();
    account->addCharacter(std::move(character));
    mStorage->flush(*account);

    return account;
}

QTEST_MAIN(SqlStorageTest)

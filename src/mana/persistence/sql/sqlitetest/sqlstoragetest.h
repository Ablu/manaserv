#ifndef SQLSTORAGETEST_H
#define SQLSTORAGETEST_H

#include "mana/entities/account.h"

#include <QObject>

#include <memory>

class SqlStorage;

class SqlStorageTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void characterSaveAndGetTest();

    void accountDeletion();
    void accountDeletionWithCharacters();
    void characterDeletionById();
    void characterDeletionByReference();

    void updateLastLogin();

    void updateCharacterPoints();
    void updateAttribute();
    void insertStatusEffect();

    void banCharacter();

private:
    std::unique_ptr<Account> provideTestAccountWithCharacter();

    SqlStorage *mStorage = nullptr;
};

#endif // SQLSTORAGETEST_H

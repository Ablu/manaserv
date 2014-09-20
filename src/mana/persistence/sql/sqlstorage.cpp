/*
 *  The Mana Server
 *  Copyright (C) 2004-2014  The Mana World Development Team
 *
 *  This file is part of The Mana Server.
 *
 *  The Mana Server is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana Server is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana Server.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>
#include <time.h>

#include "mana/persistence/sql/sqlstorage.h"

#include "mana/configuration/interfaces/iconfiguration.h"

#include "mana/entities/account.h"
#include "mana/entities/character.h"
#include "mana/entities/guild.h"
#include "mana/entities/post.h"

#include "account-server/flooritem.h"
#include "chat-server/chatchannel.h"
#include "common/manaserv_protocol.h"
#include "utils/point.h"
#include "utils/string.h"
#include "utils/throwerror.h"

#include <stdint.h>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlResult>

// Defines the supported db version
static const char *DB_VERSION_PARAMETER = "database_version";

// Table names
static const QString ACCOUNTS_TBL_NAME            =   "mana_accounts";
static const QString CHARACTERS_TBL_NAME          =   "mana_characters";
static const QString CHAR_ATTR_TBL_NAME           =   "mana_char_attr";
static const QString CHAR_STATUS_EFFECTS_TBL_NAME =   "mana_char_status_effects";
static const QString CHAR_KILL_COUNT_TBL_NAME     =   "mana_char_kill_stats";
static const QString CHAR_ABILITIES_TBL_NAME      =   "mana_char_abilities";
static const QString QUESTLOG_TBL_NAME            =   "mana_questlog";
static const QString INVENTORIES_TBL_NAME         =   "mana_inventories";
static const QString ITEMS_TBL_NAME               =   "mana_items";
static const QString GUILDS_TBL_NAME              =   "mana_guilds";
static const QString GUILD_MEMBERS_TBL_NAME       =   "mana_guild_members";
static const QString QUESTS_TBL_NAME              =   "mana_quests";
static const QString WORLD_STATES_TBL_NAME        =   "mana_world_states";
static const QString POST_TBL_NAME                =   "mana_post";
static const QString POST_ATTACHMENTS_TBL_NAME    =   "mana_post_attachments";
static const QString AUCTION_TBL_NAME             =   "mana_auctions";
static const QString AUCTION_BIDS_TBL_NAME        =   "mana_auction_bids";
static const QString ONLINE_USERS_TBL_NAME        =   "mana_online_list";
static const QString TRANSACTION_TBL_NAME         =   "mana_transactions";
static const QString FLOOR_ITEMS_TBL_NAME         =   "mana_floor_items";

SqlStorage::SqlStorage()
    : mItemDbVersion(0)
{
}

SqlStorage::~SqlStorage()
{
    if (mDb.isOpen())
        close();
}

inline void tryExecuteSql(QSqlQuery &query, const QString &sql)
{
    if (!Q_LIKELY(query.exec(sql)))
    {
        utils::throwError("Error during execution of sql statement '" + sql
                          + "': " + query.lastError().text());
    }
}

inline void tryExecutePrepared(QSqlQuery &query)
{
    if (!Q_LIKELY(query.exec()))
    {
        utils::throwError("Error during execution of prepared sql statement '"
                          + query.executedQuery() + "': "
                          + query.lastError().text());
    }
}

inline void tryPrepare(QSqlQuery &query, const QString &sql)
{
    if (!Q_LIKELY(query.prepare(sql)))
    {
        utils::throwError("Error during preparation of sql statement '"
                          + sql + "': "
                          + query.lastError().text());
    }
}

void SqlStorage::open()
{
    // Do nothing if already connected.
    if (mDb.isOpen())
        return;

    // Open a connection to the database.
    mDb.open();

    // Check database version here
    int dbversion = getWorldStateVar(DB_VERSION_PARAMETER, SystemMap).toInt();
    int supportedDbVersion = ManaServ::SUPPORTED_DB_VERSION;
    if (dbversion != supportedDbVersion)
    {
        QString errmsg;
        errmsg += "Database version is not supported. ";
        errmsg += "Needed version: '" ;
        errmsg += supportedDbVersion;
        errmsg += "', current version: '";
        errmsg += dbversion;
        errmsg += "'";
        qDebug() << errmsg; // TODO: user proper logging later again
        throw errmsg;
    }

    // Clean list of online users, this should be empty after restart
    mDb.exec("DELETE FROM " + ONLINE_USERS_TBL_NAME);
}

void SqlStorage::close()
{
    mDb.close();
}

std::unique_ptr<Account> SqlStorage::getAccountBySQL(QSqlQuery &query)
{
    tryExecutePrepared(query);

    // If the account is not even in the database then
    // we have no choice but to return nothing.
    if (!query.next())
        return 0;

    unsigned id = query.value(0).toUInt();

    // Create an Account instance
    // and initialize it with information about the user.
    std::unique_ptr<Account> account(new Account(id));
    account->setName(query.value(1).toString());
    account->setPassword(query.value(2).toString());
    account->setEmail(query.value(3).toString());
    account->setRegistrationDate(query.value(6).toUInt());
    account->setLastLogin(query.value(7).toUInt());

    int level = query.value(4).toUInt();
    // Check if the user is permanently banned, or temporarily banned.
    if (level == AL_BANNED
            || time(0) <= query.value(5).toInt())
    {
        account->setLevel(AL_BANNED);
        // It is, so skip character loading.
        return account;
    }
    account->setLevel(level);

    // Correct on-the-fly the old 0 slot characters
    // NOTE: Will be deprecated and removed at some point.
    fixCharactersSlot(id);

    // Load the characters associated with the account.
    QString sql = "select id from " + CHARACTERS_TBL_NAME + " where user_id = '"
            + QString::number(id) + "';";

    QSqlQuery charInfoQuery(mDb);
    tryExecuteSql(charInfoQuery, sql);
    if (!charInfoQuery.next())
    {
        std::map<unsigned, std::unique_ptr<CharacterData>> characters;

        qDebug() << "Account " << id << " has " << charInfoQuery.size()
                 << " character(s) in database.";

        // Two steps: it seems like multiple requests cannot be alive
        // at the same time.
        std::vector< unsigned > characterIDs;
        while(charInfoQuery.next())
            characterIDs.push_back(charInfoQuery.value(0).toUInt());

        int size = charInfoQuery.size();
        for (int k = 0; k < size; ++k)
        {
            if (auto ptr = getCharacter(characterIDs[k], account.get()))
            {
                characters[ptr->getCharacterSlot()] = std::move(ptr);
            }
            else
            {
                qWarning() << "Failed to get character " << characterIDs[k]
                           << " for account " << id << '.';
            }
        }

        account->setCharacters(std::move(characters));
    }

    return account;
}

void SqlStorage::fixCharactersSlot(int accountId)
{
    QString sql = "SELECT id, slot FROM " + CHARACTERS_TBL_NAME
            + " where user_id = " + QString::number(accountId);
    QSqlQuery sqlQuery(mDb);
    tryExecuteSql(sqlQuery, sql);

    // If the account is not even in the database then
    // we can quit now.
    if (!sqlQuery.next())
        return;

    std::map<unsigned, unsigned> slotsToUpdate;

    unsigned currentSlot = 1;

    // We parse all the characters slots to see how many are to be
    // corrected.
    while (sqlQuery.next())
    {
        // If the slot found is equal to 0.
        if (sqlQuery.value(1).toInt() == 0)
        {
            int currentIndex = sqlQuery.at();
            sqlQuery.seek(-1);
            // Find the new slot number to assign.
            while(sqlQuery.next())
            {
                if (sqlQuery.value(1).toUInt() == currentSlot)
                    currentSlot++;
            }
            sqlQuery.seek(currentIndex);
            slotsToUpdate.insert(std::make_pair(sqlQuery.value(0).toUInt(),
                                                currentSlot));
        }
    }

    if (!slotsToUpdate.empty())
    {
        mDb.transaction();

        // Update the slots in database.
        for (std::map<unsigned, unsigned>::iterator i =
             slotsToUpdate.begin(),
             i_end = slotsToUpdate.end(); i != i_end; ++i)
        {
            // Update the character slot.
            sql = "UPDATE " + CHARACTERS_TBL_NAME
                    + " SET slot = " + i->second
                    + " where id = " + i->first;
            QSqlQuery query(mDb);
            tryExecuteSql(query, sql);
        }

        mDb.commit();
    }
}

std::unique_ptr<Account> SqlStorage::getAccount(const QString &userName)
{
    QString sql = "SELECT * FROM " + ACCOUNTS_TBL_NAME + " WHERE username = :username";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":username", userName);
    return getAccountBySQL(query);
    return 0;
}

std::unique_ptr<Account> SqlStorage::getAccount(int accountID)
{
    QString sql = "SELECT * FROM " + ACCOUNTS_TBL_NAME + " WHERE id = :id";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":id", accountID);
    return getAccountBySQL(query);
}

std::unique_ptr<CharacterData> SqlStorage::getCharacterBySQL(QSqlQuery &sqlQuery, const Account *owner)
{
    tryExecutePrepared(sqlQuery);

    // If the character is not even in the database then
    // we have no choice but to return nothing.
    if (!sqlQuery.next())
        return nullptr;


    std::unique_ptr<CharacterData> character(new CharacterData(sqlQuery.value(2).toString(), sqlQuery.value(0).toInt()));
    character->setGender(sqlQuery.value(3).toInt());
    character->setHairStyle(sqlQuery.value(4).toInt());
    character->setHairColor(sqlQuery.value(5).toInt());
    character->setAttributePoints(sqlQuery.value(6).toInt());
    character->setCorrectionPoints(sqlQuery.value(7).toInt());
    Point pos(sqlQuery.value(8).toInt(), sqlQuery.value(9).toInt());
    character->setPosition(pos);

    int mapId = sqlQuery.value(10).toInt();
    character->setMapId(mapId);

    character->setCharacterSlot(sqlQuery.value(11).toInt());

    // Fill the account-related fields. Last step, as it may require a new
    // SQL query.
    if (owner)
    {
        character->setAccountLevel(owner->getLevel());
    }
    else
    {
        int id = sqlQuery.value(1).toInt();
        QString sql = "select level from " + ACCOUNTS_TBL_NAME
                + " where id = '" + QString::number(id) + "';";
        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);
        character->setAccountLevel(query.value(0).toInt(), true);
    }

    // Load attributes.
    {
        QString sql = "SELECT attr_id, attr_base, attr_mod "
                "FROM " + CHAR_ATTR_TBL_NAME + " "
                "WHERE char_id = " + QString::number(character->getDatabaseId());

        QSqlQuery query(mDb);
        while(query.next())
        {
            unsigned id = query.value(0).toUInt();
            character->setAttribute(id,    query.value(1).toDouble());
            character->setModAttribute(id, query.value(2).toDouble());
            query.next();
        }
    }

    // Load the status effects
    {
        QString sql = "select status_id, status_time FROM "
                + CHAR_STATUS_EFFECTS_TBL_NAME
                + " WHERE char_id = " + QString::number(character->getDatabaseId());

        QSqlQuery query(mDb);
        while(query.next())
        {
            character->applyStatusEffect(
                        query.value(0).toUInt(), // Status Id
                        query.value(1).toUInt()); // Time
            query.next();
        }
    }

    // Load the kill stats
    {
        QString sql = "select monster_id, kills FROM " + CHAR_KILL_COUNT_TBL_NAME
                + " WHERE char_id = " + QString::number(character->getDatabaseId());

        QSqlQuery query(mDb);
        while(query.next())
        {
            character->setKillCount(
                        query.value(0).toUInt(), // MonsterID
                        query.value(1).toUInt()); // Kills
            query.next();
        }
    }

    // Load the ability status
    {
        QString sql = "SELECT ability_id FROM "
                + CHAR_ABILITIES_TBL_NAME
                + " WHERE char_id = " + QString::number(character->getDatabaseId());

        QSqlQuery query(mDb);
        while(query.next())
        {
            character->giveAbility(query.value(0).toUInt());
            query.next();
        }
    }

    // Load the questlog
    {
        QString sql = "SELECT quest_id, quest_state, quest_title, quest_description "
                "FROM " + QUESTLOG_TBL_NAME
                + " WHERE char_id = " + QString::number(character->getDatabaseId());

        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);
        while(query.next())
        {
            QuestInfo quest;
            quest.id = query.value(0).toUInt();
            quest.state = query.value(1).toUInt();
            quest.title = query.value(2).toString();
            quest.description = query.value(3).toString();
            character->addQuest(quest);
            query.next();
        }
    }

    Possessions &poss = character->getPossessions();

    QString sql = " select id, owner_id, slot, class_id, amount, equipped from "
            + INVENTORIES_TBL_NAME + " where owner_id = '"
            + QString::number(character->getDatabaseId()) + "' order by slot asc;";

    QSqlQuery query(mDb);
    tryExecuteSql(query, sql);
    InventoryData inventoryData;
    EquipData equipmentData;
    while(query.next())
    {
        InventoryItem item;
        unsigned short slot = query.value(2).toUInt();
        item.itemId   = query.value(3).toUInt();
        item.amount   = query.value(4).toUInt();
        item.equipmentSlot = query.value(5).toUInt();
        inventoryData[slot] = item;

        if (item.equipmentSlot != 0)
            equipmentData.insert(slot);
    }
    poss.setInventory(inventoryData);
    poss.setEquipment(equipmentData);

    return character;
}

std::unique_ptr<CharacterData> SqlStorage::getCharacter(int id, const Account *owner)
{
    QString sql = "SELECT * FROM " + CHARACTERS_TBL_NAME + " WHERE id = :id";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":id", id);
    return getCharacterBySQL(query, owner);
}

std::unique_ptr<CharacterData> SqlStorage::getCharacter(const QString &name)
{
    QString sql = "SELECT * FROM " + CHARACTERS_TBL_NAME + " WHERE name = :name";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":name", name);
    return getCharacterBySQL(query, 0);
}

unsigned SqlStorage::getCharacterId(const QString &name)
{
    QString sql = "SELECT id FROM " + CHARACTERS_TBL_NAME + " WHERE name = :name";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":name", name);
    tryExecutePrepared(query);

    if (!query.next())
        return 0;

    return query.value(0).toUInt();
}

bool SqlStorage::doesUserNameExist(const QString &name)
{
    QString sql = "SELECT COUNT(username) FROM " + ACCOUNTS_TBL_NAME
            + " WHERE username = :username";
    QSqlQuery query(mDb);

    tryPrepare(query, sql);
    query.bindValue(":username", name);
    tryExecutePrepared(query);
    query.next();
    return query.value(0).toInt();
}

bool SqlStorage::doesEmailAddressExist(const QString &email)
{
    QString sql = "SELECT COUNT(email) FROM " + ACCOUNTS_TBL_NAME
            + " WHERE UPPER(email) = UPPER(:email)";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":email", email);
    tryExecutePrepared(query);
    query.next();
    return query.value(0).toInt() != 0;
}

bool SqlStorage::doesCharacterNameExist(const QString& name)
{
    QString sql = "SELECT COUNT(name) FROM " + CHARACTERS_TBL_NAME
            + " WHERE name = :name";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":name", name);
    tryExecutePrepared(query);
    query.next();

    return query.value(0).toInt() != 0;
}

void SqlStorage::updateCharacter(const CharacterData &character)
{
    mDb.transaction();

    {
        // Update the database Character data (see CharacterData for details)
        QString sql =
                "update "         + CHARACTERS_TBL_NAME + " "
                "set "
                "gender = '"      + QString::number(character.getGender()) + "', "
                "hair_style = '"  + QString::number(character.getHairStyle()) + "', "
                "hair_color = '"  + QString::number(character.getHairColor()) + "', "
                "char_pts = '"    + QString::number(character.getAttributePoints()) + "', "
                "correct_pts = '" + QString::number(character.getCorrectionPoints()) + "', "
                "x = '"           + QString::number(character.getPosition().x) + "', "
                "y = '"           + QString::number(character.getPosition().y) + "', "
                "map_id = '"      + QString::number(character.getMapId()) + "', "
                "slot = '"        + QString::number(character.getCharacterSlot()) + "' "
                "where id = '"    + QString::number(character.getDatabaseId()) + "';";

        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);
    }

    // Character attributes.
    {
        for (auto &it : character.getAttributes())
        {
            updateAttribute(character.getDatabaseId(), it.first,
                            it.second.base, it.second.modified);
        }
    }

    // Character's kill count
    {
        std::map<int, int>::const_iterator kill_it;
        for (kill_it = character.getKillCountBegin();
             kill_it != character.getKillCountEnd(); ++kill_it)
        {
            updateKillCount(character.getDatabaseId(),
                            kill_it->first, kill_it->second);
        }
    }

    //  Character's abillities
    {
        QString sql = "DELETE FROM " + CHAR_ABILITIES_TBL_NAME
                + " WHERE char_id='"
                + QString::number(character.getDatabaseId()) + "';";
        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);

        for (int abilityId : character.getAbilities())
        {
            QString insertSql = "INSERT INTO " + CHAR_ABILITIES_TBL_NAME
                    + " (char_id, ability_id)"
                    + " VALUES ("
                    + " '" + QString::number(character.getDatabaseId()) + "',"
                    + " '" + QString::number(abilityId)
                    + "');";

            QSqlQuery query(mDb);
            tryExecuteSql(query, insertSql);
        }
    }

    //  Character's questlog
    {
        QString sql = "DELETE FROM " + QUESTLOG_TBL_NAME
                + " WHERE char_id='"
                + QString::number(character.getDatabaseId()) + "';";
        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);

        for (const QuestInfo &quest : character.getQuests())
        {
            QString insertSql = "INSERT INTO " + QUESTLOG_TBL_NAME
                    + " (char_id, quest_id, quest_state, "
                    + "quest_title, quest_description)"
                    + " VALUES ("
                    + QString::number(character.getDatabaseId()) + ","
                    + " " + QString::number(quest.id) + ","
                    + " " + QString::number(quest.state) + ","
                    + " :title,"
                    + " :description"
                    + ")";
            QSqlQuery query(mDb);
            tryPrepare(query, insertSql);
            query.bindValue(":title", quest.title);
            query.bindValue(":description", quest.description);

            tryExecutePrepared(query);
        }
    }

    // Character's inventory
    // Delete the old inventory and equipment table first
    {
        QString sql = "delete from " + INVENTORIES_TBL_NAME
                + " where owner_id = '" + QString::number(character.getDatabaseId()) + "';";
        mDb.exec(sql);
    }

    // Insert the new inventory data
    {
        QString sqlPrefix = "insert into " + INVENTORIES_TBL_NAME
                + " (owner_id, slot, class_id, amount, equipped) values ("
                + QString::number(character.getDatabaseId()) + ", ";

        const Possessions &poss = character.getPossessions();
        const InventoryData &inventoryData = poss.getInventory();
        for (InventoryData::const_iterator itemIt = inventoryData.begin(),
             j_end = inventoryData.end(); itemIt != j_end; ++itemIt)
        {
            unsigned short slot = itemIt->first;
            unsigned itemId = itemIt->second.itemId;
            unsigned amount = itemIt->second.amount;
            assert(itemId);

            QString sql = sqlPrefix + slot + ", " + itemId + ", " + amount + ", "
                    + itemIt->second.equipmentSlot + ");";
            mDb.exec(sql);
        }

    }


    // Update char status effects
    {
        // Delete the old status effects first
        QString sql = "delete from " + CHAR_STATUS_EFFECTS_TBL_NAME
                + " where char_id = '" + QString::number(character.getDatabaseId()) + "';";

        mDb.exec(sql);
    }

    {
        std::map<int, Status>::const_iterator status_it;
        for (status_it = character.getStatusEffectBegin();
             status_it != character.getStatusEffectEnd(); ++status_it)
        {
            insertStatusEffect(character.getDatabaseId(),
                               status_it->first, status_it->second.time);
        }
    }

    mDb.commit();
}

void SqlStorage::addAccount(Account &account)
{
    assert(account.getCharacters().size() == 0);

    // Insert the account
    QString sql = "insert into " + ACCOUNTS_TBL_NAME
            + " (username, password, email, level, "
            + "banned, registration, lastlogin)"
            + " VALUES (:name, :password, :email, "
            + QString::number(account.getLevel()) + ", 0, "
            + QString::number(account.getRegistrationDate()) + ", "
            + QString::number(account.getLastLogin()) + ");";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":name", account.getName());
    query.bindValue(":password", account.getPassword());
    query.bindValue(":email", account.getEmail());

    tryExecutePrepared(query);
    account.setId(query.lastInsertId().toInt());
}

void SqlStorage::flush(Account &account)
{
    assert(account.getId() >= 0);

    mDb.transaction();

    {
        // Update the account
        QString sqlUpdateAccountTable =
                "update " + ACCOUNTS_TBL_NAME
                + " set username = :username, password = :password, email = :email, "
                + "level = :level, lastlogin = :lastlogin where id = :id;";

        QSqlQuery query(mDb);
        tryPrepare(query, sqlUpdateAccountTable);
        query.bindValue(":username", account.getName());
        query.bindValue(":password", account.getPassword());
        query.bindValue(":email", account.getEmail());
        query.bindValue(":level", account.getLevel());
        query.bindValue(":lastlogin", QVariant::fromValue(account.getLastLogin()));
        query.bindValue(":id", account.getId());

        tryExecutePrepared(query);
    }

    // Get the list of characters that belong to this account.
    std::map<unsigned, std::unique_ptr<CharacterData>> &characters = account.getCharacters();

    // Insert or update the characters.
    for (auto &it : characters)
    {
        CharacterData &character = *it.second;
        if (character.getDatabaseId() >= 0)
        {
            updateCharacter(character);
        }
        else
        {
            // Insert the character
            // This assumes that the characters name has been checked for
            // uniqueness
            QString sqlInsertCharactersTable =
                 "insert into " + CHARACTERS_TBL_NAME
                 + " (user_id, name, gender, hair_style, hair_color,"
                 + " char_pts, correct_pts,"
                 + " x, y, map_id, slot) values ("
                 + QString::number(account.getId()) + ", :charname, "
                 + QString::number(character.getGender()) + ", "
                 + QString::number(character.getHairStyle()) + ", "
                 + QString::number(character.getHairColor()) + ", "
                 + QString::number(character.getAttributePoints()) + ", "
                 + QString::number(character.getCorrectionPoints()) + ", "
                 + QString::number(character.getPosition().x) + ", "
                 + QString::number(character.getPosition().y) + ", "
                 + QString::number(character.getMapId()) + ", "
                 + QString::number(character.getCharacterSlot())
                 + ");";

            QSqlQuery query(mDb);
            tryPrepare(query, sqlInsertCharactersTable);
            query.bindValue(":charname", character.getName());
            tryExecutePrepared(query);

            // Update the character ID.
            character.setDatabaseId(query.lastInsertId().toInt());

            // Update all attributes.
            for (auto attrPair : character.getAttributes())
            {
                updateAttribute(character.getDatabaseId(), attrPair.first,
                                attrPair.second.base,
                                attrPair.second.modified);
            }
        }
    }

    // Existing characters in memory have been inserted
    // or updated in database.
    // Now, let's remove those who are no more in memory from database.
    {
        QString sqlSelectNameIdCharactersTable =
                "select name, id from " + CHARACTERS_TBL_NAME
                + " where user_id = '" + QString::number(account.getId()) + "';";

        QSqlQuery query(mDb);
        tryExecuteSql(query, sqlSelectNameIdCharactersTable);

        // We compare chars from memory and those existing in db,
        // and delete those not in mem but existing in db.
        bool charFound;
        while(query.next()) // In database
        {
            charFound = false;
            for (auto &it : characters)
            {
                if (query.value(0).toString() == it.second->getName())
                {
                    charFound = true;
                    break;
                }
            }
            if (!charFound)
            {
                // The char is in db but not in memory,
                // it will be removed from database.
                // We store the id of the char to delete,
                // because as deleted, the RecordSet is also emptied,
                // and that creates an error.
                unsigned charId = query.value(1).toUInt();
                delCharacter(charId);
            }
        }
    }

    mDb.commit();
}

void SqlStorage::delAccount(Account &account)
{
    // Sync the account info into the database.
    flush(account);

    // Delete the account.
    QString sql = "delete from " + ACCOUNTS_TBL_NAME
            + " where id = '" + QString::number(account.getId()) + "';";
    mDb.exec(sql);

    // Remove the account's characters.
    account.getCharacters().clear();
}

void SqlStorage::updateLastLogin(Account &account)
{
    QString sql = "UPDATE " + ACCOUNTS_TBL_NAME
            + "   SET lastlogin = '" + QString::number(account.getLastLogin()) + "'"
            + " WHERE id = '" + QString::number(account.getId()) + "';";
    mDb.exec(sql);
}

void SqlStorage::updateCharacterPoints(int charId,
                                    int charPoints, int corrPoints)
{
    QString sql = "UPDATE " + CHARACTERS_TBL_NAME
            + " SET char_pts = " + QString::number(charPoints) + ", "
            + " correct_pts = " + QString::number(corrPoints)
            + " WHERE id = " + QString::number(charId);

    mDb.exec(sql);
}

void SqlStorage::updateAttribute(int charId, unsigned attrId,
                              double base, double mod)
{
    {
        QString sql = "UPDATE " + CHAR_ATTR_TBL_NAME
                + " SET attr_base = '" + QString::number(base) + "', "
                + "attr_mod = '" + QString::number(mod) + "' "
                + "WHERE char_id = '" + QString::number(charId) + "' "
                + "AND attr_id = '" + QString::number(attrId) + "';";
        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);

        // If this has modified a row, we're done, it updated sucessfully.
        if (query.numRowsAffected() > 0)
            return;
    }

    // If it did not change anything,
    // then the record didn't previously exist. Create it.
    QString sql = "INSERT INTO " + CHAR_ATTR_TBL_NAME
            + " (char_id, attr_id, attr_base, attr_mod) VALUES ( "
            + QString::number(charId) + ", " + QString::number(attrId) + ", " + QString::number(base) + ", "
            + QString::number(mod) + ")";
    mDb.exec(sql);
}

void SqlStorage::updateKillCount(int charId, int monsterId, int kills)
{
    {
        // Try to update the kill count
        QString sql = "UPDATE " + CHAR_KILL_COUNT_TBL_NAME
                + " SET kills = " + QString::number(kills)
                + " WHERE char_id = " + QString::number(charId)
                + " AND monster_id = " + QString::number(monsterId);
        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);

        // Check if the update has modified a row
        if (query.numRowsAffected() > 0)
            return;
    }
    {
        QString sql = "INSERT INTO " + CHAR_KILL_COUNT_TBL_NAME + " "
                + "(char_id, monster_id, kills) VALUES ( "
                + charId + ", "
                + monsterId + ", "
                + kills + ")";
        mDb.exec(sql);
    }
}

void SqlStorage::insertStatusEffect(int charId, int statusId, int time)
{
    QString sql = "insert into " + CHAR_STATUS_EFFECTS_TBL_NAME
            + " (char_id, status_id, status_time) VALUES ( "
            + QString::number(charId) + ", "
            + QString::number(statusId) + ", "
            + QString::number(time) + ")";
    mDb.exec(sql);
}

void SqlStorage::addGuild(Guild *guild)
{
    {
        QString sql = "insert into " + GUILDS_TBL_NAME
                + " (name) VALUES (:name)";
        QSqlQuery query(mDb);
        tryPrepare(query, sql);
        query.bindValue(":name", guild->getName());
        tryExecutePrepared(query);

    }
    {
        QString sql = "SELECT id FROM " + GUILDS_TBL_NAME
                + " WHERE name = :name";
        QSqlQuery query(mDb);
        tryPrepare(query, sql);
        query.bindValue(":name", guild->getName());
        tryExecutePrepared(query);
        query.next();

        unsigned id = query.value(0).toUInt();
        guild->setId(id);
    }
}

void SqlStorage::removeGuild(Guild *guild)
{
    QString sql = "delete from " + GUILDS_TBL_NAME
            + " where id = '"
            + QString::number(guild->getId()) + "';";
    mDb.exec(sql);
}

void SqlStorage::addGuildMember(int guildId, int memberId)
{
    QString sql = "insert into " + GUILD_MEMBERS_TBL_NAME
            + " (guild_id, member_id, rights)"
            + " values ("
            + QString::number(guildId) + ", \""
            + QString::number(memberId) + "\", "
            + "0" + ");";
    mDb.exec(sql);
}

void SqlStorage::removeGuildMember(int guildId, int memberId)
{
    QString sql = "delete from " + GUILD_MEMBERS_TBL_NAME
            + " where member_id = \""
            + QString::number(memberId) + "\" and guild_id = '"
            + QString::number(guildId) + "';";
    mDb.exec(sql);
}

void SqlStorage::addFloorItem(int mapId, int itemId, int amount,
                           int posX, int posY)
{
    QString sql = "INSERT INTO " + FLOOR_ITEMS_TBL_NAME
            + " (map_id, item_id, amount, pos_x, pos_y)"
            + " VALUES ("
            + QString::number(mapId) + ", "
            + QString::number(itemId) + ", "
            + QString::number(amount) + ", "
            + QString::number(posX) + ", "
            + QString::number(posY) + ");";
    mDb.exec(sql);
}

void SqlStorage::removeFloorItem(int mapId, int itemId, int amount,
                              int posX, int posY)
{
    QString sql = "DELETE FROM " + FLOOR_ITEMS_TBL_NAME
            + " WHERE map_id = "
            + QString::number(mapId) + " AND item_id = "
            + QString::number(itemId) + " AND amount = "
            + QString::number(amount) + " AND pos_x = "
            + QString::number(posX) + " AND pos_y = "
            + QString::number(posY) + ";";
    mDb.exec(sql);
}

std::list<FloorItem> SqlStorage::getFloorItemsFromMap(int mapId)
{
    std::list<FloorItem> floorItems;

    QString sql = "SELECT * FROM " + FLOOR_ITEMS_TBL_NAME
            + " WHERE map_id = " + QString::number(mapId);
    QSqlQuery query(mDb);
    tryExecuteSql(query, sql);

    while(query.next())
    {
        floorItems.push_back(FloorItem(query.value(2).toInt(),
                                       query.value(3).toInt(),
                                       query.value(4).toInt(),
                                       query.value(5).toInt()));
    }

    return floorItems;
}

void SqlStorage::setMemberRights(int guildId, int memberId, int rights)
{
    QString sql = "UPDATE " + GUILD_MEMBERS_TBL_NAME
            + " SET rights = " + QString::number(rights)
            + " WHERE member_id = " + QString::number(memberId)
            + "  AND guild_id = " + QString::number(guildId) + ";";
    mDb.exec(sql);
}

std::map<int, Guild*> SqlStorage::getGuildList()
{
    std::map<int, Guild*> guilds;


    // Get the guilds stored in the db.
    QString sql = "select id, name from " + GUILDS_TBL_NAME + ";";
    QSqlQuery query(mDb);
    tryExecuteSql(query, sql);

    // Check that at least 1 guild was returned
    if (query.size() == 0)
        return guilds;

    // Loop through every row in the table and assign it to a guild
    while (query.next())
    {
        Guild *guild = new Guild(query.value(1).toString());
        guild->setId(query.value(0).toInt());
        guilds[guild->getId()] = guild;
    }

    // Add the members to the guilds.
    for (std::map<int, Guild*>::iterator it = guilds.begin();
         it != guilds.end(); ++it)
    {
        QString membersql = "select member_id, rights from "
                + GUILD_MEMBERS_TBL_NAME
                + " where guild_id = '" + QString::number(it->second->getId()) + "';";
        QSqlQuery query(mDb);
        tryExecuteSql(query, membersql);

        std::list<std::pair<int, int> > members;
        while(query.next())
        {
            members.push_back(std::pair<int, int>(query.value(0).toUInt(),
                                                  query.value(1).toUInt()));
        }

        std::list<std::pair<int, int> >::const_iterator i, i_end;
        for (i = members.begin(), i_end = members.end(); i != i_end; ++i)
        {
            CharacterData &character = *getCharacter((*i).first, 0);
           // if (character)
            //{
                character.addGuild(it->second->getName());
                it->second->addMember(character.getDatabaseId(), (*i).second);
            //}
        }
    }

    return guilds;
}

QString SqlStorage::getQuestVar(int id, const QString &name)
{
    QString sql = "select value from " + QUESTS_TBL_NAME
            + " WHERE owner_id = :ownerid AND name = :name";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":ownerid", id);
    query.bindValue(":name", name);
    tryExecutePrepared(query);

    if (query.next())
        return query.value(0).toString();
    return QString();
}

QString SqlStorage::getWorldStateVar(const QString &name, int mapId)
{
    QString sql = "SELECT value FROM "
            + WORLD_STATES_TBL_NAME
            + " WHERE state_name = :key";

    if (mapId >= 0)
        sql += " AND map_id = :mapId";

    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":key", name);
    query.bindValue(":mapId", mapId);

    tryExecutePrepared(query);
    if (query.next())
    {
        return query.value(0).toString();
    }

    return QString();
}

std::map<QString, QString> SqlStorage::getAllWorldStateVars(int mapId)
{
    std::map<QString, QString> variables;

    // Avoid a crash because prepared statements must have at least one binding.
    if (mapId < 0)
    {
        qWarning() << "getAllWorldStateVars was called with a negative map Id: "
                   << mapId;
        return variables;
    }

    QString sql = "SELECT `state_name`, `value` FROM "
            + WORLD_STATES_TBL_NAME;

    // Add map filter if map_id is given
    if (mapId >= 0)
        sql += " WHERE `map_id` = :map_id";

    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    if (mapId >= 0)
        query.bindValue(":map_id", mapId);
    tryExecutePrepared(query);

    while(query.next())
    {
        variables[query.value(0).toString()] = query.value(1).toString();
    }

    return variables;
}

void SqlStorage::setWorldStateVar(const QString &name,
                               const QString &value,
                               int mapId)
{
    // Set the value to empty means: delete the variable
    if (value.isEmpty())
    {
        QString deleteStateVar = "DELETE FROM " + WORLD_STATES_TBL_NAME
                + " WHERE state_name = '" + name + "'"
                + " AND map_id = '" + QString::number(mapId) + "';";
        mDb.exec(deleteStateVar);
        return;
    }

    // Try to update the variable in the database
    QString updateStateVar = "UPDATE " + WORLD_STATES_TBL_NAME
            + "   SET value = '" + value + "', "
            + "       moddate = '" + QString::number(time(0)) + "' "
            + " WHERE state_name = '" + name + "'"
            + " AND map_id = '" + QString::number(mapId) + "';";
    QSqlQuery query(mDb);
    tryExecuteSql(query, updateStateVar);

    // If we updated a row, were finished here
    if (query.numRowsAffected() > 0)
        return;

    // Otherwise we have to add the new variable
    QString insertStateVar = "INSERT INTO " + WORLD_STATES_TBL_NAME
            + " (state_name, map_id, value , moddate) VALUES ("
            + "'" + name + "', "
            + "'" + QString::number(mapId) + "', "
            + "'" + value + "', "
            + "'" + QString::number(time(0)) + "');";
    mDb.exec(insertStateVar);
}

void SqlStorage::setQuestVar(int id, const QString &name,
                          const QString &value)
{
    QString query1 = "delete from " + QUESTS_TBL_NAME
            + " where owner_id = '" + QString::number(id) + "' and name = :name;";
    QSqlQuery query(mDb);
    tryPrepare(query, query1);
    query.bindValue(":name", name);
    tryExecutePrepared(query);

    if (value.isEmpty())
        return;

    QString query2 = "insert into " + QUESTS_TBL_NAME
            + " (owner_id, name, value) values ('"
            + QString::number(id) + "', :name, :value);";
    QSqlQuery insertQuery(mDb);
    tryPrepare(insertQuery, query2);
    insertQuery.bindValue(":name", name);
    insertQuery.bindValue(":value", value);
    tryExecutePrepared(insertQuery);
}

void SqlStorage::banCharacter(int id, int duration)
{
    {
        // check the account of the character
        QString sql = "select user_id from " + CHARACTERS_TBL_NAME
                + " where id = '" + QString::number(id) + "';";
        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);
        if (!query.next())
        {
            qWarning() << "Tried to ban an unknown user.";
            return;
        }
    }
    {
        uint64_t bantime = (uint64_t)time(0) + (uint64_t)duration * 60u;
        // ban the character
        QString sql = "update " + ACCOUNTS_TBL_NAME
                + " set level = '" + AL_BANNED + "', banned = '"
                + QString::number(bantime)
                + "' where id = '" + QString::number(id) + "';";
        mDb.exec(sql);
    }
}

void SqlStorage::delCharacter(int charId)
{
    mDb.transaction();

    // Delete the inventory of the character
    QString sql = "DELETE FROM " + INVENTORIES_TBL_NAME
            + " WHERE owner_id = '" + QString::number(charId) + "';";
    mDb.exec(sql);

    // Delete from the quests table
    sql = "DELETE FROM " + QUESTS_TBL_NAME
            + " WHERE owner_id = '" + QString::number(charId) + "';";
    mDb.exec(sql);

    // Delete from the guilds table
    sql = "DELETE FROM " + GUILD_MEMBERS_TBL_NAME
            + " WHERE member_id = '" + QString::number(charId) + "';";
    mDb.exec(sql);

    // Delete auctions of the character
    sql = "DELETE FROM " + AUCTION_TBL_NAME
            + " WHERE char_id = '" + QString::number(charId) + "';";
    mDb.exec(sql);

    // Delete bids made on auctions made by the character
    sql = "DELETE FROM " + AUCTION_BIDS_TBL_NAME
            + " WHERE char_id = '" + QString::number(charId) + "';";
    mDb.exec(sql);

    // Now delete the character itself.
    sql = "DELETE FROM " + CHARACTERS_TBL_NAME
            + " WHERE id = '" + QString::number(charId) + "';";
    mDb.exec(sql);

    mDb.commit();
}

void SqlStorage::delCharacter(CharacterData *character)
{
    delCharacter(character->getDatabaseId());
}

void SqlStorage::checkBannedAccounts()
{
    // Update expired bans
    QString sql = "update " + ACCOUNTS_TBL_NAME
            + " set level = " + QString::number(AL_PLAYER) + ", banned = 0"
            + " where level = " + QString::number(AL_BANNED)
            + " AND banned <= " + QString::number(time(0)) + ";";
    mDb.exec(sql);
}

void SqlStorage::setAccountLevel(int id, int level)
{
    QString sql = "update " + ACCOUNTS_TBL_NAME
            + " set level = " + QString::number(level)
            + " where id = " + QString::number(id) + ";";
    mDb.exec(sql);
}

void SqlStorage::storeLetter(Letter *letter)
{
    std::ostringstream sql;
    if (letter->getId() == 0)
    {
        // The letter was never saved before
        QString sql = "INSERT INTO " + POST_TBL_NAME + " VALUES ( "
                + "NULL, "
                + QString::number(letter->getSender().getDatabaseId()) + ", "
                + QString::number(letter->getReceiver().getDatabaseId()) + ", "
                + QString::number(letter->getExpiry()) + ", "
                + QString::number(time(0)) + ", "
                + ":content)";
        QSqlQuery query(mDb);
        tryPrepare(query, sql);
        query.bindValue(":content", letter->getContents());
        tryExecutePrepared(query);

        letter->setId(query.lastInsertId().toUInt());

        // TODO: Store attachments in the database

        return;
    }

    {
        // The letter has a unique id, update the record in the db
        QString sql = "UPDATE " + POST_TBL_NAME
                + "   SET sender_id       = '"
                + QString::number(letter->getSender().getDatabaseId()) + "', "
                + "       receiver_id     = '"
                + QString::number(letter->getReceiver().getDatabaseId()) + "', "
                + "       letter_type     = '" + QString::number(letter->getType()) + "', "
                + "       expiration_date = '" + QString::number(letter->getExpiry()) + "', "
                + "       sending_date    = '" + QString::number(time(0)) + "', "
                + "       letter_text = :content "
                + " WHERE letter_id       = '" + QString::number(letter->getId()) + "'";

        QSqlQuery query(mDb);
        tryPrepare(query, sql);
        query.bindValue(":content", letter->getContents());

        tryExecutePrepared(query);

        if (query.numRowsAffected() == 0)
        {
            // This should never happen...
            utils::throwError("(DALSqlStorage::storePost) "
                              "trying to update nonexistant letter.");
        }
    }
}

Post *SqlStorage::getStoredPost(int playerId)
{
    Post *p = new Post;

    QString sql = "SELECT * FROM " + POST_TBL_NAME
            + " WHERE receiver_id = " + QString::number(playerId);
    QSqlQuery query(mDb);
    tryExecuteSql(query, sql);

    while (query.next())
    {
        // Load sender and receiver
        auto sender = getCharacter(query.value(1).toUInt(), 0);
        auto receiver = getCharacter(query.value(2).toUInt(), 0);

        Letter *letter = new Letter(query.value(3).toUInt(),
                                    std::move(sender),
                                    std::move(receiver));

        letter->setId(query.value(0).toUInt());
        letter->setExpiry(query.value(4).toUInt());
        letter->addText(query.value(6).toString());

        // TODO: Load attachments per letter from POST_ATTACHMENTS_TBL_NAME
        // needs redesign of struct ItemInventroy

        p->addLetter(letter);
    }

    return p;
}

void SqlStorage::deletePost(Letter *letter)
{
    mDb.transaction();

    // First delete all attachments of the letter
    // This could leave "dead" items in the item_instances table
    QString sql = "DELETE FROM " + POST_ATTACHMENTS_TBL_NAME
            + " WHERE letter_id = " + QString::number(letter->getId());
    mDb.exec(sql);

    // Delete the letter itself
    sql = "DELETE FROM " + POST_TBL_NAME
            + " WHERE letter_id = " + QString::number(letter->getId());
    mDb.exec(sql);

    mDb.commit();
    letter->setId(0);
}

void SqlStorage::setOnlineStatus(int charId, bool online)
{
    if (online)
    {
        // First we try to update the online status. this prevents errors
        // in case we get the online status twice
        QString sql = "SELECT COUNT(*) FROM " + ONLINE_USERS_TBL_NAME
                + " WHERE char_id = " + QString::number(charId);
        QSqlQuery query(mDb);
        tryExecuteSql(query, sql);
        query.next();

        if (query.value(0).toUInt() == 0)
            return;

        sql = "INSERT INTO " + ONLINE_USERS_TBL_NAME
                + " VALUES (" + QString::number(charId) + ", " + QString::number(time(0)) + ")";
        mDb.exec(sql);
    }
    else
    {
        QString sql = "DELETE FROM " + ONLINE_USERS_TBL_NAME
                + " WHERE char_id = " + QString::number(charId);
        mDb.exec(sql);
    }
}

void SqlStorage::addTransaction(const Transaction &trans)
{
    QString sql = "INSERT INTO " + TRANSACTION_TBL_NAME
            + " VALUES (NULL, " + QString::number(trans.mCharacterId) + ", "
            + QString::number(trans.mAction) + ", "
            + ":message, "
            + QString::number(time(0)) + ")";
    QSqlQuery query(mDb);
    tryPrepare(query, sql);
    query.bindValue(":message", trans.mMessage);
    tryExecutePrepared(query);
}

std::vector<Transaction> SqlStorage::getTransactions(unsigned num)
{
    std::vector<Transaction> transactions;

    QString sql = "SELECT * FROM " + TRANSACTION_TBL_NAME;
    QSqlQuery query(mDb);
    query.seek(num);

    // Get the last <num> records and store them in transactions
    while(query.next())
    {
        Transaction trans;
        trans.mCharacterId = query.value(1).toUInt();
        trans.mAction = query.value(2).toUInt();
        trans.mMessage = query.value(3).toString();
        transactions.push_back(trans);
    }

    return transactions;
}

std::vector<Transaction> SqlStorage::getTransactions(time_t date)
{
    std::vector<Transaction> transactions;

    QString sql = "SELECT * FROM " + TRANSACTION_TBL_NAME + " WHERE time > "
            + QString::number(date);
    QSqlQuery query(mDb);
    tryExecuteSql(query, sql);

    while(query.next())
    {
        Transaction trans;
        trans.mCharacterId = query.value(1).toUInt();
        trans.mAction = query.value(2).toUInt();
        trans.mMessage = query.value(3).toString();
        transactions.push_back(trans);
    }

    return transactions;
}

void SqlStorage::setDatabase(const QSqlDatabase &db)
{
    mDb = db;
}

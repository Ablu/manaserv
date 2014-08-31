/*
 *  The Mana Server
 *  Copyright (C) 2004-2010  The Mana World Development Team
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

#ifndef ISTORAGE_H
#define ISTORAGE_H

#include <list>
#include <map>
#include <memory>
#include <vector>

#include <QString>

#include "common/transaction.h"

class Account;
class CharacterData;
class ChatChannel;
class FloorItem;
class Guild;
class Letter;
class Post;

/**
 * The high level interface to the database. Through the storage you can access
 * all accounts, characters, guilds, worlds states, transactions, etc.
 */
class IStorage
{
public:
    virtual ~IStorage()
    {}

    /**
     * Connect to the database and initialize it if necessary.
     */
    virtual void open() = 0;

    /**
     * Disconnect from the database.
     */
    virtual void close() = 0;

    /**
     * Get an account by user name.
     *
     * @param userName the owner of the account.
     *
     * @return the account associated to the user name.
     */
    virtual Account *getAccount(const QString &userName) = 0;

    /**
     * Get an account by Id.
     *
     * @param accountID the Id of the account.
     *
     * @return the account associated with the Id.
     */
    virtual Account *getAccount(int accountId) = 0;

    /**
     * Gets a character by database Id.
     *
     * @param id the ID of the character.
     * @param owner the account the character is in.
     *
     * @return the character associated to the Id.
     */
    virtual std::unique_ptr<CharacterData> getCharacter(int id, Account *owner) = 0;

    /**
     * Gets a character by character name.
     *
     * @param name of the character
     *
     * @return the character associated to the name
     */
    virtual std::unique_ptr<CharacterData> getCharacter(const std::string &name) = 0;

    /**
     * Gets the id of a character by its name.
     *
     * @param name the name of the character.
     *
     * @return the id of the character
     */
    virtual unsigned getCharacterId(const std::string &name) = 0;

    /**
     * Add an account to the database.
     *
     * @param account the new account.
     */
    virtual void addAccount(Account *account) = 0;

    /**
     * Delete an account and its associated data from the database.
     *
     * @param account the account to delete.
     */
    virtual void delAccount(Account *account) = 0;

    /**
     * Update the date and time of the last login.
     *
     * @param account the account that recently logged in.
     */
    virtual void updateLastLogin(const Account *account) = 0;

    /**
     * Write a modification message about Character points to the database.
     *
     * @param CharId      ID of the character
     * @param CharPoints  Number of character points left for the character
     * @param CorrPoints  Number of correction points left for the character
     */
    virtual void updateCharacterPoints(int charId,
                                       int charPoints, int corrPoints) = 0;

    /**
     * Write a modification message about character attributes
     * to the database.
     *
     * @param charId    The Id of the character
     * @param attrId    The Id of the attribute
     * @param base      The base value of the attribute for this character
     * @param mod       The cached modified value for this character.
     */
    virtual void updateAttribute(int charId, unsigned attrId,
                                 double base, double mod) = 0;

    /**
     * Write a modification message about kill counts to the database.
     *
     * @param CharId      ID of the character
     * @param monsterId   ID of the monster type
     * @param kills       new amount of kills
     */
    virtual void updateKillCount(int charId, int monsterId, int kills) = 0;

    /**
     * Inserts a record about a status effect into the database
     *
     * @param charId    ID of the character in the database
     * @param statusId  ID of the status effect
     * @param time      Time left on the status effect
     */
    virtual void insertStatusEffect(int charId, int statusId, int time) = 0;

    /**
     * Sets a ban on an account (hence on all its characters).
     *
     * @param id character identifier.
     * @param duration duration in minutes.
     */
    virtual void banCharacter(int id, int duration) = 0;

    /**
     * Delete a character in the database.
     *
     * @param charId character identifier.
     */
    virtual void delCharacter(int charId) = 0;

    /**
     * Delete a character in the database. The object itself is not touched
     * by this function!
     *
     * @param character character object.
     */
    virtual void delCharacter(CharacterData *character) = 0;

    /**
     * Removes expired bans from accounts
     */
    virtual void checkBannedAccounts() = 0;

    /**
     * Tells if the user name already exists.
     *
     * @param name The user name to check
     *
     * @return true if the user name exists.
     */
    virtual bool doesUserNameExist(const std::string &name) = 0;

    /**
     * Tells if the email address already exists.
     *
     * @param email The email address to check
     *
     * @return true if the email address exists.
     */
    virtual bool doesEmailAddressExist(const std::string &email) = 0;

    /**
     * Tells if the character's name already exists.
     *
     * @param name The character name to check
     *
     * @return true if character's name exists.
     */
    virtual bool doesCharacterNameExist(const std::string &name) = 0;

    /**
     * Updates the data for a single character,
     * does not update the owning account or the characters name.
     * Primary usage should be storing characterdata
     * received from a game server.
     *
     * @param ptr Character to store values in the database.
     *
     * @return true on success
     */
    virtual void updateCharacter(const CharacterData &ptr) = 0;

    /**
     * Add a new guild.
     *
     * @param guild The guild to add in database.
     */
    virtual void addGuild(Guild *guild) = 0;

    /**
     * Delete a guild.
     *
     * @param guild The guild to remove from database.
     */
    virtual void removeGuild(Guild *guild) = 0;

    /**
     * Add member to guild.
     *
     * @param guildId The guild Id where to add the member
     * @param memberId The character Id to add in the guild.
     */
    virtual void addGuildMember(int guildId, int memberId) = 0;

    /**
     * Remove member from guild.
     *
     * @param guildId The guild Id where to remove the member
     * @param memberId The character Id to remove from the guild.
     */
    virtual void removeGuildMember(int guildId, int memberId) = 0;

    /**
     * Save guild member rights.
     *
     * @param guildId The guild Id where to set the member's right
     * @param memberId The character Id
     * @param memberId The new right level of the member.
     */
    virtual void setMemberRights(int guildId, int memberId, int rights) = 0;

    /**
     * Get the list of guilds.
     *
     * @return a list of guilds
     */
    virtual std::map<int, Guild*> getGuildList() = 0;

    /**
     * Add a floor item to map.
     *
     * Used to keep the floor item persistently between two server restart.
     *
     * @param mapId The map id
     * @param itemId The item id
     * @param posX Position X of the item in pixels
     * @param posY Position Y of the item in pixels
     */
    virtual void addFloorItem(int mapId, int itemId, int amount,
                              int posX, int posY) = 0;

    /**
     * Remove item from map persistence
     *
     * @param mapId The map id
     * @param itemId The item id
     * @param posX Position X of the item in pixels
     * @param posY Position Y of the item in pixels
     */
    virtual void removeFloorItem(int mapId, int itemId, int amount,
                                 int posX, int posY) = 0;


    /**
     * Get all persistent items from the given map id
     *
     * @param mapId The map id
     */
    virtual std::list<FloorItem> getFloorItemsFromMap(int mapId) = 0;

    /**
     * Update an account to the database.
     *
     * @param Account object to update.
     */
    virtual void flush(Account *) = 0;

    /**
     * Gets the value of a quest variable.
     *
     * @param id character id.
     * @param name quest var name to get.
     */
    virtual std::string getQuestVar(int id, const std::string &) = 0;

    /**
     * Sets the value of a quest variable.
     *
     * @param id character id.
     * @param name quest var name to set.
     * @param value value to set.
     */
    virtual void setQuestVar(int id, const std::string &, const std::string &) = 0;

    enum SpecialMapId {
        WorldMap = 0,
        SystemMap = -1
    };

    /**
     * Gets the string value of a world state variable. The \a mapId should
     * be a valid map ID or either WorldMap or SystemMap.
     *
     * @param name  Name of the requested world variable.
     * @param mapId ID of the specific map.
     */
    virtual QString getWorldStateVar(const std::string &name, int mapId) = 0;

    /**
     * Sets the value of a world state variable. The \a mapId should be a
     * valid map ID or either WorldMap or SystemMap.
     *
     * @param name  Name of the world vairable.
     * @param value New value of the world variable.
     */
    virtual void setWorldStateVar(const std::string &name,
                                  const std::string &value,
                                  int mapId) = 0;

    /**
     * Gets the value of all world state variables of a specific map. The
     * \a mapId should be a valid map ID or either WorldMap or SystemMap.
     *
     * @param mapId ID of the specific map
     */
    virtual std::map<std::string, std::string> getAllWorldStateVars(int mapId) = 0;

    /**
     * Set the level on an account.
     *
     * @param id The id of the account
     * @param level The level to set for the account
     */
    virtual void setAccountLevel(int id, int level) = 0;

    /**
     * Store letter.
     *
     * @param letter The letter to store
     */
    virtual void storeLetter(Letter *letter) = 0;

    /**
     * Retrieve post
     *
     * @param playerId The id of the character requesting his post
     */
    virtual Post *getStoredPost(int playerId) = 0;

    /**
     * Delete a letter from the database.
     * @param letter The letter to delete.
     */
    virtual void deletePost(Letter *letter) = 0;

    /**
     * Returns the version of the local item database.
     *
     * @return the database version number.
     */
    virtual unsigned getItemDatabaseVersion() const = 0;

    /**
     * Sets the status of a character to online (true) or offline (false).
     *
     * @param charId Id of the character.
     * @param online True to mark the character as being online.
     */
    virtual void setOnlineStatus(int charId, bool online) = 0;

    /**
     * Store a transaction.
     *
     * @param trans The transaction to add in the logs.
     */
    virtual void addTransaction(const Transaction &trans) = 0;

    /**
     * Retrieve the last \a num transactions that were stored.
     *
     * @return a vector of transactions.
     */
    virtual std::vector<Transaction> getTransactions(unsigned num) = 0;

    /**
     * Retrieve all transactions since the given \a date.
     *
     * @return a vector of transactions.
     */
    virtual std::vector<Transaction> getTransactions(time_t date) = 0;
};

#endif // ISTORAGE_H

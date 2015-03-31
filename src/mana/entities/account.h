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

#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <memory>
#include <QString>
#include <vector>

#include <QDateTime>

#include "character.h"

/**
 * A player's account. Stores the account information as well as the
 * player characters available under this account.
 */
class Account
{
public:
    Account(int id = -1)
        : mId(id)
        , mLevel(0)
    {}

    /**
     * Set the user name.
     *
     * @param name the user name.
     */
    void setName(const QString &name)
    { mName = name; }

    /**
     * Get the user name.
     *
     * @return the user name.
     */
    const QString &getName() const
    { return mName; }

    /**
     * Set the user password. The password is expected to be already
     * hashed with a salt.
     *
     * The hashing must be performed externally from this class or else
     * we would end up with the password being hashed many times
     * (e.g setPassword(getPassword()) would hash the password twice.
     *
     * @param password the user password (hashed with salt).
     */
    void setPassword(const QString &password)
    { mPassword = password; }


    /**
     * Get the user password (hashed with salt).
     *
     * @return the user password (hashed with salt).
     */
    const QString &getPassword() const
    { return mPassword; }

    /**
     * Set the random salt. This salt is sent to the client, so the client
     * can hash its password with this random salt.
     * This will help to protect against replay attacks.
     *
     * @param the new random salt to be sent out next login
     */
    void setRandomSalt(const QString &salt)
    { mRandomSalt = salt; }


    /**
     * Get the user random salt.
     *
     * @return the random salt used for next login.
     */
    const QString &getRandomSalt() const
    { return mRandomSalt; }

    /**
     * Set the user email address. The email address is expected to be
     * already hashed.
     *
     * @param email the user email address (hashed).
     */
    void setEmail(const QString &email)
    { mEmail = email; }


    /**
     * Get the user email address (hashed).
     *
     * @return the user email address (hashed).
     */
    const QString &getEmail() const
    { return mEmail; }


    /**
     * Set the account level.
     *
     * @param level the new level.
     */
    void setLevel(int level)
    { mLevel = level; }

    /**
     * Get the account level.
     *
     * @return the account level.
     */
    int getLevel() const
    { return mLevel; }

    /**
     * Tells whether a slot can be used.
     *
     * @param slot slot index of the character.
     */
    bool isSlotEmpty(unsigned slot) const;

    /**
     * Set the characters.
     *
     * @param characters a list of characters.
     */
    void setCharacters(std::map<unsigned, std::unique_ptr<CharacterData> > &&characters);

    /**
     * Adds a new character.
     *
     * @param character the new character.
     */
    void addCharacter(std::unique_ptr<CharacterData> character);

    /**
     * Removes a character from the account.
     *
     * @param slot slot index of the character.
     */
    void delCharacter(unsigned slot);

    /**
     * Get all the characters.
     *
     * @return all the characters.
     */
    std::map<unsigned, std::unique_ptr<CharacterData>> &getCharacters()
    { return mCharacters; }

    /**
     * Get all the characters.
     *
     * @return all the characters.
     */
    const std::map<unsigned, std::unique_ptr<CharacterData>> &getCharacters() const
    { return mCharacters; }

    /**
     * Get account ID.
     *
     * @return the unique ID of the account, a negative number if none yet.
     */
    int getId() const
    { return mId; }

    /**
     * Set account ID.
     * The account shall not have any ID yet.
     */
    void setId(int);

    /**
     * Get the time of the account registration.
     */
    const QDateTime &getRegistrationDate() const
    { return mRegistrationDate; }

    /**
     * Sets the time of the account registration.
     *
     * @param time of the account registration.
     */
    void setRegistrationDate(const QDateTime &time);

    /**
     * Get the time of the last login.
     */
    const QDateTime &getLastLogin() const
    { return mLastLogin; }

    /**
     * Sets the time of the last login.
     *
     * @param time of the last login.
     */
    void setLastLogin(const QDateTime &time);

private:
    Account(const Account &rhs) = delete;
    Account &operator=(const Account &rhs) = delete;

    QString mName;        /**< User name */
    QString mPassword;    /**< User password (hashed with salt) */
    QString mRandomSalt;  /**< A random sequence sent to client to
                                   protect against replay attacks.*/
    QString mEmail;       /**< User email address (hashed) */
    std::map<unsigned, std::unique_ptr<CharacterData>> mCharacters;   /**< Character data */
    int mId;                  /**< Unique id */
    unsigned char mLevel;     /**< Account level */
    QDateTime mRegistrationDate; /**< Date and time of the account registration */
    QDateTime mLastLogin;        /**< Date and time of the last login */
};

typedef std::vector< Account * > Accounts;

#endif // ACCOUNT_H
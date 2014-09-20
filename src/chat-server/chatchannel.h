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

#ifndef CHATCHANNEL_H
#define CHATCHANNEL_H

#include <QString>
#include <vector>

class ChatClient;

/**
 * A chat channel. Optionally a channel is private, in which case a password is
 * required to join it.
 *
 * No logic is currently associated with a chat channel except for making sure
 * that no user joins the channel twice and checking that a user who leaves
 * actually existed in the channel.
 *
 * @todo <b>b_lindeijer:</b> It would be nicer when some more logic could be
 *       placed in this class to remove some weight from the ChatHandler.
 */
class ChatChannel
{
public:
    typedef std::vector< ChatClient * > ChannelUsers;

    /**
     * Constructor.
     *
     * @param name         the name of the channel.
     * @param announcement a welcome message.
     * @param password     password (for private channels).
     * @param privacy      whether this channel is private.
     */
    ChatChannel(int id,
                const QString &name,
                const QString &announcement = QString(),
                const QString &password = QString(),
                bool joinable = true);

    /**
     * Get the ID of the channel.
     */
    int getId() const
    { return mId; }

    /**
     * Get the name of the channel.
     */
    const QString &getName() const
    { return mName; }

    /**
     * Get the announcement string of the channel.
     */
    const QString &getAnnouncement() const
    { return mAnnouncement; }

    /**
     * Get the password of the channel.
     */
    const QString& getPassword() const
    { return mPassword; }

    /**
     * Sets the name of the channel.
     */
    void setName(const QString &channelName)
    { mName = channelName; }

    /**
     * Sets the announcement string of the channel.
     */
    void setAnnouncement(const QString &channelAnnouncement)
    { mAnnouncement = channelAnnouncement; }

    /**
     * Sets the password of the channel.
     */
    void setPassword(const QString &channelPassword)
    { mPassword = channelPassword; }

    /**
     * Gets the list of the users registered in the channel.
     */
    const ChannelUsers &getUserList() const
    { return mRegisteredUsers; }

    /**
     * Adds a user to the channel.
     *
     * @return whether the user was successfully added
     */
    bool addUser(ChatClient *);

    /**
     * Removes a user from the channel.
     *
     * @return whether the user was successfully removed
     */
    bool removeUser(ChatClient *);

    /**
     * Empties a channel from its users (admin included).
     */
    void removeAllUsers();

    /**
     * Get whether a user can join this channel
     */
    bool canJoin() const;

    /**
     * Set user mode
     */
    void setUserMode(ChatClient *, unsigned char mode);

    /**
     * Get user mode
     */
    QString getUserMode(ChatClient *) const;

private:
    unsigned short mId;            /**< The ID of the channel. */
    QString mName;             /**< The name of the channel. */
    QString mAnnouncement;     /**< Welcome message. */
    QString mPassword;         /**< The channel password. */
    bool mJoinable;                /**< Whether anyone can join. */
    ChannelUsers mRegisteredUsers; /**< Users in this channel. */
    QString mOwner;             /**< Channel owner character name */
};

#endif

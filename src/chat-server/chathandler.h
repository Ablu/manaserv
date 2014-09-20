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

#ifndef CHATHANDLER_H
#define CHATHANDLER_H

#include <deque>
#include <map>
#include <QString>

#include "mana/entities/guild.h"

#include "net/connectionhandler.h"

#include "utils/tokencollector.h"

class ChatChannel;
class ChatClient;
class IStorage;

/**
 * Manages chat related things like private messaging, chat channel handling
 * as well as guild chat. The only form of chat not handled by this server is
 * local chat, which is handled by the game server.
 *
 * @todo <b>b_lindeijer:</b> Extend this class with handling of team chat once
 *       teams are implemented.
 */
class ChatHandler : public ConnectionHandler
{
private:
    /**
     * Data needed for initializing a ChatClient.
     */
    struct Pending
    {
        QString character;
        unsigned char level;
    };

    struct PartyInvite
    {
        PartyInvite(QString inviterName, QString inviteeName)
            : mInviter(inviterName)
            , mInvitee(inviteeName)
        {
            const int validTimeframe = 60;
            mExpireTime = time(nullptr) + validTimeframe;
        }

        QString mInviter;
        QString mInvitee;
        time_t mExpireTime;
    };

    IConfiguration *mConfiguration;
    IStorage *mStorage;
    std::map<QString, ChatClient*> mPlayerMap;
    std::deque<PartyInvite> mInvitations;
    std::map<QString, int> mNumInvites;

public:
    ChatHandler(IConfiguration *configuration, IStorage *storage);

    /**
     * Start the handler.
     */
    bool startListen(enet_uint16 port, const QString &host);

    /**
     * Tell a list of users about an event in a chatchannel.
     *
     * @param channel the channel to send the message in, must not be nullptr
     * @param info information pertaining to the event
     */
    void warnUsersAboutPlayerEventInChat(ChatChannel *channel,
                                         const QString &info,
                                         char eventId);

    /**
     * Called by TokenCollector when a client wrongly connected.
     */
    void deletePendingClient(ChatClient *);

    /**
     * Called by TokenCollector when a client failed to connect.
     */
    void deletePendingConnect(Pending *);

    /**
     * Called by TokenCollector when a client succesfully connected.
     */
    void tokenMatched(ChatClient *, Pending *);

    /**
     * Send information about a change in the guild list to guild members.
     */
    void sendGuildListUpdate(Guild *guild,
                             const QString &characterName,
                             char eventId);

    void handlePartyInvite(MessageIn &msg);

    /**
     * Sends an announce to all connected clients.
     */
    void handleAnnounce(const QString &message, int senderId,
                        const QString &senderName);

    /**
     * Returns ChatClient from the Player Map
     * @param The name of the character
     * @return The Chat Client
     */
    ChatClient *getClient(const QString &name) const;

protected:
    /**
     * Process chat related messages.
     */
    void processMessage(NetComputer *computer, MessageIn &message);

    /**
     * Returns a ChatClient instance.
     */
    NetComputer *computerConnected(ENetPeer *);

    /**
     * Cleans up after the disconnected client.
     */
    void computerDisconnected(NetComputer *);

    /**
     * Send messages for each guild the character belongs to.
     */
    void sendGuildRejoin(ChatClient &computer);

    /**
     * Send chat and guild info to chat client, so that they can join the
     * correct channels.
     */
    void sendGuildEnterChannel(const MessageOut &msg,
                               const QString &name);

    void sendGuildInvite(const QString &invitedName,
                         const QString &inviterName,
                         const QString &guildName);

private:
    // TODO: Unused
    void handleCommand(ChatClient &client, const QString &command);

    void handleChatMessage(ChatClient &client, MessageIn &msg);
    void handlePrivMsgMessage(ChatClient &client, MessageIn &msg);
    void handleWhoMessage(ChatClient &client);

    void handleEnterChannelMessage(ChatClient &client, MessageIn &msg);
    void handleModeChangeMessage(ChatClient &client, MessageIn &msg);
    void handleKickUserMessage(ChatClient &client, MessageIn &msg);
    void handleQuitChannelMessage(ChatClient &client, MessageIn &msg);

    void handleListChannelsMessage(ChatClient &client, MessageIn &msg);
    void handleListChannelUsersMessage(ChatClient &client, MessageIn &msg);

    void handleTopicChange(ChatClient &client, MessageIn &msg);

    void handleDisconnectMessage(ChatClient &client, MessageIn &msg);

    void handleGuildCreate(ChatClient &client, MessageIn &msg);
    void handleGuildInvite(ChatClient &client, MessageIn &msg);
    void handleGuildAcceptInvite(ChatClient &client, MessageIn &msg);
    void handleGuildGetMembers(ChatClient &client, MessageIn &msg);
    void handleGuildMemberLevelChange(ChatClient &client, MessageIn &msg);
    void removeCharacterFormGuild(ChatClient &client, Guild *guild);
    void handleGuildKickMember(ChatClient &client, MessageIn &msg);
    void handleGuildQuit(ChatClient &client, MessageIn &msg);

    void handlePartyInviteAnswer(ChatClient &client, MessageIn &msg);
    void handlePartyQuit(ChatClient &client);
    void removeExpiredPartyInvites();
    void removeUserFromParty(ChatClient &client);

    /**
     * Tell all the party members a member has left
     */
    void informPartyMemberQuit(ChatClient &client);

    /**
     * Tell the player to be more polite.
     */
    void warnPlayerAboutBadWords(ChatClient &computer);

    /**
     * Say something private to a player.
     */
    void sayToPlayer(ChatClient &computer, const QString &playerName,
                     const QString &text);

    /**
     * Finds out the name of a character by its id. Either searches it
     * in the list of online characters or otherwise gets it from the db.
     */
    unsigned getIdOfChar(const QString &name);

    /**
     * Sends a message to every client in a registered channel.
     *
     * @param channel the channel to send the message in, must not be nullptr
     * @param msg     the message to be sent
     */
    void sendInChannel(ChatChannel *channel, MessageOut &msg);

    /**
     * Retrieves the guild channel or creates one automatically
     * Automatically makes client join it
     * @param The name of the guild (and therefore the channel)
     * @param The client to join the channel
     * @return Returns the channel joined
     */
    ChatChannel *joinGuildChannel(const QString &name,
                                  ChatClient &client);

    /**
     * Set the topic of a guild channel
     */
    void guildChannelTopicChange(ChatChannel *channel, int playerId,
                                 const QString &topic);

    void updateInfo(ChatClient *client, int partyId);

    /**
     * Container for pending clients and pending connections.
     */
    TokenCollector<ChatHandler, ChatClient *, Pending *> mTokenCollector;
    friend void registerChatClient(const QString &, const QString &, int);
};

/**
 * Register future client attempt. Temporary until physical server split.
 */
void registerChatClient(const QString &, const QString &, int);

extern ChatHandler *chatHandler;

#endif

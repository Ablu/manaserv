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

#include <list>
#include <algorithm>
#include <QString>
#include <sstream>

#include "mana/persistence/interfaces/istorage.h"

#include "mana/entities/character.h"

#include "chat-server/guildmanager.h"
#include "chat-server/chatchannelmanager.h"
#include "chat-server/chatclient.h"
#include "chat-server/chathandler.h"
#include "common/manaserv_protocol.h"
#include "common/transaction.h"
#include "net/connectionhandler.h"
#include "net/messagein.h"
#include "net/messageout.h"
#include "net/netcomputer.h"
#include "utils/logger.h"
#include "utils/stringfilter.h"
#include "utils/tokendispenser.h"

using namespace ManaServ;

void registerChatClient(const QString &token,
                        const QString &name,
                        int level)
{
    ChatHandler::Pending *p = new ChatHandler::Pending;
    p->character = name;
    p->level = level;
    chatHandler->mTokenCollector.addPendingConnect(token, p);
}

ChatHandler::ChatHandler(IConfiguration *configuration, IStorage *storage):
    ConnectionHandler(configuration),
    mConfiguration(configuration),
    mStorage(storage),
    mTokenCollector(this)
{
}

bool ChatHandler::startListen(enet_uint16 port, const QString &host)
{
    LOG_INFO("Chat handler started:");
    return ConnectionHandler::startListen(port, host);
}

void ChatHandler::deletePendingClient(ChatClient *c)
{
    MessageOut msg(CPMSG_CONNECT_RESPONSE);
    msg.writeInt8(ERRMSG_TIME_OUT);

    // The computer will be deleted when the disconnect event is processed
    c->disconnect(msg);
}

void ChatHandler::deletePendingConnect(Pending *p)
{
    delete p;
}

void ChatHandler::tokenMatched(ChatClient *client, Pending *p)
{
    MessageOut msg(CPMSG_CONNECT_RESPONSE);

    client->characterName = p->character;
    client->accountLevel = p->level;

    auto character = mStorage->getCharacter(p->character);

    if (!character)
    {
        // character wasnt found
        msg.writeInt8(ERRMSG_FAILURE);
    }
    else
    {
        client->characterId = character->getDatabaseId();
        delete p;

        msg.writeInt8(ERRMSG_OK);

        // Add chat client to player map
        mPlayerMap.insert(std::pair<QString, ChatClient*>(client->characterName, client));
    }

    client->send(msg);

}

NetComputer *ChatHandler::computerConnected(ENetPeer *peer)
{
    return new ChatClient(peer);
}

void ChatHandler::computerDisconnected(NetComputer *comp)
{
    ChatClient *computer = static_cast< ChatClient * >(comp);

    if (computer->characterName.isEmpty())
    {
        // Not yet fully logged in, remove it from pending clients.
        mTokenCollector.deletePendingClient(computer);
    }
    else
    {
        // Remove user from all channels.
        chatChannelManager->removeUserFromAllChannels(computer);

        // Remove user from party
        removeUserFromParty(*computer);

        // Notify guilds about him leaving
        guildManager->disconnectPlayer(computer);

        // Remove the character from the player map
        // need to do this after removing them from party
        // as that uses the player map
        mPlayerMap.erase(computer->characterName);
    }

    delete computer;
}

void ChatHandler::processMessage(NetComputer *comp, MessageIn &message)
{
    ChatClient &computer = *static_cast< ChatClient * >(comp);

    if (computer.characterName.isEmpty())
    {
        if (message.getId() != PCMSG_CONNECT) return;

        QString magic_token = message.readString(MAGIC_TOKEN_LENGTH);
        mTokenCollector.addPendingClient(magic_token, &computer);
        sendGuildRejoin(computer);
        return;
    }

    switch (message.getId())
    {
        case PCMSG_CHAT:
            handleChatMessage(computer, message);
            break;

        case PCMSG_PRIVMSG:
            handlePrivMsgMessage(computer, message);
            break;

        case PCMSG_WHO:
            handleWhoMessage(computer);
            break;

        case PCMSG_ENTER_CHANNEL:
            handleEnterChannelMessage(computer, message);
            break;

        case PCMSG_USER_MODE:
            handleModeChangeMessage(computer, message);
            break;

        case PCMSG_KICK_USER:
            handleKickUserMessage(computer, message);

        case PCMSG_QUIT_CHANNEL:
            handleQuitChannelMessage(computer, message);
            break;

        case PCMSG_LIST_CHANNELS:
            handleListChannelsMessage(computer, message);
            break;

        case PCMSG_LIST_CHANNELUSERS:
            handleListChannelUsersMessage(computer, message);
            break;

        case PCMSG_TOPIC_CHANGE:
            handleTopicChange(computer, message);
            break;

        case PCMSG_DISCONNECT:
            handleDisconnectMessage(computer, message);
            break;

        case PCMSG_GUILD_CREATE:
            handleGuildCreate(computer, message);
            break;

        case PCMSG_GUILD_INVITE:
            handleGuildInvite(computer, message);
            break;

        case PCMSG_GUILD_ACCEPT:
            handleGuildAcceptInvite(computer, message);
            break;

        case PCMSG_GUILD_GET_MEMBERS:
            handleGuildGetMembers(computer, message);
            break;

        case PCMSG_GUILD_PROMOTE_MEMBER:
            handleGuildMemberLevelChange(computer, message);
            break;

        case PCMSG_GUILD_KICK_MEMBER:
            handleGuildKickMember(computer, message);

        case PCMSG_GUILD_QUIT:
            handleGuildQuit(computer, message);
            break;

        case PCMSG_PARTY_INVITE_ANSWER:
            handlePartyInviteAnswer(computer, message);
            break;

        case PCMSG_PARTY_QUIT:
            handlePartyQuit(computer);
            break;

        default:
            LOG_WARN("ChatHandler::processMessage, Invalid message type"
                     << message.getId());
            MessageOut result(XXMSG_INVALID);
            computer.send(result);
            break;
    }
}

void ChatHandler::handleCommand(ChatClient &computer, const QString &command)
{
    LOG_INFO("Chat: Received unhandled command: " << command);
    MessageOut result(CPMSG_ERROR);
    result.writeInt8(CHAT_UNHANDLED_COMMAND);
    computer.send(result);
}

void ChatHandler::warnPlayerAboutBadWords(ChatClient &computer)
{
    // We could later count if the player is really often unpolite.
    MessageOut result(CPMSG_ERROR);
    result.writeInt8(CHAT_USING_BAD_WORDS); // The Channel
    computer.send(result);

    LOG_INFO(computer.characterName << " says bad words.");
}

void ChatHandler::handleChatMessage(ChatClient &client, MessageIn &msg)
{
    QString text = msg.readString();

    // Pass it through the slang filter (false when it contains bad words)
    if (!stringFilter->filterContent(text))
    {
        warnPlayerAboutBadWords(client);
        return;
    }

    short channelId = msg.readInt16();
    ChatChannel *channel = chatChannelManager->getChannel(channelId);

    if (channel)
    {
        MessageOut result(CPMSG_PUBMSG);
        result.writeInt16(channelId);
        result.writeString(client.characterName);
        result.writeString(text);
        sendInChannel(channel, result);
    }
}

void ChatHandler::handleAnnounce(const QString &message, int senderId,
                                 const QString &senderName)
{
    // We do not need to check for right permissions since the game server does
    // this.
    MessageOut result(CPMSG_ANNOUNCEMENT);
    result.writeString(message);
    result.writeString(senderName);
    sendToEveryone(result);

    if (!senderId)
        return; // Do not log scripted announcements

    // log transaction
    Transaction trans;
    trans.mCharacterId = senderId;
    trans.mAction = TRANS_MSG_ANNOUNCE;
    trans.mMessage = senderName + " announced: " + message;
    mStorage->addTransaction(trans);

}

void ChatHandler::handlePrivMsgMessage(ChatClient &client, MessageIn &msg)
{
    QString user = msg.readString();
    QString text = msg.readString();

    if (!stringFilter->filterContent(text))
    {
        warnPlayerAboutBadWords(client);
        return;
    }

    // We seek the player to whom the message is told and send it to her/him.
    sayToPlayer(client, user, text);
}

void ChatHandler::handleWhoMessage(ChatClient &client)
{
    MessageOut reply(CPMSG_WHO_RESPONSE);

    std::map<QString, ChatClient*>::iterator itr, itr_end;
    itr = mPlayerMap.begin();
    itr_end = mPlayerMap.end();

    while (itr != itr_end)
    {
        reply.writeString(itr->first);
        ++itr;
    }

    client.send(reply);
}

void ChatHandler::handleEnterChannelMessage(ChatClient &client, MessageIn &msg)
{
    MessageOut reply(CPMSG_ENTER_CHANNEL_RESPONSE);

    QString channelName = msg.readString();
    QString givenPassword = msg.readString();
    ChatChannel *channel = nullptr;
    if (chatChannelManager->channelExists(channelName) ||
        chatChannelManager->tryNewPublicChannel(channelName))
    {
        channel = chatChannelManager->getChannel(channelName);
    }

    if (!channel)
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else if (!channel->getPassword().isEmpty() &&
            channel->getPassword() != givenPassword)
    {
        // Incorrect password (should probably have its own return value)
        reply.writeInt8(ERRMSG_INSUFFICIENT_RIGHTS);
    }
    else if (!channel->canJoin())
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else
    {
        if (channel->addUser(&client))
        {
            reply.writeInt8(ERRMSG_OK);
            // The user entered the channel, now give him the channel
            // id, the announcement string and the user list.
            reply.writeInt16(channel->getId());
            reply.writeString(channelName);
            reply.writeString(channel->getAnnouncement());
            const ChatChannel::ChannelUsers &users = channel->getUserList();

            for (const auto &user : users) {
                reply.writeString((user)->characterName);
                reply.writeString(channel->getUserMode((user)));
            }
            // Send an CPMSG_UPDATE_CHANNEL to warn other clients a user went
            // in the channel.
            warnUsersAboutPlayerEventInChat(channel,
                    client.characterName,
                    CHAT_EVENT_NEW_PLAYER);

            // log transaction
            Transaction trans;
            trans.mCharacterId = client.characterId;
            trans.mAction = TRANS_CHANNEL_JOIN;
            trans.mMessage = "User joined " + channelName;
            mStorage->addTransaction(trans);
        }
        else
        {
            reply.writeInt8(ERRMSG_FAILURE);
        }
    }

    client.send(reply);
}

void ChatHandler::handleModeChangeMessage(ChatClient &client, MessageIn &msg)
{
    short channelId = msg.readInt16();
    ChatChannel *channel = chatChannelManager->getChannel(channelId);

    if (channelId == 0 || !channel)
    {
        // invalid channel
        return;
    }

    if (!channel->getUserMode(&client).contains('o'))
    {
        // invalid permissions
        return;
    }

    // get the user whos mode has been changed
    QString user = msg.readString();

    // get the mode to change to
    unsigned char mode = msg.readInt8();
    channel->setUserMode(getClient(user), mode);

    // set the info to pass to all channel clients
    QString info = client.characterName + ":" + user + ":" + mode;

    warnUsersAboutPlayerEventInChat(channel, info, CHAT_EVENT_MODE_CHANGE);

    // log transaction
    Transaction trans;
    trans.mCharacterId = client.characterId;
    trans.mAction = TRANS_CHANNEL_MODE;
    trans.mMessage = "User mode ";
    trans.mMessage.append(QString::number(mode) + " set on " + user);
    mStorage->addTransaction(trans);
}

void ChatHandler::handleKickUserMessage(ChatClient &client, MessageIn &msg)
{
    short channelId = msg.readInt16();
    ChatChannel *channel = chatChannelManager->getChannel(channelId);

    if (channelId == 0 || !channel)
    {
        // invalid channel
        return;
    }

    if (!channel->getUserMode(&client).contains('o'))
    {
        // invalid permissions
        return;
    }

    // get the user whos being kicked
    QString user = msg.readString();

    if (channel->removeUser(getClient(user)))
    {
        QString logMessage = client.characterName + ":" + user;
        warnUsersAboutPlayerEventInChat(channel, logMessage,
                                        CHAT_EVENT_KICKED_PLAYER);
    }

    // log transaction
    Transaction trans;
    trans.mCharacterId = client.characterId;
    trans.mAction = TRANS_CHANNEL_KICK;
    trans.mMessage = "User kicked " + user;
    mStorage->addTransaction(trans);
}

void ChatHandler::handleQuitChannelMessage(ChatClient &client, MessageIn &msg)
{
    MessageOut reply(CPMSG_QUIT_CHANNEL_RESPONSE);

    short channelId = msg.readInt16();
    ChatChannel *channel = chatChannelManager->getChannel(channelId);

    if (channelId == 0 || !channel)
    {
        reply.writeInt8(ERRMSG_INVALID_ARGUMENT);
    }
    else if (!channel->removeUser(&client))
    {
        reply.writeInt8(ERRMSG_FAILURE);
    }
    else
    {
        reply.writeInt8(ERRMSG_OK);
        reply.writeInt16(channelId);

        // Send an CPMSG_UPDATE_CHANNEL to warn other clients a user left
        // the channel.
        warnUsersAboutPlayerEventInChat(channel,
                client.characterName,
                CHAT_EVENT_LEAVING_PLAYER);

        // log transaction
        Transaction trans;
        trans.mCharacterId = client.characterId;
        trans.mAction = TRANS_CHANNEL_QUIT;
        trans.mMessage = "User left " + channel->getName();
        mStorage->addTransaction(trans);

        if (channel->getUserList().empty())
        {
            chatChannelManager->removeChannel(channel->getId());
        }
    }

    client.send(reply);
}

void ChatHandler::handleListChannelsMessage(ChatClient &client, MessageIn &)
{
    MessageOut reply(CPMSG_LIST_CHANNELS_RESPONSE);

    std::list<const ChatChannel*> channels =
        chatChannelManager->getPublicChannels();

    for (auto &channel : channels) {
        reply.writeString((channel)->getName());
        reply.writeInt16((channel)->getUserList().size());
    }

    client.send(reply);

    // log transaction
    Transaction trans;
    trans.mCharacterId = client.characterId;
    trans.mAction = TRANS_CHANNEL_LIST;
    mStorage->addTransaction(trans);
}

void ChatHandler::handleListChannelUsersMessage(ChatClient &client,
                                                MessageIn &msg)
{
    MessageOut reply(CPMSG_LIST_CHANNELUSERS_RESPONSE);

    QString channelName = msg.readString();
    ChatChannel *channel = chatChannelManager->getChannel(channelName);

    if (channel)
    {
        reply.writeString(channel->getName());

        const ChatChannel::ChannelUsers &users = channel->getUserList();

        for (const auto &user : users) {
            reply.writeString((user)->characterName);
            reply.writeString(channel->getUserMode((user)));
        }

        client.send(reply);
    }

    // log transaction
    Transaction trans;
    trans.mCharacterId = client.characterId;
    trans.mAction = TRANS_CHANNEL_USERLIST;
    mStorage->addTransaction(trans);
}

void ChatHandler::handleTopicChange(ChatClient &client, MessageIn &msg)
{
    short channelId = msg.readInt16();
    QString topic = msg.readString();
    ChatChannel *channel = chatChannelManager->getChannel(channelId);

    if (!guildManager->doesExist(channel->getName()))
    {
        chatChannelManager->setChannelTopic(channelId, topic);
    }
    else
    {
        guildChannelTopicChange(channel, client.characterId, topic);
    }

    // log transaction
    Transaction trans;
    trans.mCharacterId = client.characterId;
    trans.mAction = TRANS_CHANNEL_TOPIC;
    trans.mMessage = "User changed topic to " + topic;
    trans.mMessage.append(" in " + channel->getName());
    mStorage->addTransaction(trans);
}

void ChatHandler::handleDisconnectMessage(ChatClient &client, MessageIn &)
{
    MessageOut reply(CPMSG_DISCONNECT_RESPONSE);
    reply.writeInt8(ERRMSG_OK);
    chatChannelManager->removeUserFromAllChannels(&client);
    guildManager->disconnectPlayer(&client);
    client.send(reply);
}

void ChatHandler::sayToPlayer(ChatClient &computer,
                              const QString &playerName,
                              const QString &text)
{
    // Send it to the being if the being exists
    MessageOut result(CPMSG_PRIVMSG);
    result.writeString(computer.characterName);
    result.writeString(text);
    for (auto &elem : clients) {
        if (static_cast<ChatClient *>(elem)->characterName == playerName) {
            (elem)->send(result);
            break;
        }
    }
}

void ChatHandler::warnUsersAboutPlayerEventInChat(ChatChannel *channel,
                                                  const QString &info,
                                                  char eventId)
{
    MessageOut msg(CPMSG_CHANNEL_EVENT);
    msg.writeInt16(channel->getId());
    msg.writeInt8(eventId);
    msg.writeString(info);
    sendInChannel(channel, msg);
}

void ChatHandler::sendInChannel(ChatChannel *channel, MessageOut &msg)
{
    const ChatChannel::ChannelUsers &users = channel->getUserList();

    for (const auto &user : users) {
        (user)->send(msg);
    }
}

ChatClient *ChatHandler::getClient(const QString &name) const
{
    std::map<QString, ChatClient*>::const_iterator itr
            = mPlayerMap.find(name);

    if (itr != mPlayerMap.end())
        return itr->second;
    else
        return nullptr;
}

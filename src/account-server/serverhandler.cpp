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

#include <cassert>
#include <sstream>
#include <list>

#include <QDebug>

#include "account-server/serverhandler.h"

#include "mana/configuration/interfaces/iconfiguration.h"

#include "mana/persistence/interfaces/istorage.h"

#include "mana/entities/character.h"
#include "mana/entities/post.h"

#include "account-server/accountclient.h"
#include "account-server/accounthandler.h"
#include "account-server/characterdatautils.h"
#include "account-server/flooritem.h"
#include "account-server/mapmanager.h"
#include "chat-server/chathandler.h"
#include "chat-server/postmanager.h"
#include "common/defines.h"
#include "common/manaserv_protocol.h"
#include "common/transaction.h"
#include "net/connectionhandler.h"
#include "net/messageout.h"
#include "net/netcomputer.h"
#include "utils/tokendispenser.h"

using namespace ManaServ;

struct MapStatistics
{
  std::vector<int> players;
  unsigned short nbEntities;
  unsigned short nbMonsters;
};

typedef std::map<unsigned short, MapStatistics> ServerStatistics;

/**
 * Stores address, maps, and statistics, of a connected game server.
 */
struct GameServer: NetComputer
{
    GameServer(ENetPeer *peer) : NetComputer(peer), server(nullptr), port(0) {}

    QString name;
    QString address;
    NetComputer *server;
    ServerStatistics maps;
    short port;
};

static GameServer *getGameServerFromMap(int);

/**
 * Manages communications with all the game servers.
 */
class ServerHandler: public ConnectionHandler
{
friend GameServer *getGameServerFromMap(int);
friend void GameServerHandler::dumpStatistics(std::ostream &);

public:
    ServerHandler(IConfiguration *configuration, IStorage *storage)
        : ConnectionHandler(configuration)
        , mConfiguration(configuration)
        , mStorage(storage)
    {}

protected:
    /**
     * Processes server messages.
     */
    void processMessage(NetComputer *computer, MessageIn &message);

    /**
     * Called when a game server connects. Initializes a simple NetComputer
     * as these connections are stateless.
     */
    NetComputer *computerConnected(ENetPeer *peer);

    /**
     * Called when a game server disconnects.
     */
    void computerDisconnected(NetComputer *comp);

private:
    IConfiguration *mConfiguration;
    IStorage *mStorage;
};

static ServerHandler *serverHandler;

bool GameServerHandler::initialize(int port,
                                   const QString &host,
                                   IConfiguration *configuration,
                                   IStorage *storage)
{
    mStorage = storage;

    MapManager::initialize(DEFAULT_MAPSDB_FILE);
    serverHandler = new ServerHandler(configuration, storage);
    qDebug() << "Game server handler started:";
    return serverHandler->startListen(port, host);
}

void GameServerHandler::deinitialize()
{
    serverHandler->stopListen();
    delete serverHandler;
}

void GameServerHandler::process()
{
    serverHandler->process(50);
}

NetComputer *ServerHandler::computerConnected(ENetPeer *peer)
{
    return new GameServer(peer);
}

void ServerHandler::computerDisconnected(NetComputer *comp)
{
    qDebug() << "Game-server disconnected.";
    delete comp;
}

static GameServer *getGameServerFromMap(int mapId)
{
    for (ServerHandler::NetComputers::const_iterator
         i = serverHandler->clients.begin(),
         i_end = serverHandler->clients.end(); i != i_end; ++i)
    {
        GameServer *server = static_cast< GameServer * >(*i);
        ServerStatistics::const_iterator it = server->maps.find(mapId);
        if (it == server->maps.end()) continue;
        return server;
    }
    return nullptr;
}

bool GameServerHandler::getGameServerFromMap(int mapId,
                                             QString &address,
                                             int &port)
{
    if (GameServer *s = ::getGameServerFromMap(mapId))
    {
        address = s->address;
        port = s->port;
        return true;
    }
    return false;
}

static void registerGameClient(GameServer *s, const QString &token,
                               CharacterData &ptr)
{
    MessageOut msg(AGMSG_PLAYER_ENTER);
    msg.writeString(token, MAGIC_TOKEN_LENGTH);
    msg.writeInt32(ptr.getDatabaseId());
    msg.writeString(ptr.getName());
    CharacterDataUtils::serialize(ptr, msg);
    s->send(msg);
}

void GameServerHandler::registerClient(const QString &token,
                                       CharacterData &ptr)
{
    GameServer *s = ::getGameServerFromMap(ptr.getMapId());
    assert(s);
    registerGameClient(s, token, ptr);
}

void ServerHandler::processMessage(NetComputer *comp, MessageIn &msg)
{
    GameServer *server = static_cast<GameServer *>(comp);

    switch (msg.getId())
    {
        case GAMSG_REGISTER:
        {
            qDebug() << "GAMSG_REGISTER";
            // TODO: check the credentials of the game server
            server->name = msg.readString();
            server->address = msg.readString();
            server->port = msg.readInt16();
            const QString password = msg.readString();

            // checks the version of the remote item database with our local copy
            unsigned dbversion = msg.readInt32();
            qDebug() << "Game server uses itemsdatabase with version " << dbversion;

            qDebug() << "AGMSG_REGISTER_RESPONSE";
            MessageOut outMsg(AGMSG_REGISTER_RESPONSE);
            if (dbversion == mStorage->getItemDatabaseVersion())
            {
                qDebug() << "Item databases between account server and "
                    "gameserver are in sync";
                outMsg.writeInt16(DATA_VERSION_OK);
            }
            else
            {
                qDebug() << "Item database of game server has a wrong version";
                outMsg.writeInt16(DATA_VERSION_OUTDATED);
            }
            if (password == mConfiguration->getValue("net_password", "changeMe"))
            {
                outMsg.writeInt16(PASSWORD_OK);

                // transmit global world state variables
                std::map<QString, QString> variables;
                variables = mStorage->getAllWorldStateVars(IStorage::WorldMap);

                for (auto &variableIt : variables)
                {
                    outMsg.writeString(variableIt.first);
                    outMsg.writeString(variableIt.second);
                }

                comp->send(outMsg);
            }
            else
            {
                qDebug() << "The password given by " << server->address << ':'
                         << server->port << " was bad.";
                outMsg.writeInt16(PASSWORD_BAD);
                comp->disconnect(outMsg);
                break;
            }

            qDebug() << "Game server " << server->address << ':' << server->port
                     << " asks for maps to activate.";

            const std::map<int, QString> &maps = MapManager::getMaps();
            for (const auto &maps_it : maps) {
                int id = maps_it.first;
                const QString &reservedServer = maps_it.second;
                if (reservedServer == server->name)
                {
                    MessageOut outMsg(AGMSG_ACTIVE_MAP);

                    // Map variables
                    outMsg.writeInt16(id);
                    qDebug() << "Issued server " << server->name << "("
                             << server->address << ":" << server->port << ") "
                             << "to enable map " << id;
                    std::map<QString, QString> variables;
                    variables = mStorage->getAllWorldStateVars(id);

                     // Map vars number
                    outMsg.writeInt16(variables.size());

                    for (auto &variableIt : variables)
                    {
                        outMsg.writeString(variableIt.first);
                        outMsg.writeString(variableIt.second);
                    }

                    // Persistent Floor Items
                    std::list<FloorItem> items;
                    items = mStorage->getFloorItemsFromMap(id);

                    outMsg.writeInt16(items.size()); //number of floor items

                    // Send each map item: item_id, amount, pos_x, pos_y
                    for (auto &item : items) {
                        outMsg.writeInt32(item.getItemId());
                        outMsg.writeInt16(item.getItemAmount());
                        outMsg.writeInt16(item.getPosX());
                        outMsg.writeInt16(item.getPosY());
                    }

                    comp->send(outMsg);
                    MapStatistics &m = server->maps[id];
                    m.nbEntities = 0;
                    m.nbMonsters = 0;
                }
            }
        } break;

        case GAMSG_PLAYER_DATA:
        {
            qDebug() << "GAMSG_PLAYER_DATA";
            int id = msg.readInt32();
            if (auto ptr = mStorage->getCharacter(id, nullptr))
            {
                CharacterDataUtils::deserialize(*ptr, msg);
                mStorage->updateCharacter(*ptr);
            }
            else
            {
                qCritical() << "Received data for non-existing character "
                            << id << '.';
            }
        } break;

        case GAMSG_PLAYER_SYNC:
        {
            qDebug() << "GAMSG_PLAYER_SYNC";
            GameServerHandler::syncDatabase(msg);
        } break;

        case GAMSG_REDIRECT:
        {
            qDebug() << "GAMSG_REDIRECT";
            int id = msg.readInt32();
            QString magic_token(utils::getMagicToken());
            if (auto ptr = mStorage->getCharacter(id, nullptr))
            {
                int mapId = ptr->getMapId();
                if (GameServer *s = getGameServerFromMap(mapId))
                {
                    registerGameClient(s, magic_token, *ptr);
                    MessageOut result(AGMSG_REDIRECT_RESPONSE);
                    result.writeInt32(id);
                    result.writeString(magic_token, MAGIC_TOKEN_LENGTH);
                    result.writeString(s->address);
                    result.writeInt16(s->port);
                    comp->send(result);
                }
                else
                {
                    qCritical() << "Server Change: No game server for map " <<
                                mapId << '.';
                }
            }
            else
            {
                qCritical() << "Received data for non-existing character "
                            << id << '.';
            }
        } break;

        case GAMSG_PLAYER_RECONNECT:
        {
            qDebug() << "GAMSG_PLAYER_RECONNECT";
            int id = msg.readInt32();
            QString magic_token = msg.readString(MAGIC_TOKEN_LENGTH);

            if (auto ptr = mStorage->getCharacter(id, nullptr))
            {
                int accountID = ptr->getAccountID();
                AccountClientHandler::prepareReconnect(magic_token, accountID);
            }
            else
            {
                qCritical() << "Received data for non-existing character "
                            << id << '.';
            }
        } break;

        case GAMSG_GET_VAR_CHR:
        {
            int id = msg.readInt32();
            QString name = msg.readString();
            QString value = mStorage->getQuestVar(id, name);
            MessageOut result(AGMSG_GET_VAR_CHR_RESPONSE);
            result.writeInt32(id);
            result.writeString(name);
            result.writeString(value);
            comp->send(result);
        } break;

        case GAMSG_SET_VAR_CHR:
        {
            int id = msg.readInt32();
            QString name = msg.readString();
            QString value = msg.readString();
            mStorage->setQuestVar(id, name, value);
        } break;

        case GAMSG_SET_VAR_WORLD:
        {
            QString name = msg.readString();
            QString value = msg.readString();
            // save the new value to the database
            mStorage->setWorldStateVar(name, value, IStorage::WorldMap);
            // relay the new value to all gameservers
            for (NetComputer *netComputer : clients)
            {
                MessageOut varUpdateMessage(AGMSG_SET_VAR_WORLD);
                varUpdateMessage.writeString(name);
                varUpdateMessage.writeString(value);
                netComputer->send(varUpdateMessage);
            }
        } break;

        case GAMSG_SET_VAR_MAP:
        {
            int mapid = msg.readInt32();
            QString name = msg.readString();
            QString value = msg.readString();
            mStorage->setWorldStateVar(name, value, mapid);
        } break;

        case GAMSG_BAN_PLAYER:
        {
            int id = msg.readInt32();
            int duration = msg.readInt32();
            QDateTime banEnd = QDateTime::currentDateTime();
            banEnd.addSecs(duration * 60);
            mStorage->banCharacter(id, banEnd);
        } break;

        case GAMSG_CHANGE_ACCOUNT_LEVEL:
        {
            int id = msg.readInt32();
            int level = msg.readInt16();

            // get the character so we can get the account id
            auto character = mStorage->getCharacter(id, nullptr);
            if (character)
            {
                mStorage->setAccountLevel(character->getAccountID(), level);
            }
        } break;

        case GAMSG_STATISTICS:
        {
            while (msg.getUnreadLength())
            {
                int mapId = msg.readInt16();
                ServerStatistics::iterator i = server->maps.find(mapId);
                if (i == server->maps.end())
                {
                    qCritical() << "Server " << server->address << ':'
                                << server->port << " should not be sending stati"
                                "stics for map " << mapId << '.';
                    // Skip remaining data.
                    break;
                }
                MapStatistics &m = i->second;
                m.nbEntities = msg.readInt16();
                m.nbMonsters = msg.readInt16();
                int nb = msg.readInt16();
                m.players.resize(nb);
                for (int j = 0; j < nb; ++j)
                {
                    m.players[j] = msg.readInt32();
                }
            }
        } break;

        case GCMSG_REQUEST_POST:
        {
            // Retrieve the post for user
            qDebug() << "GCMSG_REQUEST_POST";
            MessageOut result(CGMSG_POST_RESPONSE);

            // get the character id
            int characterId = msg.readInt32();

            // send the character id of sender
            result.writeInt32(characterId);

            // get the character based on the id
            auto ptr = mStorage->getCharacter(characterId, nullptr);
            if (!ptr)
            {
                // Invalid character
                qCritical() << "Error finding character id for post";
                break;
            }

            // get the post for that character
            Post *post = postalManager->getPost(*ptr);

            // send the post if valid
            if (post)
            {
                for (unsigned i = 0; i < post->getNumberOfLetters(); ++i)
                {
                    // get each letter, send the sender's name,
                    // the contents and any attachments
                    Letter *letter = post->getLetter(i);
                    result.writeString(letter->getSender().getName());
                    result.writeString(letter->getContents());
                    std::vector<InventoryItem> items = letter->getAttachments();
                    for (auto &item : items) {
                        result.writeInt16(item.itemId);
                        result.writeInt16(item.amount);
                    }
                }

                // clean up
                postalManager->clearPost(*ptr);
            }

            comp->send(result);
        } break;

        case GCMSG_STORE_POST:
        {
            // Store the letter for the user
            qDebug() << "GCMSG_STORE_POST";
            MessageOut result(CGMSG_STORE_POST_RESPONSE);

            // get the sender and receiver
            int senderId = msg.readInt32();
            QString receiverName = msg.readString();

            // for sending it back
            result.writeInt32(senderId);

            // get their characters
            auto sender = mStorage->getCharacter(senderId, nullptr);
            auto receiver = mStorage->getCharacter(receiverName);
            if (!sender || !receiver)
            {
                // Invalid character
                qCritical() << "Error finding character id for post";
                result.writeInt8(ERRMSG_INVALID_ARGUMENT);
                break;
            }

            // get the letter contents
            QString contents = msg.readString();

            std::vector< std::pair<int, int> > items;
            while (msg.getUnreadLength())
            {
                items.push_back(std::pair<int, int>(msg.readInt16(), msg.readInt16()));
            }

            // save the letter
            qDebug() << "Creating letter";
            Letter *letter = new Letter(0, std::move(sender), std::move(receiver));
            letter->addText(contents);
            for (auto &items_i : items) {
                InventoryItem item;
                item.itemId = items_i.first;
                item.amount = items_i.second;

                unsigned maximumNumberOfAttachments =
                        mConfiguration->getValue("mail_maxAttachments", 3);
                if (letter->getAttachments().size() < maximumNumberOfAttachments)
                {
                    letter->addAttachment(item);
                }
            }
            postalManager->addLetter(letter);

            result.writeInt8(ERRMSG_OK);
            comp->send(result);
        } break;

        case GAMSG_TRANSACTION:
        {
            qDebug() << "TRANSACTION";
            int id = msg.readInt32();
            int action = msg.readInt32();
            QString message = msg.readString();

            Transaction trans;
            trans.mCharacterId = id;
            trans.mAction = action;
            trans.mMessage = message;
            mStorage->addTransaction(trans);
        } break;

        case GCMSG_PARTY_INVITE:
            chatHandler->handlePartyInvite(msg);
            break;

        case GAMSG_CREATE_ITEM_ON_MAP:
        {
            int mapId = msg.readInt32();
            int itemId = msg.readInt32();
            int amount = msg.readInt16();
            int posX = msg.readInt16();
            int posY = msg.readInt16();

            qDebug() << "Gameserver create item " << itemId
                     << " on map " << mapId;

            mStorage->addFloorItem(mapId, itemId, amount, posX, posY);
        } break;

        case GAMSG_REMOVE_ITEM_ON_MAP:
        {
            int mapId = msg.readInt32();
            int itemId = msg.readInt32();
            int amount = msg.readInt16();
            int posX = msg.readInt16();
            int posY = msg.readInt16();

            qDebug() << "Gameserver removed item " << itemId
                     << " from map " << mapId;

            mStorage->removeFloorItem(mapId, itemId, amount, posX, posY);
        } break;

        case GAMSG_ANNOUNCE:
        {
            const QString message = msg.readString();
            const int senderId = msg.readInt16();
            const QString senderName = msg.readString();
            chatHandler->handleAnnounce(message, senderId, senderName);
        } break;

        default:
            qWarning() << "ServerHandler::processMessage, Invalid message type: "
                       << msg.getId();
            MessageOut result(XXMSG_INVALID);
            comp->send(result);
            break;
    }
}

void GameServerHandler::dumpStatistics(std::ostream &os)
{
    for (ServerHandler::NetComputers::const_iterator
         i = serverHandler->clients.begin(),
         i_end = serverHandler->clients.end(); i != i_end; ++i)
    {
        GameServer *server = static_cast< GameServer * >(*i);
        if (!server->port)
            continue;

        os << "<gameserver address=\"" << server->address.toStdString() << "\" port=\""
           << server->port << "\">\n";

        for (ServerStatistics::const_iterator j = server->maps.begin(),
             j_end = server->maps.end(); j != j_end; ++j)
        {
            const MapStatistics &m = j->second;
            os << "<map id=\"" << j->first << "\" nb_entities=\"" << m.nbEntities
               << "\" nb_monsters=\"" << m.nbMonsters << "\">\n";
            for (const auto &elem : m.players) {
                os << "<character id=\"" << elem << "\"/>\n";
            }
            os << "</map>\n";
        }
        os << "</gameserver>\n";
    }
}

void GameServerHandler::sendPartyChange(CharacterData &ptr, int partyId)
{
    GameServer *s = ::getGameServerFromMap(ptr.getMapId());
    if (s)
    {
        MessageOut msg(CGMSG_CHANGED_PARTY);
        msg.writeInt32(ptr.getDatabaseId());
        msg.writeInt32(partyId);
        s->send(msg);
    }
}

void GameServerHandler::syncDatabase(MessageIn &msg)
{
    // It is safe to perform the following updates in a transaction
    // TODO THIS COMMIT do transaction

    while (msg.getUnreadLength() > 0)
    {
        int msgType = msg.readInt8();
        switch (msgType)
        {
            case SYNC_CHARACTER_POINTS:
            {
                qDebug() << "received SYNC_CHARACTER_POINTS";
                int charId = msg.readInt32();
                int charPoints = msg.readInt32();
                int corrPoints = msg.readInt32();
                mStorage->updateCharacterPoints(charId, charPoints, corrPoints);
            } break;

            case SYNC_CHARACTER_ATTRIBUTE:
            {
                qDebug() << "received SYNC_CHARACTER_ATTRIBUTE";
                int    charId = msg.readInt32();
                int    attrId = msg.readInt32();
                double base   = msg.readDouble();
                double mod    = msg.readDouble();
                mStorage->updateAttribute(charId, attrId, base, mod);
            } break;

            case SYNC_ONLINE_STATUS:
            {
                qDebug() << "received SYNC_ONLINE_STATUS";
                int charId = msg.readInt32();
                bool online = (msg.readInt8() == 1);
                mStorage->setOnlineStatus(charId, online);
            } break;
        }
    }

    // TODO THIS COMMIT transaction.commit();
}

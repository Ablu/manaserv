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

#include "guildmanager.h"

#include "mana/persistence/interfaces/istorage.h"

#include "mana/entities/guild.h"

#include "common/defines.h"
#include "common/manaserv_protocol.h"
#include "chat-server/chatclient.h"
#include "chat-server/chatchannelmanager.h"
#include "chat-server/chathandler.h"

using namespace ManaServ;

GuildManager::GuildManager(IStorage *storage)
    : mStorage(storage)
    , mGuilds(mStorage->getGuildList())
{
}

GuildManager::~GuildManager()
{
    for (auto &elem : mGuilds) {
        delete elem.second;
    }
}

Guild* GuildManager::createGuild(const QString &name, int playerId)
{
    Guild *guild = new Guild(name);
    // Add guild to db
    mStorage->addGuild(guild);

    // Add guild
    mGuilds[guild->getId()] = guild;

    // put the owner in the guild
    addGuildMember(guild, playerId);

    // Set and save the member rights
    mStorage->setMemberRights(guild->getId(), playerId, GAL_OWNER);

    setUserRights(guild, playerId, GAL_OWNER);

    return guild;
}

void GuildManager::removeGuild(Guild *guild)
{
    mStorage->removeGuild(guild);
    mGuilds.erase(guild->getId());
    delete guild;
}

void GuildManager::addGuildMember(Guild *guild, int playerId)
{
    mStorage->addGuildMember(guild->getId(), playerId);
    guild->addMember(playerId);
}

void GuildManager::removeGuildMember(Guild *guild, int playerId,
                                     const QString &characterName,
                                     ChatClient *client)
{
    // remove the user from the guild
    mStorage->removeGuildMember(guild->getId(), playerId);

    // if the leader is leaving, assign next member as leader
    if (guild->getOwner() == playerId)
    {
        for (GuildMember *member : guild->getMembers())
        {
            if (member->mId != playerId)
            {
                setUserRights(guild, member->mId, GAL_OWNER);
            }
        }
    }

    guild->removeMember(playerId);

    chatHandler->sendGuildListUpdate(guild, characterName,
                                     GUILD_EVENT_LEAVING_PLAYER);

    // if theres no more members left delete the guild
    if (guild->memberCount() == 0)
    {
        chatChannelManager->removeChannel(
                    chatChannelManager->getChannelId(guild->getName()));
        removeGuild(guild);
    }

    if (client)
    {
        for (std::vector<Guild *>::iterator it = client->guilds.begin(),
             it_end = client->guilds.end(); it != it_end; ++it)
        {
            if (*it == guild)
            {
                client->guilds.erase(it);
                break;
            }
        }
    }
}

Guild *GuildManager::findById(short id) const
{
    std::map<int, Guild*>::const_iterator it = mGuilds.find(id);
    return it == mGuilds.end() ? nullptr : it->second;
}

Guild *GuildManager::findByName(const QString &name) const
{
    for (const auto &elem : mGuilds) {
        Guild *guild = elem.second;
        if (guild->getName() == name)
            return guild;
    }
    return nullptr;
}

bool GuildManager::doesExist(const QString &name) const
{
    return findByName(name) != nullptr;
}

std::vector<Guild *> GuildManager::getGuildsForPlayer(int playerId) const
{
    std::vector<Guild *> guilds;
    for (const auto &elem : mGuilds) {
        if (elem.second->checkInGuild(playerId)) {
            guilds.push_back(elem.second);
        }
    }
    return guilds;
}

void GuildManager::disconnectPlayer(ChatClient *player)
{
    for (auto &elem : player->guilds) {
        chatHandler->sendGuildListUpdate(elem, player->characterName,
                                         GUILD_EVENT_OFFLINE_PLAYER);
    }
}

int GuildManager::changeMemberLevel(ChatClient *player, Guild *guild,
                                    int playerId, int level)
{
    if (guild->checkInGuild(playerId))
    {
        int playerLevel = guild->getUserPermissions(player->characterId);

        if (playerLevel == GAL_OWNER)
        {
            // player can modify anyones permissions
            setUserRights(guild, playerId, level);
            return 0;
        }
    }

    return -1;
}

void GuildManager::setUserRights(Guild *guild, int playerId, int rights)
{
    // Set and save the member rights
    mStorage->setMemberRights(guild->getId(), playerId, rights);

    // Set with guild
    guild->setUserPermissions(playerId, rights);
}

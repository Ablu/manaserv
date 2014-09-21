/*
 *  The Mana Server
 *  Copyright (C) 2007-2010  The Mana World Development Team
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

#include "game-server/monstermanager.h"

#include "common/defines.h"

#include "game-server/attributemanager.h"
#include "game-server/itemmanager.h"
#include "game-server/monster.h"
#include "utils/logger.h"

#define MAX_MUTATION 99
#define DEFAULT_MONSTER_SIZE 16
#define DEFAULT_MONSTER_SPEED 4.0f

void MonsterManager::reload()
{
    deinitialize();
    initialize();
}

void MonsterManager::initialize()
{

}

void MonsterManager::deinitialize()
{
    for (auto &elem : mMonsterClasses) {
        delete elem.second;
    }
    mMonsterClasses.clear();
    mMonsterClassesByName.clear();
}

MonsterClass *MonsterManager::getMonsterByName(const QString &name) const
{
    return mMonsterClassesByName.value(name);
}

MonsterClass *MonsterManager::getMonster(int id) const
{
    MonsterClasses::const_iterator i = mMonsterClasses.find(id);
    return i != mMonsterClasses.end() ? i->second : nullptr;
}

/**
 * Read a <monster> element from settings.
 * Used by SettingsManager.
 */
void MonsterManager::readMonsterNode(xmlNodePtr node, const QString &filename)
{
    if (!xmlStrEqual(node->name, BAD_CAST "monster"))
        return;

    int monsterId = XML::getProperty(node, "id", 0);
    QString name = XML::getProperty(node, "name", QString());

    if (monsterId < 1)
    {
        LOG_WARN("Monster Manager: Ignoring monster ("
                 << name << ") without Id in "
                 << filename << "! It has been ignored.");
        return;
    }

    MonsterClasses::iterator i = mMonsterClasses.find(monsterId);
    if (i != mMonsterClasses.end())
    {
        LOG_WARN("Monster Manager: Ignoring duplicate definition of "
                 "monster '" << monsterId << "'!");
        return;
    }

    MonsterClass *monster = new MonsterClass(monsterId);
    mMonsterClasses[monsterId] = monster;

    if (!name.isEmpty())
    {
        monster->setName(name);

        if (mMonsterClassesByName.contains(name))
            LOG_WARN("Monster Manager: Name not unique for monster "
                     << monsterId);
        else
            mMonsterClassesByName.insert(name, monster);
    }

    MonsterDrops drops;

    for_each_xml_child_node(subnode, node)
    {
        if (xmlStrEqual(subnode->name, BAD_CAST "drop"))
        {
            MonsterDrop drop;
            QString item = XML::getProperty(subnode, "item",
                                                QString());
            ItemClass *itemClass;
            bool wasNumeric;
            int itemId = item.toInt(&wasNumeric);
            if (wasNumeric)
                itemClass = itemManager->getItem(itemId);
            else
                itemClass = itemManager->getItemByName(item);

            if (!itemClass)
            {
                LOG_WARN("Monster Manager: Invalid item name \"" << item
                         << "\"");
                break;
            }

            drop.item = itemClass;
            drop.probability = XML::getFloatProperty(subnode, "percent",
                                                     0.0) * 100 + 0.5;

            if (drop.probability)
                drops.push_back(drop);
        }
        else if (xmlStrEqual(subnode->name, BAD_CAST "attributes"))
        {
            monster->setSize(XML::getProperty(subnode, "size", -1));
            monster->setMutation(XML::getProperty(subnode, "mutation", 0));
            QString genderString = XML::getProperty(subnode, "gender",
                                                        QString());
            monster->setGender(getGender(genderString));

            // Checking attributes for completeness and plausibility
            if (monster->getMutation() > MAX_MUTATION)
            {
                LOG_WARN(filename
                         << ": Mutation of monster Id:" << monsterId
                         << " more than " << MAX_MUTATION
                         << "%. Defaulted to 0.");
                monster->setMutation(0);
            }

            if (monster->getSize() == -1)
            {
                LOG_WARN(filename
                         << ": No size set for monster Id:" << monsterId << ". "
                         << "Defaulted to " << DEFAULT_MONSTER_SIZE
                         << " pixels.");
                monster->setSize(DEFAULT_MONSTER_SIZE);
            }
        }
        else if (xmlStrEqual(subnode->name, BAD_CAST "attribute"))
        {
            QString attributeIdString = XML::getProperty(subnode, "id",
                                                             QString());
            AttributeInfo *info = nullptr;
            bool wasNumeric;
            const int attributeId = attributeIdString.toInt(&wasNumeric);
            if (attributeId)
            {
                info = attributeManager->getAttributeInfo(attributeId);
            }
            else
            {
                info = attributeManager->getAttributeInfo(attributeIdString);
            }

            if (!info)
            {
                LOG_WARN(filename
                         << ": Invalid attribute id " << attributeIdString
                         << " for monster Id: " << monsterId
                         << ". Skipping!");
                continue;
            }

            const double value = XML::getFloatProperty(subnode, "value", 0.0);

            monster->setAttribute(info, value);
        }
        else if (xmlStrEqual(subnode->name, BAD_CAST "ability"))
        {
            const QString idText = XML::getProperty(subnode, "id",
                                                        QString());
            AbilityManager::AbilityInfo *info = nullptr;
            bool wasNumeric;
            const int abilityId = idText.toInt(&wasNumeric);
            if (wasNumeric)
            {
                info = abilityManager->getAbilityInfo(abilityId);
            }
            else
            {
                info = abilityManager->getAbilityInfo(idText);
            }

            if (!info)
            {
                LOG_WARN(filename
                         << ": Invalid ability id " << idText
                         << " for monster id: " << monsterId
                         << " Skipping!");
                continue;
            }

            monster->addAbility(info);
        }
    }

    monster->setDrops(drops);
}

/**
 * Check the status of recently loaded configuration.
 */
void MonsterManager::checkStatus()
{
    LOG_INFO("Loaded " << mMonsterClasses.size() << " monsters");
}

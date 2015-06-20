/*
 *  The Mana Server
 *  Copyright (C) 2013  The Mana World Development Team
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

#include "game-server/settingsmanager.h"
#include "common/defines.h"
#include "utils/xml.h"

#include "game-server/abilitymanager.h"
#include "game-server/attributemanager.h"
#include "game-server/itemmanager.h"
#include "game-server/mapmanager.h"
#include "game-server/monstermanager.h"
#include "game-server/emotemanager.h"
#include "game-server/statusmanager.h"

#include <QDebug>
#include <QFileInfo>

/**
 * Initialize all managers and load configuration into them.
 *
 * Fatal errors will call exit()
 */
void SettingsManager::initialize(IConfiguration *configuration,
                                 IMapReader *mapReader)
{
    // initialize all managers in correct order
    MapManager::initialize(configuration, mapReader);
    attributeManager->initialize();
    abilityManager->initialize();
    itemManager->initialize();
    monsterManager->initialize();
    emoteManager->initialize();
    StatusManager::initialize();

    loadFile(mSettingsFile);
    checkStatus();
}

/**
 * Reload managers with new configuration.
 *
 * @note This code is untested, some of the managers didn't even have empty implementations
 *       of reload().
 */
void SettingsManager::reload()
{
    MapManager::reload();
    attributeManager->reload();
    abilityManager->reload();
    itemManager->reload();
    monsterManager->reload();
    emoteManager->reload();
    StatusManager::reload();

    loadFile(mSettingsFile);
    checkStatus();
}

/**
 * Load a configuration file.
 */
void SettingsManager::loadFile(const QString &filename)
{
    qDebug() << "Loading game settings from " << filename;

    XML::Document doc(filename);
    xmlNodePtr node = doc.rootNode();

    // add file to include set
    mIncludedFiles.insert(filename);

    // FIXME: check root node's name when bjorn decides it's time
    if (!node /*|| !xmlStrEqual(node->name, BAD_CAST "settings") */)
    {
        qCritical() << "Settings Manager: " << filename << " is not a valid database file!";
        exit(EXIT_XML_BAD_PARAMETER);
    }


    // go through every node
    for_each_xml_child_node(childNode, node)
    {
        if (childNode->type != XML_ELEMENT_NODE)
            continue;

        if (xmlStrEqual(childNode->name, BAD_CAST "include"))
        {
            // include an other file
            const QString includeFile = XML::getProperty(childNode, "file", QString());

            // check if file property was given
            if (!includeFile.isEmpty())
            {
                // build absolute path path
                QFileInfo file(filename);
                QString absolutePath = file.absoluteFilePath();

                // check if we're not entering a loop
                if (mIncludedFiles.find(absolutePath) != mIncludedFiles.end())
                {
                    qCritical() << "Circular include loop detecting while including " << includeFile << " from " << filename;
                }
                else
                {
                    // include that file
                    loadFile(absolutePath);
                }
            }
        }
        else if (xmlStrEqual(childNode->name, BAD_CAST "map"))
        {
            // map config
            MapManager::readMapNode(childNode);
        }
        else if (xmlStrEqual(childNode->name, BAD_CAST "attribute"))
        {
            // attribute config
            attributeManager->readAttributeNode(childNode);
        }
        else if (xmlStrEqual(childNode->name, BAD_CAST "ability"))
        {
            // ability config
            abilityManager->readAbilityNode(childNode, filename);
        }
        else if (xmlStrEqual(childNode->name, BAD_CAST "slot"))
        {
            // equipement slot config
            itemManager->readEquipSlotNode(childNode);
        }
        else if (xmlStrEqual(childNode->name, BAD_CAST "item"))
        {
            // item config
            itemManager->readItemNode(childNode, filename);
        }
        else if (xmlStrEqual(childNode->name, BAD_CAST "monster"))
        {
            // monster config
            monsterManager->readMonsterNode(childNode, filename);
        }
        else if (xmlStrEqual(childNode->name, BAD_CAST "emote"))
        {
            // emote config
            emoteManager->readEmoteNode(childNode, filename);
        }
        else if (xmlStrEqual(childNode->name, BAD_CAST "status-effect"))
        {
            // status effects config
            StatusManager::readStatusNode(childNode, filename);
        }
        else
        {
            // since the client and server share settings, don't be too strict
//            qWarning() << "Unexpected tag <" << childNode->name << "> in " << filename;
        }
    }

    // remove this file from include stack
    mIncludedFiles.erase(filename);
}

/**
 * Finalize the configuration loading and check if all managers are happy with it.
 */
void SettingsManager::checkStatus()
{
    MapManager::checkStatus();
    attributeManager->checkStatus();
    abilityManager->checkStatus();
    itemManager->checkStatus();
    monsterManager->checkStatus();
    emoteManager->checkStatus();
    StatusManager::checkStatus();
}

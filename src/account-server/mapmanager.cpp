/*
 *  The Mana Server
 *  Copyright (C) 2004-2010  The Mana World Development Team
 *  Copyright (C) 2010-2013  The Mana Development Team
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

#include "account-server/mapmanager.h"

#include "utils/xml.h"

#include <map>

#include <QDebug>

static std::map<int, QString> maps;

void MapManager::initialize(const QString &mapReferenceFile)
{
    maps.clear();

    XML::Document doc(mapReferenceFile);
    xmlNodePtr rootNode = doc.rootNode();

    if (!rootNode || !xmlStrEqual(rootNode->name, BAD_CAST "maps"))
    {
        qCritical() << "Map Manager: Error while parsing map database ("
                    << mapReferenceFile << ")!";
        return;
    }
    qDebug() << "Loading map reference: " << mapReferenceFile;
    for_each_xml_child_node(node, rootNode)
    {
        if (!xmlStrEqual(node->name, BAD_CAST "map"))
            continue;

        int id = XML::getProperty(node, "id", 0);
        QString name = XML::getProperty(node, "servername", QString());

        if (id > 0)
            maps[id] = name;
    }
}

std::map<int, QString> &MapManager::getMaps()
{
    return maps;
}

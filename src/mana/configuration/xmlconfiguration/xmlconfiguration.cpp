/*
 *  The Mana Server
 *  Copyright (C) 2004-2010  The Mana World Development Team
 *  Copyright (C) 2010  The Mana Developers
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

#include <cmath>
#include <map>
#include <set>
#include <libxml/xmlreader.h>

#include "xmlconfiguration.h"

#include "utils/string.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

#define DEFAULT_CONFIG_FILE       "manaserv.xml"

/**< Persistent configuration. */
static std::map< QString, QString > options;
/**< Location of config file. */
static QString configPath;
static std::set<QString> processedFiles;

static bool readFile(const QString &fileName)
{
    if (processedFiles.find(fileName) != processedFiles.end())
    {
        qWarning() << "Cycle include in configuration file '" << fileName
                   << "'.";
        return false;
    }
    else
        processedFiles.insert(fileName);

    QFile file(fileName);
    bool openSuccessful = file.open(QFile::ReadOnly);
    QXmlStreamReader reader(&file);
    if (!openSuccessful || !reader.readNextStartElement() ||
        reader.name() != "configuration")
    {
        qWarning() << "No configuration file" << fileName;
        file.close();
        return false;
    }

    while (reader.readNextStartElement())
    {
        const QXmlStreamAttributes attributes = reader.attributes();
        const QStringRef &nodeName = reader.name();
        if (nodeName == "include")
        {
            QString includeFileName = attributes.value("file").toString();
            QDir directory = QFileInfo(fileName).absoluteDir();
            includeFileName = directory.absolutePath() + "/" + includeFileName;

            if (!readFile(includeFileName))
            {
                qWarning() << "Error ocurred while parsing included "
                              "configuration file '" << includeFileName << "'.";
                file.close();
                return false;
            }
        }
        else if (nodeName == "option")
        {
            QString key = attributes.value("name").toString();
            QString value = attributes.value("value").toString();
            if (!key.isEmpty())
            {
                options[key] = value;
            }
        }

        reader.skipCurrentElement();
    }

    file.close();
    return true;
}

bool XmlConfiguration::initialize(const QString &fileName)
{
    if (fileName.isEmpty())
        configPath = DEFAULT_CONFIG_FILE;
    else
        configPath = fileName;

    const bool success = readFile(configPath);

    if (success)
        qDebug() << "Using config file: " << configPath;

    return success;
}

void XmlConfiguration::deinitialize()
{
    processedFiles.clear();
}

QString XmlConfiguration::getValue(const QString &key,
                                    const QString &deflt)
{
    std::map<QString, QString>::iterator iter = options.find(key);
    if (iter == options.end())
        return deflt;
    return iter->second;
}

int XmlConfiguration::getValue(const QString &key, int deflt)
{
    std::map<QString, QString>::iterator iter = options.find(key);
    if (iter == options.end())
        return deflt;
    return iter->second.toInt();
}

bool XmlConfiguration::getBoolValue(const QString &key, bool deflt)
{
    std::map<QString, QString>::iterator iter = options.find(key);
    if (iter == options.end())
        return deflt;
    return utils::stringToBool(iter->second, deflt);
}

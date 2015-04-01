/*
 *  XML utility functions
 *  Copyright (C) 2004-2009  The Mana World Development Team
 *  Copyright (C) 2009-2010  The Mana Developers
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

#include "utils/xml.h"

#include "common/resourcemanager.h"
#include "utils/string.h"

#include <iostream>
#include <fstream>

#include <QDebug>

namespace XML
{
Document::Document(const QString &fileName, bool useResman) : mDoc(nullptr)
    {
        QString resolvedFileName = fileName;
        if (useResman)
        {
            resolvedFileName = ResourceManager::resolve(fileName);

            if (resolvedFileName.isEmpty())
            {
                qWarning() << "(XML::Document) File not found in search path: "
                           << fileName;
                return;
            }
        }

        mDoc = xmlParseFile(resolvedFileName.toStdString().c_str());

        if (!mDoc)
        {
            qWarning() << "(XML::Document) Error parsing XML file: "
                       << resolvedFileName;
        }
    }

    Document::~Document()
    {
        if (mDoc)
            xmlFreeDoc(mDoc);
    }

    xmlNodePtr Document::rootNode()
    {
        return mDoc ? xmlDocGetRootElement(mDoc) : nullptr;
    }

    bool hasProperty(xmlNodePtr node, const char *name)
    {
        xmlChar *prop = xmlGetProp(node, BAD_CAST name);
        if (prop)
        {
            xmlFree(prop);
            return true;
        }

        return false;
    }

    bool getBoolProperty(xmlNodePtr node, const char *name, bool def)
    {
        bool ret = def;
        xmlChar *prop = xmlGetProp(node, BAD_CAST name);
        if (prop)
        {
            ret = utils::stringToBool((char*) prop, def);
            xmlFree(prop);
        }
        return ret;
    }

    int getProperty(xmlNodePtr node, const char *name, int def)
    {
        int &ret = def;

        xmlChar *prop = xmlGetProp(node, BAD_CAST name);
        if (prop)
        {
            ret = atol((char*) prop);
            xmlFree(prop);
        }

        return ret;
    }

    double getFloatProperty(xmlNodePtr node, const char *name, double def)
    {
        double &ret = def;

        xmlChar *prop = xmlGetProp(node, BAD_CAST name);
        if (prop)
        {
            ret = atof((char*) prop);
            xmlFree(prop);
        }

        return ret;
    }

    QString getProperty(xmlNodePtr node, const char *name,
                            const QString &def)
    {
        xmlChar *prop = xmlGetProp(node, BAD_CAST name);
        if (prop)
        {
            QString val = (char*) prop;
            xmlFree(prop);
            return val;
        }

        return def;
    }

    xmlNodePtr findFirstChildByName(xmlNodePtr parent, const char *name)
    {
        for_each_xml_child_node(child, parent)
            if (xmlStrEqual(child->name, BAD_CAST name))
                return child;

        return nullptr;
    }

} // namespace XML

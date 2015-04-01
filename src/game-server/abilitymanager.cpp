/*
 *  The Mana Server
 *  Copyright (C) 2004-2010  The Mana World Development Team
 *  Copyright (C) 2010-2012  The Mana Developers
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

#include "abilitymanager.h"

#include "utils/xml.h"

#include <QDebug>

static AbilityManager::TargetMode getTargetByString(const QString &str)
{
    QString strLower = str.toLower();
    if (strLower == "being")
        return AbilityManager::TARGET_BEING;
    else if (strLower == "point")
        return AbilityManager::TARGET_POINT;
    else if (strLower == "direction")
        return AbilityManager::TARGET_DIRECTION;

    qCritical() << "Unknown targetmode " << str << " assuming being.";
    return AbilityManager::TARGET_BEING;
}

/**
 * Check the status of recently loaded configuration.
 */
void AbilityManager::checkStatus()
{
    qDebug() << "Loaded " << mAbilitiesInfo.size() << " abilities";
}

void AbilityManager::readAbilityNode(xmlNodePtr abilityNode,
                                     const QString &filename)
{
    QString name = XML::getProperty(abilityNode, "name", QString()).toLower();
    int id = XML::getProperty(abilityNode, "id", 0);

    if (id <= 0 || name.isEmpty())
    {
        qWarning() << "Invalid ability (empty name or id <= 0) in " << filename;
        return;
    }

    AbilitiesInfo::iterator it = mAbilitiesInfo.find(id);
    if (it != mAbilitiesInfo.end())
    {
        qWarning() << "AbilityManager: The same id: " << id
                   << " is given for ability names: " << it->first
                   << " and " << name;
        qWarning() << "The ability reference: " << id
                   << ": '" << name << "' will be ignored.";
        return;
    }

    AbilityInfo *newInfo = new AbilityManager::AbilityInfo;
    newInfo->name = name;
    newInfo->id = id;

    newInfo->target = getTargetByString(XML::getProperty(abilityNode, "target",
                                                         QString()));

    mAbilitiesInfo[newInfo->id] = newInfo;

    mNamedAbilitiesInfo[name] = newInfo;
}

void AbilityManager::initialize()
{
    clear();
}

void AbilityManager::reload()
{
    clear();
}

void AbilityManager::clear()
{
    for (auto &elem : mAbilitiesInfo) {
        delete elem.second;
    }
    mAbilitiesInfo.clear();
    mNamedAbilitiesInfo.clear();
}

unsigned AbilityManager::getId(const QString &abilityName) const
{
    if (mNamedAbilitiesInfo.contains(abilityName))
        return mNamedAbilitiesInfo.value(abilityName)->id;
    else
        return 0;
}

const QString AbilityManager::getAbilityName(int id) const
{
    AbilitiesInfo::const_iterator it = mAbilitiesInfo.find(id);
    return it != mAbilitiesInfo.end() ? it->second->name : "";
}

AbilityManager::AbilityInfo *AbilityManager::getAbilityInfo(int id) const
{
    AbilitiesInfo::const_iterator it = mAbilitiesInfo.find(id);
    return it != mAbilitiesInfo.end() ? it->second : nullptr;
}

AbilityManager::AbilityInfo *AbilityManager::getAbilityInfo(
        const QString &abilityName) const
{
    if (mNamedAbilitiesInfo.contains(abilityName))
        return mNamedAbilitiesInfo.value(abilityName);
    else
        return nullptr;
}

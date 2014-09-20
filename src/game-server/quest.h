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

#ifndef GAMESERVER_QUEST_H
#define GAMESERVER_QUEST_H

#include <QString>

#include "scripting/scriptmanager.h"

class Entity;
class Script;


class QuestCallback
{
public:
    virtual ~QuestCallback()
    { }

    virtual void triggerCallback(Entity *ch,
                                 const QString &value) const = 0;
};

class QuestThreadCallback : public QuestCallback
{
public:
    typedef void (*Handler)(Entity *,
                            const QString &value,
                            Script *mScript);

    QuestThreadCallback(Handler handler,
                        Script *script) :
        mHandler(handler),
        mScript(script)
    { }

    void triggerCallback(Entity *ch, const QString &value) const
    { mHandler(ch, value, mScript); }

private:
    Handler mHandler;
    Script *mScript;
};

class QuestRefCallback : public QuestCallback
{
public:
    QuestRefCallback(Script *script, const QString &questName) :
        mQuestName(questName)
    { script->assignCallback(mRef); }

    void triggerCallback(Entity *ch, const QString &value) const;

private:
    Script::Ref mRef;
    QString mQuestName;
};

/**
 * Gets the value associated to a quest variable.
 * @return false if no value was in cache.
 */
bool getQuestVar(Entity *, const QString &name, QString &value);

/**
 * Sets the value associated to a quest variable.
 */
void setQuestVar(Entity *, const QString &name, const QString &value);

/**
 * Starts the recovery of a variable and returns immediatly. The callback will
 * be called once the value has been recovered.
 */
void recoverQuestVar(Entity *, const QString &name, QuestCallback *);

/**
 * Called by the handler of the account server when a value is received.
 */
void recoveredQuestVar(int id, const QString &name,
                       const QString &value);

#endif

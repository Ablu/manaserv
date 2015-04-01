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

#include "game-server/item.h"

#include "mana/configuration/interfaces/iconfiguration.h"

#include "game-server/attributemanager.h"
#include "game-server/being.h"
#include "game-server/state.h"
#include "scripting/script.h"
#include "scripting/scriptmanager.h"

#include <map>
#include <QString>

bool ItemEffectAttrMod::apply(Entity *itemUser)
{
    qDebug() << "Applying modifier.";
    itemUser->getComponent<BeingComponent>()->applyModifier(*itemUser,
                                                            mAttribute,
                                                            mMod,
                                                            mAttributeLayer,
                                                            mDuration,
                                                            mModId);
    return false;
}

void ItemEffectAttrMod::dispell(Entity *itemUser)
{
    qDebug() << "Dispelling modifier.";
    itemUser->getComponent<BeingComponent>()->removeModifier(*itemUser,
                                                             mAttribute,
                                                             mMod,
                                                             mAttributeLayer,
                                                             mModId,
                                                             !mDuration);
}

ItemEffectScript::~ItemEffectScript()
{
}

bool ItemEffectScript::apply(Entity *itemUser)
{
    if (mActivateEventName.isEmpty())
        return false;

    Script::Ref function = mItemClass->getEventCallback(mActivateEventName);
    if (function.isValid())
    {
        Script *script = ScriptManager::currentState();
        script->prepare(function);
        script->push(itemUser);
        script->push(mItemClass->getDatabaseID());
        script->execute(itemUser->getMap());
        // TODO return depending on script execution success.
        return true;
    }
    return false;
}

void ItemEffectScript::dispell(Entity *itemUser)
{
    if (mDispellEventName.isEmpty())
        return;

    Script::Ref function = mItemClass->getEventCallback(mDispellEventName);
    if (function.isValid())
    {
        Script *script = ScriptManager::currentState();
        script->prepare(function);
        script->push(itemUser);
        script->push(mItemClass->getDatabaseID());
        script->execute(itemUser->getMap());
    }
}

ItemClass::~ItemClass()
{
    while (mEffects.begin() != mEffects.end())
    {
        delete mEffects.begin()->second;
        mEffects.erase(mEffects.begin());
    }
}

void ItemClass::addEffect(ItemEffectInfo *effect,
                          ItemTriggerType id,
                          ItemTriggerType dispell)
{
    mEffects.insert(std::make_pair(id, effect));
    if (dispell)
        mDispells.insert(std::make_pair(dispell, effect));
}

bool ItemClass::useTrigger(Entity *itemUser, ItemTriggerType trigger)
{
    if (!trigger)
        return false;

    std::multimap<ItemTriggerType, ItemEffectInfo *>::iterator it, it_end;

    bool ret = false;
    for (it = mEffects.begin(), it_end = mEffects.end(); it != it_end; ++it)
        if (it->first == trigger)
            if (it->second->apply(itemUser))
                ret = true;

    for (it = mDispells.begin(), it_end = mDispells.end(); it != it_end; ++it)
        if (it->first == trigger)
            it->second->dispell(itemUser);

    return ret;
}

ItemComponent::ItemComponent(ItemClass *type,
                             int amount,
                             IConfiguration *configuration)
    : mConfiguration(configuration)
    , mType(type)
    , mAmount(amount)
{
    mLifetime = mConfiguration->getValue("game_floorItemDecayTime", 0) * 10;
}

void ItemComponent::update(Entity &entity)
{
    if (mLifetime)
    {
        mLifetime--;
        if (!mLifetime)
            GameState::enqueueRemove(&entity);
    }
}

namespace Item {

static IConfiguration *mConfiguration;

Entity *create(MapComposite *map,
              Point pos,
              ItemClass *itemClass,
              int amount)
{
    Entity *itemActor = new Entity(OBJECT_ITEM);
    ActorComponent *actorComponent = new ActorComponent(*itemActor);
    itemActor->addComponent(actorComponent);
    itemActor->addComponent(new ItemComponent(itemClass, amount, mConfiguration));
    itemActor->setMap(map);
    actorComponent->setPosition(*itemActor, pos);
    return itemActor;
}

void initialize(IConfiguration *configuration)
{
    mConfiguration = configuration;
}

} // namespace Item

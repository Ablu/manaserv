/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World  is free software; you can redistribute  it and/or modify it
 *  under the terms of the GNU General  Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or any later version.
 *
 *  The Mana  World is  distributed in  the hope  that it  will be  useful, but
 *  WITHOUT ANY WARRANTY; without even  the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *  more details.
 *
 *  You should  have received a  copy of the  GNU General Public  License along
 *  with The Mana  World; if not, write to the  Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  $Id$
 */

#include "game-server/monster.hpp"

#include "game-server/character.hpp"
#include "game-server/item.hpp"
#include "game-server/mapcomposite.hpp"
#include "game-server/state.hpp"
#include "utils/logger.h"

ItemClass *MonsterClass::getRandomDrop() const
{
    int p = rand() / (RAND_MAX / 10000);
    for (MonsterDrops::const_iterator i = mDrops.begin(),
         i_end = mDrops.end(); i != i_end; ++i)
    {
        p -= i->probability;
        if (p < 0)
        {
            return i->item;
        }
    }
    return NULL;
}

struct MonsterTargetEventDispatch: EventDispatch
{
    MonsterTargetEventDispatch()
    {
        typedef EventListenerFactory< Monster, &Monster::mTargetListener > Factory;
        removed = &Factory::create< Thing, &Monster::forgetTarget >::function;
        died = &Factory::create< Thing, &Monster::forgetTarget, Being >::function;
    }
};

static MonsterTargetEventDispatch monsterTargetEventDispatch;

Monster::Monster(MonsterClass *specy):
    Being(OBJECT_MONSTER, 65535),
    mSpecy(specy),
    mCountDown(0),
    mTargetListener(&monsterTargetEventDispatch),
    mOwner(NULL),
    mOwnerTimer(0),
    mAttackTime(0)
{
    LOG_DEBUG("Monster spawned!");

    // get basic attributes from monster database
    for (int i = BASE_ATTR_BEGIN; i < BASE_ATTR_END; i++)
    {
        setAttribute(i, specy->getAttribute(i));
    }

    // Some bogus stats for testing.
    // TODO: Get all this stuff from the monster database.
    mAgressive = false;
    mAgressionRange = 10;
    mAttackPreDelay = 10;
    mAttackAftDelay = 10;
    mAttackRange = 32;
    mAttackAngle = 10;
    setSpeed(300);
    setSize(8);
    mExpReward = 100;

    // Set positions relative to target from which the monster can attack
    mAttackPositions.push_back(AttackPosition(+32, 0, DIRECTION_LEFT));
    mAttackPositions.push_back(AttackPosition(-32, 0, DIRECTION_RIGHT));
    mAttackPositions.push_back(AttackPosition(0, +32, DIRECTION_DOWN));
    mAttackPositions.push_back(AttackPosition(0, -32, DIRECTION_UP));
}

Monster::~Monster()
{
    // Remove death listeners.
    for (std::map<Being *, int>::iterator i = mAnger.begin(),
         i_end = mAnger.end(); i != i_end; ++i)
    {
        i->first->removeListener(&mTargetListener);
    }
}

void Monster::perform()
{

    if (mAction == ATTACK)
    {
        if (mAttackTime == mAttackAftDelay)
        {
            // Hard-coded values for now.
            Damage damage;
            damage.base = getModifiedAttribute(BASE_ATTR_PHY_ATK_MIN);
            damage.delta = getModifiedAttribute(BASE_ATTR_PHY_ATK_DELTA);
            damage.cth = getModifiedAttribute(BASE_ATTR_HIT);
            damage.element = ELEMENT_NEUTRAL;
            damage.type = DAMAGE_PHYSICAL;
            damage.usedSkill = 0;
            performAttack(damage, mAttackRange, mAttackAngle);
        }
        if (!mAttackTime)
        {
            setAction(STAND);
        }
    }
}

void Monster::update()
{
    Being::update();

    if (mOwner && mOwnerTimer)
    {
        mOwnerTimer--;
    } else {
        mOwner = NULL;
    }

    // If dead do nothing but rot
    if (mAction == DEAD)
    {
        mCountDown--;
        if (mCountDown <= 0)
        {
            GameState::enqueueRemove(this);
        }
        return;
    }

    if (mAction == ATTACK)
    {
        mAttackTime--;
        return;
    }

    // Check potential attack positions
    Being *bestAttackTarget = NULL;
    int bestTargetPriority = 0;
    Point bestAttackPosition;
    Direction bestAttackDirection = DIRECTION_DOWN;

    // Iterate through objects nearby
    for (MovingObjectIterator i(getMap()->getAroundCharacterIterator(this, AROUND_AREA)); i; ++i)
    {
        // We only want to attack player characters
        if ((*i)->getType() != OBJECT_CHARACTER) continue;

        Being *target = static_cast<Being *> (*i);

        // Dead characters are ignored
        if (target->getAction() == DEAD) continue;

        // Determine how much we hate the target
        int targetPriority = 0;
        std::map<Being *, int, std::greater<Being *> >::iterator angerIterator;
        angerIterator = mAnger.find(target);
        if (angerIterator != mAnger.end())
        {
            targetPriority = angerIterator->second;
        }
        else if (mAgressive)
        {
            targetPriority = 1;
        }
        else
        {
            continue;
        }

        // Check all attack positions
        for (std::list<AttackPosition>::iterator j = mAttackPositions.begin();
             j != mAttackPositions.end();
             j++)
        {
            Point attackPosition = (*i)->getPosition();
            attackPosition.x += (*j).x;
            attackPosition.y += (*j).y;

            int posPriority = calculatePositionPriority(attackPosition,
                                                        targetPriority);
            if (posPriority > bestTargetPriority)
            {
                bestAttackTarget = target;
                bestTargetPriority = posPriority;
                bestAttackPosition = attackPosition;
                bestAttackDirection = (*j).direction;
            }
        }
    }

    // Check if an attack position has been found
    if (bestAttackTarget)
    {
        // Check if we are there
        if (bestAttackPosition == getPosition())
        {
            // We are there - let's beat the crap out of the target
            setDirection(bestAttackDirection);
            setAction(ATTACK);
            raiseUpdateFlags(UPDATEFLAG_ATTACK);
            mAttackTime = mAttackPreDelay + mAttackAftDelay;
        }
        else
        {
            // We aren't there yet - let's move
            setDestination(bestAttackPosition);
        }
    }
    else
    {
        // We have no target - let's wander around
        mCountDown--;
        if (mCountDown <= 0)
        {
            Point randomPos(rand() % 160 - 80 + getPosition().x,
                            rand() % 160 - 80 + getPosition().y);
            setDestination(randomPos);
            mCountDown = 10 + rand() % 10;
        }
    }
}

int Monster::calculatePositionPriority(Point position, int targetPriority)
{
    Point thisPos = getPosition();

    // Check if we already are on this position
    if (thisPos.x / 32 == position.x / 32 &&
        thisPos.y / 32 == position.y / 32)
    {
        return targetPriority *= mAgressionRange;
    }

    std::list<PATH_NODE> path;
    path = getMap()->getMap()->findPath(thisPos.x / 32, thisPos.y / 32,
                                        position.x / 32, position.y / 32,
                                        mAgressionRange);

    if (path.empty() || path.size() >= mAgressionRange)
    {
        return 0;
    }
    else
    {
        return targetPriority * (mAgressionRange - path.size());
    }
}

void Monster::forgetTarget(Thing *t)
{
    Being *b = static_cast< Being * >(t);
    mAnger.erase(b);
    b->removeListener(&mTargetListener);

    if (b->getType() == OBJECT_CHARACTER)
    {
        Character *c = static_cast< Character * >(b);
        mExpReceivers.erase(c);
        mLegalExpReceivers.erase(c);
    }
}

int Monster::damage(Object *source, Damage const &damage)
{
    int HPLoss = Being::damage(source, damage);
    if (HPLoss && source && source->getType() == OBJECT_CHARACTER)
    {
        Character *s = static_cast< Character * >(source);
        std::pair< std::map< Being *, int >::iterator, bool > ib =
            mAnger.insert(std::make_pair(s, HPLoss));

        if (ib.second)
        {
            s->addListener(&mTargetListener);
        }
        else
        {
            ib.first->second += HPLoss;
        }

        if (damage.usedSkill)
        {
            mExpReceivers[s].insert(damage.usedSkill);
            if (!mOwnerTimer || mOwner == s /*TODO: || mOwner->getParty() == s->getParty() */)
            {
                mOwner = s;
                mLegalExpReceivers.insert(s);
                mOwnerTimer = KILLSTEAL_PROTECTION_TIME;
            }
        }
    }
    return HPLoss;
}

void Monster::died()
{
    Being::died();
    mCountDown = 50; // Sets remove time to 5 seconds

    //drop item
    if (ItemClass *drop = mSpecy->getRandomDrop())
    {
        Item *item = new Item(drop, 1);
        item->setMap(getMap());
        item->setPosition(getPosition());
        GameState::enqueueInsert(item);
    }

    //distribute exp reward
    if (mExpReceivers.size() > 0)
    {
        std::map<Character *, std::set <size_t> > ::iterator iChar;
        std::set<size_t>::iterator iSkill;

        float expPerChar = mExpReward / mExpReceivers.size();

        for (iChar = mExpReceivers.begin(); iChar != mExpReceivers.end(); iChar++)
        {
            Character *character = (*iChar).first;
            std::set<size_t> *skillSet = &(*iChar).second;

            if (mLegalExpReceivers.find(character) == mLegalExpReceivers.end()
                || skillSet->size() < 1)
            {
                continue;
            }
            int expPerSkill = int(expPerChar / skillSet->size());
            for (iSkill = skillSet->begin(); iSkill != skillSet->end(); iSkill++)
            {
                character->receiveExperience(*iSkill, expPerSkill);
            }
        }
    }
}


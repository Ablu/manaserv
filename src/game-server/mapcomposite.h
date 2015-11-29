/*
 *  The Mana Server
 *  Copyright (C) 2006-2010  The Mana World Development Team
 *  Copyright (C) 2010-2011  The Mana Development Team
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

#ifndef MAPCOMPOSITE_H
#define MAPCOMPOSITE_H

#include <QString>
#include <vector>
#include <map>
#include <memory>

#include "scripting/script.h"
#include "mana/entities/map.h"

class Entity;
class Map;
class Point;
class Rectangle;

struct MapContent;
struct MapZone;

enum PvPRules
{
    PVP_NONE = 0,   // no PvP on this map
    PVP_FREE        // unrestricted PvP on this map
    // [space for additional PvP modes]
};

/**
 * Ordered sets of zones of a map.
 */
typedef std::vector< unsigned > MapRegion;

/**
 * Iterates through the zones of a region of the map.
 */
struct ZoneIterator
{
    MapRegion region; /**< Zones to visit. Empty means the entire map. */
    unsigned pos;
    MapZone *current;
    const MapContent *map;

    ZoneIterator(const MapRegion &, const MapContent *);
    void operator++();
    MapZone *operator*() const { return current; }
    operator bool() const { return current; }
};

/**
 * Iterates through the Characters of a region.
 */
struct CharacterIterator
{
    ZoneIterator iterator;
    unsigned short pos;
    Entity *current;

    CharacterIterator(const ZoneIterator &);
    void operator++();
    Entity *operator*() const { return current; }
    operator bool() const { return iterator; }
};

/**
 * Iterates through the Beings of a region.
 */
struct BeingIterator
{
    ZoneIterator iterator;
    unsigned short pos;
    Entity *current;

    BeingIterator(const ZoneIterator &);
    void operator++();
    Entity *operator*() const { return current; }
    operator bool() const { return iterator; }
};

/**
 * Iterates through the non-moving Actors of a region.
 */
struct FixedActorIterator
{
    ZoneIterator iterator;
    unsigned short pos;
    Entity *current;

    FixedActorIterator(const ZoneIterator &);
    void operator++();
    Entity *operator*() const { return current; }
    operator bool() const { return iterator; }
};

/**
 * Iterates through the Actors of a region.
 */
struct ActorIterator
{
    ZoneIterator iterator;
    unsigned short pos;
    Entity *current;

    ActorIterator(const ZoneIterator &);
    void operator++();
    Entity *operator*() const { return current; }
    operator bool() const { return iterator; }
};

/**
 * Combined map/entity structure.
 */
class MapComposite
{
public:
    MapComposite(int id,
                 const QString &name,
                 IConfiguration *configuration);
    MapComposite(const MapComposite &) = delete;
    ~MapComposite();

    void setMap(std::unique_ptr<Map> &&map);

    /**
     * Loads the map and initializes the map content. Should only be called
     * once!
     *
     * @return <code>true</code> when succesful, <code>false</code> when
     *         an error occurred.
     */
    bool activate();

    /**
     * Gets the underlying pathfinding map.
     */
    Map &getMap() const
    { return *mMap; }

    /**
     * Returns whether the map is active on this server or not.
     */
    bool isActive() const
    { return mActive; }

    /**
     * Gets the game ID of this map.
     */
    int getID() const
    { return mID; }

    /**
     * Gets the name of this map.
     */
    const QString &getName() const
    { return mName; }

    /**
     * Inserts a thing on the map. Sets its public ID if relevant.
     */
    bool insert(Entity *);

    /**
     * Removes a thing from the map.
     */
    void remove(Entity *);

    /**
     * Returns the actor entity matching \a publicID, or null when no such
     * entity exists.
     */
    Entity *findEntityById(int publicId) const;

    /**
     * Updates zones of every moving beings.
     */
    void update();

    /**
     * Gets the PvP rules on the map.
     */
    PvPRules getPvP() const { return mPvPRules; }

    /**
     * Gets an iterator on the objects of the whole map.
     */
    ZoneIterator getWholeMapIterator() const
    { return ZoneIterator(MapRegion(), mContent); }

    /**
     * Gets an iterator on the objects inside a given rectangle.
     */
    ZoneIterator getInsideRectangleIterator(const Rectangle &) const;

    /**
     * Gets an iterator on the objects around a given point.
     */
    ZoneIterator getAroundPointIterator(const Point &, int radius) const;

    /**
     * Gets an iterator on the objects around a given actor.
     */
    ZoneIterator getAroundActorIterator(Entity *, int radius) const;

    /**
     * Gets an iterator on the objects around the old and new positions of
     * a character (including the ones that were but are now elsewhere).
     */
    ZoneIterator getAroundBeingIterator(Entity *, int radius) const;

    /**
     * Gets everything related to the map.
     */
    const std::vector< Entity * > &getEverything() const;

    /**
     * Gets the cached value of a map-bound script variable
     */
    QString getVariable(const QString &key) const;

    /**
     * Changes a script variable and notifies the database server
     * about the change
     */
    void setVariable(const QString &key, const QString &value);

    /**
     * Changes a script variable without notifying the database server
     * about the change
     */
    void setVariableFromDbserver(const QString &key,
                                 const QString &value)
    { mScriptVariables[key] = value; }

    /**
     * Sets callback for map variable
     */
    void setMapVariableCallback(const QString &key, Script *script)
    { script->assignCallback(mMapVariableCallbacks[key]); }

    /**
     * Sets callback for global variable
     */
    void setWorldVariableCallback(const QString &key, Script *script)
    { script->assignCallback(mWorldVariableCallbacks[key]); }

    void callWorldVariableCallback(const QString &key,
                                   const QString &value);

    static void setInitializeCallback(Script *script)
    { script->assignCallback(mInitializeCallback); }

    static void setUpdateCallback(Script *script)
    { script->assignCallback(mUpdateCallback); }

    const MapObject *findMapObject(const QString &name,
                                   const QString &type) const;

private:
    void initializeContent();
    void callMapVariableCallback(const QString &key,
                                 const QString &value);

    IConfiguration *mConfiguration;
    bool mActive;         /**< Status of map. */
    std::unique_ptr<Map> mMap;
    MapContent *mContent; /**< Entities on the map. */
    QString mName;    /**< Name of the map. */
    unsigned short mID;   /**< ID of the map. */
    /** Cached persistent variables */
    std::map<QString, QString> mScriptVariables;
    PvPRules mPvPRules;
    std::map<const QString, Script::Ref> mMapVariableCallbacks;
    std::map<const QString, Script::Ref> mWorldVariableCallbacks;

    static Script::Ref mInitializeCallback;
    static Script::Ref mUpdateCallback;
};

#endif // MAPCOMPOSITE_H

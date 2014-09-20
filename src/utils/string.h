/*
 *  The Mana Server
 *  Copyright (C) 2004-2010  The Mana World Development Team
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

#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#include <QString>
#include <sstream>
#include <map>

namespace utils
{
/**
 * Returns the boolean value represented in a string, or default.
 */
bool stringToBool(const QString &s, bool defaultValue);

/**
 * A case-insensitive name map, mapping instances from a user-specified
 * type by their name.
 */
template<typename T> class NameMap
{
public:
    NameMap()
        : mDefault()
    {}

    void insert(const QString &name, T value)
    {
        mMap.insert(std::make_pair(name.toLower(), value));
    }

    const T &value(const QString &name) const
    {
        typename Map::const_iterator result = mMap.find(name.toLower());
        return result != mMap.end() ? result->second : mDefault;
    }

    T &operator[](const QString &name)
    {
        return mMap[name.toLower()];
    }

    bool contains(const QString &name) const
    {
        return mMap.find(name.toLower()) != mMap.end();
    }

    void clear()
    {
        mMap.clear();
    }

private:
    typedef std::map<QString, T> Map;

    Map mMap;
    const T mDefault;
};

} // namespace utils

#endif // UTILS_STRING_H

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

#include "utils/string.h"

#include <cctype>
#include <algorithm>
#include <sstream>

namespace utils {

bool stringToBool(const QString &s, bool defaultValue)
{
    QString a = s.toLower();
    if (a == "true" || a == "1" || a == "on" || a == "yes" || a == "y")
        return true;
    if (a == "false" || a == "0" || a == "off" || a == "no" || a == "n")
        return false;

    return defaultValue;
}

} // namespace utils

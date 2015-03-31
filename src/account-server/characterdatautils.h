/*
 *  The Mana Server
 *  Copyright (C) 2014-2014  The Mana Developers
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

#ifndef CHARACTERDATAUTILS_H
#define CHARACTERDATAUTILS_H

#include "net/messagein.h"
#include "net/messageout.h"

#include "mana/entities/character.h"

namespace CharacterDataUtils
{
void serialize(CharacterData &data, MessageOut &msg);
void deserialize(CharacterData &data, MessageIn &msg);
}

#endif // CHARACTERDATAUTILS_H
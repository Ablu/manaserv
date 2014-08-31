/*
 *  The Mana Server
 *  Copyright (C) 2008-2010  The Mana World Development Team
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

#include "post.h"

#include "character.h"

Letter::Letter(unsigned type, std::unique_ptr<CharacterData> sender, std::unique_ptr<CharacterData> receiver)
    : mId(0)
    , mType(type)
    , mExpiry(0)
    , mSender(std::move(sender))
    , mReceiver(std::move(receiver))
{
}

void Letter::setExpiry(unsigned long expiry)
{
    mExpiry = expiry;
}

unsigned long Letter::getExpiry() const
{
    return mExpiry;
}

void Letter::addText(const std::string &text)
{
    mContents = text;
}

std::string Letter::getContents() const
{
    return mContents;
}

void Letter::addAttachment(InventoryItem item)
{
    mAttachments.push_back(item);
}

CharacterData &Letter::getReceiver() const
{
    return *mReceiver;
}

CharacterData &Letter::getSender() const
{
    return *mSender;
}

std::vector<InventoryItem> Letter::getAttachments() const
{
    return mAttachments;
}

Post::~Post()
{
    std::vector<Letter*>::iterator itr_end = mLetters.end();
    for (std::vector<Letter*>::iterator itr = mLetters.begin();
         itr != itr_end;
         ++itr)
    {
        delete (*itr);
    }

    mLetters.clear();
}

void Post::addLetter(Letter *letter)
{
    mLetters.push_back(letter);
}

Letter* Post::getLetter(int letter) const
{
    if (letter < 0 || (size_t) letter > mLetters.size())
    {
        return nullptr;
    }
    return mLetters[letter];
}

unsigned Post::getNumberOfLetters() const
{
    return mLetters.size();
}

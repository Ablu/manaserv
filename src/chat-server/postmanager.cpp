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

#include "postmanager.h"

#include "mana/entities/post.h"

#include "common/configuration.h"

void PostManager::addLetter(Letter *letter)
{
    std::map<CharacterData*, Post*>::iterator itr =
        mPostBox.find(letter->getReceiver());
    if (itr != mPostBox.end())
    {
        unsigned maximumLetterCountPerMail =
                Configuration::getValue("mail_maxLetters", 10);

        if (maximumLetterCountPerMail > itr->second->getNumberOfLetters())
        {
            itr->second->addLetter(letter);
        }
    }
    else
    {
        Post *post = new Post();
        post->addLetter(letter);
        mPostBox.insert(
            std::pair<CharacterData*, Post*>(letter->getReceiver(), post)
            );
    }
}

Post *PostManager::getPost(CharacterData *player) const
{
    std::map<CharacterData*, Post*>::const_iterator itr = mPostBox.find(player);
    return (itr == mPostBox.end()) ? nullptr : itr->second;
}

void PostManager::clearPost(CharacterData *player)
{
    std::map<CharacterData*, Post*>::iterator itr =
        mPostBox.find(player);
    if (itr != mPostBox.end())
    {
        delete itr->second;
        mPostBox.erase(itr);
    }
}

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

#ifndef POSTMANAGER_H
#define POSTMANAGER_H

#include <map>
#include <memory>

class IConfiguration;
class Letter;
class Post;

class CharacterData;

class PostManager
{
public:
    PostManager(IConfiguration *configuration)
        : mConfiguration(configuration)
    {}

    /**
     * Add letter to post box
     * @param letter Letter to add
     */
    void addLetter(Letter *letter);

    /**
     * Get post for character
     * @param player Character that is getting post
     * @return Returns the post for that character
     */
    Post *getPost(CharacterData &player) const;

    /**
     * Remove the post for character
     */
    void clearPost(CharacterData &player);

private:
    std::map<int, Post*> mPostBox;
    IConfiguration *mConfiguration;
};

extern PostManager *postalManager;

#endif

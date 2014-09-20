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

#include <algorithm>

#include "utils/stringfilter.h"

#include "mana/configuration/interfaces/iconfiguration.h"

#include "utils/logger.h"

namespace utils
{

StringFilter::StringFilter(IConfiguration *configuration):
    mConfiguration(configuration),
    mInitialized(false)
{
    loadSlangFilterList();
}

bool StringFilter::loadSlangFilterList()
{
    mInitialized = false;

    const QString slangsList = mConfiguration->getValue("SlangsList",
                                                            QString());
    if (!slangsList.isEmpty()) {
        mSlangs = slangsList.split(',');
        mInitialized = true;
    }

    return mInitialized;
}

bool StringFilter::filterContent(const QString &text) const
{
    if (!mInitialized) {
        LOG_DEBUG("Slangs List is not initialized.");
        return true;
    }

    return !mSlangs.contains(text, Qt::CaseInsensitive);
}

bool StringFilter::isEmailValid(const QString &email) const
{
    int min = mConfiguration->getValue("account_minEmailLength", 7);
    int max = mConfiguration->getValue("account_maxEmailLength", 128);

    // Testing email validity
    if (email.length() < min || email.length() > max)
    {
        return false;
    }

    // TODO Find some nice regex for this...
    return (email.indexOf('@') != -1) && (email.indexOf('.') != -1) &&
           (email.indexOf(' ') == -1);
}

bool StringFilter::findDoubleQuotes(const QString &text) const
{
    return text.contains('"');
}

} // ::utils

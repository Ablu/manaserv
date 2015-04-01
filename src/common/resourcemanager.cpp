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

#include "common/resourcemanager.h"

#include "mana/configuration/interfaces/iconfiguration.h"

#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <QDebug>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

#include <physfs.h>

#ifndef PKG_DATADIR
#define PKG_DATADIR "."
#endif


IConfiguration *mConfiguration; // TODO: make this a class and hide it

void ResourceManager::initialize(IConfiguration *configuration)
{
    mConfiguration = configuration;

    PHYSFS_permitSymbolicLinks(1);

    const QString worldDataPath =
            mConfiguration->getValue("worldDataPath", "example");

    // world first to allow overriding of server's libraries
    PHYSFS_addToSearchPath(worldDataPath.toStdString().c_str(), 1);
    PHYSFS_addToSearchPath(".", 1);
    PHYSFS_addToSearchPath(PKG_DATADIR, 1);
}

/**
 * This function tries to check if a file exists based on the existence of
 * stats about it. Because simply trying to open it and check for failure is a
 * bad thing, as you don't know if you weren't able to open it because it
 * doesn't exist or because you don't have the right to.
 */
static bool fileExists(const QString &filename)
{
    struct stat buffer;
    // When stat is succesful, the file exists
    return stat(filename.toStdString().c_str(), &buffer) == 0;
}

bool ResourceManager::exists(const QString &path, bool lookInSearchPath)
{
    if (!lookInSearchPath)
        return fileExists(path);

    return PHYSFS_exists(path.toStdString().c_str());
}

QString ResourceManager::resolve(const QString &path)
{
    const char *realDir = PHYSFS_getRealDir(path.toStdString().c_str());
    if (realDir)
        return QString(realDir) + "/" + path;

    return QString();
}

char *ResourceManager::loadFile(const QString &fileName, int &fileSize)
{
    // Attempt to open the specified file using PhysicsFS
    PHYSFS_file *file = PHYSFS_openRead(fileName.toStdString().c_str());

    // If the handler is an invalid pointer indicate failure
    if (file == nullptr)
    {
        qWarning() << "Failed to load '" << fileName << "': "
                   << PHYSFS_getLastError();
        return nullptr;
    }

    // Get the size of the file
    fileSize = PHYSFS_fileLength(file);

    // Allocate memory and load the file
    char *buffer = (char *) malloc(fileSize + 1);
    if (PHYSFS_read(file, buffer, 1, fileSize) != fileSize)
    {
        free(buffer);
        qWarning() << "Failed to load '" << fileName << "': "
                   << PHYSFS_getLastError();
        return nullptr;
    }

    // Close the file and let the user deallocate the memory
    PHYSFS_close(file);

    // Add a trailing null character, so that the file can be used as a string
    buffer[fileSize] = 0;
    return buffer;
}

ResourceManager::splittedPath ResourceManager::splitFileNameAndPath(
                                                const QString &fullFilePath)
{
    // We'll reversed-search for '/' or'\' and extract the substrings
    // corresponding to the filename and the path separately.
    int slashPos = fullFilePath.lastIndexOf("/\\");

    ResourceManager::splittedPath splittedFilePath;
    // Note the last slash is kept in the path name.
    splittedFilePath.path = fullFilePath.mid(0, slashPos + 1);
    splittedFilePath.file = fullFilePath.mid(slashPos + 1);

    return splittedFilePath;
}

/**
 * Join two path elements into one.
 *
 * This function helps build relative paths.
 *
 * Examples:
 *
 *     /foo + bar = /foo/bar
 *     /foo/ + bar = /foo/bar
 *     /foo + /bar = /bar
 *
 * This will work for PhysFS paths. Windows style paths (prefixed with drive letters) won't work.
 *
 * @return Joined paths or path2 if path2 was an absolute path.
 */
QString ResourceManager::joinPaths(const QString& path1, const QString& path2)
{
    if (path2.isEmpty())
        return path1;

    if (path1.isEmpty())
        return path2;

    // check if path2 is an absolute path that cannot be joined
    if (path2[0] == '/' || path2[0] == '\\')
        return path2;

    QChar p1end = path1[path1.size()-1];
    if (p1end == '/' || p1end == '\\')
    {
        return path1 + path2;
    }
    else
    {
        return path1 + "/" + path2;
    }
}

/**
 * Removes relative elements from the path.
 */
QString ResourceManager::cleanPath(const QString& path)
{
    int prev, cur;
    QString part, result;
    std::vector<QString> pathStack;

    prev = 0;
    while (true)
    {
        cur = path.indexOf("/\\", prev);
        if (cur == -1)
        {
            // FIXME add everything from prev to the end
            pathStack.push_back(path.mid(prev));
            break;
        }

        part = path.mid(prev, cur - prev);
        if (part == "..")
        {
            // go back one level
            if (!pathStack.empty())
            {
                pathStack.pop_back();
            }
        }
        else if (part == ".")
        {
            // do nothing
        }
        else if (part == "")
        {
            if (pathStack.empty() && cur == 0)
            {
                // handle first empty match before the root slash
                pathStack.push_back(QString());
            }
            else
            {
                // empty match in the middle of the path should be ignored
            }
        }
        else
        {
            // normal path element
            pathStack.push_back(part);
        }

        cur++;
        prev = cur;
    }

    // join the pathStack into a normal path
    unsigned int i = 0;
    for (i = 0; i < pathStack.size(); i++)
    {
        result += pathStack[i];
        if (i < pathStack.size() - 1) {
            result += "/";
        }
    }

    return result;
}

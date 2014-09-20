/*
 *  The Mana Server
 *  Copyright (C) 2004-2010  The Mana World Development Team
 *  Copyright (C) 2010  The Mana Development Team
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

#include "logger.h"

#include "mana/configuration/interfaces/iconfiguration.h"

#include "common/resourcemanager.h"

#include <fstream>
#include <iostream>

#include <QDate>

#ifdef WIN32
#include <windows.h>
#endif

namespace utils
{
/** Log file. */
static std::ofstream mLogFile;

IConfiguration *Logger::mConfiguration;

/** current log filename */
QString Logger::mFilename;
/** Timestamp flag. */
bool Logger::mHasTimestamp = true;
/** Tee mode flag. */
bool Logger::mTeeMode = false;
/** Verbosity level. */
Logger::Level Logger::mVerbosity = Logger::Info;
/** Enables logrotation by size of the logfile. */
bool Logger::mLogRotation = false;
/** Maximum size of current logfile. */
long Logger::mMaxFileSize = 1024; // 1 Mb
/** Switch log file each day. */
bool Logger::mSwitchLogEachDay = false;
/** Last call date */
static QString mLastCallDate;
/**
 * Old date
 * For code simplificatiion, the old Date is kept separate
 * from the last call date.
 */
static QString mOldDate;

/**
  * Check whether the day has changed since the last call.
  *
  * @return whether the day has changed.
  */
static bool getDayChanged()
{
    QString dayDate = QDate::currentDate().toString("yyyy-MM-dd");

    if (mLastCallDate != dayDate)
    {
        // Keep track of the old date.
        mOldDate = mLastCallDate;
        // Reset the current date for next call.
        mLastCallDate = dayDate;
        return true;
    }
    return false;
}

void Logger::initialize(const QString &logFile, IConfiguration *configuration)
{
    mConfiguration = configuration;

    setLogFile(logFile, true);

    // Write the messages to both the screen and the log file.
    setTeeMode(mConfiguration->getBoolValue("log_toStandardOutput", true));
    LOG_INFO("Using log file: " << logFile);

    // Set up the options related to log rotation.
    setLogRotation(mConfiguration->getBoolValue("log_enableRotation", false));
    setMaxLogfileSize(mConfiguration->getValue("log_maxFileSize", 1024));
    setSwitchLogEachDay(mConfiguration->getBoolValue("log_perDay", false));
}

void Logger::output(std::ostream &os, const QString &msg, const char *prefix)
{
    if (mHasTimestamp)
    {
        os << "[" << QDate::currentDate().toString("yyyy-MM-dd").toStdString()
           << "]" << ' ';
    }

    if (prefix)
    {
        os << prefix << ' ';
    }

    os << msg.toStdString() << std::endl;
}

void Logger::setLogFile(const QString &logFile, bool append)
{
    // Close the current log file.
    if (mLogFile.is_open())
    {
        mLogFile.close();
    }

    // Open the file for output
    // and remove the former file contents depending on the append flag.
    mLogFile.open(logFile.toStdString().c_str(),
                  append ? std::ios::app : std::ios::trunc);

    mFilename = logFile;
    mLastCallDate = mOldDate = QDate::currentDate().toString("yyyy-MM-dd");

    if (!mLogFile.is_open())
    {
        throw std::ios::failure("unable to open " + logFile.toStdString() +
                                "for writing");
    }
    else
    {
        // By default the streams do not throw any exception
        // let std::ios::failbit and std::ios::badbit throw exceptions.
        mLogFile.exceptions(std::ios::failbit | std::ios::badbit);
    }
}

void Logger::output(const QString &msg, Level atVerbosity)
{
    if (mVerbosity >= atVerbosity)
    {
        static const char *prefixes[] =
        {
        #ifdef T_COL_LOG
            "[\033[45mFTL\033[0m]",
            "[\033[41mERR\033[0m]",
            "[\033[43mWRN\033[0m]",
        #else
            "[FTL]",
            "[ERR]",
            "[WRN]",
        #endif
            "[INF]",
            "[DBG]"
        };

        bool open = mLogFile.is_open();

        if (open)
        {
            output(mLogFile, msg, prefixes[atVerbosity]);
            switchLogs();
        }

        if (!open || mTeeMode)
        {
            output(atVerbosity <= Warn ? std::cerr : std::cout,
                   msg, prefixes[atVerbosity]);
        }
    }
}

void Logger::switchLogs()
{
    // Handles logswitch if enabled
    // and if at least one switch condition permits it.
    if (!mLogRotation || (mMaxFileSize <= 0 && !mSwitchLogEachDay))
        return;

    // Update current filesize
    long fileSize = mLogFile.tellp();

    bool dayJustChanged = getDayChanged();

    if ((fileSize >= (mMaxFileSize * 1024))
        || (mSwitchLogEachDay && dayJustChanged))
    {
        // Close logfile, rename it and open a new one
        mLogFile.flush();
        mLogFile.close();

        // Stringify the time, the format is: path/yyyy-mm-dd-n_logFilename.
        using namespace std;
        QString date =
            (dayJustChanged ? mOldDate
                            : QDate::currentDate().toString("yyyy-MM-dd"));

        int fileNum = 1;
        ResourceManager::splittedPath filePath =
                               ResourceManager::splitFileNameAndPath(mFilename);

        QString newFileName;
        // Keeping a hard limit of 100 files per day.
        do
        {
            newFileName = filePath.path + date + "-" +
                          QString::number(fileNum) + "_" + filePath.file;
        }
        while (ResourceManager::exists(newFileName, false) && ++fileNum < 100);

        if (rename(mFilename.toStdString().c_str(),
                   newFileName.toStdString().c_str()) != 0)
        {
            // Continue appending on the original file.
            setLogFile(mFilename, true);
            mLogFile << "Error renaming file: " << mFilename.toStdString()
                     << " to: " << newFileName.toStdString() << std::endl
                     << "Keep logging on the same log file." << std::endl;
        }
        else
        {
            // Keep the logging after emptying the original log file.
            setLogFile(mFilename);
            mLogFile << "---- Continue logging from former file "
                     << newFileName.toStdString() << " ----" << std::endl;
        }
    }
}

} // namespace utils

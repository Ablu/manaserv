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

#include "pqdataprovider.h"

#include "dalexcept.h"

#include "common/configuration.h"

#include "utils/logger.h"
#include "utils/string.h"

namespace dal
{

const std::string  PqDataProvider::CFGPARAM_PQ_HOST ="pq_hostname";
const std::string  PqDataProvider::CFGPARAM_PQ_PORT ="pq_port";
const std::string  PqDataProvider::CFGPARAM_PQ_DB   ="pq_database";
const std::string  PqDataProvider::CFGPARAM_PQ_USER ="pq_username";
const std::string  PqDataProvider::CFGPARAM_PQ_PWD  ="pq_password";

const std::string  PqDataProvider::CFGPARAM_PQ_HOST_DEF = "";
const unsigned int PqDataProvider::CFGPARAM_PQ_PORT_DEF = 5432;
const std::string  PqDataProvider::CFGPARAM_PQ_DB_DEF   = "mana";
const std::string  PqDataProvider::CFGPARAM_PQ_USER_DEF = "mana";
const std::string  PqDataProvider::CFGPARAM_PQ_PWD_DEF  = "mana";

PqDataProvider::PqDataProvider()
    throw()
        : mDb(0)
        , mInTransaction(false)
        , mAffectedRows(0)
{
}

PqDataProvider::~PqDataProvider()
    throw()
{
    if (mIsConnected)
        disconnect();
}

/**
 * Get the database backend name.
 */
DbBackends PqDataProvider::getDbBackend() const
    throw()
{
    return DB_BKEND_POSTGRESQL;
}

/**
 * Create a connection to the database.
 */
void PqDataProvider::connect()
{
    const std::string hostname =
            Configuration::getValue(CFGPARAM_PQ_HOST, CFGPARAM_PQ_HOST_DEF);
    const std::string dbName =
            Configuration::getValue(CFGPARAM_PQ_DB, CFGPARAM_PQ_DB_DEF);
    const std::string userName =
            Configuration::getValue(CFGPARAM_PQ_USER, CFGPARAM_PQ_USER_DEF);
    const std::string password =
            Configuration::getValue(CFGPARAM_PQ_PWD, CFGPARAM_PQ_PWD_DEF);
    const unsigned int tcpPort =
            Configuration::getValue(CFGPARAM_PQ_PORT, CFGPARAM_PQ_PORT_DEF);

    // Create string to pass to PQconnectdb
    std::string connStr = "dbname = " + dbName + " ";
    if (!userName.empty())
        connStr += "user = " + userName + " ";
    if (!password.empty())
        connStr += "password = " + password + " ";
    if (!hostname.empty())
        connStr += "hostname = " + hostname + " ";
    connStr += "port = " + tcpPort;


    // Connect to database
    mDb = PQconnectdb(connStr.c_str());

    if (PQstatus(mDb) != CONNECTION_OK)
    {
        std::string error = PQerrorMessage(mDb);
        PQfinish(mDb);
        throw DbConnectionFailure(error);
    }

    // Save the Db Name.
    mDbName = dbName;

    mIsConnected = true;
}

/**
 * Execute a SQL query.
 */
const RecordSet &PqDataProvider::execSql(const std::string& sql,
                                         const bool refresh)
{
    if (!mIsConnected)
        throw std::runtime_error("not connected to database");

    if (refresh || (sql != mSql))
    {
        mRecordSet.clear();

        // execute the query
        PGresult *res = PQexec(mDb, sql.c_str());
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
        {
            PQclear(res);
            throw DbSqlQueryExecFailure(PQerrorMessage(mDb));
        }

        // get changed row count
        string_to<unsigned> toUint;
        mAffectedRows = toUint(PQcmdTuples(res));

        // get field count
        unsigned nFields = PQnfields(res);

        // fill column names
        Row fieldNames;
        for (unsigned i = 0; i < nFields; i++)
        {
            fieldNames.push_back(PQfname(res, i));
        }
        mRecordSet.setColumnHeaders(fieldNames);

        // fill rows
        for (unsigned r = 0; r < PQntuples(res); r++)
        {
            Row row;

            for (unsigned i = 0; i < nFields; i++)
                row.push_back(PQgetvalue(res, r, i));

            mRecordSet.add(row);
        }

        // clear results
        PQclear(res);
    }
    return mRecordSet;
}

/**
 * Close connection to database.
 */
void PqDataProvider::disconnect()
{
    if (!mIsConnected)
        return;

    // finish up with Postgre.
    PQfinish(mDb);

    mDb = 0;
    mIsConnected = false;
}

void PqDataProvider::beginTransaction()
    throw (std::runtime_error)
{
    if (!mIsConnected)
    {
        const std::string error = "Trying to begin a transaction while not "
                                  "connected to the database!";
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }
    if (inTransaction())
    {
        const std::string error = "Trying to begin a transaction while another "
                                  "one is still open!";
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }
    PGresult *res = PQexec(mDb, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        const std::string error = "BEGIN command failed: %s" +
                                  PQerrorMessage(mDb);
        LOG_ERROR(error);
        PQclear(res);
        throw std::runtime_error(error);
    }
    PQclear(res);
    mInTransaction = true;
}

void PqDataProvider::commitTransaction()
    throw (std::runtime_error)
{
    if (!mIsConnected)
    {
        const std::string error = "Trying to end a transaction while not "
                                  "connected to the database!";
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }
    if (!inTransaction())
    {
        const std::string error = "Trying to end a transaction while no "
                                  "one is open!";
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }
    PGresult *res = PQexec(mDb, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        const std::string error = "COMMIT command failed: %s" +
                                  PQerrorMessage(mDb);
        LOG_ERROR(error);
        PQclear(res);
        throw std::runtime_error(error);
    }
    PQclear(res);
    mInTransaction = false;
}

void PqDataProvider::rollbackTransaction()
    throw (std::runtime_error)
{
    if (!mIsConnected)
    {
        const std::string error = "Trying to end a transaction while not "
                                  "connected to the database!";
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }
    if (!inTransaction())
    {
        const std::string error = "Trying to end a transaction while no "
                                  "one is open!";
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }
    PGresult *res = PQexec(mDb, "ROLLBACK");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        const std::string error = "ROLLBACK command failed: %s" +
                                  PQerrorMessage(mDb);
        LOG_ERROR(error);
        PQclear(res);
        throw std::runtime_error(error);
    }
    PQclear(res);
    mInTransaction = false;
}

bool PqDataProvider::inTransaction() const
{
    return mInTransaction;
}

unsigned PqDataProvider::getModifiedRows() const
{
    return mAffectedRows;
}

unsigned PqDataProvider::getLastId() const
{
    PGresult *res = PQexec(mDb, "SELECT lastval();");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        PQclear(res);
        throw DbSqlQueryExecFailure(PQerrorMessage(mDb));
    }
    string_to<unsigned> toUint;
    unsigned id = toUint(PQgetvalue(res, 0, 0));
    PQclear(res);
    return id;
}

bool PqDataProvider::prepareSql(const std::string &sql)
{

}

const RecordSet &PqDataProvider::processSql()
{

}

void PqDataProvider::bindValue(int place, const std::string &value)
{

}

void PqDataProvider::bindValue(int place, int value)
{

}

} // namespace dal

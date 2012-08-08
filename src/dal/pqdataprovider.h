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

#ifndef PQDATAPROVIDER_H
#define PQDATAPROVIDER_H

#include <iosfwd>
#include <libpq-fe.h>

#include "dataprovider.h"

namespace dal
{

/**
 * A PostgreSQL Data Provider.
 */
class PqDataProvider: public DataProvider
{
    public:
        PqDataProvider()
            throw();

        ~PqDataProvider()
            throw();

        /**
         * Get the name of the database backend.
         *
         * @return the database backend name.
         */
        virtual DbBackends getDbBackend() const
            throw();

        /**
         * Create a connection to the database.
         *
         * Each dataprovider is responsible to have default values and load
         * necessary options from the config file.
         *
         * @exception DbConnectionFailure if unsuccessful connection.
         */
        virtual void connect();


        /**
         * Execute a SQL query.
         *
         * @param sql the SQL query.
         * @param refresh if true, refresh the cache (default = false).
         *
         * @return a recordset.
         *
         * @exception DbSqlQueryExecFailure if unsuccessful execution.
         * @exception std::runtime_error if trying to query a closed database.
         */
        virtual const RecordSet&
        execSql(const std::string& sql,
                const bool refresh = false);


        /**
         * Close the connection to the database.
         *
         * @exception DbDisconnectionFailure if unsuccessful disconnection.
         */
        virtual void disconnect();

        std::string getDbName() const;

        /**
         * Starts a transaction.
         *
         * @exception std::runtime_error if a transaction is still open
         */
        virtual void beginTransaction()
            throw (std::runtime_error);

        /**
         * Commits a transaction.
         *
         * @exception std::runtime_error if no connection is currently open.
         */
        virtual void commitTransaction()
            throw (std::runtime_error);

        /**
         * Rollback a transaction.
         *
         * @exception std::runtime_error if no connection is currently open.
         */
        virtual void rollbackTransaction()
            throw (std::runtime_error);

        /**
         * Returns whether the data provider is currently in a transaction.
         */
        virtual bool inTransaction() const;

        /**
         * Returns the number of changed rows by the last executed SQL
         * statement.
         *
         * @return Number of rows that have changed.
         */
        virtual unsigned getModifiedRows() const;

        /**
         * Returns the last inserted value of an autoincrement column after an
         * INSERT statement.
         *
         * @return last autoincrement value.
         */
        virtual unsigned getLastId() const;

        /**
         * Prepare SQL statement
         */
        virtual bool prepareSql(const std::string &sql);

        /**
         * Process SQL statement
         * SQL statement needs to be prepared and parameters binded before
         * calling this function
         */
        virtual const RecordSet& processSql();

        /**
         * Bind Value (String)
         * @param place - which parameter to bind to
         * @param value - the string to bind
         */
        virtual void bindValue(int place, const std::string &value);

        /**
         * Bind Value (Integer)
         * @param place - which parameter to bind to
         * @param value - the integer to bind
         */
        virtual void bindValue(int place, int value);

    private:
        PGconn *mDb; /**<  Database connection handle */
        bool mInTransaction;
        unsigned mAffectedRows;


        /** defines the name of the hostname config parameter */
        static const std::string CFGPARAM_PQ_HOST;
        /** defines the name of the server port config parameter */
        static const std::string CFGPARAM_PQ_PORT;
        /** defines the name of the database config parameter */
        static const std::string CFGPARAM_PQ_DB;
        /** defines the name of the username config parameter */
        static const std::string CFGPARAM_PQ_USER;
        /** defines the name of the password config parameter */
        static const std::string CFGPARAM_PQ_PWD;

        /** defines the default value of the CFGPARAM_PQ_HOST parameter */
        static const std::string CFGPARAM_PQ_HOST_DEF;
        /** defines the default value of the CFGPARAM_PQ_PORT parameter */
        static const unsigned int CFGPARAM_PQ_PORT_DEF;
        /** defines the default value of the CFGPARAM_PQ_DB parameter */
        static const std::string CFGPARAM_PQ_DB_DEF;
        /** defines the default value of the CFGPARAM_PQ_USER parameter */
        static const std::string CFGPARAM_PQ_USER_DEF;
        /** defines the default value of the CFGPARAM_PQ_PWD parameter */
        static const std::string CFGPARAM_PQ_PWD_DEF;
};


} // namespace dal

#endif // PQDATAPROVIDER_H

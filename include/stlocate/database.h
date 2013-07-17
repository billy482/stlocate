/*************************************************************************\
*                  ______  __                 __                          *
*                 / __/ /_/ /  ___  _______ _/ /____                      *
*                _\ \/ __/ /__/ _ \/ __/ _ `/ __/ -_)                     *
*               /___/\__/____/\___/\__/\_,_/\__/\__/                      *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of StLocate                                        *
*                                                                         *
*  StLocate is free software; you can redistribute it and/or              *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 17 Jul 2013 22:28:50 +0200                         *
\*************************************************************************/

#ifndef __STLOCATE_DATABASE_H__
#define __STLOCATE_DATABASE_H__

struct sl_hashtable;
struct stat;

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct sl_filesystem;

/**
 * \struct sl_database_connection
 * \brief A database connection
 */
struct sl_database_connection {
	/**
	 * \struct sl_database_connection_ops
	 * \brief Operations on a database connection
	 *
	 * \var ops
	 * \brief Operations
	 */
	struct sl_database_connection_ops {
		/**
		 * \brief close \a db connection
		 *
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 */
		int (*close)(struct sl_database_connection * connect);
		/**
		 * \brief free memory associated with database connection
		 *
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 *
		 * \warning Implementation of this function SHOULD NOT call
		 * \code
		 * free(db);
		 * \endcode
		 */
		int (*free)(struct sl_database_connection * connect);
		/**
		 * \brief check if the connection to database is closed
		 *
		 * \param[in] db a database connection
		 * \return 0 if the connection is not closed
		 */
		bool (*is_connection_closed)(struct sl_database_connection * connect);

		/**
		 * \brief Rool back a transaction
		 *
		 * \param[in] db a database connection
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*cancel_transaction)(struct sl_database_connection * connect);
		/**
		 * \brief Finish a transaction
		 *
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*finish_transaction)(struct sl_database_connection * connect);
		/**
		 * \brief Starts a transaction
		 *
		 * \param[in] connection a database connection
		 * \param[in] readOnly is a read only transaction
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*start_transaction)(struct sl_database_connection * connect);

		int (*create_database)(struct sl_database_connection * connect, int version);
		int (*get_database_version)(struct sl_database_connection * connect);

		int (*end_session)(struct sl_database_connection * connect, int session_id);
		int (*get_host_by_name)(struct sl_database_connection * connect, const char * hostname);
		int (*start_session)(struct sl_database_connection * connect);
		int (*sync_file)(struct sl_database_connection * connect, int s2fs, const char * filename, struct stat * st);
		int (*sync_filesystem)(struct sl_database_connection * connect, int host_id, int session_id, struct sl_filesystem * fs);
	} * ops;

	/**
	 * \brief private data
	 */
	void * data;
	/**
	 * \brief Reference to a database driver
	 */
	struct sl_database * driver;
	/**
	 * \brief Reference to a database configuration
	 */
	struct sl_database_config * config;
};

/**
 * \struct sl_database_config
 * \brief Describe a database configuration
 */
struct sl_database_config {
	/**
	 * \brief name of config
	 */
	char * name;

	/**
	 * \struct sl_database_config_ops
	 *
	 * \var ops
	 * \brief Database operations which require a configuration
	 */
	struct sl_database_config_ops {
		/**
		 * \brief Create a new connection to database
		 *
		 * \returns a database connection
		 */
		struct sl_database_connection * (*connect)(struct sl_database_config * db_config);
		/**
		 * \brief This function releases all memory associated to database configuration
		 *
		 * \param[in] database configuration
		 */
		void (*free)(struct sl_database_config * db_config);
		/**
		 * \brief Check if database is online
		 */
		int (*ping)(struct sl_database_config * db_config);
	} * ops;

	/**
	 * \brief private data
	 */
	void * data;
	/**
	 * \brief Reference to a database driver
	 */
	struct sl_database * driver;
};

/**
 * \struct sl_database
 * \brief Database driver
 *
 * A unique instance by type of database
 */
struct sl_database {
	/**
	 * \brief Name of driver
	 *
	 * \note Should be unique and equals to libdb-name.so where name is the name of driver.
	 */
	char * name;
	/**
	 * \struct sl_database_ops
	 * \brief Operations on one database driver
	 *
	 * \var ops
	 * \brief Database operation
	 */
	struct sl_database_ops {
		/**
		 * \brief Add configure to this database driver
		 *
		 * \param[in] params hashtable which contains parameters
		 * \returns \b 0 if ok
		 */
		struct sl_database_config * (*add)(const struct sl_hashtable * params);
		/**
		 * \brief Get default database configuration
		 */
		struct sl_database_config * (*get_default_config)(void);

		int (*get_max_version_supported)(void);
	} * ops;

	/**
	 * \brief cookie
	 *
	 * Is a value returns by dlopen and should not be used nor released
	 */
	void * cookie;
	/**
	 * \brief api level supported by this driver
	 *
	 * Should be define by using STLOCATE_DATABASE_API_LEVEL only
	 */
	const unsigned int api_level;
	/**
	 * \brief Sha1 sum of plugins source code
	 */
	const char * src_checksum;
};

/**
 * \def STLOCATE_DATABASE_API_LEVEL
 * \brief Current api level
 *
 * Will increment with new version of struct sl_database or struct sl_database_connection
 */
#define STLOCATE_DATABASE_API_LEVEL 1


/**
 * \brief Get a database configuration by his name
 *
 * This configuration should be previous loaded.
 *
 * \param[in] name name of config
 * \returns \b NULL if not found
 */
struct sl_database_config * sl_database_get_config_by_name(const char * name);

/**
 * \brief get the default database driver
 *
 * \return NULL if failed
 */
struct sl_database * sl_database_get_default_driver(void);

/**
 * \brief get a database driver
 *
 * \param[in] database database name
 * \return NULL if failed
 *
 * \note if this driver is not loaded, this function will load it
 * \warning the returned value <b>SHALL NOT BE RELEASE</b> with \a free
 */
struct sl_database * sl_database_get_driver(const char * database);

/**
 * \brief Register a database driver
 *
 * \param[in] database a statically allocated struct sl_database
 *
 * \note Each database driver should call this function only one time
 * \code
 * static void database_myDb_init() __attribute__((constructor)) {
 *    sl_database_register_driver(&database_myDb_module);
 * }
 * \endcode
 */
void sl_database_register_driver(struct sl_database * database);

#endif


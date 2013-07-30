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
*  Last modified: Tue, 30 Jul 2013 21:57:20 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// pthread_mutex_lock, pthread_mutex_unlock, pthread_setcancelstate
#include <pthread.h>
// realloc
#include <stdlib.h>
// strcmp
#include <string.h>

#include <stlocate/hashtable.h>
#include <stlocate/log.h>
#include <stlocate/string.h>

#include "database.h"
#include "loader.h"

static struct sl_hashtable * sl_database_configs = NULL;
static struct sl_database * sl_database_default_driver = NULL;
static struct sl_hashtable * sl_database_drivers = NULL;

static pthread_mutex_t sl_database_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static void sl_database_exit(void) __attribute__((destructor));
static void sl_database_conf_free(void * key, void * value);
static void sl_database_init(void) __attribute__((constructor));


void sl_database_conf(const struct sl_hashtable * params) {
	struct sl_hashtable_value driver = sl_hashtable_get(params, "driver");
	struct sl_hashtable_value storage = sl_hashtable_get(params, "storage");

	if (driver.type != sl_hashtable_value_string || storage.type != sl_hashtable_value_string) {
		if (driver.type != sl_hashtable_value_string)
			sl_log_write(sl_log_level_err, sl_log_type_conf, "Database: a driver value is required");
		if (storage.type != sl_hashtable_value_string)
			sl_log_write(sl_log_level_err, sl_log_type_conf, "Database: a storage value is required");
		return;
	}

	int nsk = 0;
	struct sl_hashtable_value nb_session_kept = sl_hashtable_get(params, "nb_session_kept");
	if (nb_session_kept.type == sl_hashtable_value_null) {
		nsk = 7;
		sl_log_write(sl_log_level_notice, sl_log_type_conf, "Database: no nb_session_kept found, using default value 7");
	} else {
		nsk = sl_hashtable_val_convert_to_signed_integer(&nb_session_kept);
		if (nsk < 1) {
			sl_log_write(sl_log_level_err, sl_log_type_conf, "Database: nb_session_kept should be a positive integer but not %d", nsk);
			return;
		}
	}

	struct sl_database * db = sl_database_get_driver(driver.value.string);
	if (db != NULL) {
		struct sl_database_config * conf = db->ops->add(params);
		conf->nb_session_kept = nsk;

		if (conf != NULL) {
			pthread_mutex_lock(&sl_database_lock);

			if (sl_database_default_driver == NULL)
				sl_database_default_driver = conf->driver;
			sl_hashtable_put(sl_database_configs, conf->name, sl_hashtable_val_custom(conf));

			pthread_mutex_unlock(&sl_database_lock);
		}
	}
}

static void sl_database_exit() {
	/**
	 * sometimes, program crash here
	 */
	// sl_hashtable_free(sl_database_configs);
	// sl_hashtable_free(sl_database_drivers);
}

static void sl_database_conf_free(void * key __attribute__((unused)), void * value) {
	struct sl_database_config * conf = value;
	conf->ops->free(conf);
}

struct sl_database_config * sl_database_get_config_by_name(const char * name) {
	if (name == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_database, "Try to get a database configuration with name is NULL");
		return NULL;
	}

	pthread_mutex_lock(&sl_database_lock);
	struct sl_hashtable_value val = sl_hashtable_get(sl_database_configs, name);
	pthread_mutex_unlock(&sl_database_lock);

	struct sl_database_config * config = NULL;
	if (val.type == sl_hashtable_value_custom)
		config = val.value.custom;

	return config;
}

struct sl_database * sl_database_get_default_driver() {
	pthread_mutex_lock(&sl_database_lock);
	struct sl_database * driver = sl_database_default_driver;
	pthread_mutex_unlock(&sl_database_lock);

	return driver;
}

struct sl_database * sl_database_get_driver(const char * driver) {
	if (driver == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_database, "Try to get a database driver with driver is NULL");
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	struct sl_hashtable_value dr_val = sl_hashtable_get(sl_database_drivers, driver);
	struct sl_database * dr = NULL;
	if (dr_val.type == sl_hashtable_value_null) {
		void * cookie = sl_loader_load("database", driver);

		if (cookie == NULL) {
			sl_log_write(sl_log_level_err, sl_log_type_database, "Failed to load driver %s", driver);
			pthread_mutex_unlock(&lock);
			return NULL;
		}

		dr_val = sl_hashtable_get(sl_database_drivers, driver);
		if (dr_val.type == sl_hashtable_value_null) {
			sl_log_write(sl_log_level_err, sl_log_type_core, "Database: Driver %s not found", driver);
			pthread_mutex_unlock(&lock);
		} else {
			dr = dr_val.value.custom;
			dr->cookie = cookie;

			sl_log_write(sl_log_level_debug, sl_log_type_core, "Driver '%s' is now registred, src checksum: %s", driver, dr->src_checksum);
		}
	}

	pthread_mutex_unlock(&lock);

	return dr;
}

static void sl_database_init() {
	sl_database_configs = sl_hashtable_new2(sl_string_compute_hash, sl_database_conf_free);
	sl_database_drivers = sl_hashtable_new(sl_string_compute_hash);
}

void sl_database_register_driver(struct sl_database * driver) {
	if (driver == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_database, "Try to register with a NULL driver");
		return;
	}

	if (driver->api_level != STLOCATE_DATABASE_API_LEVEL) {
		sl_log_write(sl_log_level_err, sl_log_type_database, "Driver '%s' has not the correct api version", driver->name);
		return;
	}

	if (sl_hashtable_has_key(sl_database_drivers, driver->name)) {
		sl_log_write(sl_log_level_warn, sl_log_type_database, "Driver '%s' is already registred", driver->name);
		return;
	}

	sl_hashtable_put(sl_database_drivers, driver->name, sl_hashtable_val_custom(driver));

	sl_loader_register_ok();

	sl_log_write(sl_log_level_info, sl_log_type_database, "Driver '%s' is now registred", driver->name);
}


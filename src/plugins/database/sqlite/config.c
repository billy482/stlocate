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
*  Last modified: Sun, 14 Jul 2013 00:42:54 +0200                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// sqlite3_open
#include <sqlite3.h>

#include <stlocate/hashtable.h>
#include <stlocate/log.h>

#include "common.h"

struct sl_database_sqlite_config_private {
	char * path;
};

static struct sl_database_connection * sl_database_sqlite_config_connect(struct sl_database_config * config);
static void sl_database_sqlite_config_free(struct sl_database_config * config);
static int sl_database_sqlite_config_ping(struct sl_database_config * config);

static struct sl_database_config_ops sl_database_sqlite_config_ops = {
	.connect = sl_database_sqlite_config_connect,
	.free    = sl_database_sqlite_config_free,
	.ping    = sl_database_sqlite_config_ping,
};


struct sl_database_config * sl_database_sqlite_config_add(struct sl_database * driver, const struct sl_hashtable * params) {
	struct sl_hashtable_value storage = sl_hashtable_get(params, "storage");
	struct sl_hashtable_value path = sl_hashtable_get(params, "path");

	if (storage.type != sl_hashtable_value_string || path.type != sl_hashtable_value_string) {
		if (storage.type != sl_hashtable_value_string)
			sl_log_write(sl_log_level_err, sl_log_type_database, "Sqlite: storage value is required");
		if (path.type != sl_hashtable_value_string)
			sl_log_write(sl_log_level_err, sl_log_type_database, "Sqlite: path value is required");
		return NULL;
	}

	struct sl_database_sqlite_config_private * self = malloc(sizeof(struct sl_database_sqlite_config_private));
	self->path = strdup(path.value.string);

	struct sl_database_config * config = malloc(sizeof(struct sl_database_config));
	config->name = strdup(storage.value.string);
	config->ops = &sl_database_sqlite_config_ops;
	config->data = self;
	config->driver = driver;

	return config;
}

static struct sl_database_connection * sl_database_sqlite_config_connect(struct sl_database_config * config) {
	if (config == NULL)
		return NULL;

	struct sl_database_sqlite_config_private * self = config->data;
	return sl_database_sqlite_connection_add(config, self->path);
}

static void sl_database_sqlite_config_free(struct sl_database_config * config) {
	if (config == NULL)
		return;

	struct sl_database_sqlite_config_private * self = config->data;
	free(self->path);
	free(self);

	free(config->name);
	free(config);
}

static int sl_database_sqlite_config_ping(struct sl_database_config * config) {
	struct sl_database_sqlite_config_private * self = config->data;

	sqlite3 * db_handler = NULL;
	int ret = sqlite3_open(self->path, &db_handler);

	if (ret == SQLITE_OK)
		sqlite3_close(db_handler);

	return ret == SQLITE_OK;
}


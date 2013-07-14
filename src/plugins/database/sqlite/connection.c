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
*  Last modified: Sun, 14 Jul 2013 16:47:38 +0200                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// sqlite3_open
#include <sqlite3.h>

#include <stlocate/log.h>

#include "common.h"

struct sl_database_sqlite_connection_private {
	sqlite3 * db_handler;
};

static int sl_database_sqlite_connection_close(struct sl_database_connection * connect);
static int sl_database_sqlite_connection_free(struct sl_database_connection * connect);
static bool sl_database_sqlite_connection_is_connection_closed(struct sl_database_connection * connect);

static int sl_database_sqlite_connection_cancel_transaction(struct sl_database_connection * connect);
static int sl_database_sqlite_connection_finish_transaction(struct sl_database_connection * connect);
static int sl_database_sqlite_connection_start_transaction(struct sl_database_connection * connect);

static struct sl_database_connection_ops sl_database_sqlite_connection_ops = {
	.close                = sl_database_sqlite_connection_close,
	.free                 = sl_database_sqlite_connection_free,
	.is_connection_closed = sl_database_sqlite_connection_is_connection_closed,

	.cancel_transaction = sl_database_sqlite_connection_cancel_transaction,
	.finish_transaction = sl_database_sqlite_connection_finish_transaction,
	.start_transaction  = sl_database_sqlite_connection_start_transaction,
};


struct sl_database_connection * sl_database_sqlite_connection_add(struct sl_database_config * config, const char * path) {
	sqlite3 * handler = NULL;
	int ret = sqlite3_open(path, &handler);
	if (ret != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to open database at '%s'", path);
		return NULL;
	}

	struct sl_database_sqlite_connection_private * self = malloc(sizeof(struct sl_database_sqlite_connection_private));
	self->db_handler = handler;

	struct sl_database_connection * connection = malloc(sizeof(struct sl_database_connection));
	connection->ops = &sl_database_sqlite_connection_ops;
	connection->data = self;
	connection->driver = config->driver;
	connection->config = config;

	sl_log_write(sl_log_level_info, sl_log_type_plugin_database, "Sqlite: open database at '%s', OK", path);

	return connection;
}

static int sl_database_sqlite_connection_close(struct sl_database_connection * connect) {
	struct sl_database_sqlite_connection_private * self = connect->data;

	int failed = 0;
	if (self->db_handler != NULL) {
		failed = sqlite3_close(self->db_handler);
		self->db_handler = NULL;
	}

	return failed != SQLITE_OK;
}

static int sl_database_sqlite_connection_free(struct sl_database_connection * connect) {
	struct sl_database_sqlite_connection_private * self = connect->data;

	int failed = 0;
	if (self->db_handler != NULL)
		failed = sl_database_sqlite_connection_close(connect);

	if (failed)
		return failed;

	free(self);
	free(connect);

	return 0;
}

static bool sl_database_sqlite_connection_is_connection_closed(struct sl_database_connection * connect) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	return self->db_handler == NULL;
}


static int sl_database_sqlite_connection_cancel_transaction(struct sl_database_connection * connect) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	char * error = NULL;
	int failed = sqlite3_exec(self->db_handler, "ROLLBACK", NULL, NULL, &error);

	if (failed)
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error when cancel transaction because %s", error);
	else
		sl_log_write(sl_log_level_notice, sl_log_type_plugin_database, "Sqlite: cancel transaction ok");

	if (error != NULL)
		sqlite3_free(error);

	return failed != SQLITE_OK;
}

static int sl_database_sqlite_connection_finish_transaction(struct sl_database_connection * connect) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	char * error = NULL;
	int failed = sqlite3_exec(self->db_handler, "COMMIT", NULL, NULL, &error);

	if (failed)
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error when finish transaction because %s", error);
	else
		sl_log_write(sl_log_level_notice, sl_log_type_plugin_database, "Sqlite: finish transaction ok");

	if (error != NULL)
		sqlite3_free(error);

	return failed != SQLITE_OK;
}

static int sl_database_sqlite_connection_start_transaction(struct sl_database_connection * connect) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	char * error = NULL;
	int failed = sqlite3_exec(self->db_handler, "BEGIN TRANSACTION", NULL, NULL, &error);

	if (failed)
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error when start transaction because %s", error);
	else
		sl_log_write(sl_log_level_notice, sl_log_type_plugin_database, "Sqlite: start transaction ok");

	if (error != NULL)
		sqlite3_free(error);

	return failed != SQLITE_OK;
}


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
*  Last modified: Mon, 15 Jul 2013 22:32:16 +0200                         *
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

static int sl_database_sqlite_connection_create_database(struct sl_database_connection * connect, int version);
static int sl_database_sqlite_connection_create_table(sqlite3 * db, const char * table, const char * query);
static int sl_database_sqlite_connection_get_database_version(struct sl_database_connection * connect);

static int sl_database_sqlite_connection_end_session(struct sl_database_connection * connect, int session_id);
static int sl_database_sqlite_connection_get_host_by_name(struct sl_database_connection * connect, const char * hostname);
static int sl_database_sqlite_connection_start_session(struct sl_database_connection * connect);

static struct sl_database_connection_ops sl_database_sqlite_connection_ops = {
	.close                = sl_database_sqlite_connection_close,
	.free                 = sl_database_sqlite_connection_free,
	.is_connection_closed = sl_database_sqlite_connection_is_connection_closed,

	.cancel_transaction = sl_database_sqlite_connection_cancel_transaction,
	.finish_transaction = sl_database_sqlite_connection_finish_transaction,
	.start_transaction  = sl_database_sqlite_connection_start_transaction,

	.create_database      = sl_database_sqlite_connection_create_database,
	.get_database_version = sl_database_sqlite_connection_get_database_version,

	.end_session      = sl_database_sqlite_connection_end_session,
	.get_host_by_name = sl_database_sqlite_connection_get_host_by_name,
	.start_session    = sl_database_sqlite_connection_start_session,
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


static int sl_database_sqlite_connection_create_database(struct sl_database_connection * connect, int version) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	if (version < 1 || version > 1) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: wrong version of database (%d)", version);
		return 1;
	}

	int failed = sl_database_sqlite_connection_create_table(self->db_handler, "host", "CREATE TABLE host (id INTEGER PRIMARY KEY, name TEXT UNIQUE)");
	if (failed)
		return failed;

	failed = sl_database_sqlite_connection_create_table(self->db_handler, "session", "CREATE TABLE session (id INTEGER PRIMARY KEY, start_time INTEGER NOT NULL, end_time INTEGER NULL, CHECK (start_time <= end_time))");
	if (failed)
		return failed;

	return 0;
}

static int sl_database_sqlite_connection_create_table(sqlite3 * db, const char * table, const char * query) {
	char * error = NULL;
	int failed = sqlite3_exec(db, query, NULL, NULL, &error);

	if (failed != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to create table '%s' with query '%s' because %s", table, query, error);
		sqlite3_free(error);
	}

	return failed != SQLITE_OK;
}

static int sl_database_sqlite_connection_get_database_version(struct sl_database_connection * connect) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	sqlite3_stmt * smt;
	static const char * query = "SELECT value FROM config WHERE key = \"version\" LIMIT 1";
	int failed = sqlite3_prepare_v2(self->db_handler, query, -1, &smt, NULL);
	if (failed != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to prepare query '%s', because %s", query, sqlite3_errmsg(self->db_handler));
		return -1;
	}

	failed = sqlite3_step(smt);
	if (failed != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to execute prepared query '%s'", query);
		sqlite3_finalize(smt);
		return -1;
	}

	int version = 0;
	version = sqlite3_column_int(smt, 0);

	sqlite3_finalize(smt);

	return version;
}


static int sl_database_sqlite_connection_end_session(struct sl_database_connection * connect, int session_id) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	sqlite3_stmt * stmt_update;
	static const char * query = "UPDATE session SET end_time = datetime('now') WHERE id = ?1";
	int failed = sqlite3_prepare_v2(self->db_handler, query, -1, &stmt_update, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'update session'");
		return -1;
	}

	sqlite3_bind_int(stmt_update, 1, session_id);
	failed = sqlite3_step(stmt_update);

	return failed != SQLITE_DONE;
}

static int sl_database_sqlite_connection_get_host_by_name(struct sl_database_connection * connect, const char * hostname) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	sqlite3_stmt * stmt_select;
	static const char * query = "SELECT id FROM host WHERE name = ?1 LIMIT 1";
	int failed = sqlite3_prepare_v2(self->db_handler, query, -1, &stmt_select, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'get host by name'");
		return -1;
	}

	sqlite3_bind_text(stmt_select, 1, hostname, -1, SQLITE_STATIC);

	failed = sqlite3_step(stmt_select);
	if (failed == SQLITE_ROW) {
		int host_id = sqlite3_column_int(stmt_select, 0);
		sqlite3_finalize(stmt_select);
		return host_id;
	} else if (failed == SQLITE_DONE) {
		sqlite3_finalize(stmt_select);

		static const char * insert = "INSERT INTO host(name) VALUES (?1)";
		sqlite3_stmt * stmt_insert;
		failed = sqlite3_prepare_v2(self->db_handler, insert, -1, &stmt_insert, NULL);
		if (failed) {
			sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'insert into host'");
			sqlite3_finalize(stmt_select);
			return -2;
		}

		sqlite3_bind_text(stmt_insert, 1, hostname, -1, SQLITE_STATIC);
		failed = sqlite3_step(stmt_insert);

		int host_id = sqlite3_last_insert_rowid(self->db_handler);
		sqlite3_finalize(stmt_insert);

		return host_id;
	} else {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to get an host id");
		return -3;
	}
}

static int sl_database_sqlite_connection_start_session(struct sl_database_connection * connect) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	sqlite3_stmt * stmt_insert;
	static const char * query = "INSERT INTO session(start_time) VALUES (datetime('now'))";
	int failed = sqlite3_prepare_v2(self->db_handler, query, -1, &stmt_insert, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'insert into session'");
		return -1;
	}

	failed = sqlite3_step(stmt_insert);

	int session_id = sqlite3_last_insert_rowid(self->db_handler);
	sqlite3_finalize(stmt_insert);

	return session_id;
}


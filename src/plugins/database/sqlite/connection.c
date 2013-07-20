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
*  Last modified: Sat, 20 Jul 2013 14:01:44 +0200                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// sqlite3_open
#include <sqlite3.h>
// struct stat
#include <sys/stat.h>

#include <stlocate/filesystem.h>
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
static int sl_database_sqlite_connection_exec(sqlite3 * db, const char * query);
static int sl_database_sqlite_connection_get_database_version(struct sl_database_connection * connect);

static int sl_database_sqlite_connection_delete_old_session(struct sl_database_connection * connect, int nb_session_kept);
static int sl_database_sqlite_connection_end_session(struct sl_database_connection * connect, int session_id);
static int sl_database_sqlite_connection_get_host_by_name(struct sl_database_connection * connect, const char * hostname);
static int sl_database_sqlite_connection_start_session(struct sl_database_connection * connect, int host_id);
static int sl_database_sqlite_connection_sync_file(struct sl_database_connection * connect, int s2fs, const char * filename, struct stat * st);
static int sl_database_sqlite_connection_sync_filesystem(struct sl_database_connection * connect, int session_id, struct sl_filesystem * fs);

static struct sl_database_connection_ops sl_database_sqlite_connection_ops = {
	.close                = sl_database_sqlite_connection_close,
	.free                 = sl_database_sqlite_connection_free,
	.is_connection_closed = sl_database_sqlite_connection_is_connection_closed,

	.cancel_transaction = sl_database_sqlite_connection_cancel_transaction,
	.finish_transaction = sl_database_sqlite_connection_finish_transaction,
	.start_transaction  = sl_database_sqlite_connection_start_transaction,

	.create_database      = sl_database_sqlite_connection_create_database,
	.get_database_version = sl_database_sqlite_connection_get_database_version,

	.delete_old_session = sl_database_sqlite_connection_delete_old_session,
	.end_session        = sl_database_sqlite_connection_end_session,
	.get_host_by_name   = sl_database_sqlite_connection_get_host_by_name,
	.start_session      = sl_database_sqlite_connection_start_session,
	.sync_file          = sl_database_sqlite_connection_sync_file,
	.sync_filesystem    = sl_database_sqlite_connection_sync_filesystem,
};


struct sl_database_connection * sl_database_sqlite_connection_add(struct sl_database_config * config, const char * path) {
	sqlite3 * handler = NULL;
	int ret = sqlite3_open(path, &handler);
	if (ret != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to open database at '%s'", path);
		return NULL;
	}

	sl_database_sqlite_connection_exec(handler, "PRAGMA foreign_keys = ON");

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

	int failed = sl_database_sqlite_connection_exec(self->db_handler, "CREATE TABLE host (id INTEGER PRIMARY KEY, name TEXT UNIQUE)");
	if (failed)
		return failed;

	failed = sl_database_sqlite_connection_exec(self->db_handler, "CREATE TABLE session (id INTEGER PRIMARY KEY, start_time INTEGER NOT NULL, end_time INTEGER NULL, host INTEGER NOT NULL REFERENCES host(id) ON UPDATE CASCADE ON DELETE CASCADE, CHECK (start_time <= end_time))");
	if (failed)
		return failed;

	failed = sl_database_sqlite_connection_exec(self->db_handler, "CREATE TABLE filesystem (id INTEGER PRIMARY KEY, uuid TEXT NOT NULL UNIQUE, label TEXT, type TEXT NOT NULL)");
	if (failed)
		return failed;

	failed = sl_database_sqlite_connection_exec(self->db_handler, "CREATE TABLE session2filesystem (id INTEGER PRIMARY KEY, session INTEGER NOT NULL REFERENCES session(id) ON UPDATE CASCADE ON DELETE CASCADE, filesystem INTEGER NOT NULL REFERENCES filesystem(id) ON UPDATE CASCADE ON DELETE CASCADE, mount_point TEXT NOT NULL, dev_no INTEGER NOT NULL CHECK (dev_no >= 0), disk_free INTEGER NOT NULL CHECK (disk_free >= 0), disk_total INTEGER NOT NULL CHECK (disk_total >= 0), block_size INTEGER NOT NULL CHECK (block_size >= 0), CHECK (disk_free <= disk_total))");
	if (failed)
		return failed;

	failed = sl_database_sqlite_connection_exec(self->db_handler, "CREATE TABLE file (s2fs INTEGER NOT NULL REFERENCES session2filesystem(id) ON UPDATE CASCADE ON DELETE CASCADE, inode INTEGER NOT NULL CHECK (inode >= 0), path TEXT NOT NULL, mode INTEGER NOT NULL CHECK (mode >= 0), uid INTEGER NOT NULL CHECK (uid >= 0), gid INTEGER NOT NULL CHECK (gid >= 0), size INTEGER NOT NULL CHECK (size >= 0), access_time INTEGER NOT NULL, modif_time INTEGER NOT NULL)");
	if (failed)
		return failed;

	failed = sl_database_sqlite_connection_exec(self->db_handler, "CREATE INDEX inode ON file(s2fs, inode)");
	if (failed)
		return failed;

	failed = sl_database_sqlite_connection_exec(self->db_handler, "CREATE TABLE config (key TEXT NOT NULL UNIQUE, VALUE TEXT)");
	if (failed)
		return failed;

	sqlite3_stmt * stmt_insert;
	static const char * query = "INSERT INTO config VALUES ('version', '1')";
	failed = sqlite3_prepare_v2(self->db_handler, query, -1, &stmt_insert, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'insert into config'");
		return failed;
	}

	failed = sqlite3_step(stmt_insert);
	sqlite3_finalize(stmt_insert);

	return (failed != SQLITE_DONE);
}

static int sl_database_sqlite_connection_exec(sqlite3 * db, const char * query) {
	char * error = NULL;
	int failed = sqlite3_exec(db, query, NULL, NULL, &error);

	if (failed != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to execute query %s because %s", query, error);
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
	if (failed != SQLITE_ROW) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to execute prepared query '%s'", query);
		sqlite3_finalize(smt);
		return -1;
	}

	int version = 0;
	version = sqlite3_column_int(smt, 0);

	sqlite3_finalize(smt);

	return version;
}


static int sl_database_sqlite_connection_delete_old_session(struct sl_database_connection * connect, int nb_session_kept) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	sqlite3_stmt * stmt_ctt;
	static const char * query_ctt = "CREATE TEMP TABLE remove_session AS SELECT s1.id AS session FROM session s1 LEFT JOIN session s2 ON s1.id < s2.id AND s1.host = s2.host GROUP BY s1.id HAVING COUNT(s2.id) >= ?1";
	int failed = sqlite3_prepare_v2(self->db_handler, query_ctt, -1, &stmt_ctt, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'create table remove_session'");
		return -1;
	}

	sqlite3_bind_int(stmt_ctt, 1, nb_session_kept);
	failed = sqlite3_step(stmt_ctt);
	sqlite3_finalize(stmt_ctt);

	if (failed != SQLITE_DONE) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while creating temporary table remove_session");
		return -1;
	}

	char * error;
	failed = sqlite3_exec(self->db_handler, "DELETE FROM session WHERE id IN (SELECT session FROM remove_session)", NULL, NULL, &error);
	if (failed != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'delete from session' because %s", error);
		sqlite3_free(error);
		return -1;
	}

	int nb_changed = sqlite3_changes(self->db_handler);

	failed = sqlite3_exec(self->db_handler, "DROP TABLE remove_session", NULL, NULL, &error);
	if (failed != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while dropping table remove_session because %s", error);
		sqlite3_free(error);
		return -1;
	}

	if (nb_changed < 1)
		return 0;

	failed = sqlite3_exec(self->db_handler, "VACUUM", NULL, NULL, &error);

	if (failed != SQLITE_OK) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to vaccum, error: %s", error);
		sqlite3_free(error);
	}

	return failed != SQLITE_OK;
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

	sqlite3_finalize(stmt_update);

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

static int sl_database_sqlite_connection_start_session(struct sl_database_connection * connect, int host_id) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	sqlite3_stmt * stmt_insert;
	static const char * query = "INSERT INTO session(start_time, host) VALUES (datetime('now'), ?1)";
	int failed = sqlite3_prepare_v2(self->db_handler, query, -1, &stmt_insert, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'insert into session'");
		return -1;
	}

	sqlite3_bind_int(stmt_insert, 1, host_id);
	failed = sqlite3_step(stmt_insert);

	int session_id = sqlite3_last_insert_rowid(self->db_handler);
	sqlite3_finalize(stmt_insert);

	return session_id;
}

static int sl_database_sqlite_connection_sync_file(struct sl_database_connection * connect, int s2fs, const char * filename, struct stat * st) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	static const char * insert = "INSERT INTO file(s2fs, inode, path, mode, uid, gid, size, access_time, modif_time) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, datetime(?8, 'unixepoch'), datetime(?9, 'unixepoch'))";
	sqlite3_stmt * stmt_insert;
	int failed = sqlite3_prepare_v2(self->db_handler, insert, -1, &stmt_insert, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'insert into file2session'");
		sqlite3_finalize(stmt_insert);
		return -4;
	}

	sqlite3_bind_int64(stmt_insert, 1, s2fs);
	sqlite3_bind_int64(stmt_insert, 2, st->st_ino);
	sqlite3_bind_text(stmt_insert, 3, filename, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt_insert, 4, st->st_mode);
	sqlite3_bind_int(stmt_insert, 5, st->st_uid);
	sqlite3_bind_int(stmt_insert, 6, st->st_gid);
	sqlite3_bind_int64(stmt_insert, 7, st->st_size);
	sqlite3_bind_int64(stmt_insert, 8, st->st_atime);
	sqlite3_bind_int64(stmt_insert, 9, st->st_mtime);

	failed = sqlite3_step(stmt_insert);

	if (failed != SQLITE_DONE)
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to insert new file2session (file id: 0, session id: %d)", s2fs);

	sqlite3_finalize(stmt_insert);

	return failed != SQLITE_DONE;
}

static int sl_database_sqlite_connection_sync_filesystem(struct sl_database_connection * connect, int session_id, struct sl_filesystem * fs) {
	struct sl_database_sqlite_connection_private * self = connect->data;
	if (self->db_handler == NULL)
		return 1;

	sqlite3_stmt * stmt_select;
	static const char * query = "SELECT id FROM filesystem WHERE uuid = ?1 LIMIT 1";
	int failed = sqlite3_prepare_v2(self->db_handler, query, -1, &stmt_select, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'get host by uuid'");
		return -1;
	}

	sqlite3_bind_text(stmt_select, 1, fs->uuid, -1, SQLITE_STATIC);

	failed = sqlite3_step(stmt_select);
	if (failed == SQLITE_ROW) {
		fs->id = sqlite3_column_int(stmt_select, 0);
		sqlite3_finalize(stmt_select);
	} else if (failed == SQLITE_DONE) {
		sqlite3_finalize(stmt_select);

		static const char * insert = "INSERT INTO filesystem(uuid, label, type) VALUES (?1, ?2, ?3)";
		sqlite3_stmt * stmt_insert;
		failed = sqlite3_prepare_v2(self->db_handler, insert, -1, &stmt_insert, NULL);
		if (failed) {
			sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'insert into filesystem'");
			sqlite3_finalize(stmt_select);
			return -2;
		}

		sqlite3_bind_text(stmt_insert, 1, fs->uuid, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_insert, 2, fs->label, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_insert, 3, fs->type, -1, SQLITE_STATIC);
		failed = sqlite3_step(stmt_insert);

		if (failed == SQLITE_DONE)
			fs->id = sqlite3_last_insert_rowid(self->db_handler);
		sqlite3_finalize(stmt_insert);
	} else {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: failed to get an host id");
		return -3;
	}

	static const char * insert = "INSERT INTO session2filesystem(session, filesystem, mount_point, dev_no, disk_free, disk_total, block_size) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)";
	sqlite3_stmt * stmt_insert;
	failed = sqlite3_prepare_v2(self->db_handler, insert, -1, &stmt_insert, NULL);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_database, "Sqlite: error while preparing query 'insert into session2filesystem'");
		sqlite3_finalize(stmt_select);
		return -4;
	}

	sqlite3_bind_int(stmt_insert, 1, session_id);
	sqlite3_bind_int(stmt_insert, 2, fs->id);
	sqlite3_bind_text(stmt_insert, 3, fs->mount_point, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt_insert, 4, fs->device);
	sqlite3_bind_int64(stmt_insert, 5, fs->disk_free);
	sqlite3_bind_int64(stmt_insert, 6, fs->disk_total);
	sqlite3_bind_int(stmt_insert, 7, fs->block_size);
	failed = sqlite3_step(stmt_insert);

	if (failed != SQLITE_DONE) {
		sqlite3_finalize(stmt_insert);
		return -5;
	}

	int s2f_id = sqlite3_last_insert_rowid(self->db_handler);
	sqlite3_finalize(stmt_insert);
	return s2f_id;
}


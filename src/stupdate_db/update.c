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
*  Last modified: Tue, 16 Jul 2013 22:32:14 +0200                         *
\*************************************************************************/

#include <stlocate/database.h>
#include <stlocate/filesystem.h>
#include <stlocate/log.h>

// blkid_*
#include <blkid/blkid.h>
// free
#include <stdlib.h>
// strcmp
#include <string.h>
// stat
#include <sys/stat.h>
// stat
#include <sys/types.h>
// uname
#include <sys/utsname.h>
// stat
#include <unistd.h>

#include "common.h"

static blkid_cache cache;

static int sl_db_update_filesystem(struct sl_database_connection * db, int host_id, int session_id, const char * path);
static void sl_db_update_init(void) __attribute__((constructor));


int sl_db_update(struct sl_database_connection * db, int version __attribute__((unused))) {
	struct utsname name;
	uname(&name);

	int host_id = db->ops->get_host_by_name(db, name.nodename);

	int failed = db->ops->start_transaction(db);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to start new transaction");
		return failed;
	}

	int session_id = db->ops->start_session(db);
	if (session_id < 0) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to create new session");
		return failed;
	}

	failed = sl_db_update_filesystem(db, host_id, session_id, "/");
	if (failed) {
		db->ops->cancel_transaction(db);
		return failed;
	}

	failed = db->ops->end_session(db, session_id);
	if (failed) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to finish current session");
		return failed;
	}

	failed = db->ops->finish_transaction(db);
	if (failed) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to commit current transaction");
		return failed;
	}

	return 0;
}

static int sl_db_update_filesystem(struct sl_database_connection * db, int host_id, int session_id, const char * path) {
	struct stat st;
	int failed = stat(path, &st);
	if (failed)
		return failed;

	char * device = blkid_devno_to_devname(st.st_dev);

	blkid_dev dev = blkid_get_dev(cache, device, 0);

	const char * uuid = NULL;
	const char * label = NULL;
	const char * type = NULL;

	blkid_tag_iterate iter = blkid_tag_iterate_begin(dev);
	const char * key, * value;
	while (!blkid_tag_next(iter, &key, &value)) {
		if (!strcmp("UUID", key))
			uuid = value;
		else if (!strcmp("LABEL", key))
			label = value;
		else if (!strcmp("TYPE", key))
			type = value;
	}

	struct sl_filesystem * fs = sl_filesystem_new(uuid, label, type);

	blkid_tag_iterate_end(iter);
	free(device);

	failed = db->ops->sync_filesystem(db, fs);
	if (failed)
		return failed;

	return 0;
}

static void sl_db_update_init() {
	blkid_get_cache(&cache, NULL);
}


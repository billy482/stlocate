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
*  Last modified: Mon, 29 Jul 2013 23:15:18 +0200                         *
\*************************************************************************/

// asprintf, versionsort
#define _GNU_SOURCE
// blkid_*
#include <blkid/blkid.h>
// versionsort, scandir
#include <dirent.h>
// free
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// lstat
#include <sys/stat.h>
// statfs
#include <sys/statfs.h>
// lstat
#include <sys/types.h>
// time
#include <time.h>
// lstat
#include <unistd.h>

#include <stlocate/database.h>
#include <stlocate/filesystem.h>
#include <stlocate/log.h>

#include "common.h"

static blkid_cache cache;

static int sl_db_update_file(struct sl_database_connection * db, int host_id, int session_id, int s2fs, const char * root, const char * path, struct stat * st);
static int sl_db_update_file_filter(const struct dirent * file);
static int sl_db_update_filesystem(struct sl_database_connection * db, int host_id, int session_id, const char * path);
static void sl_db_update_init(void) __attribute__((constructor));


int sl_db_update(struct sl_database_connection * db, int host_id, int version __attribute__((unused))) {
	int failed = db->ops->start_transaction(db);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to start new transaction");
		return failed;
	}
	sl_log_write(sl_log_level_info, sl_log_type_core, "Start new transaction: OK");

	int session_id = db->ops->start_session(db, host_id);
	if (session_id < 0) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to create new session");
		return failed;
	}
	sl_log_write(sl_log_level_info, sl_log_type_core, "Create new session, id: %d", session_id);

	sl_log_write(sl_log_level_info, sl_log_type_core, "Start update db");
	failed = sl_db_update_filesystem(db, host_id, session_id, "/");
	if (failed) {
		db->ops->cancel_transaction(db);
		return failed;
	}
	sl_log_write(sl_log_level_info, sl_log_type_core, "Start update db, finished with status %d", failed);

	failed = db->ops->end_session(db, session_id);
	if (failed) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to finish current session");
		return failed;
	}
	sl_log_write(sl_log_level_info, sl_log_type_core, "Finish session: OK");

	failed = db->ops->finish_transaction(db);
	if (failed) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to commit current transaction");
		return failed;
	}
	sl_log_write(sl_log_level_info, sl_log_type_core, "Commit current transaction: OK");

	return 0;
}

static int sl_db_update_file(struct sl_database_connection * db, int host_id, int session_id, int s2fs, const char * root, const char * path, struct stat * sfile) {
	static time_t last = 0;
	time_t now = time(NULL);

	char * file;
	if (path != NULL)
		asprintf(&file, "%s/%s", !strcmp("/", root) ? "" : root, path);
	else
		file = strdup(root);

	if (now > last) {
		sl_log_write(sl_log_level_notice, sl_log_type_core, "Current file: %s", file);
		last = now;
	}

	int failed = db->ops->sync_file(db, s2fs, path != NULL ? path : "/", sfile);
	if (failed) {
		free(file);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to synchronize file with database, { root: %s, path: %s }", root, path);
		return failed;
	}

	if (S_ISDIR(sfile->st_mode)) {
		struct dirent ** nl = NULL;
		int i, nb_files = scandir(file, &nl, sl_db_update_file_filter, versionsort);
		for (i = 0; i < nb_files; i++) {
			if (!failed) {
				char * subfile = NULL;
				if (path != NULL)
					asprintf(&subfile, "%s/%s/%s", !strcmp("/", root) ? "" : root, path, nl[i]->d_name);
				else
					asprintf(&subfile, "%s/%s", !strcmp("/", root) ? "" : root, nl[i]->d_name);

				struct stat ssubfile;
				failed = lstat(subfile, &ssubfile);

				if (!failed) {
					if (sfile->st_dev != ssubfile.st_dev)
						failed = sl_db_update_filesystem(db, host_id, session_id, subfile);
					else {
						free(subfile);
						if (path != NULL)
							asprintf(&subfile, "%s/%s", path, nl[i]->d_name);
						else
							subfile = strdup(nl[i]->d_name);
						failed = sl_db_update_file(db, host_id, session_id, s2fs, root, subfile, &ssubfile);
					}
				}

				free(subfile);
			}

			free(nl[i]);
		}
		free(nl);
	}

	free(file);

	return failed;
}

static int sl_db_update_file_filter(const struct dirent * file) {
	if (file->d_name[0] != '.')
		return 1;

	if (file->d_name[1] == '\0')
		return 0;

	return file->d_name[1] != '.' || file->d_name[2] != '\0';
}

static int sl_db_update_filesystem(struct sl_database_connection * db, int host_id, int session_id, const char * path) {
	struct stat st;
	int failed = stat(path, &st);
	if (failed)
		return failed;

	struct statfs stfs;
	failed = statfs(path, &stfs);
	if (failed)
		return failed;

	char * device = blkid_devno_to_devname(st.st_dev);
	if (device == NULL) {
		sl_log_write(sl_log_level_notice, sl_log_type_core, "Skip path: { path: %s } because it is not a block device", path);
		return 0;
	}

	sl_log_write(sl_log_level_notice, sl_log_type_core, "Update filesystem: { path: %s }", path);

	// dev can be null
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

	struct sl_filesystem * fs = sl_filesystem_new(uuid, label, type, st.st_dev, path, stfs.f_bfree, stfs.f_blocks, stfs.f_bsize);

	blkid_tag_iterate_end(iter);
	free(device);

	int s2fs = db->ops->sync_filesystem(db, session_id, fs);
	if (s2fs < 0) {
		sl_filesystem_free(fs);
		return s2fs;
	}

	failed = sl_db_update_file(db, host_id, session_id, s2fs, path, NULL, &st);

	sl_filesystem_free(fs);

	return failed;
}

static void sl_db_update_init() {
	blkid_get_cache(&cache, NULL);
}


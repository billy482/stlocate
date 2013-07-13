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
*  Last modified: Sat, 13 Jul 2013 12:27:31 +0200                         *
\*************************************************************************/

// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// sscanf
#include <stdio.h>
// strchr
#include <string.h>
// fstat, open
#include <sys/stat.h>
// fstat, open
#include <sys/types.h>
// access, close, fstat, read
#include <unistd.h>

#include <stlocate/conf.h>
#include <stlocate/hashtable.h>
#include <stlocate/string.h>
#include <stlocate/util.h>

#include "database.h"
#include "log.h"

static void sl_conf_exit(void) __attribute__((destructor));
static void sl_conf_free_key(void * key, void * value);
static void sl_conf_init(void) __attribute__((constructor));

static struct sl_hashtable * sl_conf_callback = NULL;


static void sl_conf_exit() {
	sl_hashtable_free(sl_conf_callback);
	sl_conf_callback = NULL;
}

static void sl_conf_free_key(void * key, void * value __attribute__((unused))) {
	free(key);
}

static void sl_conf_init(void) {
	sl_conf_callback = sl_hashtable_new2(sl_string_compute_hash, sl_conf_free_key);
	sl_hashtable_put(sl_conf_callback, "database", sl_hashtable_val_custom(sl_database_conf));
	sl_hashtable_put(sl_conf_callback, "log", sl_hashtable_val_custom(sl_log_conf));
}

int sl_conf_read_config(const char * conf_file) {
	if (conf_file == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_conf, "Conf: read_config: conf_file is NULL");
		return -1;
	}

	if (access(conf_file, R_OK)) {
		sl_log_write(sl_log_level_err, sl_log_type_conf, "Conf: read_config: Can't access to '%s'", conf_file);
		return -1;
	}

	int fd = open(conf_file, O_RDONLY);

	sl_log_write(sl_log_level_debug, sl_log_type_conf, "Opening config file '%s' = %d", conf_file, fd);

	struct stat st;
	fstat(fd, &st);

	char * buffer = malloc(st.st_size + 1);
	ssize_t nb_read = read(fd, buffer, st.st_size);
	close(fd);
	buffer[nb_read] = '\0';

	sl_log_write(sl_log_level_debug, sl_log_type_conf, "Reading %zd bytes from config file '%s'", nb_read, conf_file);

	if (nb_read < 0) {
		sl_log_write(sl_log_level_err, sl_log_type_conf, "Conf: read_config: error while reading from '%s'", conf_file);
		free(buffer);
		return 1;
	}

	char * ptr = buffer;
	char section[24] = { '\0' };
	struct sl_hashtable * params = sl_hashtable_new2(sl_string_compute_hash, sl_util_basic_free);

	while (ptr != NULL && *ptr) {
		switch (*ptr) {
			case ';':
				ptr = strchr(ptr, '\n');
				continue;

			case '\n':
				ptr++;
				if (*ptr == '\n') {
					if (*section) {
						sl_conf_callback_f f = sl_hashtable_get(sl_conf_callback, section).value.custom;
						if (f != NULL)
							f(params);
					}

					*section = 0;
					sl_hashtable_clear(params);
				}
				continue;

			case '[':
				sscanf(ptr, "[%23[^]]]", section);

				ptr = strchr(ptr, '\n');
				continue;

			default:
				if (!*section)
					continue;

				if (strchr(ptr, '=') < strchr(ptr, '\n')) {
					char key[32];
					char value[64];
					int val = sscanf(ptr, "%s = %s", key, value);
					if (val == 2)
						sl_hashtable_put(params, strdup(key), sl_hashtable_val_string(strdup(value)));
				}
				ptr = strchr(ptr, '\n');
		}
	}

	if (params->nb_elements > 0 && *section) {
		sl_conf_callback_f f = sl_hashtable_get(sl_conf_callback, section).value.custom;
		if (f != NULL) {
			sl_log_write(sl_log_level_info, sl_log_type_conf, "Section '%s' is managed by %p", section, f);
			f(params);
		} else
			sl_log_write(sl_log_level_warn, sl_log_type_conf, "Section '%s' has no handler, section ignored", section);
	}

	sl_hashtable_free(params);
	free(buffer);

	sl_log_start_logger();

	return 0;
}

void sl_conf_register_callback(const char * section, sl_conf_callback_f callback) {
	if (section == NULL || callback == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_conf, "Register callback function: error because");

		if (section == NULL)
			sl_log_write(sl_log_level_err, sl_log_type_conf, "section is NULL");

		if (callback == NULL)
			sl_log_write(sl_log_level_err, sl_log_type_conf, "callback function is NULL");

		return;
	}

	sl_log_write(sl_log_level_info, sl_log_type_conf, "Registring config handler { section: '%s', function: %p }", section, callback);
	sl_hashtable_put(sl_conf_callback, strdup(section), sl_hashtable_val_custom(callback));
}


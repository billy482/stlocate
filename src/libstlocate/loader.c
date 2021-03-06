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
*  Last modified: Sun, 07 Jul 2013 19:17:53 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// bool
#include <stdbool.h>
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>
// dlclose, dlerror, dlopen
#include <dlfcn.h>
// glob
#include <glob.h>
// asprintf
#include <stdio.h>
// free
#include <stdlib.h>
// access
#include <unistd.h>

#include <stlocate/log.h>

#include "config.h"
#include "loader.h"

static void * sl_loader_load_file(const char * filename);

static bool sl_loader_loaded = false;


void * sl_loader_load(const char * module, const char * name) {
	if (module == NULL || name == NULL)
		return NULL;

	char * path;
	asprintf(&path, MODULE_PATH "/lib%s-%s.so", module, name);

	void * cookie = sl_loader_load_file(path);

	free(path);

	return cookie;
}

static void * sl_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		sl_log_write(sl_log_level_debug, sl_log_type_core, "Loader: access to file %s failed because %m", filename);
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	sl_loader_loaded = false;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (cookie == NULL) {
		sl_log_write(sl_log_level_debug, sl_log_type_core, "Loader: failed to load '%s' because %s", filename, dlerror());
	} else if (!sl_loader_loaded) {
		dlclose(cookie);
		cookie = NULL;
	}

	pthread_mutex_unlock(&lock);
	return cookie;
}

void sl_loader_register_ok(void) {
	sl_loader_loaded = true;
}


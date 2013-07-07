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
*  Last modified: Sun, 07 Jul 2013 11:24:56 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// bool
#include <stdbool.h>
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

//#include <libstone/log.h>

#include "config.h"
#include "loader.h"

static void * sl_loader_load_file(const char * filename);

static bool sl_loader_loaded = false;


void * sl_loader_load(const char * module, const char * name) {
	if (module == NULL || name == NULL)
		return NULL;

	char * path;
	asprintf(&path, MODULE_PATH "/lib%s-%s.so", module, name);

	void * cookie = st_loader_load_file(path);

	free(path);

	return cookie;
}

static void * sl_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		// st_log_write_all(st_log_level_debug, st_log_type_daemon, "Loader: access to file %s failed because %m", filename);
		return NULL;
	}

	st_loader_loaded = false;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (cookie == NULL) {
		// st_log_write_all(st_log_level_debug, st_log_type_daemon, "Loader: failed to load '%s' because %s", filename, dlerror());
	} else if (!st_loader_loaded) {
		dlclose(cookie);
		cookie = NULL;
	}

	return cookie;
}

void sl_loader_register_ok(void) {
	sl_loader_loaded = true;
}


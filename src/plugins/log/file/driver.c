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
*  Last modified: Tue, 09 Jul 2013 23:03:30 +0200                         *
\*************************************************************************/

// realloc
#include <stdlib.h>

#include <stlocate/hashtable.h>

#include <liblog-file.chcksum>

#include "common.h"

static int sl_log_file_add(enum sl_log_level level, const struct sl_hashtable * params);
static void sl_log_file_init(void) __attribute__((constructor));

static struct sl_log_driver sl_log_file_driver = {
	.name = "file",

	.add = sl_log_file_add,

	.cookie = NULL,
	.api_level = STLOCATE_LOG_API_LEVEL,
	.src_checksum = STLOCATE_LOG_FILE_SRCSUM,
};


static int sl_log_file_add(enum sl_log_level level, const struct sl_hashtable * params) {
	if (params == NULL)
		return 1;

	char * path = sl_hashtable_get(params, "path").value.string;
	if (path == NULL)
		return 2;

	struct sl_log_handler * handler = sl_log_file_new(level, path);
	if (handler != NULL)
		sl_log_add_handler(handler);

	return handler != NULL ? 0 : 3;
}

static void sl_log_file_init(void) {
	sl_log_register_driver(&sl_log_file_driver);
}


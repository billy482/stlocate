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
*  Last modified: Sat, 13 Jul 2013 23:58:08 +0200                         *
\*************************************************************************/

// NULL
#include <stddef.h>

#include <stlocate/hashtable.h>

#include "common.h"

#include <libdatabase-sqlite.chcksum>

static struct sl_database_config * sl_database_sqlite_add(const struct sl_hashtable * params);
static struct sl_database_config * sl_database_sqlite_get_default_config(void);
static void sl_database_sqlite_init(void) __attribute__((constructor));

static struct sl_database_ops sl_database_sqlite_ops = {
	.add                = sl_database_sqlite_add,
	.get_default_config = sl_database_sqlite_get_default_config,
};

static struct sl_database sl_database_sqlite = {
	.name         = "sqlite",
	.ops          = &sl_database_sqlite_ops,
	.cookie       = NULL,
	.api_level    = STLOCATE_DATABASE_API_LEVEL,
	.src_checksum = STLOCATE_DATABASE_SQLITE_SRCSUM,
};


static struct sl_database_config * sl_database_sqlite_add(const struct sl_hashtable * params) {
	return sl_database_sqlite_config_add(&sl_database_sqlite, params);
}

static struct sl_database_config * sl_database_sqlite_get_default_config() {
	return NULL;
}

static void sl_database_sqlite_init(void) {
	sl_database_register_driver(&sl_database_sqlite);
}


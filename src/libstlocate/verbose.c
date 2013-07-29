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
*  Last modified: Mon, 29 Jul 2013 22:35:56 +0200                         *
\*************************************************************************/

// bool
#include <stdbool.h>
// printf
#include <stdio.h>
// gettimeofday
#include <sys/time.h>
// localtime_r, strftime
#include <time.h>

#include <stlocate/log.h>

#include "log.h"

static void sl_log_verbose_free(struct sl_log_handler * handler);
static void sl_log_verbose_write(struct sl_log_handler * handler, const struct sl_log_message * message);

static struct sl_log_handler_ops sl_log_verbose_ops = {
	.free  = sl_log_verbose_free,
	.write = sl_log_verbose_write,
};

static struct sl_log_handler sl_verbose_log = {
	.level = sl_log_level_err,
	.ops   = &sl_log_verbose_ops,
	.data  = NULL,
};


void sl_log_set_verbose(short verbose) {
	switch (verbose) {
		case 0:
			break;

		case 1:
			sl_verbose_log.level = sl_log_level_warn;
			break;

		case 2:
			sl_verbose_log.level = sl_log_level_info;
			break;

		default:
			sl_verbose_log.level = sl_log_level_debug;
			break;
	}

	static bool handler_added = false;
	if (!handler_added) {
		sl_log_add_handler(&sl_verbose_log);
		handler_added = true;
	}
}

static void sl_log_verbose_free(struct sl_log_handler * handler __attribute__((unused))) {}

static void sl_log_verbose_write(struct sl_log_handler * handler __attribute__((unused)), const struct sl_log_message * message) {
	struct tm curTime2;
	char strtime[32];

	localtime_r(&message->timestamp, &curTime2);
	strftime(strtime, 32, "%F %T", &curTime2);

	printf("[L:%-*s | T:%-*s | @%s]: %s\n", 9, sl_log_level_to_string(message->level), 15, sl_log_type_to_string(message->type), strtime, message->message);
}


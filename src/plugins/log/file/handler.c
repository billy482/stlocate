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
*  Last modified: Tue, 09 Jul 2013 23:33:19 +0200                         *
\*************************************************************************/

// open
#include <fcntl.h>
// free, malloc
#include <malloc.h>
// dprintf
#include <stdio.h>
// strdup
#include <string.h>
// fstat, open, stat
#include <sys/stat.h>
// gettimeofday
#include <sys/time.h>
// fstat, open, stat
#include <sys/types.h>
// localtime_r, strftime
#include <time.h>
// close, fstat, sleep, stat
#include <unistd.h>

#include "common.h"

struct sl_log_file_private {
	char * path;
	int fd;

	struct stat current;
};

static void sl_log_file_check_logrotate(struct sl_log_file_private * self);
static void sl_log_file_handler_free(struct sl_log_handler * handler);
static void sl_log_file_handler_write(struct sl_log_handler * handler, const struct sl_log_message * message);

static struct sl_log_handler_ops sl_log_file_handler_ops = {
	.free  = sl_log_file_handler_free,
	.write = sl_log_file_handler_write,
};


static void sl_log_file_check_logrotate(struct sl_log_file_private * self) {
	if (self-> fd < 0) {
		self->fd = open(self->path, O_WRONLY | O_APPEND | O_CREAT, 0640);
		if (self->fd < 0)
			return;
	}

	struct stat reference;
	int failed = stat(self->path, &reference);

	if (failed || self->current.st_ino != reference.st_ino) {
		sl_log_write(sl_log_level_info, sl_log_type_plugin_log, "Log: file: logrotate detected");

		close(self->fd);

		self->fd = open(self->path, O_WRONLY | O_APPEND | O_CREAT, 0640);
		if (self->fd < 0)
			sl_log_write(sl_log_level_err, sl_log_type_plugin_log, "Log: file: error while re-opening file '%s' becase %m", self->path);
		else
			fstat(self->fd, &self->current);
	}
}

static void sl_log_file_handler_free(struct sl_log_handler * handler) {
	struct sl_log_file_private * self = handler->data;

	handler->ops = NULL;
	handler->data = NULL;

	free(self->path);
	self->path = NULL;
	if (self->fd >= 0)
		close(self->fd);
	self->fd = -1;

	free(self);
	free(handler);
}

struct sl_log_handler * sl_log_file_new(enum sl_log_level level, const char * path) {
	if (path == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_log, "Log: file: path is require for log file");
		return NULL;
	}

	int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0640);
	if (fd < 0) {
		sl_log_write(sl_log_level_err, sl_log_type_plugin_log, "Log: file: error while opening file '%s' becase %m", path);
		return NULL;
	}

	struct sl_log_file_private * self = malloc(sizeof(struct sl_log_file_private));
	self->path = strdup(path);
	self->fd = fd;

	fstat(fd, &self->current);

	struct sl_log_handler * h = malloc(sizeof(struct sl_log_handler));
	h->level = level;
	h->ops = &sl_log_file_handler_ops;
	h->data = self;

	return h;
}

static void sl_log_file_handler_write(struct sl_log_handler * handler, const struct sl_log_message * message) {
	struct sl_log_file_private * self = handler->data;

	sl_log_file_check_logrotate(self);
	if (self->fd < 0)
		return;

	struct tm curTime2;
	char strtime[32];

	localtime_r(&message->timestamp, &curTime2);
	strftime(strtime, 32, "%F %T", &curTime2);

	dprintf(self->fd, "[L:%-*s | T:%-15s | @%s]: %s\n", 10, sl_log_level_to_string(message->level), sl_log_type_to_string(message->type), strtime, message->message);
}


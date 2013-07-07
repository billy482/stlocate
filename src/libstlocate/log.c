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
*  Last modified: Sun, 07 Jul 2013 15:52:44 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// bool
#include <stdbool.h>
// pthread_mutex_lock, pthread_mutex_unlock, pthread_setcancelstate
#include <pthread.h>
// va_end, va_start
#include <stdarg.h>
// realloc
#include <stdlib.h>
// printf, snprintf
#include <stdio.h>
// strcasecmp, strcmp
#include <string.h>
// sleep
#include <unistd.h>

#include <stlocate/thread_pool.h>

#include "loader.h"
#include "log.h"

static void sl_log_exit(void) __attribute__((destructor));
static void sl_log_sent_message(void * arg);

static bool sl_log_display_at_exit = true;
static volatile bool sl_log_logger_running = false;
static bool sl_log_finished = false;

static struct sl_log_driver ** sl_log_drivers = NULL;
static unsigned int sl_log_nb_drivers = 0;

struct sl_log_message_unsent {
	struct sl_log_message data;
	struct sl_log_message_unsent * next;
};
static struct sl_log_message_unsent * volatile sl_log_message_first = NULL;
static struct sl_log_message_unsent * volatile sl_log_message_last = NULL;

static pthread_mutex_t sl_log_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_cond_t sl_log_wait = PTHREAD_COND_INITIALIZER;

static struct sl_log_level2 {
	enum sl_log_level level;
	const char * name;
} sl_log_levels[] = {
	{ sl_log_level_alert,   "Alert" },
	{ sl_log_level_crit,    "Critical" },
	{ sl_log_level_debug,   "Debug" },
	{ sl_log_level_emerg,   "Emergency" },
	{ sl_log_level_err,     "Error" },
	{ sl_log_level_info,    "Info" },
	{ sl_log_level_notice,  "Notice" },
	{ sl_log_level_warn,    "Warning" },

	{ sl_log_level_unknown, "Unknown level" },
};

static struct sl_log_type2 {
	enum sl_log_type type;
	const char * name;
} sl_log_types[] = {
	{ sl_log_type_core,     "Core" },
	{ sl_log_type_database, "Database" },

	{ sl_log_type_unknown, "Unknown type" },
};


void sl_log_disable_display_log(void) {
	sl_log_display_at_exit = 0;
}

static void sl_log_exit(void) {
	if (sl_log_display_at_exit) {
		struct sl_log_message_unsent * mes;
		for (mes = sl_log_message_first; mes != NULL; mes = mes->next)
			printf("%c: %s\n", sl_log_level_to_string(mes->data.level)[0], mes->data.message);
	}

	unsigned int i;
	for (i = 0; i < sl_log_nb_drivers; i++) {
		struct sl_log_driver * driver = sl_log_drivers[i];
		unsigned int j;
		for (j = 0; j < driver->nb_modules; j++) {
			struct sl_log_module * mod = driver->modules + j;
			mod->ops->free(mod);
		}
		free(driver->modules);
		driver->modules = NULL;
		driver->nb_modules = 0;
	}

	free(sl_log_drivers);
	sl_log_drivers = NULL;
	sl_log_nb_drivers = 0;

	struct sl_log_message_unsent * message = sl_log_message_first;
	sl_log_message_first = sl_log_message_last = NULL;

	while (message != NULL) {
		struct sl_log_message * mes = &message->data;
		free(mes->message);

		struct sl_log_message_unsent * next = message->next;
		free(message);

		message = next;
	}

	sl_log_finished = true;
}

struct sl_log_driver * sl_log_get_driver(const char * driver) {
	if (driver == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_core, "Get checksum driver with driver's name is NULL");
		return NULL;
	}

	pthread_mutex_lock(&sl_log_lock);

	unsigned int i;
	struct sl_log_driver * dr = NULL;
	for (i = 0; i < sl_log_nb_drivers && dr == NULL; i++)
		if (!strcmp(driver, sl_log_drivers[i]->name))
			dr = sl_log_drivers[i];

	if (dr == NULL) {
		void * cookie = sl_loader_load("log", driver);

		if (dr == NULL && cookie == NULL) {
			sl_log_write(sl_log_level_err, sl_log_type_core, "Log: Failed to load driver %s", driver);
			pthread_mutex_unlock(&sl_log_lock);
			return NULL;
		}

		for (i = 0; i < sl_log_nb_drivers && dr == NULL; i++)
			if (!strcmp(driver, sl_log_drivers[i]->name)) {
				dr = sl_log_drivers[i];
				dr->cookie = cookie;

				sl_log_write(sl_log_level_debug, sl_log_type_core, "Driver '%s' is now registred, src checksum: %s", driver, dr->src_checksum);
			}

		if (dr == NULL)
			sl_log_write(sl_log_level_err, sl_log_type_core, "Log: Driver %s not found", driver);
	}

	pthread_mutex_unlock(&sl_log_lock);

	return dr;
}

const char * sl_log_level_to_string(enum sl_log_level level) {
	unsigned int i;
	for (i = 0; sl_log_levels[i].level != sl_log_level_unknown; i++)
		if (sl_log_levels[i].level == level)
			return sl_log_levels[i].name;

	return sl_log_levels[i].name;
}

void sl_log_register_driver(struct sl_log_driver * driver) {
	if (driver == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_core, "Log: Try to register with driver=0");
		return;
	}

	if (driver->api_level != STLOCATE_LOG_API_LEVEL) {
		sl_log_write(sl_log_level_err, sl_log_type_core, "Log: Driver '%s' has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_level, STLOCATE_LOG_API_LEVEL);
		return;
	}

	unsigned int i;
	for (i = 0; i < sl_log_nb_drivers; i++)
		if (sl_log_drivers[i] == driver || !strcmp(driver->name, sl_log_drivers[i]->name)) {
			sl_log_write(sl_log_level_info, sl_log_type_core, "Log: Driver '%s' is already registred", driver->name);
			return;
		}

	void * new_addr = realloc(sl_log_drivers, (sl_log_nb_drivers + 1) * sizeof(struct sl_log_driver *));
	if (new_addr == NULL) {
		sl_log_write(sl_log_level_info, sl_log_type_core, "Driver '%s' cannot be registred because there is not enough memory", driver->name);
		return;
	}

	sl_log_drivers = new_addr;
	sl_log_drivers[sl_log_nb_drivers] = driver;
	sl_log_nb_drivers++;

	sl_loader_register_ok();

	sl_log_write(sl_log_level_info, sl_log_type_core, "Log: Driver '%s' is now registred", driver->name);
}

static void sl_log_sent_message(void * arg __attribute__((unused))) {
	for (;;) {
		pthread_mutex_lock(&sl_log_lock);
		if (!sl_log_logger_running && sl_log_message_first == NULL)
			break;
		if (sl_log_message_first == NULL)
			pthread_cond_wait(&sl_log_wait, &sl_log_lock);

		struct sl_log_message_unsent * message = sl_log_message_first;
		sl_log_message_first = sl_log_message_last = NULL;
		pthread_mutex_unlock(&sl_log_lock);

		while (message != NULL) {
			unsigned int i;
			struct sl_log_message * mes = &message->data;
			for (i = 0; i < sl_log_nb_drivers; i++) {
				unsigned int j;
				for (j = 0; j < sl_log_drivers[i]->nb_modules; j++)
					if (sl_log_drivers[i]->modules[j].level >= mes->level)
						sl_log_drivers[i]->modules[j].ops->write(sl_log_drivers[i]->modules + j, mes);
			}

			struct sl_log_message_unsent * next = message->next;
			free(mes->message);
			free(message);

			message = next;
		}
	}

	pthread_cond_signal(&sl_log_wait);
	pthread_mutex_unlock(&sl_log_lock);
}

void sl_log_start_logger(void) {
	pthread_mutex_lock(&sl_log_lock);

	if (sl_log_nb_drivers == 0) {
		sl_log_write(sl_log_level_err, sl_log_type_core, "Start logger without log modules loaded");
	} else if (!sl_log_logger_running) {
		sl_log_logger_running = 1;
		sl_thread_pool_run2(sl_log_sent_message, NULL, 4);
		sl_log_display_at_exit = 0;
	}

	pthread_mutex_unlock(&sl_log_lock);
}

void sl_log_stop_logger(void) {
	pthread_mutex_lock(&sl_log_lock);

	if (sl_log_logger_running) {
		sl_log_logger_running = false;
		pthread_cond_signal(&sl_log_wait);
		pthread_cond_wait(&sl_log_wait, &sl_log_lock);
	}

	pthread_mutex_unlock(&sl_log_lock);
}

enum sl_log_level sl_log_string_to_level(const char * level) {
	if (level == NULL)
		return sl_log_level_unknown;

	unsigned int i;
	for (i = 0; sl_log_levels[i].level != sl_log_level_unknown; i++)
		if (!strcasecmp(sl_log_levels[i].name, level))
			return sl_log_levels[i].level;

	return sl_log_levels[i].level;
}

enum sl_log_type sl_log_string_to_type(const char * type) {
	if (type == NULL)
		return sl_log_type_unknown;

	unsigned int i;
	for (i = 0; sl_log_types[i].type != sl_log_type_unknown; i++)
		if (!strcasecmp(type, sl_log_types[i].name))
			return sl_log_types[i].type;

	return sl_log_types[i].type;
}

const char * sl_log_type_to_string(enum sl_log_type type) {
	unsigned int i;
	for (i = 0; sl_log_types[i].type != sl_log_type_unknown; i++)
		if (sl_log_types[i].type == type)
			return sl_log_types[i].name;

	return sl_log_types[i].name;
}

void sl_log_write(enum sl_log_level level, enum sl_log_type type, const char * format, ...) {
	if (sl_log_finished)
		return;

	char * message = NULL;

	va_list va;
	va_start(va, format);
	vasprintf(&message, format, va);
	va_end(va);

	struct sl_log_message_unsent * mes = malloc(sizeof(struct sl_log_message_unsent));
	struct sl_log_message * data = &mes->data;
	data->level = level;
	data->type = type;
	data->message = message;
	data->timestamp = time(NULL);
	mes->next = NULL;

	pthread_mutex_lock(&sl_log_lock);

	if (!sl_log_message_first)
		sl_log_message_first = sl_log_message_last = mes;
	else
		sl_log_message_last = sl_log_message_last->next = mes;

	pthread_cond_signal(&sl_log_wait);
	pthread_mutex_unlock(&sl_log_lock);
}


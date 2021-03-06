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
*  Last modified: Sun, 14 Jul 2013 23:45:59 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
#include <pthread.h>
// bool
#include <stdbool.h>
// free, malloc, realloc
#include <stdlib.h>
// setpriority
#include <sys/resource.h>
// syscall
#include <sys/syscall.h>
// gettimeofday, setpriority
#include <sys/time.h>
// pid
#include <sys/types.h>
// syscall
#include <unistd.h>

#include <stlocate/log.h>
#include <stlocate/thread_pool.h>

struct sl_thread_pool_thread {
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t wait;

	void (*function)(void * arg);
	void * arg;
	int nice;
	bool want_exit;

	volatile enum {
		sl_thread_pool_state_exited,
		sl_thread_pool_state_running,
		sl_thread_pool_state_waiting,
	} state;
};

static struct sl_thread_pool_thread ** sl_thread_pool_threads = NULL;
static unsigned int sl_thread_pool_nb_threads = 0;
static pthread_mutex_t sl_thread_pool_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static void sl_thread_pool_exit(void) __attribute__((destructor));
static void * sl_thread_pool_work(void * arg);


static void sl_thread_pool_exit() {
	unsigned int i;
	for (i = 0; i < sl_thread_pool_nb_threads; i++) {
		struct sl_thread_pool_thread * th = sl_thread_pool_threads[i];

		pthread_mutex_lock(&th->lock);
		th->want_exit = true;
		if (th->state == sl_thread_pool_state_waiting)
			pthread_cond_signal(&th->wait);
		pthread_mutex_unlock(&th->lock);

		if (th->state == sl_thread_pool_state_running)
			pthread_join(th->thread, NULL);

		free(th);
	}

	free(sl_thread_pool_threads);
	sl_thread_pool_threads = NULL;
	sl_thread_pool_nb_threads = 0;
}

int sl_thread_pool_run(void (*function)(void * arg), void * arg) {
	return sl_thread_pool_run2(function, arg, 0);
}

int sl_thread_pool_run2(void (*function)(void * arg), void * arg, int nice) {
	pthread_mutex_lock(&sl_thread_pool_lock);
	unsigned int i;
	for (i = 0; i < sl_thread_pool_nb_threads; i++) {
		struct sl_thread_pool_thread * th = sl_thread_pool_threads[i];

		if (th->state == sl_thread_pool_state_waiting) {
			pthread_mutex_lock(&th->lock);

			th->function = function;
			th->arg = arg;
			th->nice = nice;
			th->state = sl_thread_pool_state_running;
			th->want_exit = false;

			pthread_cond_signal(&th->wait);
			pthread_mutex_unlock(&th->lock);

			pthread_mutex_unlock(&sl_thread_pool_lock);

			return 0;
		}
	}

	for (i = 0; i < sl_thread_pool_nb_threads; i++) {
		struct sl_thread_pool_thread * th = sl_thread_pool_threads[i];

		if (th->state == sl_thread_pool_state_exited) {
			th->function = function;
			th->arg = arg;
			th->nice = nice;
			th->state = sl_thread_pool_state_running;
			th->want_exit = false;

			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

			pthread_create(&th->thread, &attr, sl_thread_pool_work, th);

			pthread_mutex_unlock(&sl_thread_pool_lock);

			pthread_attr_destroy(&attr);

			return 0;
		}
	}

	void * new_addr = realloc(sl_thread_pool_threads, (sl_thread_pool_nb_threads + 1) * sizeof(struct sl_thread_pool_thread *));
	if (new_addr == NULL) {
		sl_log_write(sl_log_level_err, sl_log_type_core, "Error, not enought memory to start new thread");
		return 1;
	}

	sl_thread_pool_threads = new_addr;
	struct sl_thread_pool_thread * th = sl_thread_pool_threads[sl_thread_pool_nb_threads] = malloc(sizeof(struct sl_thread_pool_thread));
	sl_thread_pool_nb_threads++;

	th->function = function;
	th->arg = arg;
	th->nice = nice;
	th->state = sl_thread_pool_state_running;
	th->want_exit = false;

	pthread_mutex_init(&th->lock, NULL);
	pthread_cond_init(&th->wait, NULL);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	pthread_create(&th->thread, &attr, sl_thread_pool_work, th);

	pthread_mutex_unlock(&sl_thread_pool_lock);

	pthread_attr_destroy(&attr);

	return 0;
}

static void * sl_thread_pool_work(void * arg) {
	struct sl_thread_pool_thread * th = arg;

	pid_t tid = syscall(SYS_gettid);

	sl_log_write(sl_log_level_debug, sl_log_type_core, "Starting new thread #%ld (pid: %d) to function: %p with parameter: %p", th->thread, tid, th->function, th->arg);

	do {
		setpriority(PRIO_PROCESS, tid, th->nice);

		th->function(th->arg);

		sl_log_write(sl_log_level_debug, sl_log_type_core, "Thread #%ld (pid: %d) is going to sleep", th->thread, tid);

		pthread_mutex_lock(&th->lock);

		th->function = NULL;
		th->arg = NULL;
		th->state = sl_thread_pool_state_waiting;

		if (!th->want_exit) {
			struct timeval now;
			struct timespec timeout;
			gettimeofday(&now, NULL);
			timeout.tv_sec = now.tv_sec + 300;
			timeout.tv_nsec = now.tv_usec * 1000;

			pthread_cond_timedwait(&th->wait, &th->lock, &timeout);
		}

		if (th->state != sl_thread_pool_state_running)
			th->state = sl_thread_pool_state_exited;

		pthread_mutex_unlock(&th->lock);

		if (th->state == sl_thread_pool_state_running)
			sl_log_write(sl_log_level_debug, sl_log_type_core, "Restarting thread #%ld (pid: %d) to function: %p with parameter: %p", th->thread, tid, th->function, th->arg);

	} while (th->state == sl_thread_pool_state_running);

	sl_log_write(sl_log_level_debug, sl_log_type_core, "Thread #%ld is dead", th->thread);

	return NULL;
}


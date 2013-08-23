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
*  Last modified: Fri, 23 Aug 2013 22:46:23 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// getopt_long
#include <getopt.h>
// dirname
#include <libgen.h>
// realpath
#include <limits.h>
// bool
#include <stdbool.h>
// printf, rename
#include <stdio.h>
// free, realpath
#include <stdlib.h>
// strcmp, strlen, strrchr
#include <string.h>
// lstat, mkdir
#include <sys/stat.h>
// lstat
#include <sys/types.h>
// uname
#include <sys/utsname.h>
// access, chown, lstat
#include <unistd.h>

#include <stlocate/conf.h>
#include <stlocate/database.h>
#include <stlocate/log.h>
#include <stlocate/result.h>

#include "prompt.h"

#include <config.h>
#include <stlocate.version>
#include <stmvfile.chcksum>

static void sl_show_help(void);

int main(int argc, char * argv[]) {
	sl_log_write(sl_log_level_notice, sl_log_type_core, "Starting StMvFile, version: " STLOCATE_VERSION ", build: " __DATE__ " " __TIME__);

	enum {
		OPT_CONFIG  = 'c',
		OPT_HELP    = 'h',
		OPT_HOST    = 'H',
		OPT_VERBOSE = 'v',
		OPT_VERSION = 'V',
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "config",  1, NULL, OPT_CONFIG },
		{ "help",    0, NULL, OPT_HELP },
		{ "host",    1, NULL, OPT_HOST },
		{ "verbose", 0, NULL, OPT_VERBOSE },
		{ "version", 0, NULL, OPT_VERSION },

		{NULL, 0, NULL, 0},
	};

	static const char * config = CONFIG_FILE;
	const char * host = NULL;
	short verbose = 0;

	// parse option
	int opt;
	do {
		opt = getopt_long(argc, argv, "c:hH:vV", long_options, &option_index);

		switch (opt) {
			case -1:
				break;

			case OPT_CONFIG:
				config = optarg;
				sl_log_write(sl_log_level_notice, sl_log_type_core, "Using configuration file: '%s'", optarg);
				break;

			case OPT_HELP:
				sl_log_disable_display_log();

				sl_show_help();
				return 0;

			case OPT_HOST:
				host = optarg;
				sl_log_write(sl_log_level_notice, sl_log_type_core, "Using alternative host; '%s'", optarg);
				break;

			case OPT_VERBOSE:
				if (verbose < 3)
					verbose++;
				break;

			case OPT_VERSION:
				sl_log_disable_display_log();

				printf("StMvFile, version: " STLOCATE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
				printf("sha1sum: " STMVFILE_SRCSUM ", commit: " STLOCATE_GIT_COMMIT "\n");
				return 0;

			default:
				sl_log_write(sl_log_level_crit, sl_log_type_core, "Unsupported parameter '%d : %s'", opt, optarg);
				return 1;
		}
	} while (opt > -1);

	sl_log_set_verbose(verbose);

	// read configuration
	if (sl_conf_read_config(config)) {
		sl_log_write(sl_log_level_crit, sl_log_type_core, "Error while parsing '%s'", config);
		return 4;
	}

	// check db connection
	int failed = 0;
	struct sl_database_config * db_config = sl_database_get_config_by_name("main");
	if (db_config == NULL) {
		sl_log_write(sl_log_level_crit, sl_log_type_core, "There is no database config with storage = main in config file '%s'", config);
		failed = 5;
	}

	struct sl_database_connection * connect = NULL;
	if (failed == 0) {
		connect = db_config->ops->connect(db_config);
		if (connect == NULL) {
			sl_log_write(sl_log_level_crit, sl_log_type_core, "Connection to database filed");
			failed = 6;
		}
	}

	if (!failed) {
		if (host == NULL) {
			static struct utsname name;
			uname(&name);

			host = name.nodename;
		}

		sl_log_write(sl_log_level_debug, sl_log_type_core, "Looking for host_id of '%s'...", host);

		int host_id = connect->ops->get_host_by_name(connect, host);

		if (host_id < 0) {
			sl_log_write(sl_log_level_err, sl_log_type_core, "Host '%s' not found", host);
			failed = 7;
		} else
			sl_log_write(sl_log_level_debug, sl_log_type_core, "Host '%s' found with id: %d", host, host_id);

		for (; !failed && optind < argc; optind++) {
			struct stat st;
			int failed = stat(argv[optind], &st);
			if (failed) {
				sl_log_write(sl_log_level_warn, sl_log_type_core, "failed to get info of '%s' because %m", argv[optind]);
				continue;
			}

			struct sl_request req;
			sl_request_init(&req);

			req.dev_no = st.st_dev;
			req.inode = st.st_ino;

			char * parent = realpath(argv[optind], NULL);
			struct stat st_mp;
			char old = '\0';
			do {
				char * slash = strrchr(parent, '/');
				if (slash != NULL)
					old = slash[1];

				parent = dirname(parent);
				failed = stat(parent, &st_mp);
			} while (!failed && st.st_dev == st_mp.st_dev);

			if (old)
				parent[strlen(parent)] = old;

			struct sl_result_files * result = connect->ops->find_file(connect, host_id, &req);
			if (result == NULL) {
				sl_log_write(sl_log_level_err, sl_log_type_core, "An error occured when getting result");
				free(parent);
				break;
			}

			if (result->nb_files == 0) {
				sl_log_write(sl_log_level_info, sl_log_type_core, "No file found: %s", argv[optind]);
				printf("%s: No file found into database\n", argv[optind]);
			} else {
				bool stop = false, move = false;
				unsigned int index = 0;

				char * computed;
				while (!stop) {
					char * current = realpath(argv[optind], NULL);
					asprintf(&computed, "%s/%s", parent, result->files[index].path);

					if (!strcmp(current, computed)) {
						printf("File '%s' is already at the correct position\n", argv[optind]);
						stop = true;
					} else {
						printf("There is %u file%c that match '%s' into database\n", result->nb_files, result->nb_files != 1 ? 's' : '\0', argv[optind]);
						char * action = sl_prompt("Move '%s' to '%s' [yes/Next/quit] ? ", argv[optind], computed);

						if (action == NULL) {
							printf("\n");
							stop = true;
						} else if (!strcmp(action, "q") || !strcmp(action, "quit"))
							stop = true;
						else if (!strcmp(action, "y") || !strcmp(action, "yes"))
							stop = true, move = true;
						else if (strlen(action) == 0 || !strcmp(action, "next") || !strcmp(action, "next")) {
							index++;
							if (index == result->nb_files)
								index = 0;
						}

						free(action);
					}

					free(current);
				}

				if (move) {
					char * computed2 = dirname(computed);

					unsigned short i;
					for (i = 0; strcmp(computed2, ".") && access(computed2, F_OK); i++)
						computed2 = dirname(computed2);

					struct sl_result_file * rf = result->files + index;

					if (i > 0) {
						sl_log_write(sl_log_level_info, sl_log_type_core, "Rebuild path to: %s/%s", parent, rf->path);

						size_t parent_size = strlen(parent) + 1;

						while (!failed && i > 0) {
							computed[strlen(computed)] = '/';
							i--;

							sl_log_write(sl_log_level_info, sl_log_type_core, "Restore directory '%s'", computed);

							// find file info into database
							struct sl_result_file * file = connect->ops->get_file_info(connect, rf->session_id, rf->fs_id, computed + parent_size);
							if (file != NULL) {
								failed = mkdir(computed, file->mode);
								if (failed)
									sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to create directory '%s' because %m", computed);

								if (!failed) {
									failed = chown(computed, file->uid, file->gid);
									if (failed)
										sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to restore owner and group for directory '%s' because %m", computed);
								}

								sl_result_file_free(file);
								free(file);
							} else {
								// warn, no info of file computed
								failed = mkdir(computed, 0777);
								if (failed)
									sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to create directory '%s' because %m", computed);
							}
						}
					}

					computed[strlen(computed)] = '/';

					if (!failed) {
						failed = rename(argv[optind], computed);
						if (failed)
							sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to move file from '%s' to '%s' because %m", argv[optind], computed);
						else
							sl_log_write(sl_log_level_info, sl_log_type_core, "Move file from '%s' to '%s', OK", argv[optind], computed);
					}
				}

				free(computed);
			}

			free(parent);
			sl_result_files_free(result);
		}
	}

	connect->ops->free(connect);

	sl_log_stop_logger();

	return failed;
}

static void sl_show_help() {
	sl_log_disable_display_log();

	printf("StMvFile [OPTIONS]... FILE\n");
	printf("    --config,                  -c : Read this config file instead of \"" CONFIG_FILE "\"\n");
	printf("    --help,                    -h : Show this and exit\n");
	printf("    --version,                 -V : Show the version of STone then exit\n");
}


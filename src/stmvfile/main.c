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
*  Last modified: Sat, 10 Aug 2013 17:53:04 +0200                         *
\*************************************************************************/

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>
// lstat
#include <sys/stat.h>
// lstat
#include <sys/types.h>
// uname
#include <sys/utsname.h>
// lstat
#include <unistd.h>

#include <stlocate/conf.h>
#include <stlocate/database.h>
#include <stlocate/log.h>
#include <stlocate/result.h>

#include <config.h>
#include <stlocate.version>
#include <stmvfile.chcksum>

static void sl_show_help(void);

int main(int argc, char * argv[]) {
	sl_log_write(sl_log_level_notice, sl_log_type_core, "Starting StMvFile, version: " STLOCATE_VERSION ", build: " __DATE__ " " __TIME__);

	enum {
		OPT_CONFIG  = 'c',
		OPT_HELP    = 'h',
		OPT_VERBOSE = 'v',
		OPT_VERSION = 'V',
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "config",  1, NULL, OPT_CONFIG },
		{ "help",    0, NULL, OPT_HELP },
		{ "verbose", 0, NULL, OPT_VERBOSE },
		{ "version", 0, NULL, OPT_VERSION },

		{NULL, 0, NULL, 0},
	};

	static const char * config = CONFIG_FILE;
	short verbose = 0;

	// parse option
	int opt;
	do {
		opt = getopt_long(argc, argv, "c:hvV", long_options, &option_index);

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
		static struct utsname name;
		uname(&name);

		int host_id = connect->ops->get_host_by_name(connect, name.nodename);

		for (; optind < argc; optind++) {
			struct stat st;
			int failed = stat(argv[optind], &st);
			if (failed) {
				printf("failed to get info of '%s' because %m", argv[optind]);
				continue;
			}

			struct sl_request req;
			sl_request_init(&req);

			req.dev_no = st.st_dev;
			req.inode = st.st_ino;

			struct sl_result_files * result = connect->ops->find_file(connect, host_id, &req);
			if (result == NULL) {
				printf("An error occured when getting result\n");
				continue;
			}

			unsigned int i;
			for (i = 0; i < result->nb_files; i++) {
				struct sl_result_file * f = result->files + i;
				printf("%u: %s/%s s:%zd\n", i, f->mount_point, f->path, f->size);
			}

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


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
*  Last modified: Tue, 30 Jul 2013 22:52:38 +0200                         *
\*************************************************************************/

// getopt_long
#include <getopt.h>
// printf, sscanf
#include <stdio.h>
// uname
#include <sys/utsname.h>

#include <stlocate/conf.h>
#include <stlocate/database.h>
#include <stlocate/log.h>

#include "common.h"

#include <config.h>
#include <stlocate.version>
#include <stupdate_db.chcksum>

static void sl_show_help(void);

int main(int argc, char * argv[]) {
	sl_log_write(sl_log_level_notice, sl_log_type_core, "Starting StUpdate_db, version: " STLOCATE_VERSION ", build: " __DATE__ " " __TIME__);

	enum {
		OPT_CONFIG  = 'c',
		OPT_HELP    = 'h',
		OPT_VERBOSE = 'v',
		OPT_VERSION = 'V',

		OPT_KEEP_SESSION = 100,
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "config",       1, NULL, OPT_CONFIG },
		{ "keep-session", 1, NULL, OPT_KEEP_SESSION },
		{ "help",         0, NULL, OPT_HELP },
		{ "verbose",      0, NULL, OPT_VERBOSE },
		{ "version",      0, NULL, OPT_VERSION },

		{NULL, 0, NULL, 0},
	};

	static const char * config = CONFIG_FILE;
	int keep_session = 0;
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

			case OPT_KEEP_SESSION:
				if (sscanf(optarg, "%d", &keep_session) == 0) {
					sl_log_write(sl_log_level_crit, sl_log_type_core, "parameter: --keep-session require an integer as option instead of %s", optarg);
					return 1;
				}
				if (keep_session <= 0) {
					sl_log_write(sl_log_level_crit, sl_log_type_core, "parameter: --keep-session require a positive integer as option instead of %d", keep_session);
					return 1;
				}
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

				printf("StUpdate_db, version: " STLOCATE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
				printf("sha1sum: " STUPDATE_DB_SRCSUM ", commit: " STLOCATE_GIT_COMMIT "\n");
				return 0;

			default:
				sl_log_write(sl_log_level_crit, sl_log_type_core, "Unsupported parameter '%d : %s'", opt, optarg);
				return 1;
		}
	} while (opt > -1);

	sl_log_set_verbose(verbose);

	sl_log_write(sl_log_level_info, sl_log_type_core, "Parsing option: ok");

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

	int current_db_version = 0;
	if (failed == 0)
		current_db_version = connect->ops->get_database_version(connect);

	if (current_db_version < 0) {
		int db_max_version = db_config->driver->ops->get_max_version_supported();
		current_db_version = db_max_version < CURRENT_DB_VERSION ? db_max_version : CURRENT_DB_VERSION;

		sl_log_write(sl_log_level_notice, sl_log_type_core, "There is no database, create new one (version %d)", current_db_version);

		failed = connect->ops->create_database(connect, current_db_version);

		if (failed)
			sl_log_write(sl_log_level_crit, sl_log_type_core, "Failed to create new database");
		else
			sl_log_write(sl_log_level_notice, sl_log_type_core, "Creation of new database: ok");
	} else if (current_db_version > CURRENT_DB_VERSION) {
		sl_log_write(sl_log_level_crit, sl_log_type_core, "Current version of StUpdate_db do not manage database with version %d", current_db_version);
		sl_log_write(sl_log_level_crit, sl_log_type_core, "Version managed up to %d", CURRENT_DB_VERSION);
		failed = 6;
	}

	static struct utsname name;
	uname(&name);

	int host_id = connect->ops->get_host_by_name(connect, name.nodename);

	if (keep_session == 0 && failed == 0) {
		sl_log_write(sl_log_level_notice, sl_log_type_core, "Start updating");
		failed = sl_db_update(connect, host_id, current_db_version);

		if (failed)
			sl_log_write(sl_log_level_err, sl_log_type_core, "Updating finished with errors");
		else
			sl_log_write(sl_log_level_notice, sl_log_type_core, "Updating finished without errors");
	}

	if (failed == 0) {
		if (keep_session == 0)
			keep_session = db_config->nb_session_kept;

		sl_log_write(sl_log_level_notice, sl_log_type_core, "Start removing old %d session", keep_session);
		failed = connect->ops->delete_old_session(connect, host_id, keep_session);

		if (failed)
			sl_log_write(sl_log_level_err, sl_log_type_core, "Deleting old session finished with errors");
		else
			sl_log_write(sl_log_level_notice, sl_log_type_core, "Deleting old session finished without errors");
	}

	connect->ops->free(connect);

	sl_log_write(sl_log_level_notice, sl_log_type_core, "StUpdate_db finished");

	sl_log_stop_logger();

	return failed;
}

static void sl_show_help() {
	sl_log_disable_display_log();

	printf("StUpdate_db, version: " STLOCATE_VERSION ", build: " __DATE__ " " __TIME__ "\n");
	printf("    --config,                -c : Read this config file instead of \"" CONFIG_FILE "\"\n");
	printf("    --keep-session <nb_session> : Keep at least nb_session from database\n");
	printf("    --help,                  -h : Show this and exit\n");
	printf("    --version,               -V : Show the version of STone then exit\n");
}


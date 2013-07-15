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
*  Last modified: Mon, 15 Jul 2013 22:26:36 +0200                         *
\*************************************************************************/

#include <stlocate/database.h>
#include <stlocate/log.h>

#include <sys/utsname.h>

#include "common.h"

int sl_db_update(struct sl_database_connection * db, int version __attribute__((unused))) {
	struct utsname name;
	uname(&name);

	int host_id = db->ops->get_host_by_name(db, name.nodename);

	int failed = db->ops->start_transaction(db);
	if (failed) {
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to start new transaction");
		return failed;
	}

	int session_id = db->ops->start_session(db);
	if (session_id < 0) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to create new session");
		return failed;
	}

	failed = db->ops->end_session(db, session_id);
	if (failed) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to finish current session");
		return failed;
	}

	failed = db->ops->finish_transaction(db);
	if (failed) {
		db->ops->cancel_transaction(db);
		sl_log_write(sl_log_level_err, sl_log_type_core, "Failed to commit current transaction");
		return failed;
	}

	return 0;
}


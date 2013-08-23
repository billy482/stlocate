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
*  Last modified: Fri, 23 Aug 2013 09:55:13 +0200                         *
\*************************************************************************/

#ifndef __STLOCATE_RESULT_H__
#define __STLOCATE_RESULT_H__

#include <sys/types.h>

struct sl_request {
	int session_min_id;
	int session_max_id;

	dev_t dev_no;
	ino_t inode;
};

struct sl_result_files {
	struct sl_result_file {
		int session_id;
		time_t session_start;
		time_t session_end;

		int fs_id;
		char * fs_uuid;
		char * fs_label;

		dev_t dev_no;
		char * mount_point;

		ino_t inode;
		char * path;
		mode_t mode;
		uid_t uid;
		gid_t gid;
		off_t size;
		time_t atime;
		time_t mtime;
	} * files;
	unsigned int nb_files;
};

void sl_request_init(struct sl_request * request);

void sl_result_file_free(struct sl_result_file * file);
void sl_result_files_free(struct sl_result_files * result);

#endif


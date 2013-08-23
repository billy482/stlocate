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
*  Last modified: Fri, 23 Aug 2013 09:55:10 +0200                         *
\*************************************************************************/

// free
#include <stdlib.h>

#include <stlocate/result.h>

void sl_request_init(struct sl_request * request) {
	request->session_min_id = 0;
	request->session_max_id = 0;

	request->dev_no = -1;
	request->inode = -1;
}


void sl_result_file_free(struct sl_result_file * file) {
	free(file->fs_uuid);
	free(file->fs_label);
	free(file->mount_point);
	free(file->path);
}

void sl_result_files_free(struct sl_result_files * result) {
	unsigned int i;
	for (i = 0; i < result->nb_files; i++)
		sl_result_file_free(result->files + i);
	free(result->files);
	free(result);
}


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
*  Last modified: Tue, 16 Jul 2013 23:12:19 +0200                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>

#include <stlocate/filesystem.h>

void sl_filesystem_free(struct sl_filesystem * fs) {
	free(fs->uuid);
	free(fs->label);
	free(fs->type);
	free(fs);
}

struct sl_filesystem * sl_filesystem_new(const char * uuid, const char * label, const char * type, dev_t device, const char * mount_point, fsblkcnt_t disk_free, fsblkcnt_t disk_total, fsblkcnt_t block_size) {
	struct sl_filesystem * fs = malloc(sizeof(struct sl_filesystem));
	fs->id = -1;
	fs->uuid = strdup(uuid);
	fs->label = NULL;
	if (label != NULL)
		fs->label = strdup(label);
	fs->type = strdup(type);
	fs->device = device;
	fs->mount_point = strdup(mount_point);
	fs->disk_free = disk_free;
	fs->disk_total = disk_total;
	fs->block_size = block_size;
	return fs;
}


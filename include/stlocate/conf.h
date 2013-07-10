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
*  Last modified: Wed, 10 Jul 2013 20:42:29 +0200                         *
\*************************************************************************/

#ifndef __STLOCATE_CONF_H__
#define __STLOCATE_CONF_H__

struct sl_hashtable;

/**
 * \brief Callback function
 *
 * \param[in] params : All keys values associated to section
 */
typedef void (*sl_conf_callback_f)(const struct sl_hashtable * params);

/**
 * \brief sl_read config file
 *
 * \param[in] conf_file : config file
 * \return a value which correspond to
 * \li 0 if ok
 * \li 1 if error
 */
int sl_conf_read_config(const char * conf_file);

/**
 * \brief Register a function which will be called if a \a section is found
 *
 * First, register a function \a callback associated to a \a section.
 * Then, call sl_conf_read_config and if \a section is found, the function 
 * \a callback will be called.
 *
 * \param[in] section : a section name
 * \param[in] callback : a function called if \a section is found into a config file.
 */
void sl_conf_register_callback(const char * section, sl_conf_callback_f callback);

#endif


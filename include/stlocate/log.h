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
*  Last modified: Mon, 29 Jul 2013 22:39:07 +0200                         *
\*************************************************************************/

#ifndef __STLOCATE_LOG_H__
#define __STLOCATE_LOG_H__

// time_t
#include <sys/time.h>

// forward declarations
struct sl_hashtable;
struct sl_log_module;


/**
 * \enum sl_log_level
 * \brief Enumerate level of message
 *
 * Each level has a priority (i.e. debug > info > notice > warn > error > crit > alert > emerg)
 */
enum sl_log_level {
	/**
	 * \brief Alert level
	 */
	sl_log_level_alert = 0x1,

	/**
	 * \brief Critical level
	 */
	sl_log_level_crit = 0x2,

	/**
	 * \brief Debug level
	 */
	sl_log_level_debug = 0x7,

	/**
	 * \brief Emergency level
	 */
	sl_log_level_emerg = 0x0,

	/**
	 * \brief Error level
	 */
	sl_log_level_err = 0x3,

	/**
	 * \brief Informational level
	 */
	sl_log_level_info = 0x6,

	/**
	 * \brief Notice level
	 */
	sl_log_level_notice = 0x5,

	/**
	 * \brief Warning level
	 */
	sl_log_level_warn = 0x4,

	/**
	 * \brief Should not be used
	 *
	 * Used only by sl_log_string_to_level to report an error
	 * \see sl_log_string_to_level
	 */
	sl_log_level_unknown = 0xF,
};

/**
 * \enum sl_log_type
 * \brief Enumerate type of message
 */
enum sl_log_type {
	sl_log_type_conf,
	sl_log_type_core,
	sl_log_type_database,
	sl_log_type_plugin_database,
	sl_log_type_plugin_log,

	/**
	 * \brief Should not be used
	 *
	 * Used only by sl_log_string_to_type to report an error
	 * \see sl_log_string_to_type
	 */
	sl_log_type_unknown,
};


/**
 * \struct sl_log_message
 * \brief This structure represents a message send to a log module
 *
 * Only usefull for log module
 */
struct sl_log_message {
	/**
	 * \brief Level of message
	 */
	enum sl_log_level level;
	/**
	 * \brief Kind of message
	 */
	enum sl_log_type type;
	/**
	 * \brief Message
	 */
	char * message;
	/**
	 * \brief timestamp of message
	 *
	 * \note Number of seconds since \b Epoch (i.e. Thu, 01 Jan 1970 00:00:00 +0000)
	 */
	time_t timestamp;
};

/**
 * \struct sl_log_driver
 * \brief Structure used used only one time by each log module
 *
 * \note This structure should be staticaly allocated and passed to function sl_log_register_driver
 * \see sl_log_register_driver
 */
struct sl_log_driver {
	/**
	 * \brief Name of driver
	 *
	 * \note Should be unique and equals to liblog-name.so where name is the name of driver.
	 */
	char * name;
	/**
	 * \brief Add a module to this driver
	 *
	 * \param[in] alias : name of module
	 * \param[in] level : level of message
	 * \param[in] params : addictional parameters
	 * \returns 0 if \b ok
	 *
	 * \pre
	 * \li alias is not null and should be duplicated
	 * \li level is not equal to st_log_level_unknown
	 * \li params is not null
	 */
	int (*add)(enum sl_log_level level, const struct sl_hashtable * params);

	/**
	 * \brief cookie
	 *
	 * Is a value returns by dlopen and should not be used nor released
	 */
	void * cookie;
	/**
	 * \brief api level supported by this driver
	 *
	 * Should be define by using STLOCATE_LOG_API_LEVEL only
	 */
	const unsigned int api_level;
	const char * src_checksum;
};

/**
 * \struct sl_log_handler
 * \brief A log module
 */
struct sl_log_handler {
	/**
	 * \brief Minimal level
	 */
	enum sl_log_level level;
	/**
	 * \struct st_log_module_ops
	 * \brief Functions associated to a log module
	 */
	struct sl_log_handler_ops {
		/**
		 * \brief release a module
		 *
		 * \param[in] module : release this module
		 */
		void (*free)(struct sl_log_handler * module);
		/**
		 * \brief Write a message to a module
		 *
		 * \param[in] module : write to this module
		 * \param[in] message : write this message
		 */
		void (*write)(struct sl_log_handler * module, const struct sl_log_message * message);
	} * ops;

	/**
	 * \brief Private data of a log module
	 */
	void * data;
};

/**
 * \def STLOCATE_LOG_API_LEVEL
 * \brief Current api level
 *
 * Will increment with new version of struct st_log_driver or struct st_log
 */
#define STLOCATE_LOG_API_LEVEL 1


void sl_log_add_handler(struct sl_log_handler * handler);

/**
 * \brief Disable display remains messages.
 *
 * Default behaviour is to display all messages into terminal
 * if no log modules are loaded at exit of process.
 */
void sl_log_disable_display_log(void);

/**
 * \brief Get a log driver
 *
 * \param module : driver's name
 * \return 0 if failed
 * \note if the driver is not loaded, st_log_get_driver will try to load it
 *
 * \pre module should not be null
 */
struct sl_log_driver * sl_log_get_driver(const char * module);

/**
 * \brief Each log driver should call this function only one time
 *
 * \param driver : a static allocated structure
 * \code
 * \_\_attribute\_\_((constructor))
 * static void log_myLog_init() {
 *    sl_log_register_driver(&log_myLog_module);
 * }
 * \endcode
 */
void sl_log_register_driver(struct sl_log_driver * driver);

/**
 * \brief Convert an enumeration to a statically allocated string
 *
 * \param level : one log level
 * \return a statically allocated string
 * \note returned value should not be released
 */
const char * sl_log_level_to_string(enum sl_log_level level);

/**
 * \brief Start thread which write messages to log modules
 *
 * \note Should be used only one time
 * \note This function is thread-safe
 */
void sl_log_start_logger(void);

void sl_log_set_verbose(short verbose);

void sl_log_stop_logger();

/**
 * \brief Convert a c string to an enumeration
 *
 * \param string : one string level
 * \return an enumeration
 */
enum sl_log_level sl_log_string_to_level(const char * string);

/**
 * \brief Convert a c string to an enum st_log_type
 *
 * \param string : one string type
 * \return an enumeration
 */
enum sl_log_type sl_log_string_to_type(const char * string);

/**
 * \brief Convert an enum st_log_type to a statically allocated c string
 *
 * \param[in] type : a log type
 * \return a statically allocated c string
 *
 * \note sl_log_type_to_string never returns NULL and you <b>SHOULD NOT RELEASE</b> returned value
 */
const char * sl_log_type_to_string(enum sl_log_type type);

/**
 * \brief Write a message to all log modules
 *
 * \param[in] level : level of message
 * \param[in] type : type of message
 * \param[in] format : message with printf-like syntax
 *
 * \note Message can be wrote after that this function has returned.
 * \note This function is thread-safe
 */
void sl_log_write(enum sl_log_level level, enum sl_log_type type, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

#endif


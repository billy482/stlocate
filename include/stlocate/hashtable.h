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
*  Last modified: Sun, 07 Jul 2013 14:21:24 +0200                         *
\*************************************************************************/

#ifndef __STLOCATE_UTIL_HASHTABLE_H__
#define __STLOCATE_UTIL_HASHTABLE_H__

// bool
#include <stdbool.h>
// {,u}int*_t
#include <stdint.h>

/**
 * \brief Function used to compute hash of keys
 *
 * \param[in] key : a \a key
 * \returns hash
 *
 * \note Mathematical consideration : \f$ \forall (x, y), x \ne y \Rightarrow f(x) \ne f(y) \f$ \n
 * If \f$ x \ne y, f(x) = f(y) \f$, the hash function admits a collision. \n
 * Hash function returns a 64 bits integer so we can have \f$ 2^{64}-1 \f$ (= 18446744073709551615) differents values
 */
typedef uint64_t (*sl_hashtable_compute_hash_f)(const void * key);

/**
 * \brief Function used by a hashtable to release a pair of key value
 *
 * \param[in] key : a \a key
 * \param[in] value : a \a value
 */
typedef void (*sl_hashtable_free_f)(void * key, void * value);

enum sl_hashtable_type {
	sl_hashtable_value_null,
	sl_hashtable_value_boolean,
	sl_hashtable_value_signed_integer,
	sl_hashtable_value_unsigned_integer,
	sl_hashtable_value_float,
	sl_hashtable_value_string,
	sl_hashtable_value_custom,
};

struct sl_hashtable_value {
	enum sl_hashtable_type type;
	union {
		bool boolean;
		int64_t signed_integer;
		uint64_t unsigned_integer;
		double floating;
		char * string;
		void * custom;
	} value;
};

/**
 * \struct sl_hashtable
 * \brief This is a hashtable
 *
 * A hashtable is a collection of pairs of keys values.
 */
struct sl_hashtable {
	/**
	 * \struct sl_hashtable_node
	 * \brief A node of one hashtable
	 */
	struct sl_hashtable_node {
		/**
		 * \brief Hash value of key
		 */
		uint64_t hash;
		void * key;
		struct sl_hashtable_value value;
		struct sl_hashtable_node * next;
	} ** nodes;
	/**
	 * \brief Numbers of elements in hashtable
	 */
	uint32_t nb_elements;
	/**
	 * \brief Only for internal use of hashtable
	 */
	uint32_t size_node;
	/**
	 * \brief Only for internal use of hashtable
	 */
	bool allow_rehash;

	/**
	 * \brief Function used by hashtable for computing hash of one key
	 *
	 * \see sl_compute_hash_f
	 */
	sl_hashtable_compute_hash_f compute_hash;
	/**
	 * \brief Function used by hashtable for releasing elements in hashtable
	 *
	 * \see sl_release_key_value_f
	 */
	sl_hashtable_free_f release_key_value;
};

struct sl_hashtable_iterator {
	struct sl_hashtable * hashtable;

	uint32_t index;
	struct sl_hashtable_node * current_node;
};


struct sl_hashtable_value sl_hashtable_val_boolean(bool val);
struct sl_hashtable_value sl_hashtable_val_custom(void * val);
struct sl_hashtable_value sl_hashtable_val_float(double val);
struct sl_hashtable_value sl_hashtable_val_null(void);
struct sl_hashtable_value sl_hashtable_val_signed_integer(int64_t val);
struct sl_hashtable_value sl_hashtable_val_string(char * string);
struct sl_hashtable_value sl_hashtable_val_unsigned_integer(uint64_t val);

bool sl_hashtable_val_can_convert(struct sl_hashtable_value * val, enum sl_hashtable_type type);
bool sl_hashtable_val_convert_to_bool(struct sl_hashtable_value * val);
double sl_hashtable_val_convert_to_float(struct sl_hashtable_value * val);
int64_t sl_hashtable_val_convert_to_signed_integer(struct sl_hashtable_value * val);
char * sl_hashtable_val_convert_to_string(struct sl_hashtable_value * val);
uint64_t sl_hashtable_val_convert_to_unsigned_integer(struct sl_hashtable_value * val);
bool sl_hashtable_val_equals(struct sl_hashtable_value * val_a, struct sl_hashtable_value * val_b);


/**
 * \brief Remove (and release) all elements in hashtable
 *
 * \param[in] hashtable : a hashtable
 *
 * \see sl_release_key_value_f
 */
void sl_hashtable_clear(struct sl_hashtable * hashtable);

/**
 * \brief Release memory associated to a hashtable
 *
 * \param[in] hashtable : a hashtable
 *
 * \see sl_hashtable_clear
 */
void sl_hashtable_free(struct sl_hashtable * hashtable);

bool sl_hashtable_equals(struct sl_hashtable * table_a, struct sl_hashtable * table_b);

/**
 * \brief Get a value associated with this \a key
 *
 * Compute hash of \a key to find a pair of key value. If the pair is no found,
 * this function returns \b NULL.\n Otherwise returns value of found pair.
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 * \returns \b 0 if there is no value associated with this \a key
 *
 * \note Parameters \a hashtable and \a key are not supposed to be \b NULL so
 * sl_hashtable_value returns \b 0 if \a hashtable is \b NULL or \a key is \b NULL
 *
 * \attention You <b> SHOULD NOT RELEASE </b> the returned value because this value is
 * shared with this \a hashtable
 */
struct sl_hashtable_value sl_hashtable_get(const struct sl_hashtable * hashtable, const void * key);

/**
 * \brief Check if this \a hashtable contains this key
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 * \returns 1 if found otherwise 0
 */
bool sl_hashtable_has_key(const struct sl_hashtable * hashtable, const void * key);

/**
 * \brief Gets keys from hashtable
 *
 * \param[in] hashtable : a hashtable
 * \returns a calloc allocated and null terminated array which contains keys
 *
 * \attention You should release the returned array with \a free but not keys
 * because keys are shared with hashtable
 */
const void ** sl_hashtable_keys(struct sl_hashtable * hashtable, uint32_t * nb_value);

/**
 * \brief Create a new hashtable
 *
 * \param[in] ch : function used by hashtable to compute hash of keys
 * \returns a hashtable
 *
 * \attention You sould use a function that not contains known collisions.
 *
 * \see sl_compute_hash_f
 */
struct sl_hashtable * sl_hashtable_new(sl_hashtable_compute_hash_f ch);

/**
 * \brief Create a new hashtable
 *
 * \param[in] ch : function used by hashtable to compute hash of keys
 * \param[in] release : function used by hashtable to release a pair of key and value
 * \returns a hashtable
 *
 * \see sl_hashtable_new
 * \see sl_release_key_value_f
 */
struct sl_hashtable * sl_hashtable_new2(sl_hashtable_compute_hash_f ch, sl_hashtable_free_f release);

/**
 * \brief Put a new pair of key value into this \a hashtable
 *
 * Compute hash of this key by using function compute_hash provide at creation of hashtable.\n
 * If a hash is already present (in this case, there is a collision), we release old pair of
 * key and value.\n In all cases, it adds new pair of key and value.
 *
 * \note Parameters \a hashtable and \a key are not supposed to be \b NULL so
 * sl_hashtable_put does nothing in this case
 *
 * \note Release old value is done by using release_key_value function passed at creation of
 * hashtable
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 * \param[in] value : a value
 *
 * \see sl_hashtable_new2
 */
void sl_hashtable_put(struct sl_hashtable * hashtable, void * key, struct sl_hashtable_value value);

/**
 * \brief Remove a pair of key and value
 *
 * Compute hash of \a key to find a pair of key value. If the pair is no found,
 * this function does nothing.\n Otherwise it removes the founded pair and release
 * key and value if release_key_value was provided while creating this
 * \a hashtable by using sl_hashtable_new2
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 *
 * \note Parameters \a hashtable and \a key are not supposed to be \b NULL so
 * sl_hashtable_remove does nothing in this case
 *
 * \see sl_hashtable_new2
 * \see sl_hashtable_clear
 */
void sl_hashtable_remove(struct sl_hashtable * hashtable, const void * key);

/**
 * \brief Get all values of this \a hashtable
 *
 * \param[in] hashtable : a hashtable
 * \returns a calloc allocated and null terminated array which contains all values
 *
 * \note Parameter \a hashtable is not supposed to be \b NULL so sl_hashtable_values
 * returns \b 0 if \a hashtable is \b NULL
 *
 * \attention You should release the returned array with \a free but not values
 * because values are shared with this \a hashtable
 *
 * \see sl_hashtable_keys
 */
struct sl_hashtable_value * sl_hashtable_values(struct sl_hashtable * hashtable, uint32_t * nb_value);


void sl_hashtable_iterator_free(struct sl_hashtable_iterator * iterator);
bool sl_hashtable_iterator_has_next(struct sl_hashtable_iterator * iterator);
void * sl_hashtable_iterator_next(struct sl_hashtable_iterator * iterator);
struct sl_hashtable_iterator * sl_hashtable_iterator_new(struct sl_hashtable * hashtable);

#endif


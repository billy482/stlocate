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
*  Last modified: Sun, 07 Jul 2013 14:52:41 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// PRI*64
#include <inttypes.h>
// calloc, free, malloc
#include <malloc.h>
// NAN
#include <math.h>
// asprintf, sscanf
#include <stdio.h>
// strcasecmp, strdup
#include <string.h>

#include <stlocate/hashtable.h>

static struct sl_hashtable_value mhv_null = { .type = sl_hashtable_value_null };

static void sl_hashtable_put2(struct sl_hashtable * hashtable, unsigned int index, struct sl_hashtable_node * new_node);
static void sl_hashtable_rehash(struct sl_hashtable * hashtable);
static void sl_hashtable_release_value(sl_hashtable_free_f release, struct sl_hashtable_node * node);


struct sl_hashtable_value sl_hashtable_val_boolean(bool val) {
	struct sl_hashtable_value v = {
		.type          = sl_hashtable_value_boolean,
		.value.boolean = val,
	};
	return v;
}

struct sl_hashtable_value sl_hashtable_val_custom(void * val) {
	struct sl_hashtable_value v = {
		.type         = sl_hashtable_value_custom,
		.value.custom = val,
	};
	return v;
}

struct sl_hashtable_value sl_hashtable_val_float(double val) {
	struct sl_hashtable_value v = {
		.type           = sl_hashtable_value_float,
		.value.floating = val,
	};
	return v;
}

struct sl_hashtable_value sl_hashtable_val_null() {
	struct sl_hashtable_value v = {
		.type          = sl_hashtable_value_null,
	};
	return v;
}

struct sl_hashtable_value sl_hashtable_val_signed_integer(int64_t val) {
	struct sl_hashtable_value v = {
		.type                 = sl_hashtable_value_signed_integer,
		.value.signed_integer = val,
	};
	return v;
}

struct sl_hashtable_value sl_hashtable_val_string(char * string) {
	struct sl_hashtable_value v = {
		.type         = sl_hashtable_value_string,
		.value.string = string,
	};
	return v;
}

struct sl_hashtable_value sl_hashtable_val_unsigned_integer(uint64_t val) {
	struct sl_hashtable_value v = {
		.type                   = sl_hashtable_value_unsigned_integer,
		.value.unsigned_integer = val,
	};
	return v;
}


bool sl_hashtable_val_can_convert(struct sl_hashtable_value * val, enum sl_hashtable_type type) {
	if (val == NULL)
		return false;

	unsigned int i;
	int64_t signed_integer;
	uint64_t unsigned_integer;
	double floating;
	static const struct {
		char * str;
		bool val;
	} string_to_boolean[] = {
		{ "f", false },
		{ "false", false },
		{ "t", true },
		{ "true", true },

		{ NULL, false },
	};

	switch (type) {
		case sl_hashtable_value_boolean:
			switch (val->type) {
				case sl_hashtable_value_boolean:
				case sl_hashtable_value_signed_integer:
				case sl_hashtable_value_unsigned_integer:
					return true;

				case sl_hashtable_value_string:
					for (i = 0; string_to_boolean[i].str != NULL; i++)
						if (!strcasecmp(val->value.string, string_to_boolean[i].str))
							return string_to_boolean[i].val;

				default:
					return false;
			}

		case sl_hashtable_value_custom:
			return val->type == sl_hashtable_value_custom;

		case sl_hashtable_value_float:
			switch (val->type) {
				case sl_hashtable_value_float:
				case sl_hashtable_value_signed_integer:
				case sl_hashtable_value_unsigned_integer:
					return true;

				case sl_hashtable_value_string:
					return sscanf(val->value.string, "%lf", &floating) == 1;

				default:
					return false;
			}

		case sl_hashtable_value_null:
			switch (val->type) {
				case sl_hashtable_value_null:
				case sl_hashtable_value_string:
				case sl_hashtable_value_custom:
					return true;

				default:
					return false;
			}

		case sl_hashtable_value_signed_integer:
			switch (val->type) {
				case sl_hashtable_value_signed_integer:
				case sl_hashtable_value_unsigned_integer:
					return true;

				case sl_hashtable_value_string:
					return sscanf(val->value.string, "%" PRId64, &signed_integer) == 1;

				default:
					return false;
			}

		case sl_hashtable_value_string:
			return val->type != sl_hashtable_value_custom;

		case sl_hashtable_value_unsigned_integer:
			switch (val->type) {
				case sl_hashtable_value_signed_integer:
				case sl_hashtable_value_unsigned_integer:
					return true;

				case sl_hashtable_value_string:
					return sscanf(val->value.string, "%" PRIu64, &unsigned_integer) == 1;

				default:
					return false;
			}
	}

	return false;
}

bool sl_hashtable_val_convert_to_bool(struct sl_hashtable_value * val) {
	if (val == NULL)
		return false;

	unsigned int i;
	static const struct {
		char * str;
		bool val;
	} string_to_boolean[] = {
		{ "f", false },
		{ "false", false },
		{ "t", true },
		{ "true", true },

		{ NULL, false },
	};

	switch (val->type) {
		case sl_hashtable_value_boolean:
			return true;

		case sl_hashtable_value_signed_integer:
			return val->value.signed_integer != 0;

		case sl_hashtable_value_string:
			for (i = 0; string_to_boolean[i].str != NULL; i++)
				if (!strcasecmp(val->value.string, string_to_boolean[i].str))
					return string_to_boolean[i].val;

		case sl_hashtable_value_unsigned_integer:
			return val->value.unsigned_integer != 0;

		default:
			return false;
	}
}

double sl_hashtable_val_convert_to_float(struct sl_hashtable_value * val) {
	if (val == NULL)
		return NAN;

	double floating;
	switch (val->type) {
		case sl_hashtable_value_float:
			return val->value.floating;

		case sl_hashtable_value_signed_integer:
			return val->value.signed_integer;

		case sl_hashtable_value_unsigned_integer:
			return val->value.unsigned_integer;

		case sl_hashtable_value_string:
			if (sscanf(val->value.string, "%lf", &floating) == 1)
				return floating;

		default:
			return NAN;
	}
}

int64_t sl_hashtable_val_convert_to_signed_integer(struct sl_hashtable_value * val) {
	if (val == NULL)
		return 0;

	int64_t signed_integer;
	switch (val->type) {
		case sl_hashtable_value_signed_integer:
			return val->value.signed_integer;

		case sl_hashtable_value_unsigned_integer:
			return val->value.unsigned_integer;

		case sl_hashtable_value_string:
			if (sscanf(val->value.string, "%" PRId64, &signed_integer) == 1)
				return signed_integer;

		default:
			return 0;
	}
}

char * sl_hashtable_val_convert_to_string(struct sl_hashtable_value * val) {
	if (val == NULL)
		return NULL;

	char * value;
	switch (val->type) {
		case sl_hashtable_value_boolean:
			if (val->value.boolean)
				return strdup("true");
			else
				return strdup("false");

		case sl_hashtable_value_float:
			asprintf(&value, "%lg", val->value.floating);
			return value;

		case sl_hashtable_value_signed_integer:
			asprintf(&value, "%" PRId64, val->value.signed_integer);
			return value;

		case sl_hashtable_value_string:
			return strdup(val->value.string);

		case sl_hashtable_value_unsigned_integer:
			asprintf(&value, "%" PRIu64, val->value.unsigned_integer);
			return value;

		default:
			return NULL;
	}
}

uint64_t sl_hashtable_val_convert_to_unsigned_integer(struct sl_hashtable_value * val) {
	if (val == NULL)
		return 0;

	uint64_t unsigned_integer;
	switch (val->type) {
		case sl_hashtable_value_signed_integer:
			return val->value.signed_integer;

		case sl_hashtable_value_unsigned_integer:
			return val->value.unsigned_integer;

		case sl_hashtable_value_string:
			if (sscanf(val->value.string, "%" PRIu64, &unsigned_integer) == 1)
				return unsigned_integer;

		default:
			return 0;
	}
}

bool sl_hashtable_val_equals(struct sl_hashtable_value * val_a, struct sl_hashtable_value * val_b) {
	if (val_a == NULL || val_b == NULL)
		return false;

	if (val_a->type != val_b->type)
		return false;

	switch (val_a->type) {
		case sl_hashtable_value_null:
			return val_b->type == sl_hashtable_value_null;

		case sl_hashtable_value_boolean:
			return val_a->value.boolean == val_b->value.boolean;

		case sl_hashtable_value_custom:
			return val_a->value.custom == val_b->value.custom;

		case sl_hashtable_value_float:
			return val_a->value.floating == val_b->value.floating;

		case sl_hashtable_value_signed_integer:
			return val_a->value.signed_integer == val_b->value.signed_integer;

		case sl_hashtable_value_string:
			return !strcmp(val_a->value.string, val_b->value.string);

		case sl_hashtable_value_unsigned_integer:
			return val_a->value.unsigned_integer == val_b->value.unsigned_integer;
	}

	return false;
}


void sl_hashtable_clear(struct sl_hashtable * hashtable) {
	if (hashtable == NULL)
		return;

	uint32_t i;
	for (i = 0; i < hashtable->size_node; i++) {
		struct sl_hashtable_node * ptr = hashtable->nodes[i];
		hashtable->nodes[i] = NULL;

		while (ptr != NULL) {
			struct sl_hashtable_node * tmp = ptr;
			ptr = ptr->next;

			sl_hashtable_release_value(hashtable->release_key_value, tmp);
		}
	}

	hashtable->nb_elements = 0;
}

void sl_hashtable_free(struct sl_hashtable * hashtable) {
	if (hashtable == NULL)
		return;

	sl_hashtable_clear(hashtable);

	free(hashtable->nodes);
	hashtable->nodes = NULL;
	hashtable->compute_hash = NULL;
	hashtable->release_key_value = NULL;

	free(hashtable);
}

bool sl_hashtable_equals(struct sl_hashtable * table_a, struct sl_hashtable * table_b) {
	if (table_a == NULL || table_b == NULL || table_a->nb_elements != table_b->nb_elements)
		return false;

	if (table_a->nb_elements == 0)
		return true;

	const void ** keys = sl_hashtable_keys(table_a, NULL);

	uint32_t i;
	bool ok = true;
	for (i = 0; i < table_a->nb_elements; i++) {
		struct sl_hashtable_value val_a = sl_hashtable_get(table_a, keys[i]);
		struct sl_hashtable_value val_b = sl_hashtable_get(table_b, keys[i]);

		ok = sl_hashtable_val_equals(&val_a, &val_b);
	}

	free(keys);

	return ok;
}

struct sl_hashtable_value sl_hashtable_get(const struct sl_hashtable * hashtable, const void * key) {
	if (hashtable == NULL || key == NULL)
		return mhv_null;

	uint64_t hash = hashtable->compute_hash(key);
	uint32_t index = hash % hashtable->size_node;

	struct sl_hashtable_node * node = hashtable->nodes[index];
	while (node != NULL) {
		if (node->hash == hash)
			return node->value;
		node = node->next;
	}

	return mhv_null;
}

bool sl_hashtable_has_key(const struct sl_hashtable * hashtable, const void * key) {
	if (hashtable == NULL || key == NULL)
		return false;

	uint64_t hash = hashtable->compute_hash(key);
	uint32_t index = hash % hashtable->size_node;

	const struct sl_hashtable_node * node = hashtable->nodes[index];
	while (node != NULL) {
		if (node->hash == hash)
			return true;

		node = node->next;
	}

	return false;
}

const void ** sl_hashtable_keys(struct sl_hashtable * hashtable, uint32_t * nb_value) {
	if (hashtable == NULL)
		return NULL;

	const void ** keys = calloc(sizeof(void *), hashtable->nb_elements + 1);
	uint32_t i_node = 0, index = 0;

	while (i_node < hashtable->size_node) {
		struct sl_hashtable_node * node = hashtable->nodes[i_node];
		while (node != NULL) {
			keys[index] = node->key;
			index++;
			node = node->next;
		}
		i_node++;
	}
	keys[index] = 0;

	if (nb_value != NULL)
		*nb_value = hashtable->nb_elements;

	return keys;
}

struct sl_hashtable * sl_hashtable_new(sl_hashtable_compute_hash_f ch) {
	return sl_hashtable_new2(ch, NULL);
}

struct sl_hashtable * sl_hashtable_new2(sl_hashtable_compute_hash_f ch, sl_hashtable_free_f release) {
	if (ch == NULL)
		return NULL;

	struct sl_hashtable * l_hash = malloc(sizeof(struct sl_hashtable));

	l_hash->nodes = calloc(sizeof(struct sl_hashtable_node *), 16);
	l_hash->nb_elements = 0;
	l_hash->size_node = 16;
	l_hash->allow_rehash = 1;
	l_hash->compute_hash = ch;
	l_hash->release_key_value = release;

	return l_hash;
}

void sl_hashtable_put(struct sl_hashtable * hashtable, void * key, struct sl_hashtable_value value) {
	if (hashtable == NULL || key == NULL)
		return;

	uint64_t hash = hashtable->compute_hash(key);
	uint32_t index = hash % hashtable->size_node;

	struct sl_hashtable_node * new_node = malloc(sizeof(struct sl_hashtable_node));
	new_node->hash = hash;
	new_node->key = key;
	new_node->value = value;
	new_node->next = NULL;

	sl_hashtable_put2(hashtable, index, new_node);
	hashtable->nb_elements++;
}

static void sl_hashtable_put2(struct sl_hashtable * hashtable, unsigned int index, struct sl_hashtable_node * new_node) {
	struct sl_hashtable_node * node = hashtable->nodes[index];
	if (node != NULL) {
		if (node->hash == new_node->hash) {
			hashtable->nodes[index] = new_node;
			new_node->next = node->next;

			sl_hashtable_release_value(hashtable->release_key_value, node);
			hashtable->nb_elements--;
		} else {
			uint16_t nb_element = 1;
			while (node->next != NULL) {
				if (node->next->hash == new_node->hash) {
					struct sl_hashtable_node * old = node->next;
					node->next = new_node;
					new_node->next = old->next;

					sl_hashtable_release_value(hashtable->release_key_value, node);
					hashtable->nb_elements--;
					return;
				}
				node = node->next;
				nb_element++;
			}
			node->next = new_node;

			if (nb_element > 4)
				sl_hashtable_rehash(hashtable);
		}
	} else
		hashtable->nodes[index] = new_node;
}

static void sl_hashtable_rehash(struct sl_hashtable * hashtable) {
	if (!hashtable->allow_rehash)
		return;

	hashtable->allow_rehash = false;

	uint32_t old_size_node = hashtable->size_node;
	struct sl_hashtable_node ** old_nodes = hashtable->nodes;

	hashtable->size_node <<= 1;
	hashtable->nodes = calloc(sizeof(struct sl_hashtable_node *), hashtable->size_node);

	uint32_t i;
	for (i = 0; i < old_size_node; i++) {
		struct sl_hashtable_node * node = old_nodes[i];
		while (node != NULL) {
			struct sl_hashtable_node * current_node = node;
			node = node->next;
			current_node->next = NULL;

			uint32_t index = current_node->hash % hashtable->size_node;
			sl_hashtable_put2(hashtable, index, current_node);
		}
	}
	free(old_nodes);

	hashtable->allow_rehash = true;
}

static void sl_hashtable_release_value(sl_hashtable_free_f release, struct sl_hashtable_node * node) {
	if (release != NULL) {
		if (node->value.type == sl_hashtable_value_custom)
			release(node->key, node->value.value.custom);
		else if (node->value.type == sl_hashtable_value_string)
			release(node->key, node->value.value.string);
		else
			release(node->key, NULL);

		node->value.value.floating = 0;
	}

	node->key = node->next = NULL;
	free(node);
}

void sl_hashtable_remove(struct sl_hashtable * hashtable, const void * key) {
	if (hashtable == NULL || key == NULL)
		return;

	uint64_t hash = hashtable->compute_hash(key);
	uint32_t index = hash % hashtable->size_node;

	struct sl_hashtable_node * node = hashtable->nodes[index];
	if (node == NULL)
		return;

	if (node->hash == hash) {
		hashtable->nodes[index] = node->next;

		sl_hashtable_release_value(hashtable->release_key_value, node);
		hashtable->nb_elements--;
		return;
	}

	while (node->next != NULL) {
		if (node->next->hash == hash) {
			struct sl_hashtable_node * current_node = node->next;
			node->next = current_node->next;

			sl_hashtable_release_value(hashtable->release_key_value, current_node);
			hashtable->nb_elements--;
			return;
		}
		node->next = node->next->next;
	}

	return;
}

struct sl_hashtable_value * sl_hashtable_values(struct sl_hashtable * hashtable, uint32_t * nb_value) {
	if (hashtable == NULL)
		return NULL;

	struct sl_hashtable_value * values = calloc(sizeof(struct sl_hashtable_value), hashtable->nb_elements + 1);
	uint32_t i_node = 0, index = 0;
	while (i_node < hashtable->size_node) {
		struct sl_hashtable_node * node = hashtable->nodes[i_node];
		while (node != NULL) {
			values[index] = node->value;
			index++;
			node = node->next;
		}
		i_node++;
	}
	values[index] = mhv_null;

	if (nb_value != NULL)
		*nb_value = hashtable->nb_elements;

	return values;
}


void sl_hashtable_iterator_free(struct sl_hashtable_iterator * iterator) {
	free(iterator);
}

bool sl_hashtable_iterator_has_next(struct sl_hashtable_iterator * iterator) {
	if (iterator == NULL)
		return false;

	return iterator->current_node != NULL;
}

void * sl_hashtable_iterator_next(struct sl_hashtable_iterator * iterator) {
	if (iterator == NULL || iterator->current_node == NULL)
		return NULL;

	struct sl_hashtable_node * cn = iterator->current_node;
	if (cn == NULL)
		return NULL;

	void * key = cn->key;

	for (iterator->current_node = cn->next; iterator->index < iterator->hashtable->size_node && iterator->current_node == NULL; iterator->index++)
		iterator->current_node = iterator->hashtable->nodes[iterator->index];

	return key;
}

struct sl_hashtable_iterator * sl_hashtable_iterator_new(struct sl_hashtable * hashtable) {
	if (hashtable == NULL)
		return NULL;

	struct sl_hashtable_iterator * iter = malloc(sizeof(struct sl_hashtable_iterator));
	iter->hashtable = hashtable;
	iter->current_node = NULL;

	for (iter->index = 0; iter->index < hashtable->size_node && iter->current_node == NULL; iter->index++)
		iter->current_node = hashtable->nodes[iter->index];

	return iter;
}


/*
 * Copyright 2011 Tom Stellard <tstellar@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef RADEON_LIST_H
#define RADEON_LIST_H

struct memory_pool;

struct rc_list {
	void * Item;
	struct rc_list * Prev;
	struct rc_list * Next;
};

struct rc_list * rc_list(struct memory_pool * pool, void * item);
void rc_list_add(struct rc_list ** list, struct rc_list * new_value);
void rc_list_remove(struct rc_list ** list, struct rc_list * rm_value);
unsigned int rc_list_count(struct rc_list * list);
void rc_list_print(struct rc_list * list);

#endif /* RADEON_LIST_H */


/*
 * Copyright 2011 Tom Stellard <tstellar@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "radeon_list.h"

#include <stdlib.h>
#include <stdio.h>

#include "memory_pool.h"

struct rc_list * rc_list(struct memory_pool * pool, void * item)
{
	struct rc_list * new = memory_pool_malloc(pool, sizeof(struct rc_list));
	new->Item = item;
	new->Next = NULL;
	new->Prev = NULL;

	return new;
}

void rc_list_add(struct rc_list ** list, struct rc_list * new_value)
{
	struct rc_list * temp;

	if (*list == NULL) {
		*list = new_value;
		return;
	}

	for (temp = *list; temp->Next; temp = temp->Next);

	temp->Next = new_value;
	new_value->Prev = temp;
}

void rc_list_remove(struct rc_list ** list, struct rc_list * rm_value)
{
	if (*list == rm_value) {
		*list = rm_value->Next;
		return;
	}

	rm_value->Prev->Next = rm_value->Next;
	if (rm_value->Next) {
		rm_value->Next->Prev = rm_value->Prev;
	}
}

unsigned int rc_list_count(struct rc_list * list)
{
	unsigned int count = 0;
	while (list) {
		count++;
		list = list->Next;
	}
	return count;
}

void rc_list_print(struct rc_list * list)
{
	while(list) {
		fprintf(stderr, "%p->", list->Item);
		list = list->Next;
	}
	fprintf(stderr, "\n");
}

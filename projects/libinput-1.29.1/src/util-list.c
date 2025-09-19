/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2013-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "util-list.h"

#include "libinput-util.h"

#if 0
static void
list_debug(struct list *list)
{
	struct list *head = list;

	assert((head->next != NULL && head->prev != NULL) ||
	       !"list->next|prev is NULL, possibly missing list_init()");

	trace("list %p:", head);
	trace("  head->next %p", head->next);
	trace("  head->prev %p", head->prev);
	struct list *elm = head->next;
	int index = 0;
	while (elm != head) {
		trace("..%-3d: %p->next %p",
		      index, elm, elm->next);
		trace("..%-3d: %p->prev %p",
		      index, elm, elm->prev);
		elm = elm->next;
		index++;
		if (index > 20)
			break;
	}
}
#endif

void
list_init(struct list *list)
{
	*list = LIST_INIT(*list);
}

void
list_insert(struct list *list, struct list *elm)
{
	assert((list->next != NULL && list->prev != NULL) ||
	       !"list->next|prev is NULL, possibly missing list_init()");
	assert(((elm->next == NULL && elm->prev == NULL) || list_empty(elm)) ||
	       !"elm->next|prev is not NULL, list node used twice?");

	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

void
list_append(struct list *list, struct list *elm)
{
	assert((list->next != NULL && list->prev != NULL) ||
	       !"list->next|prev is NULL, possibly missing list_init()");
	assert(((elm->next == NULL && elm->prev == NULL) || list_empty(elm)) ||
	       !"elm->next|prev is not NULL, list node used twice?");

	elm->next = list;
	elm->prev = list->prev;
	list->prev = elm;
	elm->prev->next = elm;
}

void
list_chain(struct list *list, struct list *other)
{
	assert((list->next != NULL && list->prev != NULL) ||
	       !"list->next|prev is NULL, possibly missing list_init()");
	assert((other->next != NULL && other->prev != NULL) ||
	       !"other->next|prev is NULL, possibly missing list_init()");

	if (list_empty(other))
		return;

	struct list *last = list->prev;
	struct list *first = other->next;

	first->prev = last;
	last->next = first;

	list->prev->next = other->next;
	other->next->prev = list->prev;
	other->prev->next = list;
	list->prev = other->prev;
	list_init(other);
}

size_t
list_length(const struct list *list)
{
	assert((list->next != NULL && list->prev != NULL) ||
	       !"list->next|prev is NULL, possibly missing list_init()");

	size_t count = 0;
	const struct list *elm;

	for (elm = list->next; elm != list; elm = elm->next)
		count++;

	return count;
}

void
list_remove(struct list *elm)
{
	assert((elm->next != NULL && elm->prev != NULL) ||
	       !"list->next|prev is NULL, possibly missing list_init()");

	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->next = NULL;
	elm->prev = NULL;
}

bool
list_empty(const struct list *list)
{
	assert((list->next != NULL && list->prev != NULL) ||
	       !"list->next|prev is NULL, possibly missing list_init()");

	return list->next == list;
}

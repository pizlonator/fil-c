/*
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: iter
 * @title: Iterator
 * @short_description: unified iterator
 *
 * The iterator keeps the direction and the last position for access to the
 * internal library tables/lists.
 *
 * It's very unusual to use the same iterator on multiple places in your
 * application or share the same iterator, for this purpose libfdisk does not
 * provide reference counting for this object. It's recommended to initialize
 * the iterator by fdisk_new_iter() at begin of your function and then
 * fdisk_free_iter() before you return from the function.
 *
 * Don't forget to call fdisk_reset_iter() if you want to use the iterator more
 * than once.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fdiskP.h"

/**
 * fdisk_new_iter:
 * @direction: FDISK_INTER_{FOR,BACK}WARD direction
 *
 * Returns: newly allocated generic libmount iterator.
 */
struct fdisk_iter *fdisk_new_iter(int direction)
{
	struct fdisk_iter *itr = calloc(1, sizeof(*itr));
	if (!itr)
		return NULL;
	itr->direction = direction;
	return itr;
}

/**
 * fdisk_free_iter:
 * @itr: iterator pointer
 *
 * Deallocates the iterator.
 */
void fdisk_free_iter(struct fdisk_iter *itr)
{
	free(itr);
}

/**
 * fdisk_reset_iter:
 * @itr: iterator pointer
 * @direction: FDISK_ITER_{FOR,BACK}WARD or -1 to keep the direction unchanged
 *
 * Resets the iterator.
 */
void fdisk_reset_iter(struct fdisk_iter *itr, int direction)
{
	if (direction == -1)
		direction = itr->direction;

	memset(itr, 0, sizeof(*itr));
	itr->direction = direction;
}

/**
 * fdisk_iter_get_direction:
 * @itr: iterator pointer
 *
 * Returns: FDISK_INTER_{FOR,BACK}WARD
 */
int fdisk_iter_get_direction(struct fdisk_iter *itr)
{
	return itr->direction;
}

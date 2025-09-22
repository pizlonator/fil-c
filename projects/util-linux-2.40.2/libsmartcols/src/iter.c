/*
 * Copyright (C) 2009-2014 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2014 Ondrej Oprala <ooprala@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: iter
 * @title: Iterator
 * @short_description: unified iterator
 *
 * The iterator keeps the direction and the last position
 * for access to the internal library tables/lists.
 */

#include <string.h>
#include <stdlib.h>

#include "smartcolsP.h"

/**
 * scols_new_iter:
 * @direction: SCOLS_INTER_{FOR,BACK}WARD direction
 *
 * Returns: newly allocated generic libmount iterator.
 */
struct libscols_iter *scols_new_iter(int direction)
{
	struct libscols_iter *itr = calloc(1, sizeof(*itr));
	if (!itr)
		return NULL;
	itr->direction = direction;
	return itr;
}

/**
 * scols_free_iter:
 * @itr: iterator pointer
 *
 * Deallocates the iterator.
 */
void scols_free_iter(struct libscols_iter *itr)
{
	free(itr);
}

/**
 * scols_reset_iter:
 * @itr: iterator pointer
 * @direction: SCOLS_INTER_{FOR,BACK}WARD or -1 to keep the direction unchanged
 *
 * Resets the iterator.
 */
void scols_reset_iter(struct libscols_iter *itr, int direction)
{
	if (direction == -1)
		direction = itr->direction;

	memset(itr, 0, sizeof(*itr));
	itr->direction = direction;
}

/**
 * scols_iter_get_direction:
 * @itr: iterator pointer
 *
 * Returns: SCOLS_INTER_{FOR,BACK}WARD
 */
int scols_iter_get_direction(const struct libscols_iter *itr)
{
	return itr->direction;
}

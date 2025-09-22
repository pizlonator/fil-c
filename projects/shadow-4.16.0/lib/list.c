/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <assert.h>

#include "alloc.h"
#include "prototypes.h"
#include "defines.h"
/*
 * add_list - add a member to a list of group members
 *
 *	the array of member names is searched for the new member
 *	name, and if not present it is added to a freshly allocated
 *	list of users.
 */
/*@only@*/char **
add_list(/*@returned@*/ /*@only@*/char **list, const char *member)
{
	int i;
	char **tmp;

	assert (NULL != member);
	assert (NULL != list);

	/*
	 * Scan the list for the new name.  Return the original list
	 * pointer if it is present.
	 */

	for (i = 0; list[i] != NULL; i++) {
		if (strcmp (list[i], member) == 0) {
			return list;
		}
	}

	/*
	 * Allocate a new list pointer large enough to hold all the
	 * old entries, and the new entries as well.
	 */

	tmp = XMALLOC(i + 2, char *);

	/*
	 * Copy the original list to the new list, then append the
	 * new member and NULL terminate the result.  This new list
	 * is returned to the invoker.
	 */

	for (i = 0; list[i] != NULL; i++) {
		tmp[i] = list[i];
	}

	tmp[i] = xstrdup (member);
	tmp[i+1] = NULL;

	return tmp;
}

/*
 * del_list - delete a member from a list of group members
 *
 *	the array of member names is searched for the old member
 *	name, and if present it is deleted from a freshly allocated
 *	list of users.
 */

/*@only@*/char **
del_list(/*@returned@*/ /*@only@*/char **list, const char *member)
{
	int i, j;
	char **tmp;

	assert (NULL != member);
	assert (NULL != list);

	/*
	 * Scan the list for the old name.  Return the original list
	 * pointer if it is not present.
	 */

	for (i = j = 0; list[i] != NULL; i++) {
		if (strcmp (list[i], member) != 0) {
			j++;
		}
	}

	if (j == i) {
		return list;
	}

	/*
	 * Allocate a new list pointer large enough to hold all the
	 * old entries.
	 */

	tmp = XMALLOC(j + 1, char *);

	/*
	 * Copy the original list except the deleted members to the
	 * new list, then NULL terminate the result.  This new list
	 * is returned to the invoker.
	 */

	for (i = j = 0; list[i] != NULL; i++) {
		if (strcmp (list[i], member) != 0) {
			tmp[j] = list[i];
			j++;
		}
	}

	tmp[j] = NULL;

	return tmp;
}

/*
 * Duplicate a list.
 * The input list is not modified, but in order to allow the use of this
 * function with list of members, the list elements are not enforced to be
 * constant strings here.
 */
/*@only@*/char **
dup_list(char *const *list)
{
	int i;
	char **tmp;

	assert (NULL != list);

	for (i = 0; NULL != list[i]; i++);

	tmp = XMALLOC(i + 1, char *);

	i = 0;
	while (NULL != *list) {
		tmp[i] = xstrdup (*list);
		i++;
		list++;
	}

	tmp[i] = NULL;
	return tmp;
}

/*
 * Check if member is part of the input list
 * The input list is not modified, but in order to allow the use of this
 * function with list of members, the list elements are not enforced to be
 * constant strings here.
 */
bool is_on_list (char *const *list, const char *member)
{
	assert (NULL != member);
	assert (NULL != list);

	while (NULL != *list) {
		if (strcmp (*list, member) == 0) {
			return true;
		}
		list++;
	}

	return false;
}

/*
 * comma_to_list - convert comma-separated list to (char *) array
 */

/*@only@*/char **comma_to_list (const char *comma)
{
	char *members;
	char **array;
	int i;
	char *cp;
	char *cp2;

	assert (NULL != comma);

	/*
	 * Make a copy since we are going to be modifying the list
	 */

	members = xstrdup (comma);

	/*
	 * Count the number of commas in the list
	 */

	for (cp = members, i = 0;; i++) {
		cp2 = strchr (cp, ',');
		if (NULL != cp2) {
			cp = cp2 + 1;
		} else {
			break;
		}
	}

	/*
	 * Add 2 - one for the ending NULL, the other for the last item
	 */

	i += 2;

	/*
	 * Allocate the array we're going to store the pointers into.
	 */

	array = XMALLOC(i, char *);

	/*
	 * Empty list is special - 0 members, not 1 empty member.  --marekm
	 */

	if ('\0' == *members) {
		*array = NULL;
		free (members);
		return array;
	}

	/*
	 * Now go walk that list all over again, this time building the
	 * array of pointers.
	 */

	for (cp = members, i = 0; cp != NULL; i++)
		array[i] = strsep(&cp, ",");
	array[i] = NULL;

	/*
	 * Return the new array of pointers
	 */

	return array;
}


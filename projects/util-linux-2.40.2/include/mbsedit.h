/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This file may be distributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef UTIL_LINUX_MBSEDIT_H
# define UTIL_LINUX_MBSEDIT_H

#include "mbsalign.h"
#include "widechar.h"

struct mbs_editor {
	char	*buf;		/* buffer */
	size_t	max_bytes;	/* size of the buffer */
	size_t	max_cells;	/* maximal allowed number of cells */
	size_t	cur_cells;	/* number of cells to print the buffer */
	size_t  cur_bytes;	/* number of chars in bytes */
	size_t  cursor;		/* cursor position in bytes */
	size_t  cursor_cells;	/* cursor position in cells */
};

enum {
	MBS_EDIT_LEFT,
	MBS_EDIT_RIGHT,
	MBS_EDIT_END,
	MBS_EDIT_HOME
};

struct mbs_editor *mbs_new_edit(char *buf, size_t bufsz, size_t ncells);
char *mbs_free_edit(struct mbs_editor *edit);

int mbs_edit_goto(struct mbs_editor *edit, int where);
int mbs_edit_delete(struct mbs_editor *edit);
int mbs_edit_backspace(struct mbs_editor *edit);
int mbs_edit_insert(struct mbs_editor *edit, wint_t c);

#endif /* UTIL_LINUX_MBSEDIT_H */

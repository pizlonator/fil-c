/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Very simple multibyte buffer editor. Allows to maintaine the current
 * position in the string, add and remove chars on the current position.
 *
 * This file may be distributed under the terms of the
 * GNU Lesser General Public License.
 *
 * Copyright (C) 2017 Karel Zak <kzak@redhat.com>
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "mbsalign.h"
#include "mbsedit.h"

struct mbs_editor *mbs_new_edit(char *buf, size_t bufsz, size_t ncells)
{
	struct mbs_editor *edit = calloc(1, sizeof(*edit));

	if (edit) {
		edit->buf = buf;
		edit->max_bytes = bufsz;
		edit->max_cells = ncells;
		edit->cur_cells = mbs_safe_width(buf);
		edit->cur_bytes = strlen(buf);
	}
	return edit;
}

char *mbs_free_edit(struct mbs_editor *edit)
{
	char *ret = edit ? edit->buf : NULL;

	free(edit);
	return ret;
}

static size_t mbs_next(const char *str, size_t *ncells)
{
#ifdef HAVE_WIDECHAR
	wchar_t wc;
	size_t n = 0;

	if (!str || !*str)
		return 0;

	n = mbrtowc(&wc, str, MB_CUR_MAX, NULL);
	*ncells = wcwidth(wc);
	return n;
#else
	if (!str || !*str)
		return 0;
	*ncells = 1;
	return 1;
#endif
}

static size_t mbs_prev(const char *start, const char *end, size_t *ncells)
{
#ifdef HAVE_WIDECHAR
	wchar_t wc = 0;
	const char *p, *prev;
	size_t n = 0;

	if (!start || !end || start == end || !*start)
		return 0;

	prev = p = start;
	while (p < end) {
		n = mbrtowc(&wc, p, MB_CUR_MAX, NULL);
		prev = p;

		if (n == (size_t) -1 || n == (size_t) -2)
			p++;
		else
			p += n;
	}

	if (prev == end)
		return 0;
	*ncells = wcwidth(wc);
	return n;
#else
	if (!start || !end || start == end || !*start)
		return 0;
	*ncells = 1;
	return 1;
#endif
}

int mbs_edit_goto(struct mbs_editor *edit, int where)
{
	switch (where) {
	case MBS_EDIT_LEFT:
		if (edit->cursor == 0)
			return 1;
		else {
			size_t n, cells;
			n = mbs_prev(edit->buf, edit->buf + edit->cursor, &cells);
			if (n) {
				edit->cursor -= n;
				edit->cursor_cells -= cells;
			}
		}
		break;
	case MBS_EDIT_RIGHT:
		if (edit->cursor_cells >= edit->cur_cells)
			return 1;
		else {
			size_t n, cells;
			n = mbs_next(edit->buf + edit->cursor, &cells);
			if (n) {
				edit->cursor += n;
				edit->cursor_cells += cells;
			}
		}
		break;
	case MBS_EDIT_HOME:
		edit->cursor = 0;
		edit->cursor_cells = 0;
		break;
	case MBS_EDIT_END:
		edit->cursor = edit->cur_bytes;
		edit->cursor_cells = edit->cur_cells;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Remove next MB from @str, returns number of removed bytes */
static size_t remove_next(char *str, size_t *ncells)
{
	/* all in bytes! */
	size_t bytes, move_bytes, n;

	n          = mbs_next(str, ncells);
	bytes      = strlen(str);
	move_bytes = bytes - n;

	memmove(str, str + n, move_bytes);
	str[bytes - n] = '\0';
	return n;
}

static size_t mbs_insert(char *str, wint_t c, size_t *ncells)
{
	/* all in bytes! */
	size_t n = 1, bytes;
	char *in;

#ifdef HAVE_WIDECHAR
	wchar_t wc = (wchar_t) c;
	in = malloc(MB_CUR_MAX);
	if (!in)
		return -1;

	n = wctomb(in, wc);
	if (n == (size_t) -1)
		goto out;
	*ncells = wcwidth(wc);
#else
	*ncells = 1;
	in = (char *) &c;
#endif
	bytes       = strlen(str);

	memmove(str + n, str, bytes);
	memcpy(str, in, n);
	str[bytes + n] = '\0';
out:
#ifdef HAVE_WIDECHAR
	free(in);
#endif
	return n;
}

static int mbs_edit_remove(struct mbs_editor *edit)
{
	size_t n, ncells;

	if (edit->cur_cells == 0 || edit->cursor >= edit->cur_bytes)
		return 1;

	n = remove_next(edit->buf + edit->cursor, &ncells);
	if (n == (size_t)-1)
		return 1;

	edit->cur_bytes -= n;
	edit->cur_cells = mbs_safe_width(edit->buf);
	return 0;
}

int mbs_edit_delete(struct mbs_editor *edit)
{
	if (edit->cursor >= edit->cur_bytes
	    && mbs_edit_goto(edit, MBS_EDIT_LEFT) == 1)
		return 1;

	return mbs_edit_remove(edit);
}

int mbs_edit_backspace(struct mbs_editor *edit)
{
	if (mbs_edit_goto(edit, MBS_EDIT_LEFT) == 0)
		return mbs_edit_remove(edit);
	return 1;
}

int mbs_edit_insert(struct mbs_editor *edit, wint_t c)
{
	size_t n, ncells;

	if (edit->cur_bytes + MB_CUR_MAX > edit->max_bytes)
		return 1;

	n = mbs_insert(edit->buf + edit->cursor, c, &ncells);
	if (n == (size_t)-1)
		return 1;

	edit->cursor += n;
	edit->cursor_cells += ncells;
	edit->cur_bytes += n;
	edit->cur_cells = mbs_safe_width(edit->buf);
	return 0;
}

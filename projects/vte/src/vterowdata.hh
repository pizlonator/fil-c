/*
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

/* The interfaces in this file are subject to change at any time. */

#pragma once

#include <string.h>

#include "vteunistr.h"
#include "vtemacros.h"
#include "vtedefines.hh"

#include "attr.hh"
#include "cell.hh"

G_BEGIN_DECLS

/*
 * VteRowAttr: A single row's attributes
 */

typedef struct _VteRowAttr {
        guint8 soft_wrapped  : 1;
        guint8 bidi_flags    : 4;
} VteRowAttr;
static_assert(sizeof (VteRowAttr) == 1, "VteRowAttr has wrong size");

/*
 * VteRowData: A single row's data
 */

typedef struct _VteRowData {
	VteCell *cells;
	guint16 len;
	VteRowAttr attr;
} VteRowData;


#define _vte_row_data_length(__row)			((__row)->len + 0)

static inline const VteCell *
_vte_row_data_get (const VteRowData *row, gulong col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->cells[col];
}

static inline VteCell *
_vte_row_data_get_writable (VteRowData *row, gulong col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->cells[col];
}

void _vte_row_data_init (VteRowData *row);
void _vte_row_data_clear (VteRowData *row);
void _vte_row_data_fini (VteRowData *row);
void _vte_row_data_insert (VteRowData *row, gulong col, const VteCell *cell);
void _vte_row_data_append (VteRowData *row, const VteCell *cell);
void _vte_row_data_remove (VteRowData *row, gulong col);
void _vte_row_data_fill (VteRowData *row, const VteCell *cell, gulong len);
void _vte_row_data_expand (VteRowData *row, gulong len);
void _vte_row_data_shrink (VteRowData *row, gulong max_len);
void _vte_row_data_copy (const VteRowData *src, VteRowData *dst);
void _vte_row_data_fill_cells(VteRowData* row,
                              gulong start_idx,
                              VteCell const* fill_cell, // for filling
                              VteCell const* cells,
                              gulong len);
bool _vte_row_data_ensure_len (VteRowData* row,
                               gulong len);

guint16 _vte_row_data_nonempty_length (const VteRowData *row);

G_END_DECLS

/*
 * Copyright (C) 2002,2009 Red Hat, Inc.
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
 *
 * Red Hat Author(s): Nalin Dahyabhai, Behdad Esfahbod
 */

#include <config.h>

#include "debug.h"
#include "vterowdata.hh"

#include <string.h>

#include <algorithm>
#include <iterator>
#include <type_traits>

/* This will be true now that VteCell is POD, but make sure it'll be true
 * once that changes.
 */
static_assert(std::is_trivially_copy_constructible<VteCell>::value, "VteCell is not copy constructible");
static_assert(std::is_trivially_move_constructible<VteCell>::value, "VteCell is not move constructible");
static_assert(std::is_trivially_copyable<VteCell>::value, "VteCell is not trivially copyable");
static_assert(std::is_trivially_copy_assignable<VteCell>::value, "VteCell is not copy assignable");
static_assert(std::is_trivially_move_assignable<VteCell>::value, "VteCell is not move assignable");

/*
 * VteCells: A row's cell array
 */

typedef struct _VteCells VteCells;
struct _VteCells {
	guint32 alloc_len;
	VteCell cells[1];
};

static inline VteCells *
_vte_cells_for_cell_array (VteCell *cells)
{
	if (G_UNLIKELY (!cells))
		return NULL;

	return reinterpret_cast<VteCells*>(((guchar *) cells) - G_STRUCT_OFFSET (VteCells, cells));
}

static VteCells *
_vte_cells_realloc (VteCells *cells, guint32 len)
{
	guint32 alloc_len = (1 << g_bit_storage (MAX (len, 80))) - 1;

	_vte_debug_print(VTE_DEBUG_RING, "Enlarging cell array of %d cells to %d cells\n", cells ? cells->alloc_len : 0, alloc_len);
	cells = (VteCells *)g_realloc (cells, G_STRUCT_OFFSET (VteCells, cells) + alloc_len * sizeof (cells->cells[0]));
	cells->alloc_len = alloc_len;

	return cells;
}

static void
_vte_cells_free (VteCells *cells)
{
	_vte_debug_print(VTE_DEBUG_RING, "Freeing cell array of %d cells\n", cells->alloc_len);
	g_free (cells);
}


/*
 * VteRowData: A row's data
 */

void
_vte_row_data_init (VteRowData *row)
{
	memset (row, 0, sizeof (*row));
}

void
_vte_row_data_clear (VteRowData *row)
{
	VteCell *cells = row->cells;
	_vte_row_data_init (row);
	row->cells = cells;
}

void
_vte_row_data_fini (VteRowData *row)
{
	if (row->cells)
		_vte_cells_free (_vte_cells_for_cell_array (row->cells));
	row->cells = NULL;
}

static inline bool
_vte_row_data_ensure (VteRowData *row, gulong len)
{
	VteCells *cells = _vte_cells_for_cell_array (row->cells);
	if (G_LIKELY (cells && len <= cells->alloc_len))
		return TRUE;

	if (G_UNLIKELY (len >= 0xFFFF))
		return FALSE;

	row->cells = _vte_cells_realloc (cells, len)->cells;

	return TRUE;
}

bool
_vte_row_data_ensure_len (VteRowData *row, gulong len)
{
        return _vte_row_data_ensure(row, len);
}

void
_vte_row_data_insert (VteRowData *row, gulong col, const VteCell *cell)
{
	gulong i;

	if (G_UNLIKELY (!_vte_row_data_ensure (row, row->len + 1)))
		return;

	for (i = row->len; i > col; i--)
		row->cells[i] = row->cells[i - 1];

	row->cells[col] = *cell;
	row->len++;
}

void _vte_row_data_append (VteRowData *row, const VteCell *cell)
{
	if (G_UNLIKELY (!_vte_row_data_ensure (row, row->len + 1)))
		return;

	row->cells[row->len] = *cell;
	row->len++;
}

void _vte_row_data_remove (VteRowData *row, gulong col)
{
	gulong i;

	for (i = col + 1; i < row->len; i++)
		row->cells[i - 1] = row->cells[i];

	if (G_LIKELY (row->len))
		row->len--;
}

void _vte_row_data_fill (VteRowData *row, const VteCell *cell, gulong len)
{
	if (row->len < len) {
		if (G_UNLIKELY (!_vte_row_data_ensure (row, len)))
			return;

		std::fill_n(&row->cells[row->len], len - row->len, *cell);
		row->len = len;
	}
}

/* This leaves the new cells uninitialized, potentially containing random data.
 * It's the caller's responsibility to initialize them. */
void _vte_row_data_expand (VteRowData *row, gulong len)
{
	if (row->len < len) {
		if (G_UNLIKELY (!_vte_row_data_ensure (row, len)))
			return;

		row->len = len;
	}
}

void _vte_row_data_shrink (VteRowData *row, gulong max_len)
{
	if (max_len < row->len)
		row->len = max_len;
}

void _vte_row_data_copy (const VteRowData *src, VteRowData *dst)
{
        _vte_row_data_ensure (dst, src->len);
        dst->len = src->len;
        dst->attr = src->attr;
        memcpy(dst->cells, src->cells, src->len * sizeof (src->cells[0]));
}

void _vte_row_data_fill_cells(VteRowData* row,
                              gulong start_idx,
                              VteCell const* fill_cell,
                              VteCell const* cells,
                              gulong len)
{
        auto const needlen = start_idx + len;
        if (!_vte_row_data_ensure(row, needlen))
                return;

        // Fill up to start_idx with @fill_cell
        _vte_row_data_fill(row, fill_cell, start_idx);
        // ... then copy the cells over ...
        std::copy_n(cells, len, &row->cells[start_idx]);
        // ... and adjust the row length
        if (row->len < needlen)
                row->len = needlen;
}

/* Get the length, ignoring trailing empty cells (with a custom background color). */
guint16 _vte_row_data_nonempty_length (const VteRowData *row)
{
        guint16 len;
        const VteCell *cell;
        for (len = row->len; len > 0; len--) {
                cell = &row->cells[len - 1];
                if (cell->attr.fragment() || cell->c != 0)
                        break;
        }
        return len;
}


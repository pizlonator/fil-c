/*
 * Copyright © 2018–2019 Egmont Koblinger
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

#include <config.h>

#include "bidi.hh"
#include "debug.h"
#include "vtedefines.hh"
#include "vteinternal.hh"

using namespace vte::base;

RingView::RingView()
{
        m_bidirunner = std::make_unique<BidiRunner>(this);
}

RingView::~RingView()
{
        pause();
}

/* Pausing a RingView frees up pretty much all of its memory.
 *
 * This is to be used when the terminal is unlikely to be painted or interacted with
 * in the near future, e.g. the widget is unmapped. Not to be called too frequently,
 * in order to avoid memory fragmentation.
 *
 * The RingView is resumed automatically on demand.
 */
void
RingView::pause()
{
        int i;

        if (m_paused)
                return;

        _vte_debug_print (VTE_DEBUG_RINGVIEW, "Ringview: pause, freeing %d rows, %d bidirows.\n",
                                              m_rows_alloc_len, m_bidirows_alloc_len);

        for (i = 0; i < m_rows_alloc_len; i++) {
                _vte_row_data_fini(m_rows[i]);
                g_free (m_rows[i]);
        }
        g_free (m_rows);
        m_rows_alloc_len = 0;

        for (i = 0; i < m_bidirows_alloc_len; i++) {
                delete m_bidirows[i];
        }
        g_free (m_bidirows);
        m_bidirows_alloc_len = 0;

        m_invalid = true;
        m_paused = true;
}

/* Allocate (again) the required memory. */
void
RingView::resume()
{
        vte_assert_cmpint (m_len, >=, 1);

        /* +16: A bit of arbitrary heuristics to likely prevent a quickly following
         * realloc for the required context lines. */
        m_rows_alloc_len = m_len + 16;
        m_rows = (VteRowData **) g_malloc (sizeof (VteRowData *) * m_rows_alloc_len);
        for (int i = 0; i < m_rows_alloc_len; i++) {
                m_rows[i] = (VteRowData *) g_malloc (sizeof (VteRowData));
                _vte_row_data_init (m_rows[i]);
        }

        /* +2: Likely prevent a quickly following realloc.
         * The number of lines of interest keeps jumping up and down by one
         * due to per-pixel scrolling, and by another one due sometimes having
         * to reshuffle another line below the bottom for the overflowing bits
         * of the outline rectangle cursor. */
        m_bidirows_alloc_len = m_len + 2;
        m_bidirows = (BidiRow **) g_malloc (sizeof (BidiRow *) * m_bidirows_alloc_len);
        for (int i = 0; i < m_bidirows_alloc_len; i++) {
                m_bidirows[i] = new BidiRow();
        }

        _vte_debug_print (VTE_DEBUG_RINGVIEW, "Ringview: resume, allocating %d rows, %d bidirows\n",
                                              m_rows_alloc_len, m_bidirows_alloc_len);

        m_paused = false;
}

void
RingView::set_ring(Ring *ring)
{
        if (G_LIKELY (ring == m_ring))
                return;

        m_ring = ring;
        m_invalid = true;
}

void
RingView::set_width(vte::grid::column_t width)
{
        if (G_LIKELY (width == m_width))
                return;

        m_width = width;
        m_invalid = true;
}

void
RingView::set_rows(vte::grid::row_t start, vte::grid::row_t len)
{
        /* Force at least 1 row, see bug 134. */
        len = MAX(len, 1);

        if (start == m_start && len == m_len)
                return;

        /* With per-pixel scrolling, the desired viewport often shrinks by
         * one row at one end, and remains the same at the other end.
         * Save work by just keeping the current valid data in this case. */
        if (!m_invalid && start >= m_start && start + len <= m_start + m_len)
                return;

        /* m_rows is expanded on demand in update() */

        /* m_bidirows needs exactly this many lines */
        if (G_UNLIKELY (!m_paused && len > m_bidirows_alloc_len)) {
                int i = m_bidirows_alloc_len;
                while (len > m_bidirows_alloc_len) {
                        /* Don't realloc too aggressively. */
                        m_bidirows_alloc_len = std::max(m_bidirows_alloc_len + 1, m_bidirows_alloc_len * 5 / 4 /* whatever */);
                }
                _vte_debug_print (VTE_DEBUG_RINGVIEW, "Ringview: reallocate to %d bidirows\n",
                                                      m_bidirows_alloc_len);
                m_bidirows = (BidiRow **) g_realloc (m_bidirows, sizeof (BidiRow *) * m_bidirows_alloc_len);
                for (; i < m_bidirows_alloc_len; i++) {
                        m_bidirows[i] = new BidiRow();
                }
        }

        m_start = start;
        m_len = len;
        m_invalid = true;
}

VteRowData const*
RingView::get_row(vte::grid::row_t row) const
{
        vte_assert_cmpint(row, >=, m_top);
        vte_assert_cmpint(row, <, m_top + m_rows_len);

        return m_rows[row - m_top];
}

void
RingView::set_enable_bidi(bool enable_bidi)
{
        if (G_LIKELY (enable_bidi == m_enable_bidi))
                return;

        m_enable_bidi = enable_bidi;
        m_invalid = true;
}

void
RingView::set_enable_shaping(bool enable_shaping)
{
        if (G_LIKELY (enable_shaping == m_enable_shaping))
                return;

        m_enable_shaping = enable_shaping;
        m_invalid = true;
}

void
RingView::update()
{
        if (!m_invalid)
                return;
        if (m_paused)
                resume();

        /* Find the beginning of the topmost paragraph.
         *
         * Extract at most VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX context rows.
         * If this safety limit is reached then together with the first
         * non-context row this paragraph fragment is already longer
         * than VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX lines, and thus the
         * BiDi code will skip it. */
        vte::grid::row_t row = m_start;
        const VteRowData *row_data;

        _vte_debug_print (VTE_DEBUG_RINGVIEW, "Ringview: updating for [%ld..%ld] (%ld rows).\n",
                                              m_start, m_start + m_len - 1, m_len);

        int i = VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX;
        while (i--) {
                if (!m_ring->is_soft_wrapped(row - 1))
                        break;
                row--;
        }

        /* Extract the data beginning at the found row.
         *
         * Extract at most VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX rows
         * beyond the end of the specified area. Again, if this safety
         * limit is reached then together with the last non-context row
         * this paragraph fragment is already longer than
         * VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX lines, and thus the
         * BiDi code will skip it. */
        m_top = row;
        m_rows_len = 0;
        while (row < m_start + m_len + VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX) {
                if (G_UNLIKELY (m_rows_len == m_rows_alloc_len)) {
                        /* Don't realloc too aggressively. */
                        m_rows_alloc_len = std::max(m_rows_alloc_len + 1, m_rows_alloc_len * 5 / 4 /* whatever */);
                        _vte_debug_print (VTE_DEBUG_RINGVIEW, "Ringview: reallocate to %d rows\n",
                                                              m_rows_alloc_len);
                        m_rows = (VteRowData **) g_realloc (m_rows, sizeof (VteRowData *) * m_rows_alloc_len);
                        for (int j = m_rows_len; j < m_rows_alloc_len; j++) {
                                m_rows[j] = (VteRowData *) g_malloc (sizeof (VteRowData));
                                _vte_row_data_init (m_rows[j]);
                        }
                }

                row_data = m_ring->contains(row) ? m_ring->index(row) : nullptr;
                if (G_LIKELY (row_data != nullptr)) {
                        _vte_row_data_copy (row_data, m_rows[m_rows_len]);
                        /* Make sure that the extracted data is not wider than the screen,
                         * something that can happen if the window was narrowed with rewrapping disabled.
                         * Also make sure that we won't end up with unfinished characters.
                         * FIXME remove this once bug 135 is addressed. */
                        if (G_UNLIKELY (_vte_row_data_length(m_rows[m_rows_len]) > m_width)) {
                                int j = m_width;
                                while (j > 0) {
                                        VteCell const* cell = _vte_row_data_get(m_rows[m_rows_len], j);
                                        if (!cell->attr.fragment())
                                                break;
                                        j--;
                                }
                                _vte_row_data_shrink(m_rows[m_rows_len], j);
                        }
                } else {
                        _vte_row_data_clear (m_rows[m_rows_len]);
                }
                m_rows_len++;
                row++;

                /* Once the bottom of the specified area is reached, stop at a hard newline. */
                if (row >= m_start + m_len && (!row_data || !row_data->attr.soft_wrapped))
                        break;
        }

        _vte_debug_print (VTE_DEBUG_RINGVIEW, "Ringview: extracted %ld+%ld context lines: [%ld..%ld] (%d rows).\n",
                                              m_start - m_top, (m_top + m_rows_len) - (m_start + m_len),
                                              m_top, m_top + m_rows_len - 1, m_rows_len);

        /* Loop through paragraphs of the extracted text, and do whatever we need to do on each paragraph. */
        auto top = m_top;
        row = top;
        while (row < m_top + m_rows_len) {
                row_data = m_rows[row - m_top];
                if (!row_data->attr.soft_wrapped || row == m_top + m_rows_len - 1) {
                        /* Found a paragraph from @top to @row, inclusive. */

                        /* Run the BiDi algorithm. */
                        m_bidirunner->paragraph(top, row + 1,
                                                m_enable_bidi, m_enable_shaping);

                        /* Doing syntax highlighting etc. come here in the future. */

                        top = row + 1;
                }
                row++;
        }

        m_invalid = false;
}

/* For internal use by BidiRunner. Get where the BiDi mapping for the given row
 * needs to be stored, of nullptr if it's a context row. */
BidiRow* RingView::get_bidirow_writable(vte::grid::row_t row) const
{
        if (row < m_start || row >= m_start + m_len)
                return nullptr;

        return m_bidirows[row - m_start];
}

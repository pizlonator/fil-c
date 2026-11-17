/*
 * Copyright (C) 2001-2004,2009,2010 Red Hat, Inc.
 * Copyright © 2008, 2009, 2010, 2020 Christian Persch
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

#include "config.h"

#include <math.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#if __has_include(<stropts.h>)
#include <stropts.h>
#define HAVE_STROPTS_H
#endif
#if __has_include(<sys/stream.h>)
#include <sys/stream.h>
#endif

#include <glib.h>
#include <glib-unix.h>
#include <glib/gi18n-lib.h>

#include <vte/vte.h>
#include "vteinternal.hh"
#include "bidi.hh"
#include "buffer.h"
#include "debug.h"
#include "reaper.hh"
#include "ring.hh"
#include "ringview.hh"
#include "caps.hh"
#include "widget.hh"
#include "cairo-glue.hh"
#include "scheduler.h"

#if VTE_GTK == 4
#include "graphene-glue.hh"
#endif

#if __has_include(<wchar.h>)
#include <wchar.h>
#endif
#if __has_include(<sys/syslimits.h>)
#include <sys/syslimits.h>
#endif
#if __has_include(<sys/wait.h>)
#include <sys/wait.h>
#define HAVE_SYS_WAIT_H
#endif
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include "keymap.h"
#include "marshal.h"
#include "pastify.hh"
#include "vtepty.h"
#include "vtegtk.hh"
#include "cxx-utils.hh"
#include "gobject-glue.hh"

#if WITH_A11Y
#if VTE_GTK == 3
#include "vteaccess.h"
#endif /* VTE_GTK == 3 */
#endif /* WITH_A11Y */

#include "unicode-width.hh"

#include <new> /* placement new */

using namespace std::literals;

#if !HAVE_ROUND
static inline double round(double x) {
	if(x - floor(x) < 0.5) {
		return floor(x);
	} else {
		return ceil(x);
	}
}
#endif /* !HAVE_ROUND */

#define WORD_CHAR_EXCEPTIONS_DEFAULT "-#%&+,./=?@\\_~\302\267"sv

#define I_(string) (g_intern_static_string(string))

#if VTE_GTK == 3
#define VTE_STYLE_CLASS_READ_ONLY GTK_STYLE_CLASS_READ_ONLY
#elif VTE_GTK == 4
#define VTE_STYLE_CLASS_READ_ONLY "read-only"
#endif

namespace vte {
namespace terminal {

using namespace vte::color_palette;

// _vte_unichar_width() determines the number of cells that a character
// would occupy. The primary likely case is hoisted into a define so
// it ends up in the caller without inlining the entire function.
#define _vte_unichar_width(c,u) \
        (G_LIKELY ((c) < 0x80) ? 1 : (_vte_unichar_width)((c),(u)))

static void stop_processing(vte::terminal::Terminal* that);
static void add_process_timeout(vte::terminal::Terminal* that);
static void process_timeout (GtkWidget *widget, gpointer data) noexcept;

#if VTE_GTK == 3
static vte::Freeable<cairo_region_t> vte_cairo_get_clip_region(cairo_t* cr);
#endif

class Terminal::ProcessingContext {
public:
        vte::grid::row_t m_bbox_top{-G_MAXINT};
        vte::grid::row_t m_bbox_bottom{G_MAXINT};
        bool m_modified{false};
        bool m_bottom{false};
        bool m_invalidated_text{false};
        bool m_in_scroll_region{false};
        bool m_saved_cursor_visible{false};
        CursorStyle m_saved_cursor_style;
        VteVisualPosition m_saved_cursor;
        VteScreen const* m_saved_screen{nullptr};
        Terminal *m_terminal;

        ProcessingContext(Terminal& terminal) noexcept
        {
                m_terminal = &terminal;

                auto screen = m_saved_screen = terminal.m_screen;

                // FIXMEchpe make this a method on VteScreen
                m_bottom = screen->insert_delta == long(screen->scroll_delta);

                /* Save the current cursor position. */
                m_saved_cursor = screen->cursor;
                m_saved_cursor_visible = terminal.m_modes_private.DEC_TEXT_CURSOR();
                m_saved_cursor_style = terminal.m_cursor_style;

                m_in_scroll_region = terminal.m_scrolling_region.is_restricted()
                        && (screen->cursor.row >= (screen->insert_delta + terminal.m_scrolling_region.top()))
                        && (screen->cursor.row <= (screen->insert_delta + terminal.m_scrolling_region.bottom()));

                //context.modified = false;
                //context.invalidated_text = false;

                //context.bbox_bottom = -G_MAXINT;
                //context.bbox_top = G_MAXINT;
        }

        ~ProcessingContext() = default;

        ProcessingContext(ProcessingContext const&) = delete;
        ProcessingContext(ProcessingContext&&) = delete;

        ProcessingContext& operator=(ProcessingContext const&) = delete;
        ProcessingContext& operator=(ProcessingContext&&) = delete;

        [[gnu::always_inline]]
        inline void pre_GRAPHIC() noexcept
        {
                m_bbox_top = std::min(m_bbox_top,
                                      m_terminal->m_screen->cursor.row);
        }

        [[gnu::always_inline]]
        inline void post_GRAPHIC() noexcept
        {
                /* Add the cells over which we have moved to the region
                 * which we need to refresh for the user. */
                m_bbox_bottom = std::max(m_bbox_bottom,
                                         m_terminal->m_screen->cursor.row);

                m_invalidated_text = true;
                m_modified = true;
        }

        [[gnu::always_inline]]
        inline void post_CMD() noexcept
        {
                m_modified = true;

                // FIXME m_terminal->m_screen may be != m_saved_screen, check for that!

                auto const* screen = m_terminal->m_screen;
                auto const new_in_scroll_region = m_terminal->m_scrolling_region.is_restricted() &&
                        (screen->cursor.row >= (screen->insert_delta + m_terminal->m_scrolling_region.top())) &&
                        (screen->cursor.row <= (screen->insert_delta + m_terminal->m_scrolling_region.bottom()));

                /* if we have moved greatly during the sequence handler, or moved
                 * into a scroll_region from outside it, restart the bbox.
                 */
                if (m_invalidated_text &&
                    ((new_in_scroll_region && !m_in_scroll_region) ||
                     (screen->cursor.row > m_bbox_bottom + VTE_CELL_BBOX_SLACK ||
                      screen->cursor.row < m_bbox_top - VTE_CELL_BBOX_SLACK))) {
                        m_terminal->invalidate_rows_and_context(m_bbox_top, m_bbox_bottom);
                        m_invalidated_text = false;
                        m_bbox_bottom = -G_MAXINT;
                        m_bbox_top = G_MAXINT;
                }

                m_in_scroll_region = new_in_scroll_region;
        }

}; // class ProcessingContext

static void
vte_char_attr_list_fill (VteCharAttrList *array,
                         const struct _VteCharAttributes *item,
                         guint final_size)
{
        guint old_len = vte_char_attr_list_get_size(array);

        if (old_len >= final_size)
                return;

        vte_char_attr_list_set_size(array, final_size);
        std::fill_n(vte_char_attr_list_get(array, old_len), final_size - old_len, *item);
}

void
Terminal::unset_widget() noexcept
{
#if WITH_A11Y && VTE_GTK == 3
        set_accessible(nullptr);
#endif

        m_real_widget = nullptr;
        m_terminal = nullptr;
        m_widget = nullptr;
}

//FIXMEchpe this function is bad
inline vte::view::coord_t
Terminal::scroll_delta_pixel() const
{
        return round(m_screen->scroll_delta * m_cell_height);
}

/*
 * Terminal::pixel_to_row:
 * @y: Y coordinate is relative to viewport, top padding excluded
 *
 * Returns: absolute row
 */
inline vte::grid::row_t
Terminal::pixel_to_row(vte::view::coord_t y) const
{
        return (scroll_delta_pixel() + y) / m_cell_height;
}

/*
 * Terminal::pixel_to_row:
 * @row: absolute row
 *
 * Returns: Y coordinate relative to viewport with top padding excluded. If the row is
 *   outside the viewport, may return any value < 0 or >= height
 */
inline vte::view::coord_t
Terminal::row_to_pixel(vte::grid::row_t row) const
{
        // FIXMEchpe this is bad!
        return row * m_cell_height - (long)round(m_screen->scroll_delta * m_cell_height);
}

inline vte::grid::row_t
Terminal::first_displayed_row() const
{
        return pixel_to_row(0);
}

inline vte::grid::row_t
Terminal::last_displayed_row() const
{
        /* Get the logical row number displayed at the bottom pixel position */
        auto r = pixel_to_row(m_view_usable_extents.height() - 1);

        /* If we have an extra padding at the bottom which is currently unused,
         * this number is one too big. Adjust here.
         * E.g. have a terminal of size 80 x 24.5.
         * Initially the bottom displayed row is (0-based) 23, but r is now 24.
         * After producing more than a screenful of content and scrolling back
         * all the way to the top, the bottom displayed row is (0-based) 24. */
        r = MIN (r, m_screen->insert_delta + m_row_count - 1);
        return r;
}

/* Checks if the cursor is potentially at least partially onscreen.
 * An outline cursor has an additional height of VTE_LINE_WIDTH pixels.
 * It's also intentionally painted over the padding, up to VTE_LINE_WIDTH
 * pixels under the real contents area. This method takes these into account.
 * Only checks the cursor's row; not its visibility, shape, or offscreen column.
 */
inline bool
Terminal::cursor_is_onscreen() const noexcept
{
        /* Note: the cursor can only be offscreen below the visible area, not above. */
        auto cursor_top = row_to_pixel (m_screen->cursor.row) - VTE_LINE_WIDTH;
        auto display_bottom = m_view_usable_extents.height() + MIN(m_border.bottom, VTE_LINE_WIDTH);
        return cursor_top < display_bottom;
}

/* Invalidate the requested rows. This is to be used when only the desired
 * rendering changes but not the underlying data, e.g. moving or blinking
 * cursor, highligthing with the mouse etc.
 *
 * Note that row_end is inclusive. This is not as nice as end-exclusive,
 * but saves us from a +1 almost everywhere where this method is called.
 */
void
Terminal::invalidate_rows(vte::grid::row_t row_start,
                          vte::grid::row_t row_end /* inclusive */)
{
#if VTE_GTK == 3
	if (G_UNLIKELY (!widget_realized()))
                return;

        if (m_invalidated_all)
		return;

        if (G_UNLIKELY (row_end < row_start))
                return;

	_vte_debug_print (VTE_DEBUG_UPDATES,
                          "Invalidating rows %ld..%ld.\n",
                          row_start, row_end);
	_vte_debug_print (VTE_DEBUG_WORK, "?");

        /* Scrolled back, visible parts didn't change. */
        if (row_start > last_displayed_row())
                return;

        /* Recognize if we're about to invalidate everything. */
        if (row_start <= first_displayed_row() &&
            row_end >= last_displayed_row()) {
		invalidate_all();
		return;
	}

#if VTE_GTK == 3
        cairo_rectangle_int_t rect;
	/* Convert the column and row start and end to pixel values
	 * by multiplying by the size of a character cell.
	 * Always include the extra pixel border and overlap pixel.
	 */
        // FIXMEegmont invalidate the left and right padding too
        rect.x = -1;
        int xend = m_column_count * m_cell_width + 1;
        rect.width = xend - rect.x;

        /* Always add at least VTE_LINE_WIDTH pixels so the outline block cursor fits */
        rect.y = row_to_pixel(row_start) - std::max(cell_overflow_top(), VTE_LINE_WIDTH);
        int yend = row_to_pixel(row_end + 1) + std::max(cell_overflow_bottom(), VTE_LINE_WIDTH);
        rect.height = yend - rect.y;

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"Invalidating pixels at (%d,%d)x(%d,%d).\n",
			rect.x, rect.y, rect.width, rect.height);
#endif

	if (is_processing()) {
#if VTE_GTK == 3
                g_array_append_val(m_update_rects, rect);
#endif
		/* Wait a bit before doing any invalidation, just in
		 * case updates are coming in really soon. */
		add_process_timeout(this);
	} else {
#if VTE_GTK == 3
                auto allocation = get_allocated_rect();
                rect.x += allocation.x + m_border.left;
                rect.y += allocation.y + m_border.top;
                cairo_region_t *region = cairo_region_create_rectangle(&rect);
		gtk_widget_queue_draw_region(m_widget, region);
                cairo_region_destroy(region);
#elif VTE_GTK == 4
                gtk_widget_queue_draw(m_widget); // FIXMEgtk4
#endif
	}

	_vte_debug_print (VTE_DEBUG_WORK, "!");
#elif VTE_GTK == 4
        invalidate_all();
#endif
}

/* Invalidate the requested rows, extending the region in both directions up to
 * an explicit newline (or a safety limit) to invalidate entire paragraphs of text.
 * This is to be used whenever the underlying data changes, because any such
 * change might alter the desired BiDi, syntax highlighting etc. of all other
 * rows of the involved paragraph(s).
 *
 * Note that row_end is inclusive. This is not as nice as end-exclusive,
 * but saves us from a +1 almost everywhere where this method is called.
 */
void
Terminal::invalidate_rows_and_context(vte::grid::row_t row_start,
                                      vte::grid::row_t row_end /* inclusive */)
{
        if (G_UNLIKELY (!widget_realized()))
                return;

        if (m_invalidated_all)
                return;

        if (G_UNLIKELY (row_end < row_start))
                return;

        _vte_debug_print (VTE_DEBUG_UPDATES,
                          "Invalidating rows %ld..%ld and context.\n",
                          row_start, row_end);

        /* Safety limit: Scrolled back by so much that changes to the
         * writable area may not affect the current viewport's rendering. */
        if (m_screen->insert_delta - VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX > last_displayed_row())
                return;

        /* Extending the start is a bit tricky.
         * First extend it (towards lower numbered indices), but only up to
         * insert_delta - 1. Remember that the row at insert_delta - 1 is
         * still in the ring, hence checking its soft_wrapped flag is fast. */
        while (row_start >= m_screen->insert_delta) {
                if (!m_screen->row_data->is_soft_wrapped(row_start - 1))
                        break;
                row_start--;
        }

        /* If we haven't seen a newline yet, stop walking backwards row by row.
         * This is because we might need to access row_stream in order to check
         * the wrapped state, a way too expensive operation while processing
         * incoming data. Let displaying do extra work instead.
         * So just invalidate everything to the top. */
        if (row_start < m_screen->insert_delta) {
                row_start = first_displayed_row();
        }

        /* Extending the end is simple. Just walk until we go offscreen or
         * find an explicit newline. */
        while (row_end < last_displayed_row()) {
                if (!m_screen->row_data->is_soft_wrapped(row_end))
                        break;
                row_end++;
        }

        invalidate_rows(row_start, row_end);
}

/* Convenience methods */
void
Terminal::invalidate_row(vte::grid::row_t row)
{
        invalidate_rows(row, row);
}

void
Terminal::invalidate_row_and_context(vte::grid::row_t row)
{
        invalidate_rows_and_context(row, row);
}

/* This is only used by the selection code, so no need to extend the area. */
void
Terminal::invalidate(vte::grid::span const& s)
{
        if (!s.empty())
                invalidate_rows(s.start_row(), s.last_row());
}

/* Invalidates the symmetrical difference ("XOR" area) of the two spans.
 * This is only used by the selection code, so no need to extend the area. */
void
Terminal::invalidate_symmetrical_difference(vte::grid::span const& a, vte::grid::span const& b, bool block)
{
        if (a.empty() || b.empty() || a.start() >= b.end() || b.start() >= a.end()) {
                /* One or both are empty (invalidate() will figure out which), or disjoint intervals. */
                invalidate (a);
                invalidate (b);
                return;
        }

        if (block) {
                /* We could optimize when the columns don't change, probably not worth it. */
                invalidate_rows (std::min (a.start_row(), b.start_row()),
                                 std::max (a.last_row(),  b.last_row()));
        } else {
                if (a.start() != b.start()) {
                        invalidate_rows (std::min (a.start_row(), b.start_row()),
                                         std::max (a.start_row(), b.start_row()));
                }
                if (a.end() != b.end()) {
                        invalidate_rows (std::min (a.last_row(), b.last_row()),
                                         std::max (a.last_row(), b.last_row()));
                }
        }
}

void
Terminal::invalidate_all()
{
	if (G_UNLIKELY (!widget_realized()))
                return;

	if (m_invalidated_all) {
		return;
	}

	_vte_debug_print (VTE_DEBUG_WORK, "*");
	_vte_debug_print (VTE_DEBUG_UPDATES, "Invalidating all.\n");

	reset_update_rects();
	m_invalidated_all = TRUE;

        if (is_processing ()) {
#if VTE_GTK == 3
                /* replace invalid regions with one covering the whole terminal */
                auto allocation = get_allocated_rect();
                cairo_rectangle_int_t rect;
                rect.x = -m_border.left;
                rect.y = -m_border.top;
                rect.width = allocation.width;
                rect.height = allocation.height;

                g_array_append_val(m_update_rects, rect);
#endif /* VTE_GTK == 3 */

		/* Wait a bit before doing any invalidation, just in
		 * case updates are coming in really soon. */
		add_process_timeout(this);
	} else {
                gtk_widget_queue_draw(m_widget);
	}
}

/* Find the row in the given position in the backscroll buffer.
 * Note that calling this method may invalidate the return value of
 * a previous find_row_data() call. */
// FIXMEchpe replace this with a method on VteRing
VteRowData const*
Terminal::find_row_data(vte::grid::row_t row) const
{
	VteRowData const* rowdata = nullptr;

	if (G_LIKELY(m_screen->row_data->contains(row))) {
		rowdata = m_screen->row_data->index(row);
	}
	return rowdata;
}

/* Find the row in the given position in the backscroll buffer. */
// FIXMEchpe replace this with a method on VteRing
VteRowData*
Terminal::find_row_data_writable(vte::grid::row_t row) const
{
	VteRowData *rowdata = nullptr;

	if (G_LIKELY (m_screen->row_data->contains(row))) {
		rowdata = m_screen->row_data->index_writable(row);
	}
	return rowdata;
}

/* Find the character an the given position in the backscroll buffer.
 * Note that calling this method may invalidate the return value of
 * a previous find_row_data() call. */
// FIXMEchpe replace this with a method on VteRing
VteCell const*
Terminal::find_charcell(vte::grid::column_t col,
                                  vte::grid::row_t row) const
{
	VteRowData const* rowdata;
	VteCell const* ret = nullptr;

	if (m_screen->row_data->contains(row)) {
		rowdata = m_screen->row_data->index(row);
		ret = _vte_row_data_get (rowdata, col);
	}
	return ret;
}

// FIXMEchpe replace this with a method on VteRing
vte::grid::column_t
Terminal::find_start_column(vte::grid::column_t col,
                                      vte::grid::row_t row) const
{
	VteRowData const* row_data = find_row_data(row);
	if (G_UNLIKELY (col < 0))
		return col;
	if (row_data != nullptr) {
		const VteCell *cell = _vte_row_data_get (row_data, col);
		while (col > 0 && cell != NULL && cell->attr.fragment()) {
			cell = _vte_row_data_get (row_data, --col);
		}
	}
	return MAX(col, 0);
}

// FIXMEchpe replace this with a method on VteRing
vte::grid::column_t
Terminal::find_end_column(vte::grid::column_t col,
                                    vte::grid::row_t row) const
{
	VteRowData const* row_data = find_row_data(row);
	gint columns = 0;
	if (G_UNLIKELY (col < 0))
		return col;
	if (row_data != NULL) {
		const VteCell *cell = _vte_row_data_get (row_data, col);
		while (col > 0 && cell != NULL && cell->attr.fragment()) {
			cell = _vte_row_data_get (row_data, --col);
		}
		if (cell) {
			columns = cell->attr.columns() - 1;
		}
	}
        // FIXMEchp m__column_count - 1 ?
	return MIN(col + columns, m_column_count);
}

/* Sets the line ending to hard wrapped (explicit newline).
 * Takes care of invalidating if this operation splits a paragraph into two. */
void
Terminal::set_hard_wrapped(vte::grid::row_t row)
{
        /* We can set the row just above insert_delta to hard wrapped. */
        vte_assert_cmpint(row, >=, m_screen->insert_delta - 1);
        vte_assert_cmpint(row, <, m_screen->insert_delta + m_row_count);

        VteRowData *row_data = find_row_data_writable(row);

        /* It's okay for this row not to be covered by the ring. */
        if (row_data == nullptr || !row_data->attr.soft_wrapped)
                return;

        row_data->attr.soft_wrapped = false;

        m_ringview.invalidate();
        invalidate_rows_and_context(row, row + 1);
}

/* Sets the line ending to soft wrapped (overflow to the next line).
 * Takes care of invalidating if this operation joins two paragraphs into one.
 * Also makes sure that the joined new paragraph receives the first one's bidi flags. */
void
Terminal::set_soft_wrapped(vte::grid::row_t row)
{
        vte_assert_cmpint(row, >=, m_screen->insert_delta);
        vte_assert_cmpint(row, <, m_screen->insert_delta + m_row_count);

        VteRowData *row_data = find_row_data_writable(row);
        g_assert(row_data != nullptr);

        if (row_data->attr.soft_wrapped)
                return;

        row_data->attr.soft_wrapped = true;

        /* Each paragraph has to have consistent bidi flags across all of its rows.
         * Spread the first paragraph's flags across the second one (if they differ). */
        guint8 bidi_flags = row_data->attr.bidi_flags;
        vte::grid::row_t i = row + 1;
        row_data = find_row_data_writable(i);
        if (row_data != nullptr && row_data->attr.bidi_flags != bidi_flags) {
                do {
                        row_data->attr.bidi_flags = bidi_flags;
                        if (!row_data->attr.soft_wrapped)
                                break;
                        row_data = find_row_data_writable(++i);
                } while (row_data != nullptr);
        }

        m_ringview.invalidate();
        invalidate_rows_and_context(row, row + 1);
}

/* Determine the width of the portion of the preedit string which lies
 * to the left of the cursor, or the entire string, in columns. */
// FIXMEchpe this is for the view, so use int not gssize
// FIXMEchpe this is only ever called with left_only=false, so remove the param
gssize
Terminal::get_preedit_width(bool left_only)
{
	gssize ret = 0;

        char const *preedit = m_im_preedit.c_str();
        for (int i = 0;
             // FIXMEchpe preddit is != NULL at the start, and next_char never returns NULL either
             (preedit != NULL) &&
		     (preedit[0] != '\0') &&
		     (!left_only || (i < m_im_preedit_cursor));
             i++) {
                gunichar c = g_utf8_get_char(preedit);
                ret += _vte_unichar_width(c, m_utf8_ambiguous_width);
                preedit = g_utf8_next_char(preedit);
        }

	return ret;
}

/* Determine the length of the portion of the preedit string which lies
 * to the left of the cursor, or the entire string, in gunichars. */
// FIXMEchpe this returns gssize but inside it uses int...
gssize
Terminal::get_preedit_length(bool left_only)
{
	ssize_t i = 0;

        char const *preedit = m_im_preedit.c_str();
        for (i = 0;
             // FIXMEchpe useless check, see above
             (preedit != NULL) &&
		     (preedit[0] != '\0') &&
		     (!left_only || (i < m_im_preedit_cursor));
             i++) {
                preedit = g_utf8_next_char(preedit);
        }

	return i;
}

void
Terminal::invalidate_cursor_once(bool periodic)
{
        if (G_UNLIKELY(!widget_realized()))
                return;

	if (m_invalidated_all) {
		return;
	}

	if (periodic) {
		if (!m_cursor_blinks) {
			return;
		}
	}

        // Note that even with invisible cursor, still need
        // to invalidate if preedit is active.
        // See https://gitlab.gnome.org/GNOME/vte/-/issues/2873 .
        if (m_modes_private.DEC_TEXT_CURSOR() || m_im_preedit_active) {
                auto row = m_screen->cursor.row;

		_vte_debug_print(VTE_DEBUG_UPDATES,
                                 "Invalidating cursor in row %ld.\n",
                                 row);
                invalidate_row(row);
	}
}

/* Invalidate the cursor repeatedly. */
// FIXMEchpe this continually adds and removes the blink timeout. Find a better solution
bool
Terminal::cursor_blink_timer_callback()
{
	m_cursor_blink_state = !m_cursor_blink_state;
	m_cursor_blink_time_ms += m_cursor_blink_cycle_ms;

	invalidate_cursor_once(true);

	/* only disable the blink if the cursor is currently shown.
	 * else, wait until next time.
	 */
	if (m_cursor_blink_time_ms >= m_cursor_blink_timeout_ms &&
	    m_cursor_blink_state) {
		return false;
        }

        m_cursor_blink_timer.schedule(m_cursor_blink_cycle_ms,
                                      vte::glib::Timer::Priority::eLOW);
        return false;
}

/* Emit a "selection_changed" signal. */
void
Terminal::emit_selection_changed()
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `selection-changed'.\n");
	g_signal_emit(m_terminal, signals[SIGNAL_SELECTION_CHANGED], 0);
}

/* Emit a "commit" signal.
 * FIXMEchpe: remove this function
 */
void
Terminal::emit_commit(std::string_view const& str)
{
        if (str.size() == 0)
                return;

        if (!widget() || !widget()->should_emit_signal(SIGNAL_COMMIT))
                return;

	_vte_debug_print(VTE_DEBUG_SIGNALS,
                         "Emitting `commit' of %" G_GSSIZE_FORMAT" bytes.\n", str.size());

        // FIXMEchpe we do know for a fact that all uses of this function
        // actually passed a 0-terminated string, so we can use @str directly
        std::string result{str}; // 0-terminated

        _VTE_DEBUG_IF(VTE_DEBUG_KEYBOARD) {
                for (size_t i = 0; i < result.size(); i++) {
                        if ((((guint8) result[i]) < 32) ||
                            (((guint8) result[i]) > 127)) {
                                g_printerr(
                                           "Sending <%02x> "
                                           "to child.\n",
                                           result[i]);
                        } else {
                                g_printerr(
                                           "Sending '%c' "
                                           "to child.\n",
                                           result[i]);
                        }
                }
        }

	g_signal_emit(m_terminal, signals[SIGNAL_COMMIT], 0, result.c_str(), (guint)result.size());
}

void
Terminal::queue_contents_changed()
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Queueing `contents-changed'.\n");
	m_contents_changed_pending = true;
}

//FIXMEchpe this has only one caller
void
Terminal::queue_cursor_moved()
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Queueing `cursor-moved'.\n");
	m_cursor_moved_pending = true;
}

void
Terminal::emit_eof()
{
        if (widget())
                widget()->emit_eof();
}

static gboolean
emit_eof_idle_cb(VteTerminal *terminal)
try
{
        _vte_terminal_get_impl(terminal)->emit_eof();

        return G_SOURCE_REMOVE;
}
catch (...)
{
        vte::log_exception();
        return G_SOURCE_REMOVE;
}

void
Terminal::queue_eof()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Queueing `eof'.\n");

        g_idle_add_full(G_PRIORITY_HIGH,
                        (GSourceFunc)emit_eof_idle_cb,
                        g_object_ref(m_terminal),
                        g_object_unref);
}

void
Terminal::emit_child_exited()
{
        auto const status = m_child_exit_status;
        m_child_exit_status = -1;

        if (widget())
                widget()->emit_child_exited(status);
}

static gboolean
emit_child_exited_idle_cb(VteTerminal *terminal)
try
{
        _vte_terminal_get_impl(terminal)->emit_child_exited();

        return G_SOURCE_REMOVE;
}
catch (...)
{
        vte::log_exception();
        return G_SOURCE_REMOVE;
}

/* Emit a "child-exited" signal on idle, so that if the handler destroys
 * the terminal, we're not deep within terminal code callstack
 */
void
Terminal::queue_child_exited()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Queueing `child-exited'.\n");

        g_idle_add_full(G_PRIORITY_HIGH,
                        (GSourceFunc)emit_child_exited_idle_cb,
                        g_object_ref(m_terminal),
                        g_object_unref);
}

/* Emit an "increase-font-size" signal. */
void
Terminal::emit_increase_font_size()
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `increase-font-size'.\n");
	g_signal_emit(m_terminal, signals[SIGNAL_INCREASE_FONT_SIZE], 0);
}

/* Emit a "decrease-font-size" signal. */
void
Terminal::emit_decrease_font_size()
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `decrease-font-size'.\n");
	g_signal_emit(m_terminal, signals[SIGNAL_DECREASE_FONT_SIZE], 0);
}

void
Terminal::emit_copy_clipboard()
{
	_vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting 'copy-clipboard'.\n");
	g_signal_emit(m_terminal, signals[SIGNAL_COPY_CLIPBOARD], 0);
}

void
Terminal::emit_paste_clipboard()
{
	_vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting 'paste-clipboard'.\n");
	g_signal_emit(m_terminal, signals[SIGNAL_PASTE_CLIPBOARD], 0);
}

/* Emit a "hyperlink_hover_uri_changed" signal. */
void
Terminal::emit_hyperlink_hover_uri_changed(const GdkRectangle *bbox)
{
        GObject *object = G_OBJECT(m_terminal);

        _vte_debug_print(VTE_DEBUG_SIGNALS,
                         "Emitting `hyperlink-hover-uri-changed'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_HYPERLINK_HOVER_URI_CHANGED], 0, m_hyperlink_hover_uri, bbox);
        g_object_notify_by_pspec(object, pspecs[PROP_HYPERLINK_HOVER_URI]);
}

void
Terminal::deselect_all()
{
        if (!m_selection_resolved.empty()) {
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Deselecting all text.\n");

                m_selection_origin = m_selection_last = { -1, -1, 1 };
                resolve_selection();

		/* Don't free the current selection, as we need to keep
		 * hold of it for async copying from the clipboard. */

		emit_selection_changed();
	}
}

/* Clear the cache of the screen contents we keep. */
void
Terminal::match_contents_clear()
{
	match_hilite_clear();

        g_string_truncate(m_match_contents, 0);
        vte_char_attr_list_set_size(&m_match_attributes, 0);
}

void
Terminal::match_contents_refresh()

{
	match_contents_clear();

        g_assert (m_match_contents != nullptr);
        g_assert (m_match_contents->len == 0);
        g_assert (vte_char_attr_list_get_size(&m_match_attributes) == 0);

        get_text_displayed(m_match_contents, &m_match_attributes);
}

void
Terminal::regex_match_remove_all() noexcept
{
        auto& match_regexes = match_regexes_writable();
        match_regexes.clear();
        match_regexes.shrink_to_fit();

	match_hilite_clear();
}

void
Terminal::regex_match_remove(int tag) noexcept
{
        auto i = regex_match_get_iter(tag);
        if (i == std::end(m_match_regexes))
                return;

        match_regexes_writable().erase(i);
}

/*
 * match_rowcol_to_offset:
 * @terminal:
 * @column:
 * @row:
 * @offset_ptr: (out):
 * @sattr_ptr: (out):
 * @ettr_ptr: (out):
 *
 * Maps (row, column) to an offset in m_match_attributes, and returns
 * that offset in @offset_ptr, and the start and end of the corresponding
 * line in @sattr_ptr and @eattr_ptr.
 */
bool
Terminal::match_rowcol_to_offset(vte::grid::column_t column,
                                 vte::grid::row_t row,
                                 gsize *offset_ptr,
                                 gsize *sattr_ptr,
                                 gsize *eattr_ptr)
{
        /* FIXME: use gsize, after making sure the code below doesn't underflow offset */
        gssize offset, sattr, eattr;
        struct _VteCharAttributes *attr = NULL;

        if (m_match_contents->len == 0)
                return false;

        auto const match_contents = m_match_contents->str;

	/* Map the pointer position to a portion of the string. */
        // FIXME do a bsearch here?
	eattr = vte_char_attr_list_get_size(&m_match_attributes);
	for (offset = eattr; offset--; ) {
                attr = vte_char_attr_list_get(&m_match_attributes, offset);
		if (row < attr->row) {
			eattr = offset;
		}
		if (row == attr->row &&
		    column >= attr->column && column < attr->column + attr->columns) {
			break;
		}
	}

	_VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
		if (offset < 0)
			g_printerr("Cursor is not on a character.\n");
		else {
                        gunichar c;
                        char utf[7];
                        c = g_utf8_get_char (match_contents + offset);
                        utf[g_unichar_to_utf8(g_unichar_isprint(c) ? c : 0xFFFD, utf)] = 0;

			g_printerr("Cursor is on character U+%04X '%s' at %" G_GSSIZE_FORMAT ".\n",
                                   c, utf, offset);
                }
	}

	/* If the pointer isn't on a matchable character, bug out. */
	if (offset < 0) {
		return false;
	}

	/* If the pointer is on a newline, bug out. */
	if (match_contents[offset] == '\0') {
		_vte_debug_print(VTE_DEBUG_EVENTS,
                                 "Cursor is on newline.\n");
		return false;
	}

	/* Snip off any final newlines. */
	while (match_contents[eattr] == '\n' ||
               match_contents[eattr] == '\0') {
		eattr--;
	}
	/* and scan forwards to find the end of this line */
	while (!(match_contents[eattr] == '\n' ||
                 match_contents[eattr] == '\0')) {
		eattr++;
	}

	/* find the start of row */
	if (row == 0) {
		sattr = 0;
	} else {
		for (sattr = offset; sattr > 0; sattr--) {
                        attr = vte_char_attr_list_get(&m_match_attributes, sattr);
			if (row > attr->row) {
				break;
			}
		}
	}
	/* Scan backwards to find the start of this line */
	while (sattr > 0 &&
		! (match_contents[sattr] == '\n' ||
                   match_contents[sattr] == '\0')) {
		sattr--;
	}
	/* and skip any initial newlines. */
	while (match_contents[sattr] == '\n' ||
               match_contents[sattr] == '\0') {
		sattr++;
	}
	if (eattr <= sattr) { /* blank line */
		return false;
	}
	if (eattr <= offset || sattr > offset) {
		/* nothing to match on this line */
		return false;
	}

        *offset_ptr = offset;
        *sattr_ptr = sattr;
        *eattr_ptr = eattr;

        _VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
                struct _VteCharAttributes *_sattr, *_eattr;
                _sattr = vte_char_attr_list_get(&m_match_attributes, sattr);
                _eattr = vte_char_attr_list_get(&m_match_attributes, eattr - 1);
                g_printerr("Cursor is in line from %" G_GSIZE_FORMAT "(%ld,%ld) to %" G_GSIZE_FORMAT "(%ld,%ld)\n",
                           sattr, _sattr->column, _sattr->row,
                           eattr - 1, _eattr->column, _eattr->row);
        }

        return true;
}

/* creates a pcre match context with appropriate limits */
vte::Freeable<pcre2_match_context_8>
Terminal::create_match_context()
{
        auto context = vte::take_freeable(pcre2_match_context_create_8(nullptr /* general context */));
        pcre2_set_match_limit_8(context.get(), 65536); /* should be plenty */
        pcre2_set_recursion_limit_8(context.get(), 64); /* should be plenty */

        return context;
}

bool
Terminal::match_check_pcre(pcre2_match_data_8 *match_data,
                           pcre2_match_context_8 *match_context,
                           vte::base::Regex const* regex,
                           uint32_t match_flags,
                           gsize sattr,
                           gsize eattr,
                           gsize offset,
                           char **result_ptr,
                           gsize *start,
                           gsize *end,
                           gsize *sblank_ptr,
                           gsize *eblank_ptr)
{
        int (* match_fn) (const pcre2_code_8 *,
                          PCRE2_SPTR8, PCRE2_SIZE, PCRE2_SIZE, uint32_t,
                          pcre2_match_data_8 *, pcre2_match_context_8 *);
        gsize sblank = 0, eblank = G_MAXSIZE;
        gsize position, line_length;
        const char *line;
        int r = 0;

        if (regex->jited())
                match_fn = pcre2_jit_match_8;
        else
                match_fn = pcre2_match_8;

        line = m_match_contents->str;
        /* FIXME: what we really want is to pass the whole data to pcre2_match, but
         * limit matching to between sattr and eattr, so that the extra data can
         * satisfy lookahead assertions. This needs new pcre2 API though.
         */
        line_length = eattr;

        /* Iterate throught the matches until we either find one which contains the
         * offset, or we get no more matches.
         */
        pcre2_set_offset_limit_8(match_context, eattr);
        position = sattr;
        while (position < eattr &&
               ((r = match_fn(regex->code(),
                              (PCRE2_SPTR8)line, line_length, /* subject, length */
                              position, /* start offset */
                              match_flags |
                              PCRE2_NO_UTF_CHECK | PCRE2_NOTEMPTY | PCRE2_PARTIAL_SOFT /* FIXME: HARD? */,
                              match_data,
                              match_context)) >= 0 || r == PCRE2_ERROR_PARTIAL)) {
                gsize ko = offset;
                gsize rm_so, rm_eo;
                gsize *ovector;

                ovector = pcre2_get_ovector_pointer_8(match_data);
                rm_so = ovector[0];
                rm_eo = ovector[1];
                if (G_UNLIKELY(rm_so == PCRE2_UNSET || rm_eo == PCRE2_UNSET))
                        break;

                /* The offsets should be "sane". We set NOTEMPTY, but check anyway */
                if (G_UNLIKELY(position == rm_eo)) {
                        /* rm_eo is before the end of subject string's length, so this is safe */
                        position = g_utf8_next_char(line + rm_eo) - line;
                        continue;
                }

                _VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
                        gchar *result;
                        struct _VteCharAttributes *_sattr, *_eattr;
                        result = g_strndup(line + rm_so, rm_eo - rm_so);
                        _sattr = vte_char_attr_list_get(&m_match_attributes, rm_so);
                        _eattr = vte_char_attr_list_get(&m_match_attributes, rm_eo - 1);
                        g_printerr("%s match `%s' from %" G_GSIZE_FORMAT "(%ld,%ld) to %" G_GSIZE_FORMAT "(%ld,%ld) (%" G_GSSIZE_FORMAT ").\n",
                                   r == PCRE2_ERROR_PARTIAL ? "Partial":"Full",
                                   result,
                                   rm_so,
                                   _sattr->column,
                                   _sattr->row,
                                   rm_eo - 1,
                                   _eattr->column,
                                   _eattr->row,
                                   offset);
                        g_free(result);
                }

                /* advance position */
                position = rm_eo;

                /* FIXME: do handle newline / partial matches at end of line/start of next line */
                if (r == PCRE2_ERROR_PARTIAL)
                        continue;

                /* If the pointer is in this substring, then we're done. */
                if (ko >= rm_so && ko < rm_eo) {
                        *result_ptr = g_strndup(line + rm_so, rm_eo - rm_so);
                        *start = rm_so;
                        *end = rm_eo - 1;
                        return true;
                }

                if (ko >= rm_eo && rm_eo > sblank) {
                        sblank = rm_eo;
                }
                if (ko < rm_so && rm_so < eblank) {
                        eblank = rm_so;
                }
        }

        if (G_UNLIKELY(r < PCRE2_ERROR_PARTIAL))
                _vte_debug_print(VTE_DEBUG_REGEX, "Unexpected pcre2_match error code: %d\n", r);

        *sblank_ptr = sblank;
        *eblank_ptr = eblank;
        return false;
}

char *
Terminal::match_check_internal_pcre(vte::grid::column_t column,
                                    vte::grid::row_t row,
                                    MatchRegex const** match,
                                    size_t* start,
                                    size_t* end)
{
	gsize offset, sattr, eattr, start_blank, end_blank;

	_vte_debug_print(VTE_DEBUG_REGEX,
                         "Checking for pcre match at (%ld,%ld).\n", row, column);

        if (!match_rowcol_to_offset(column, row,
                                    &offset, &sattr, &eattr))
                return nullptr;

	start_blank = sattr;
	end_blank = eattr;

        auto match_context = create_match_context();
        auto match_data = vte::take_freeable(pcre2_match_data_create_8(256 /* should be plenty */,
                                                                       nullptr /* general context */));

	/* Now iterate over each regex we need to match against. */
        char* dingu_match{nullptr};
        for (auto const& rem : m_match_regexes) {
                gsize sblank, eblank;

                if (match_check_pcre(match_data.get(), match_context.get(),
                                     rem.regex(),
                                     rem.match_flags(),
                                     sattr, eattr, offset,
                                     &dingu_match,
                                     start, end,
                                     &sblank, &eblank)) {
                        _vte_debug_print(VTE_DEBUG_REGEX, "Matched dingu with tag %d\n", rem.tag());
                        *match = std::addressof(rem);
                        break;
                }

                if (sblank > start_blank) {
                        start_blank = sblank;
                }
                if (eblank < end_blank) {
                        end_blank = eblank;
                }
	}

        if (dingu_match == nullptr) {
                /* If we get here, there was no dingu match.
                 * Record smallest span where none of the dingus match.
                 */
                *start = start_blank;
                *end = end_blank - 1;
                *match = nullptr;

                _VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
                        struct _VteCharAttributes *_sattr, *_eattr;
                        _sattr = vte_char_attr_list_get(&m_match_attributes, start_blank);
                        _eattr = vte_char_attr_list_get(&m_match_attributes, end_blank - 1);
                        g_printerr("No-match region from %" G_GSIZE_FORMAT "(%ld,%ld) to %" G_GSIZE_FORMAT "(%ld,%ld)\n",
                                   start_blank, _sattr->column, _sattr->row,
                                   end_blank - 1, _eattr->column, _eattr->row);
                }
        }

	return dingu_match;
}

/*
 * vte_terminal_match_check_internal:
 * @terminal:
 * @column:
 * @row:
 * @match: (out):
 * @start: (out):
 * @end: (out):
 *
 * Checks m_match_contents for dingu matches, and returns the start, and
 * end of the match in @start, @end, and the matched regex in @match.
 * If no match occurs, @match will be set to %nullptr,
 * and if they are nonzero, @start and @end mark the smallest span in the @row
 * in which none of the dingus match.
 *
 * Returns: (transfer full): the matched string, or %nullptr
 */
char *
Terminal::match_check_internal(vte::grid::column_t column,
                               vte::grid::row_t row,
                               MatchRegex const** match,
                               size_t* start,
                               size_t* end)
{
        if (m_match_contents->len == 0)
                match_contents_refresh();

        assert(match != nullptr);
        assert(start != nullptr);
        assert(end != nullptr);

        *match = nullptr;
        *start = 0;
        *end = 0;

        return match_check_internal_pcre(column, row, match, start, end);
}

char*
Terminal::regex_match_check(vte::grid::column_t column,
                            vte::grid::row_t row,
                            int* tag)
{

        // Caller needs to update the ringview.
        if (!m_ringview.is_updated())
                [[unlikely]] return nullptr;

	long delta = m_screen->scroll_delta;
	_vte_debug_print(VTE_DEBUG_EVENTS | VTE_DEBUG_REGEX,
			"Checking for match at (%ld,%ld).\n",
			row, column);

        char* ret{nullptr};
        Terminal::MatchRegex const* match{nullptr};

        if (m_match_span.contains(row + delta, column)) {
                match = regex_match_current(); /* may be nullptr */
                ret = g_strdup(m_match);
	} else {
                gsize start, end;

                ret = match_check_internal(column, row + delta,
                                           &match,
                                           &start, &end);
	}
	_VTE_DEBUG_IF(VTE_DEBUG_EVENTS | VTE_DEBUG_REGEX) {
		if (ret != NULL) g_printerr("Matched `%s'.\n", ret);
	}
        if (tag != nullptr)
                *tag = (match != nullptr) ? match->tag() : -1;

	return ret;
}

/*
 * Terminal::view_coords_from_event:
 * @event: a mouse event
 *
 * Translates the event coordinates to view coordinates, by
 * subtracting the padding and window offset.
 * Coordinates < 0 or >= m_usable_view_extents.width() or .height()
 * mean that the event coordinates are outside the usable area
 * at that side; use view_coords_visible() to check for that.
 */
vte::view::coords
Terminal::view_coords_from_event(vte::platform::MouseEvent const& event) const
{
        return vte::view::coords(event.x() - m_border.left, event.y() - m_border.top);
}

bool
Terminal::widget_realized() const noexcept
{
        return m_real_widget ? m_real_widget->realized() : false;
}

/*
 * Terminal::grid_coords_from_event:
 * @event: a mouse event
 *
 * Translates the event coordinates to view coordinates, by
 * subtracting the padding and window offset.
 * Coordinates < 0 or >= m_usable_view_extents.width() or .height()
 * mean that the event coordinates are outside the usable area
 * at that side; use grid_coords_visible() to check for that.
 */
vte::grid::coords
Terminal::grid_coords_from_event(vte::platform::MouseEvent const& event) const
{
        return grid_coords_from_view_coords(view_coords_from_event(event));
}

/*
 * Terminal::confined_grid_coords_from_event:
 * @event: a mouse event
 *
 * Like grid_coords_from_event(), but also confines the coordinates
 * to an actual cell in the visible area.
 */
vte::grid::coords
Terminal::confined_grid_coords_from_event(vte::platform::MouseEvent const& event) const
{
        auto pos = view_coords_from_event(event);
        return confined_grid_coords_from_view_coords(pos);
}

/*
 * Terminal::grid_coords_from_view_coords:
 * @pos: the view coordinates
 *
 * Translates view coordinates to grid coordinates. If the view coordinates point to
 * cells that are not visible, may return any value < 0 or >= m_column_count, and
 * < first_displayed_row() or > last_displayed_row(), resp.
 */
vte::grid::coords
Terminal::grid_coords_from_view_coords(vte::view::coords const& pos) const
{
        // Callers need to update the ringview. However, don't assert, just
        // return out-of-view coords. FIXME: may want to throw instead
        if (!m_ringview.is_updated())
                [[unlikely]] return {-1, -1};

        vte::grid::column_t col;
        if (pos.x >= 0 && pos.x < m_view_usable_extents.width())
                col = pos.x / m_cell_width;
        else if (pos.x < 0)
                col = -1;
        else
                col = m_column_count;

        vte::grid::row_t row = pixel_to_row(pos.y);

        /* BiDi: convert to logical column. */
        vte::base::BidiRow const* bidirow = m_ringview.get_bidirow(confine_grid_row(row));
        col = bidirow->vis2log(col);

        return vte::grid::coords(row, col);
}

vte::grid::row_t
Terminal::confine_grid_row(vte::grid::row_t const& row) const
{
        auto first_row = first_displayed_row();
        auto last_row = last_displayed_row();

        return vte::clamp(row, first_row, last_row);
}

/*
 * Terminal::confined_grid_coords_from_view_coords:
 * @pos: the view coordinates
 *
 * Like grid_coords_from_view_coords(), but also confines the coordinates
 * to an actual cell in the visible area.
 */
vte::grid::coords
Terminal::confined_grid_coords_from_view_coords(vte::view::coords const& pos) const
{
        auto rowcol = grid_coords_from_view_coords(pos);
        return confine_grid_coords(rowcol);
}

/*
 * Terminal::view_coords_from_grid_coords:
 * @rowcol: the grid coordinates
 *
 * Translates grid coordinates to view coordinates. If the view coordinates are
 * outside the usable area, may return any value < 0 or >= m_usable_view_extents.
 *
 * Returns: %true if the coordinates are inside the usable area
 */
vte::view::coords
Terminal::view_coords_from_grid_coords(vte::grid::coords const& rowcol) const
{
        return vte::view::coords(rowcol.column() * m_cell_width,
                                 row_to_pixel(rowcol.row()));
}

bool
Terminal::view_coords_visible(vte::view::coords const& pos) const
{
        return pos.x >= 0 && pos.x < m_view_usable_extents.width() &&
               pos.y >= 0 && pos.y < m_view_usable_extents.height();
}

bool
Terminal::grid_coords_visible(vte::grid::coords const& rowcol) const
{
        return rowcol.column() >= 0 &&
               rowcol.column() < m_column_count &&
               rowcol.row() >= first_displayed_row() &&
               rowcol.row() <= last_displayed_row();
}

vte::grid::coords
Terminal::confine_grid_coords(vte::grid::coords const& rowcol) const
{
        /* Confine clicks to the nearest actual cell. This is especially useful for
         * fullscreen vte so that you can click on the very edge of the screen.
         */
        auto first_row = first_displayed_row();
        auto last_row = last_displayed_row();

        return vte::grid::coords(CLAMP(rowcol.row(), first_row, last_row),
                                 CLAMP(rowcol.column(), 0, m_column_count - 1));
}

/*
 * Track mouse click and drag positions (the "origin" and "last" coordinates) with half cell accuracy,
 * that is, know whether the event occurred over the left/start or right/end half of the cell.
 * This is required because some selection modes care about the cell over which the event occurred,
 * while some care about the closest boundary between cells.
 *
 * Storing the actual view coordinates would become problematic when the font size changes (bug 756058),
 * and would cause too much work when the mouse moves within the half cell.
 *
 * Left/start margin or anything further to the left/start is denoted by column -1's right half,
 * right/end margin or anything further to the right/end is denoted by column m_column_count's left half.
 *
 * BiDi: returns logical position (start or end) for normal selection modes, visual position (left or
 * right) for block mode.
 */
vte::grid::halfcoords
Terminal::selection_grid_halfcoords_from_view_coords(vte::view::coords const& pos) const
{
        // Callers need to update the ringview. However, don't assert, just
        // return out-of-view coords. FIXME: may want to throw instead
        if (!m_ringview.is_updated())
                [[unlikely]] return {-1, {-1, 1}};

        vte::grid::row_t row = pixel_to_row(pos.y);
        vte::grid::column_t col;
        vte::grid::half_t half;

        if (pos.x < 0) {
                col = -1;
                half = 1;
        } else if (pos.x >= m_column_count * m_cell_width) {
                col = m_column_count;
                half = 0;
        } else {
                col = pos.x / m_cell_width;
                half = (pos.x * 2 / m_cell_width) % 2;
        }

        if (!m_selection_block_mode) {
                /* BiDi: convert from visual to logical half column. */
                vte::base::BidiRow const* bidirow = m_ringview.get_bidirow(confine_grid_row(row));

                if (bidirow->vis_is_rtl(col))
                        half = 1 - half;
                col = bidirow->vis2log(col);
        }

        return { row, vte::grid::halfcolumn_t(col, half) };
}

/*
 * Called on Shift+Click to continue (extend or shrink) the previous selection.
 * Swaps the two endpoints of the selection if needed, so that m_selection_origin
 * contains the new fixed point and m_selection_last is the newly dragged end.
 * In block mode it might even switch to the other two corners.
 * As per GTK+'s generic selection behavior, retains the origin and last
 * endpoints if the Shift+click happened inside the selection.
 */
void
Terminal::selection_maybe_swap_endpoints(vte::view::coords const& pos)
{
        if (m_selection_resolved.empty())
                return;

        /* Need to ensure the ringview is updated. */
        ringview_update();

        auto current = selection_grid_halfcoords_from_view_coords (pos);

        if (m_selection_block_mode) {
                if ((current.row() <= m_selection_origin.row() && m_selection_origin.row() < m_selection_last.row()) ||
                    (current.row() >= m_selection_origin.row() && m_selection_origin.row() > m_selection_last.row())) {
                        // FIXME see if we can use std::swap()
                        auto tmp = m_selection_origin.row();
                        m_selection_origin.set_row(m_selection_last.row());
                        m_selection_last.set_row(tmp);
                }
                if ((current.halfcolumn() <= m_selection_origin.halfcolumn() && m_selection_origin.halfcolumn() < m_selection_last.halfcolumn()) ||
                    (current.halfcolumn() >= m_selection_origin.halfcolumn() && m_selection_origin.halfcolumn() > m_selection_last.halfcolumn())) {
                        // FIXME see if we can use std::swap()
                        auto tmp = m_selection_origin.halfcolumn();
                        m_selection_origin.set_halfcolumn(m_selection_last.halfcolumn());
                        m_selection_last.set_halfcolumn(tmp);
                }
        } else {
                if ((current <= m_selection_origin && m_selection_origin < m_selection_last) ||
                    (current >= m_selection_origin && m_selection_origin > m_selection_last)) {
                        using std::swap;
                        swap(m_selection_origin, m_selection_last);
                }
        }

        _vte_debug_print(VTE_DEBUG_SELECTION,
                         "Selection maybe swap endpoints: origin=%s last=%s\n",
                         m_selection_origin.to_string(),
                         m_selection_last.to_string());
}

bool
Terminal::rowcol_from_event(vte::platform::MouseEvent const& event,
                            long *column,
                            long *row)
{
        auto rowcol = grid_coords_from_event(event);
        if (!grid_coords_visible(rowcol))
                return false;

        *column = rowcol.column();
        *row = rowcol.row();
        return true;
}

#if VTE_GTK == 4

bool
Terminal::rowcol_at(double x,
                    double y,
                    long* column,
                    long* row)
{
        auto const vcoords = vte::view::coords(x - m_border.left, y - m_border.top);
        auto const rowcol = grid_coords_from_view_coords(vcoords);
        if (!grid_coords_visible(rowcol))
                return false;

        *column = rowcol.column();
        *row = rowcol.row();
        return true;
}

char*
Terminal::hyperlink_check_at(double x,
                             double y)
{
        /* Need to ensure the ringview is updated. */
        ringview_update();

        long col, row;
        if (!rowcol_at(x, y, &col, &row))
                return nullptr;

        return hyperlink_check(col, row);
}

#endif /* VTE_GTK == 4 */

char*
Terminal::hyperlink_check(vte::platform::MouseEvent const& event)
{
        /* Need to ensure the ringview is updated. */
        ringview_update();

        long col, row;
        if (!rowcol_from_event(event, &col, &row))
                return nullptr;

        return hyperlink_check(col, row);
}

char*
Terminal::hyperlink_check(vte::grid::column_t col,
                          vte::grid::row_t row)
{
        const char *hyperlink;
        const char *separator;

        if (!m_allow_hyperlink)
                return NULL;

        // Caller needs to update the ringview.
        if (!m_ringview.is_updated())
                [[unlikely]] return nullptr;

        m_screen->row_data->get_hyperlink_at_position(row, col, false, &hyperlink);

        if (hyperlink != NULL) {
                /* URI is after the first semicolon */
                separator = strchr(hyperlink, ';');
                g_assert(separator != NULL);
                hyperlink = separator + 1;
        }

        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                          "hyperlink_check: \"%s\"\n",
                          hyperlink);

        return g_strdup(hyperlink);
}

char*
Terminal::regex_match_check(vte::platform::MouseEvent const& event,
                            int *tag)
{
        /* Need to ensure the ringview is updated. */
        ringview_update();

        long col, row;
        if (!rowcol_from_event(event, &col, &row))
                return nullptr;

        /* FIXME Shouldn't rely on a deprecated, not sub-row aware method. */
        // FIXMEchpe fix this scroll_delta substraction!
        return regex_match_check(col, row - long(m_screen->scroll_delta), tag);
}

#if VTE_GTK == 4

char*
Terminal::regex_match_check_at(double x,
                               double y,
                               int *tag)
{
        /* Need to ensure the ringview is updated. */
        ringview_update();

        long col, row;
        if (!rowcol_at(x, y, &col, &row)) {
                if (tag)
                        *tag = -1;
                return nullptr;
        }

        /* FIXME Shouldn't rely on a deprecated, not sub-row aware method. */
        // FIXMEchpe fix this scroll_delta substraction!
        return regex_match_check(col, row - long(m_screen->scroll_delta), tag);
}

bool
Terminal::regex_match_check_extra_at(double x,
                                     double y,
                                     vte::base::Regex const** regexes,
                                     size_t n_regexes,
                                     uint32_t match_flags,
                                     char** matches)
{
        /* Need to ensure the ringview is updated. */
        ringview_update();

        long col, row;
        if (!rowcol_at(x, y, &col, &row))
                return false;

        return regex_match_check_extra(col, row, regexes, n_regexes, match_flags, matches);
}

#endif /* VTE_GTK == 4 */

bool
Terminal::regex_match_check_extra(vte::platform::MouseEvent const& event,
                                  vte::base::Regex const** regexes,
                                  size_t n_regexes,
                                  uint32_t match_flags,
                                  char** matches)
{
        /* Need to ensure the ringview is updated. */
        ringview_update();

        long col, row;
        if (!rowcol_from_event(event, &col, &row))
                return false;

        return regex_match_check_extra(col, row, regexes, n_regexes, match_flags, matches);
}

bool
Terminal::regex_match_check_extra(vte::grid::column_t col,
                                  vte::grid::row_t row,
                                  vte::base::Regex const** regexes,
                                  size_t n_regexes,
                                  uint32_t match_flags,
                                  char** matches)
{
	gsize offset, sattr, eattr;
        bool any_matches = false;
        guint i;

        assert(regexes != nullptr || n_regexes == 0);
        assert(matches != nullptr);

        // Caller needs to update the ringview.
        if (!m_ringview.is_updated())
                [[unlikely]] return false;

	if (m_match_contents->len == 0)
		match_contents_refresh();

        if (!match_rowcol_to_offset(col, row,
                                    &offset, &sattr, &eattr))
                return false;

        auto match_context = create_match_context();
        auto match_data = vte::take_freeable(pcre2_match_data_create_8(256 /* should be plenty */,
                                                                       nullptr /* general context */));

        for (i = 0; i < n_regexes; i++) {
                gsize start, end, sblank, eblank;
                char *match_string;

                g_return_val_if_fail(regexes[i] != nullptr, false);

                if (match_check_pcre(match_data.get(), match_context.get(),
                                     regexes[i], match_flags,
                                     sattr, eattr, offset,
                                     &match_string,
                                     &start, &end,
                                     &sblank, &eblank)) {
                        _vte_debug_print(VTE_DEBUG_REGEX, "Matched regex with text: %s\n", match_string);
                        matches[i] = match_string;
                        any_matches = true;
                } else
                        matches[i] = nullptr;
        }

        return any_matches;
}

/* Emit an adjustment changed signal on our adjustment object. */
void
Terminal::emit_adjustment_changed()
{
        if (!widget())
                return;

        if (m_adjustment_changed_pending) {
                widget()->notify_scroll_bounds_changed(m_adjustment_value_changed_pending);

                m_adjustment_changed_pending = m_adjustment_value_changed_pending = false;
        }
        else if (m_adjustment_value_changed_pending) {
                widget()->notify_scroll_value_changed();

                m_adjustment_value_changed_pending = false;
        }
}

/* Queue an adjustment-changed signal to be delivered when convenient. */
// FIXMEchpe this has just one caller, fold it into the call site
void
Terminal::queue_adjustment_changed()
{
	m_adjustment_changed_pending = true;
	add_process_timeout(this);
}

void
Terminal::queue_adjustment_value_changed(double v)
{
        /* FIXME: do this check in pixel space? */
	if (_vte_double_equal(v, m_screen->scroll_delta))
                return;

        _vte_debug_print(VTE_DEBUG_ADJ,
                         "Scroll value changed to %f\n", v);

	/* Save the difference. */
	auto const dy = v - m_screen->scroll_delta;

        m_screen->scroll_delta = v;
        m_adjustment_value_changed_pending = true;
        add_process_timeout(this);

        if (!widget_realized()) [[unlikely]]
                return;

        _vte_debug_print(VTE_DEBUG_ADJ,
                         "Scrolling by %f\n", dy);

        m_ringview.invalidate();
        invalidate_all();
        match_contents_clear();
        emit_text_scrolled(dy);
        queue_contents_changed();
}

void
Terminal::queue_adjustment_value_changed_clamped(double v)
{
        auto const lower = m_screen->row_data->delta();
        auto const upper_minus_row_count = m_screen->insert_delta;

        v = std::clamp(v,
                       double(lower),
                       double(std::max(long(lower), upper_minus_row_count)));
	queue_adjustment_value_changed(v);
}

void
Terminal::adjust_adjustments()
{
	queue_adjustment_changed();

	/* The lower value should be the first row in the buffer. */
	long delta = m_screen->row_data->delta();
	/* Snap the insert delta and the cursor position to be in the visible
	 * area.  Leave the scrolling delta alone because it will be updated
	 * when the adjustment changes. */
	m_screen->insert_delta = MAX(m_screen->insert_delta, delta);
        m_screen->cursor.row = MAX(m_screen->cursor.row,
                                   m_screen->insert_delta);

	if (m_screen->scroll_delta > m_screen->insert_delta) {
		queue_adjustment_value_changed(m_screen->insert_delta);
	}
}

/* Update the adjustment field of the widget.  This function should be called
 * whenever we add rows to or remove rows from the history or switch screens. */
void
Terminal::adjust_adjustments_full()
{
	adjust_adjustments();
	queue_adjustment_changed();
}

/* Scroll a fixed number of lines up or down in the current screen. */
void
Terminal::scroll_lines(long lines)
{
	double destination;
	_vte_debug_print(VTE_DEBUG_ADJ, "Scrolling %ld lines.\n", lines);
	/* Calculate the ideal position where we want to be before clamping. */
	destination = m_screen->scroll_delta;
        /* Snap to whole cell offset. */
        if (lines > 0)
                destination = floor(destination);
        else if (lines < 0)
                destination = ceil(destination);
	destination += lines;
	/* Tell the scrollbar to adjust itself. */
	queue_adjustment_value_changed_clamped(destination);
}

/* Scroll so that the scroll delta is the minimum value. */
void
Terminal::scroll_to_top()
{
	queue_adjustment_value_changed(m_screen->row_data->delta());
}

void
Terminal::scroll_to_bottom()
{
	queue_adjustment_value_changed(m_screen->insert_delta);
	_vte_debug_print(VTE_DEBUG_ADJ,
			"Snapping to bottom of screen\n");
}

void
Terminal::scroll_to_previous_prompt()
{
        long row = ceil(m_screen->scroll_delta) - 1;
        row = MAX(row, (long) m_screen->row_data->delta());

        while (row > (long) m_screen->row_data->delta()) {
                if (m_screen->row_data->contains_prompt_beginning(row)) {
                        break;
                }
                row--;
        }

        queue_adjustment_value_changed_clamped(row);
}

void
Terminal::scroll_to_next_prompt()
{
        long row = floor(m_screen->scroll_delta) + 1;
        row = MIN(row, m_screen->insert_delta);

        while (row < m_screen->insert_delta) {
                if (m_screen->row_data->contains_prompt_beginning(row)) {
                        break;
                }
                row++;
        }

        queue_adjustment_value_changed_clamped(row);
}

/*
 * Terminal::set_encoding:
 * @charset: (allow-none): target charset, or %NULL to use UTF-8
 *
 * Changes the encoding the terminal will expect data from the child to
 * be encoded with.  If @charset is %NULL, it uses "UTF-8".
 *
 * Returns: %true if the encoding could be changed to the specified one
 */
bool
Terminal::set_encoding(char const* charset,
                       GError** error)
{
        auto const to_utf8 = bool{charset == nullptr || g_ascii_strcasecmp(charset, "UTF-8") == 0};
        auto const primary_is_current = (current_data_syntax() == primary_data_syntax());

#if WITH_ICU
        /* Note that if the current data syntax is not a primary one, the change
         * will only be applied when returning to the primrary data syntax.
         */

        if (to_utf8) {
                if (primary_data_syntax() == DataSyntax::ECMA48_UTF8)
                        return true;

                m_converter.reset();
                m_oneoff_decoder.reset();
                m_primary_data_syntax = DataSyntax::ECMA48_UTF8;
        } else {
                if (primary_data_syntax() == DataSyntax::ECMA48_PCTERM &&
                    m_converter->charset() == charset)
                        return true;

                try {
                        auto converter = vte::base::ICUConverter::make(charset, error);
                        if (!converter)
                               return false;

                        m_converter = std::move(converter);
                        m_primary_data_syntax = DataSyntax::ECMA48_PCTERM;

                } catch (...) {
                        return vte::glib::set_error_from_exception(error);
                }
        }

        /* Note: we DON'T convert any pending output from the previous charset to
         * the new charset, since that is in general not possible without loss, and
         * also the output may include binary data (Terminal::feed_child_binary()).
         * So we just clear the outgoing queue. (FIXMEchpe: instead, we could flush
         * the outgooing and only change charsets once it's empty.)
         * Do not clear the incoming queue.
         */
        _vte_byte_array_clear(m_outgoing);

        reset_decoder();

        if (pty())
                pty()->set_utf8(primary_data_syntax() == DataSyntax::ECMA48_UTF8);

        if (primary_is_current)
                m_current_data_syntax = m_primary_data_syntax;

	_vte_debug_print(VTE_DEBUG_IO,
                         "Set terminal encoding to \"%s\" data syntax %d\n",
                         encoding(), int(primary_data_syntax()));

        return true;

#else

        if (to_utf8)
                return true;

        g_set_error_literal(error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
                            "ICU support not available");
        return false;
#endif
}

bool
Terminal::set_cjk_ambiguous_width(int width)
{
        g_assert(width == 1 || width == 2);

        if (m_utf8_ambiguous_width == width)
                return false;

        m_utf8_ambiguous_width = width;
        return true;
}

VteRowData *
Terminal::ensure_cursor()
{
	VteRowData *row = ensure_row();
        _vte_row_data_fill(row, &basic_cell, m_screen->cursor.col);

	return row;
}

/* Update the insert delta so that the screen which includes it also
 * includes the end of the buffer. */
void
Terminal::update_insert_delta()
{
	/* The total number of lines.  Add one to the cursor offset
	 * because it's zero-based. */
	auto rows = long(m_screen->row_data->next());
        auto delta = m_screen->cursor.row - rows + 1;
	if (G_UNLIKELY (delta > 0)) {
		insert_rows(delta);
		rows = m_screen->row_data->next();
	}

	/* Make sure that the bottom row is visible, and that it's in
	 * the buffer (even if it's empty).  This usually causes the
	 * top row to become a history-only row. */
	delta = m_screen->insert_delta;
	delta = MIN (delta, rows - m_row_count);
	delta = MAX (long(delta),
                     m_screen->cursor.row - (m_row_count - 1));
	delta = MAX (delta, long(m_screen->row_data->delta()));

	/* Adjust the insert delta and scroll if needed. */
	if (delta != m_screen->insert_delta) {
		m_screen->insert_delta = delta;
		adjust_adjustments();
	}
}

/* Apply the desired mouse pointer, based on certain member variables. */
void
Terminal::apply_mouse_cursor()
{
        if (!widget_realized())
                return;

        /* Show the cursor if over the widget and not autohidden, this is obvious.
         * Also show the cursor if outside the widget regardless of the autohidden state, so that if a popover is opened
         * and then the cursor returns (which doesn't trigger enter/motion events), it is visible.
         * That is, only hide the cursor if it's over the widget and is autohidden.
         * See bug 789390 and bug 789536 comment 6 for details. */
        if (!(m_mouse_autohide && m_mouse_cursor_autohidden && m_mouse_cursor_over_widget)) {
                if (m_hyperlink_hover_idx != 0) {
                        _vte_debug_print(VTE_DEBUG_CURSOR,
                                        "Setting hyperlink mouse cursor.\n");
                        m_real_widget->set_cursor(vte::platform::Widget::CursorType::eHyperlink);
                } else if (regex_match_has_current()) {
                        m_real_widget->set_cursor(regex_match_current()->cursor());
                } else if (m_mouse_tracking_mode != MouseTrackingMode::eNONE) {
			_vte_debug_print(VTE_DEBUG_CURSOR,
					"Setting mousing cursor.\n");
                        m_real_widget->set_cursor(vte::platform::Widget::CursorType::eMousing);
		} else {
			_vte_debug_print(VTE_DEBUG_CURSOR,
					"Setting default mouse cursor.\n");
                        m_real_widget->set_cursor(vte::platform::Widget::CursorType::eDefault);
		}
	} else {
		_vte_debug_print(VTE_DEBUG_CURSOR,
				"Setting to invisible cursor.\n");
                m_real_widget->set_cursor(vte::platform::Widget::CursorType::eInvisible);
	}
}

/* Show or hide the pointer if autohiding is enabled. */
void
Terminal::set_pointer_autohidden(bool autohidden)
{
        if (autohidden == m_mouse_cursor_autohidden)
                return;

        m_mouse_cursor_autohidden = autohidden;

        if (m_mouse_autohide) {
                hyperlink_hilite_update();
                match_hilite_update();
                apply_mouse_cursor();
        }
}

/*
 * Get the actually used color from the palette.
 * The return value can be NULL only if entry is one of VTE_CURSOR_BG,
 * VTE_CURSOR_FG, VTE_HIGHLIGHT_BG or VTE_HIGHLIGHT_FG.
 */
vte::color::rgb const*
Terminal::get_color(int entry) const
{
	VtePaletteColor const* palette_color = &m_palette[entry];
	guint source;
	for (source = 0; source < G_N_ELEMENTS(palette_color->sources); source++)
		if (palette_color->sources[source].is_set)
			return &palette_color->sources[source].color;
	return nullptr;
}

vte::color::rgb const*
Terminal::get_color(color_palette::ColorPaletteIndex entry) const noexcept
{
        return get_color(entry.value());
}

auto
Terminal::get_color_opt(const ColorPaletteIndex entry) const noexcept -> std::optional<vte::color::rgb>
{
        auto const color = get_color(entry);
        return color != nullptr ? std::make_optional(*color) : std::nullopt;
}

/* Set up a palette entry with a more-or-less match for the requested color. */
void
Terminal::set_color(int entry,
                    color_palette::ColorSource source_,
                    vte::color::rgb const& proposed)
{
        g_assert(entry >= 0 && entry < VTE_PALETTE_SIZE);

	VtePaletteColor *palette_color = &m_palette[entry];

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Set %s color[%d] to (%04x,%04x,%04x).\n",
                         source_ == color_palette::ColorSource::Escape ? "escape" : "API",
                         entry, proposed.red, proposed.green, proposed.blue);

        auto const source = vte::to_integral(source_);
        if (palette_color->sources[source].is_set &&
            palette_color->sources[source].color == proposed) {
                return;
        }
        palette_color->sources[source].is_set = TRUE;
        palette_color->sources[source].color = proposed;

	/* If we're not realized yet, there's nothing else to do. */
	if (!widget_realized())
		return;

	/* and redraw */
	if (entry == VTE_CURSOR_BG || entry == VTE_CURSOR_FG)
		invalidate_cursor_once();
	else
		invalidate_all();
}

void
Terminal::set_color(color_palette::ColorPaletteIndex entry,
                    color_palette::ColorSource source,
                    vte::color::rgb const& proposed)
{
        return set_color(entry.value(), source, proposed);
}

void
Terminal::reset_color(int entry,
                      color_palette::ColorSource source_)
{
        g_assert(entry >= 0 && entry < VTE_PALETTE_SIZE);

	VtePaletteColor *palette_color = &m_palette[entry];

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Reset %s color[%d].\n",
                         source_ == color_palette::ColorSource::Escape ? "escape" : "API",
                         entry);

        auto const source = vte::to_integral(source_);
        if (!palette_color->sources[source].is_set) {
                return;
        }
        palette_color->sources[source].is_set = FALSE;

	/* If we're not realized yet, there's nothing else to do. */
	if (!widget_realized())
		return;

	/* and redraw */
	if (entry == VTE_CURSOR_BG || entry == VTE_CURSOR_FG)
		invalidate_cursor_once();
	else
		invalidate_all();
}

void
Terminal::reset_color(color_palette::ColorPaletteIndex entry,
                      color_palette::ColorSource source)
{
        return reset_color(entry.value(), source);
}

bool
Terminal::set_background_alpha(double alpha)
{
        g_assert(alpha >= 0. && alpha <= 1.);

        if (_vte_double_equal(alpha, m_background_alpha))
                return false;

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Setting background alpha to %.3f\n", alpha);
        m_background_alpha = alpha;

        invalidate_all();

        return true;
}

void
Terminal::set_colors_default()
{
        set_colors(nullptr, nullptr, nullptr, 0);
}

/*
 * Terminal::set_colors:
 * @terminal: a #VteTerminal
 * @foreground: (allow-none): the new foreground color, or %NULL
 * @background: (allow-none): the new background color, or %NULL
 * @palette: (array length=palette_size zero-terminated=0): the color palette
 * @palette_size: the number of entries in @palette
 *
 * @palette specifies the new values for the 256 palette colors: 8 standard colors,
 * their 8 bright counterparts, 6x6x6 color cube, and 24 grayscale colors.
 * Omitted entries will default to a hardcoded value.
 *
 * @palette_size must be 0, 8, 16, 232 or 256.
 *
 * If @foreground is %NULL and @palette_size is greater than 0, the new foreground
 * color is taken from @palette[7].  If @background is %NULL and @palette_size is
 * greater than 0, the new background color is taken from @palette[0].
 */
void
Terminal::set_colors(vte::color::rgb const* foreground,
                               vte::color::rgb const* background,
                               vte::color::rgb const* new_palette,
                               gsize palette_size)
{
	_vte_debug_print(VTE_DEBUG_MISC,
			"Set color palette [%" G_GSIZE_FORMAT " elements].\n",
			palette_size);

	/* Accept NULL as the default foreground and background colors if we
	 * got a palette. */
	if ((foreground == NULL) && (palette_size >= 8)) {
		foreground = &new_palette[7];
	}
	if ((background == NULL) && (palette_size >= 8)) {
		background = &new_palette[0];
	}

	/* Initialize each item in the palette if we got any entries to work
	 * with. */
	for (gsize i = 0; i < G_N_ELEMENTS(m_palette); i++) {
                vte::color::rgb color;
		bool unset = false;

		if (i < 16) {
			color.blue = (i & 4) ? 0xc000 : 0;
			color.green = (i & 2) ? 0xc000 : 0;
			color.red = (i & 1) ? 0xc000 : 0;
			if (i > 7) {
				color.blue += 0x3fff;
				color.green += 0x3fff;
				color.red += 0x3fff;
			}
		}
		else if (i < 232) {
			int j = i - 16;
			int r = j / 36, g = (j / 6) % 6, b = j % 6;
			int red =   (r == 0) ? 0 : r * 40 + 55;
			int green = (g == 0) ? 0 : g * 40 + 55;
			int blue =  (b == 0) ? 0 : b * 40 + 55;
			color.red   = red | red << 8  ;
			color.green = green | green << 8;
			color.blue  = blue | blue << 8;
		} else if (i < 256) {
			int shade = 8 + (i - 232) * 10;
			color.red = color.green = color.blue = shade | shade << 8;
		}
		else switch (i) {
			case VTE_DEFAULT_BG:
				if (background) {
					color = *background;
				} else {
					color.red = 0;
					color.blue = 0;
					color.green = 0;
				}
				break;
			case VTE_DEFAULT_FG:
				if (foreground) {
					color = *foreground;
				} else {
					color.red = 0xc000;
					color.blue = 0xc000;
					color.green = 0xc000;
				}
				break;
			case VTE_BOLD_FG:
                                unset = true;
                                break;
			case VTE_HIGHLIGHT_BG:
				unset = true;
				break;
			case VTE_HIGHLIGHT_FG:
				unset = true;
				break;
			case VTE_CURSOR_BG:
				unset = true;
				break;
			case VTE_CURSOR_FG:
				unset = true;
				break;
			}

		/* Override from the supplied palette if there is one. */
		if (i < palette_size) {
			color = new_palette[i];
		}

		/* Set up the color entry. */
                if (unset)
                        reset_color(i, color_palette::ColorSource::API);
                else
                        set_color(i, color_palette::ColorSource::API, color);
	}
}

/*
 * Terminal::set_color_bold:
 * @bold: (allow-none): the new bold color or %NULL
 *
 * Sets the color used to draw bold text in the default foreground color.
 * If @bold is %NULL then the default color is used.
 */
void
Terminal::set_color_bold(vte::color::rgb const& color)
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Set %s color to (%04x,%04x,%04x).\n", "bold",
                         color.red, color.green, color.blue);
        set_color(ColorPaletteIndex::bold_fg(), ColorSource::API, color);
}

void
Terminal::reset_color_bold()
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Reset %s color.\n", "bold");
        reset_color(ColorPaletteIndex::bold_fg(), ColorSource::API);
}

/*
 * Terminal::set_color_foreground:
 * @foreground: the new foreground color
 *
 * Sets the foreground color used to draw normal text.
 */
void
Terminal::set_color_foreground(vte::color::rgb const& color)
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Set %s color to (%04x,%04x,%04x).\n", "foreground",
                         color.red, color.green, color.blue);
	set_color(ColorPaletteIndex::default_fg(), ColorSource::API, color);
}

/*
 * Terminal::set_color_background:
 * @background: the new background color
 *
 * Sets the background color for text which does not have a specific background
 * color assigned.  Only has effect when no background image is set and when
 * the terminal is not transparent.
 */
void
Terminal::set_color_background(vte::color::rgb const& color)
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Set %s color to (%04x,%04x,%04x).\n", "background",
                         color.red, color.green, color.blue);
	set_color(ColorPaletteIndex::default_bg(), ColorSource::API, color);
}

/*
 * Terminal::set_color_cursor_background:
 * @cursor_background: (allow-none): the new color to use for the text cursor, or %NULL
 *
 * Sets the background color for text which is under the cursor.  If %NULL, text
 * under the cursor will be drawn with foreground and background colors
 * reversed.
 */
void
Terminal::set_color_cursor_background(vte::color::rgb const& color)
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Set %s color to (%04x,%04x,%04x).\n", "cursor background",
                         color.red, color.green, color.blue);
	set_color(ColorPaletteIndex::cursor_bg(), ColorSource::API, color);
}

void
Terminal::reset_color_cursor_background()
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Reset %s color.\n", "cursor background");
        reset_color(ColorPaletteIndex::cursor_bg(), ColorSource::API);
}

/*
 * Terminal::set_color_cursor_foreground:
 * @cursor_foreground: (allow-none): the new color to use for the text cursor, or %NULL
 *
 * Sets the foreground color for text which is under the cursor.  If %NULL, text
 * under the cursor will be drawn with foreground and background colors
 * reversed.
 */
void
Terminal::set_color_cursor_foreground(vte::color::rgb const& color)
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Set %s color to (%04x,%04x,%04x).\n", "cursor foreground",
                         color.red, color.green, color.blue);
	set_color(ColorPaletteIndex::cursor_fg(), ColorSource::API, color);
}

void
Terminal::reset_color_cursor_foreground()
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Reset %s color.\n", "cursor foreground");
        reset_color(ColorPaletteIndex::cursor_fg(), ColorSource::API);
}

/*
 * Terminal::set_color_highlight_background:
 * @highlight_background: (allow-none): the new color to use for highlighted text, or %NULL
 *
 * Sets the background color for text which is highlighted.  If %NULL,
 * it is unset.  If neither highlight background nor highlight foreground are set,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 */
void
Terminal::set_color_highlight_background(vte::color::rgb const& color)
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Set %s color to (%04x,%04x,%04x).\n", "highlight background",
                         color.red, color.green, color.blue);
	set_color(ColorPaletteIndex::highlight_bg(), ColorSource::API, color);
}

void
Terminal::reset_color_highlight_background()
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Reset %s color.\n", "highlight background");
        reset_color(ColorPaletteIndex::highlight_bg(), ColorSource::API);
}

/*
 * Terminal::set_color_highlight_foreground:
 * @highlight_foreground: (allow-none): the new color to use for highlighted text, or %NULL
 *
 * Sets the foreground color for text which is highlighted.  If %NULL,
 * it is unset.  If neither highlight background nor highlight foreground are set,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 */
void
Terminal::set_color_highlight_foreground(vte::color::rgb const& color)
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Set %s color to (%04x,%04x,%04x).\n", "highlight foreground",
                         color.red, color.green, color.blue);
	set_color(ColorPaletteIndex::highlight_fg(), ColorSource::API, color);
}

void
Terminal::reset_color_highlight_foreground()
{
        _vte_debug_print(VTE_DEBUG_MISC,
                         "Reset %s color.\n", "highlight foreground");
        reset_color(ColorPaletteIndex::highlight_fg(), ColorSource::API);
}

/*
 * Terminal::cleanup_fragments:
 * @rownum: the row to operate on
 * @start: the starting column, inclusive
 * @end: the end column, exclusive
 *
 * Needs to be called before modifying the contents in the given row,
 * between the two given columns.  Cleans up TAB and CJK fragments to the
 * left of @start and to the right of @end.  If a CJK is split in half,
 * the remaining half is replaced by a space.  If a TAB at @start is split,
 * it is replaced by spaces.  If a TAB at @end is split, it is replaced by
 * a shorter TAB.  @start and @end can be equal if characters will be
 * inserted at the location rather than overwritten.
 *
 * The area between @start and @end is not cleaned up, hence the whole row
 * can be left in an inconsistent state.  It is expected that the caller
 * will fill up that range afterwards, resulting in a consistent row again.
 *
 * Invalidates the cells that visually change outside of the range,
 * because the caller can't reasonably be expected to take care of this.
 * FIXME This is obviously a leftover from the days when we invalidated
 * arbitrary rectangles rather than entire rows; we should revise this.
 */
void
Terminal::cleanup_fragments(VteRowData* row,
                            long rownum,
                            long start,
                            long end)
{
        const VteCell *cell_start;
        VteCell *cell_end, *cell_col;
        gboolean cell_start_is_fragment;
        long col;

        g_assert(end >= start);

        /* Remember whether the cell at start is a fragment.  We'll need to know it when
         * handling the left hand side, but handling the right hand side first might
         * overwrite it if start == end (inserting to the middle of a character). */
        cell_start = _vte_row_data_get (row, start);
        cell_start_is_fragment = cell_start != NULL && cell_start->attr.fragment();

        /* On the right hand side, try to replace a TAB by a shorter TAB if we can.
         * This requires that the TAB on the left (which might be the same TAB) is
         * not yet converted to spaces, so start on the right hand side. */
        cell_end = _vte_row_data_get_writable (row, end);
        if (G_UNLIKELY (cell_end != NULL && cell_end->attr.fragment())) {
                col = end;
                do {
                        col--;
                        g_assert(col >= 0);  /* The first cell can't be a fragment. */
                        cell_col = _vte_row_data_get_writable (row, col);
                } while (cell_col->attr.fragment());
                if (cell_col->c == '\t') {
                        _vte_debug_print(VTE_DEBUG_MISC,
                                         "Replacing right part of TAB with a shorter one at %ld (%ld cells) => %ld (%ld cells)\n",
                                         col, (long) cell_col->attr.columns(), end, (long) cell_col->attr.columns() - (end - col));
                        cell_end->c = '\t';
                        cell_end->attr.set_fragment(false);
                        g_assert(cell_col->attr.columns() > end - col);
                        cell_end->attr.set_columns(cell_col->attr.columns() - (end - col));
                } else {
                        _vte_debug_print(VTE_DEBUG_MISC,
                                         "Cleaning CJK right half at %ld\n",
                                         end);
                        g_assert(end - col == 1 && cell_col->attr.columns() == 2);
                        cell_end->c = ' ';
                        cell_end->attr.set_fragment(false);
                        cell_end->attr.set_columns(1);
                        invalidate_row_and_context(rownum);  /* FIXME can we do cheaper? */
                }
        }

        /* Handle the left hand side.  Converting longer TABs to shorter ones probably
         * wouldn't make that much sense here, so instead convert to spaces. */
        if (G_UNLIKELY (cell_start_is_fragment)) {
                gboolean keep_going = TRUE;
                col = start;
                do {
                        col--;
                        g_assert(col >= 0);  /* The first cell can't be a fragment. */
                        cell_col = _vte_row_data_get_writable (row, col);
                        if (!cell_col->attr.fragment()) {
                                if (cell_col->c == '\t') {
                                        _vte_debug_print(VTE_DEBUG_MISC,
                                                         "Replacing left part of TAB with spaces at %ld (%ld => %ld cells)\n",
                                                         col, (long)cell_col->attr.columns(), start - col);
                                        /* nothing to do here */
                                } else {
                                        _vte_debug_print(VTE_DEBUG_MISC,
                                                         "Cleaning CJK left half at %ld\n",
                                                         col);
                                        g_assert(start - col == 1);
                                        invalidate_row_and_context(rownum);  /* FIXME can we do cheaper? */
                                }
                                keep_going = FALSE;
                        }
                        cell_col->c = ' ';
                        cell_col->attr.set_fragment(false);
                        cell_col->attr.set_columns(1);
                } while (keep_going);
        }
}

/* Terminal::scroll_text_up:
 *
 * Scrolls the text upwards by the given amount, within the given custom scrolling region.
 * The DECSTBM / DECSLRM scrolling region (or lack thereof) is irrelevant (unless, of course,
 * it is the one passed to this method).
 *
 * If the entire topmost row is part of the scrolling region then the scrolled out lines go
 * to the scrollback buffer.
 *
 * "fill" tells whether to fill the new lines with the background color.
 *
 * The cursor's position is irrelevant, and it stays where it was (relative to insert_delta).
 */
void
Terminal::scroll_text_up(scrolling_region const& scrolling_region,
                         vte::grid::row_t amount, bool fill)
{
        auto const top = m_screen->insert_delta + scrolling_region.top();
        auto const bottom = m_screen->insert_delta + scrolling_region.bottom();
        auto const left = scrolling_region.left();
        auto const right = scrolling_region.right();

        amount = CLAMP(amount, 1, bottom - top + 1);

        /* Make sure the ring covers the area we'll operate on. */
        while (long(m_screen->row_data->next()) <= bottom) [[unlikely]]
                ring_append(false /* no fill */);

        if (!scrolling_region.is_restricted()) [[likely]] {
                /* Scroll up the entire screen, with history. This is functionally equivalent to the
                 * next branch, but is a bit faster, and speed does matter in this very common case. */
                m_screen->insert_delta += amount;
                m_screen->cursor.row += amount;
                while (amount--) {
                        ring_append(fill);
                }
                /* Force scroll. */
                adjust_adjustments();
        } else if (scrolling_region.top() == 0 && left == 0 && right == m_column_count - 1) {
                /* Scroll up whole rows at the top (but not the entire screen), with history. */

                /* Set the boundary to hard wrapped where we'll tear apart the contents. */
                set_hard_wrapped(bottom);
                /* Scroll (and add to history) by inserting new lines. */
                m_screen->insert_delta += amount;
                m_screen->cursor.row += amount;
                for (auto insert_at = bottom + 1; insert_at <= bottom + amount; insert_at++) {
                        ring_insert(insert_at, fill);
                }
                /* Repaint the affected lines, which is _below_ the region, see
                 * https://gitlab.gnome.org/GNOME/vte/-/issues/131.
                 * No need to extend, set_hard_wrapped() took care of invalidating
                 * the context lines if necessary. */
                invalidate_rows(bottom + 1, m_screen->insert_delta + m_row_count - 1);
                /* Force scroll. */
                adjust_adjustments();
        } else if (left == 0 && right == m_column_count - 1) {
                /* Scroll up whole rows (but not at the top), along with their hard/soft line ending, and BiDi flags.
                 * Don't add to history. */

                /* Set the boundaries to hard wrapped where we'll tear apart or glue together the contents.
                 * Do it before scrolling up, for the bottom row to be the desired one. */
                set_hard_wrapped(top - 1);
                set_hard_wrapped(bottom);
                /* Scroll up by removing a line at the top and inserting a new one at the bottom. */
                // FIXME The runtime is quadratical to the number of lines scrolled,
                // modify ring_remove() and ring_insert() to take an "amount" parameter.
                while (amount--) {
                        ring_remove(top);
                        ring_insert(bottom, fill);
                }
                /* Repaint the affected lines. No need to extend, set_hard_wrapped() took care of
                 * invalidating the context lines if necessary. */
                invalidate_rows(top, bottom);
                /* We've modified the display. Make a note of it. */
                m_text_deleted_flag = TRUE;
        } else {
                /* Scroll up partial rows. The line endings and the BiDi flags don't scroll. */

                /* Make sure the area we're about to scroll is present in memory. */
                long row = top;
                for (row = top; row <= bottom; row++) {
                        _vte_row_data_fill(m_screen->row_data->index_writable(row), &basic_cell, right + 1);
                }
                /* Handle TABs, CJKs, emojis that will be cut in half. */
                for (row = top; row <= bottom; row++) {
                        cleanup_fragments(row, left, left);
                        cleanup_fragments(row, right + 1, right + 1);
                }
                /* Scroll up by copying the cell data. */
                for (row = top; row <= bottom - amount; row++) {
                        VteRowData *dst = m_screen->row_data->index_writable(row);
                        VteRowData *src = m_screen->row_data->index_writable(row + amount);
                        memcpy(dst->cells + left, src->cells + left, (right - left + 1) * sizeof(VteCell));
                }
                /* Erase the cells we scrolled away from. */
                const VteCell *cell = fill ? &m_color_defaults : &basic_cell;
                for (; row <= bottom; row++) {
                        VteRowData *empty = m_screen->row_data->index_writable(row);
                        std::fill_n(&empty->cells[left], right - left + 1, *cell);
                }
                /* Repaint the affected lines, with context if necessary. */
                invalidate_rows_and_context(top, bottom);
                /* We've modified the display. Make a note of it. */
                m_text_deleted_flag = TRUE;
        }
}

/* Terminal::scroll_text_down:
 *
 * Scrolls the text downwards by the given amount, within the given custom scrolling region.
 * The DECSTBM / DECSLRM scrolling region (or lack thereof) is irrelevant (unless, of course,
 * it is the one passed to this method).
 *
 * "fill" tells whether to fill the new lines with the background color.
 *
 * The cursor's position is irrelevant, and it stays where it was.
 */
void
Terminal::scroll_text_down(scrolling_region const& scrolling_region,
                           vte::grid::row_t amount, bool fill)
{
        auto const top = m_screen->insert_delta + scrolling_region.top();
        auto const bottom = m_screen->insert_delta + scrolling_region.bottom();
        auto const left = scrolling_region.left();
        auto const right = scrolling_region.right();

        amount = CLAMP(amount, 1, bottom - top + 1);

        /* Make sure the ring covers the area we'll operate on. */
        while (long(m_screen->row_data->next()) <= bottom) [[unlikely]]
                ring_append(false /* no fill */);

        /* Scroll down. This code is the counterpart of the branches in scroll_text_up() that don't
         * add to the history. */

        if (left == 0 && right == m_column_count - 1) {
                /* Scroll down whole rows, along with their hard/soft line ending, and BiDi flags. */

                /* Scroll down by removing a line at the bottom and inserting a new one at the top. */
                // FIXME The runtime is quadratical to the number of lines scrolled,
                // modify ring_remove() and ring_insert() to take an "amount" parameter.
                while (amount--) {
                        ring_remove(bottom);
                        ring_insert(top, fill);
                }
                /* Set the boundaries to hard wrapped where we tore apart or glued together the contents.
                 * Do it after scrolling down, for the bottom row to be the desired one. */
                set_hard_wrapped(top - 1);
                set_hard_wrapped(bottom);
                /* Repaint the affected lines. No need to extend, set_hard_wrapped() took care of
                 * invalidating the context lines if necessary. */
                invalidate_rows(top, bottom);
                /* We've modified the display. Make a note of it. */
                m_text_deleted_flag = TRUE;
        } else {
                /* Scroll down partial rows. The line endings and the BiDi flags don't scroll. */

                /* Make sure the area we're about to scroll is present in memory. */
                long row = top;
                for (row = top; row <= bottom; row++) {
                        _vte_row_data_fill(m_screen->row_data->index_writable(row), &basic_cell, right + 1);
                }
                /* Handle TABs, CJKs, emojis that will be cut in half. */
                for (row = top; row <= bottom; row++) {
                        cleanup_fragments(row, left, left);
                        cleanup_fragments(row, right + 1, right + 1);
                }
                /* Scroll down by copying the cell data. */
                for (row = bottom; row >= top + amount; row--) {
                        VteRowData *dst = m_screen->row_data->index_writable(row);
                        VteRowData *src = m_screen->row_data->index_writable(row - amount);
                        memcpy(dst->cells + left, src->cells + left, (right - left + 1) * sizeof(VteCell));
                }
                /* Erase the cells we scrolled away from. */
                const VteCell *cell = fill ? &m_color_defaults : &basic_cell;
                for (; row >= top; row--) {
                        VteRowData *empty = m_screen->row_data->index_writable(row);
                        std::fill_n(&empty->cells[left], right - left + 1, *cell);
                }
                /* Repaint the affected lines, with context if necessary. */
                invalidate_rows_and_context(top, bottom);
                /* We've modified the display. Make a note of it. */
                m_text_deleted_flag = TRUE;
        }
}

/* Terminal::scroll_text_left:
 *
 * Scrolls the text to the left by the given amount, within the given custom scrolling region.
 * The DECSTBM / DECSLRM scrolling region (or lack thereof) is irrelevant (unless, of course,
 * it is the one passed to this method).
 *
 * "fill" tells whether to fill the new lines with the background color.
 *
 * The cursor's position is irrelevant, and it stays where it was.
 */
void
Terminal::scroll_text_left(scrolling_region const& scrolling_region,
                           vte::grid::row_t amount, bool fill)
{
        auto const top = m_screen->insert_delta + scrolling_region.top();
        auto const bottom = m_screen->insert_delta + scrolling_region.bottom();
        auto const left = scrolling_region.left();
        auto const right = scrolling_region.right();

        amount = CLAMP(amount, 1, right - left + 1);

        /* Make sure the ring covers the area we'll operate on. */
        while (long(m_screen->row_data->next()) <= bottom) [[unlikely]]
                ring_append(false /* no fill */);

        const VteCell *cell = fill ? &m_color_defaults : &basic_cell;

        /* Scroll left in each row separately. */
        for (auto row = top; row <= bottom; row++) {
                /* Make sure the area we're about to scroll is present in memory. */
                _vte_row_data_fill(m_screen->row_data->index_writable(row), &basic_cell, right + 1);
                /* Handle TABs, CJKs, emojis that will be cut in half. */
                cleanup_fragments(row, left, left + amount);
                cleanup_fragments(row, right + 1, right + 1);
                /* Scroll left by copying the cell data. */
                VteRowData *rowdata = m_screen->row_data->index_writable(row);
                memmove(rowdata->cells + left, rowdata->cells + left + amount, (right - left + 1 - amount) * sizeof(VteCell));
                /* Erase the cells we scrolled away from. */
                std::fill_n(&rowdata->cells[right + 1 - amount], amount, *cell);
                /* To match xterm, we modify the line endings of the scrolled lines to hard newline.
                 * This differs from scroll_text_right(). */
                set_hard_wrapped(row);
        }
        /* Repaint the affected lines, with context if necessary. */
        invalidate_rows_and_context(top, bottom);
        /* We've modified the display. Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Terminal::scroll_text_right:
 *
 * Scrolls the text to the right by the given amount, within the given custom scrolling region.
 * The DECSTBM / DECSLRM scrolling region (or lack thereof) is irrelevant (unless, of course,
 * it is the one passed to this method).
 *
 * "fill" tells whether to fill the new lines with the background color.
 *
 * The cursor's position is irrelevant, and it stays where it was.
 */
void
Terminal::scroll_text_right(scrolling_region const& scrolling_region,
                            vte::grid::row_t amount, bool fill)
{
        auto const top = m_screen->insert_delta + scrolling_region.top();
        auto const bottom = m_screen->insert_delta + scrolling_region.bottom();
        auto const left = scrolling_region.left();
        auto const right = scrolling_region.right();

        amount = CLAMP(amount, 1, right - left + 1);

        /* Make sure the ring covers the area we'll operate on. */
        while (long(m_screen->row_data->next()) <= bottom) [[unlikely]]
                ring_append(false /* no fill */);

        const VteCell *cell = fill ? &m_color_defaults : &basic_cell;

        /* Scroll right in each row separately. */
        for (auto row = top; row <= bottom; row++) {
                /* Make sure the area we're about to scroll is present in memory. */
                _vte_row_data_fill(m_screen->row_data->index_writable(row), &basic_cell, right + 1);
                /* Handle TABs, CJKs, emojis that will be cut in half. */
                cleanup_fragments(row, left, left);
                cleanup_fragments(row, right + 1 - amount, right + 1);
                /* Scroll right by copying the cell data. */
                VteRowData *rowdata = m_screen->row_data->index_writable(row);
                memmove(rowdata->cells + left + amount, rowdata->cells + left, (right - left + 1 - amount) * sizeof(VteCell));
                /* Erase the cells we scrolled away from. */
                std::fill_n(&rowdata->cells[left], amount, *cell);
                /* To match xterm, we don't modify the line endings. This differs from scroll_text_left(). */
        }
        /* Repaint the affected lines, with context if necessary. */
        invalidate_rows_and_context(top, bottom);
        /* We've modified the display. Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Terminal::cursor_down_with_scrolling:
 * Cursor down by one line, with scrolling if needed (respecting the DECSTBM / DECSLRM scrolling region).
 *
 * See the "RI, IND/LF, DECFI, DECBI" picture in ../doc/scrolling-region.txt.
 *
 * DEC STD 070 says not to do anything if the cursor hits the margin outside of the scrolling area.
 * Xterm follows this, and so do we. Reportedly (#2526) DEC terminals move the cursor despite their doc.
 *
 * If the region's top, left and right edges are at the screen's top, left and right then
 * the scrolled out line goes to the scrollback buffer.
 *
 * "fill" tells whether to fill the new line with the background color.
 */
void
Terminal::cursor_down_with_scrolling(bool fill)
{
        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        if (cursor_row == m_scrolling_region.bottom()) {
                /* Hit the bottom row of the scrolling region. */
                if (cursor_col >= m_scrolling_region.left() && cursor_col <= m_scrolling_region.right()) {
                        /* Inside the horizontal margins, scroll the text in the scrolling region. */
                        scroll_text_up(m_scrolling_region, 1, fill);
                } else {
                        /* Outside of the horizontal margins, do nothing. */
                }
        } else if (cursor_row == m_row_count - 1) {
                /* Hit the bottom of the screen outside of the scrolling region, do nothing. */
        } else {
                /* No boundary hit, move the cursor down. process_incoming() takes care of invalidating both rows. */
                m_screen->cursor.row++;
        }
}

/* Terminal::cursor_up_with_scrolling:
 * Cursor up by one line, with scrolling if needed (respecting the DECSTBM / DECSLRM scrolling region).
 *
 * See the "RI, IND/LF, DECFI, DECBI" picture in ../doc/scrolling-region.txt.
 *
 * DEC STD 070 says not to do anything if the cursor hits the margin outside of the scrolling area.
 * Xterm follows this, and so do we. Reportedly (#2526) DEC terminals move the cursor despite their doc.
 *
 * "fill" tells whether to fill the new line with the background color.
 */
void
Terminal::cursor_up_with_scrolling(bool fill)
{
        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        if (cursor_row == m_scrolling_region.top()) {
                /* Hit the top row of the scrolling region. */
                if (cursor_col >= m_scrolling_region.left() && cursor_col <= m_scrolling_region.right()) {
                        /* Inside the horizontal margins, scroll the text in the scrolling region. */
                        scroll_text_down(m_scrolling_region, 1, fill);
                } else {
                        /* Outside of the horizontal margins, do nothing. */
                }
        } else if (cursor_row == 0) {
                /* Hit the top of the screen outside of the scrolling region, do nothing. */
        } else {
                /* No boundary hit, move the cursor up. process_incoming() takes care of invalidating both rows. */
                m_screen->cursor.row--;
        }
}

/* Terminal::cursor_right_with_scrolling:
 * Cursor right by one column, with scrolling if needed (respecting the DECSTBM / DECSLRM scrolling region).
 *
 * See the "RI, IND/LF, DECFI, DECBI" picture in ../doc/scrolling-region.txt.
 *
 * DEC STD 070 says not to do anything if the cursor hits the margin outside of the scrolling area.
 * Xterm follows this, and so do we. Reportedly (#2526) DEC terminals move the cursor despite their doc.
 *
 * "fill" tells whether to fill the new line with the background color.
 */
void
Terminal::cursor_right_with_scrolling(bool fill)
{
        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        if (cursor_col == m_scrolling_region.right()) {
                /* Hit the right column of the scrolling region. */
                if (cursor_row >= m_scrolling_region.top() && cursor_row <= m_scrolling_region.bottom()) {
                        /* Inside the vertical margins, scroll the text in the scrolling region. */
                        scroll_text_left(m_scrolling_region, 1, fill);
                } else {
                        /* Outside of the vertical margins, do nothing. */
                }
        } else if (cursor_col == m_column_count - 1) {
                /* Hit the right edge of the screen outside of the scrolling region, do nothing. */
        } else {
                /* No boundary hit, move the cursor right. process_incoming() takes care of invalidating its row. */
                m_screen->cursor.col++;
        }
}

/* Terminal::cursor_left_with_scrolling:
 * Cursor left by one column, with scrolling if needed (respecting the DECSTBM / DECSLRM scrolling region).
 *
 * See the "RI, IND/LF, DECFI, DECBI" picture in ../doc/scrolling-region.txt.
 *
 * DEC STD 070 says not to do anything if the cursor hits the margin outside of the scrolling area.
 * Xterm follows this, and so do we. Reportedly (#2526) DEC terminals move the cursor despite their doc.
 *
 * "fill" tells whether to fill the new line with the background color.
 */
void
Terminal::cursor_left_with_scrolling(bool fill)
{
        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        if (cursor_col == m_scrolling_region.left()) {
                /* Hit the left column of the scrolling region. */
                if (cursor_row >= m_scrolling_region.top() && cursor_row <= m_scrolling_region.bottom()) {
                        /* Inside the vertical margins, scroll the text in the scrolling region. */
                        scroll_text_right(m_scrolling_region, 1, fill);
                } else {
                        /* Outside of the vertical margins, do nothing. */
                }
        } else if (cursor_col == 0) {
                /* Hit the left edge of the screen outside of the scrolling region, do nothing. */
        } else {
                /* No boundary hit, move the cursor left. process_incoming() takes care of invalidating its row. */
                m_screen->cursor.col--;
        }
}

/* Drop the scrollback. */
void
Terminal::drop_scrollback()
{
        /* Only for normal screen; alternate screen doesn't have a scrollback. */
        m_normal_screen.row_data->drop_scrollback(m_normal_screen.insert_delta);

        if (m_screen == &m_normal_screen) {
                queue_adjustment_value_changed(m_normal_screen.insert_delta);
                adjust_adjustments_full();
                m_ringview.invalidate();
                invalidate_all();
                match_contents_clear();
        }
}

/* Restore cursor on a screen. */
void
Terminal::restore_cursor(VteScreen *screen__)
{
        screen__->cursor.col = screen__->saved.cursor.col;
        screen__->cursor.row = screen__->insert_delta + CLAMP(screen__->saved.cursor.row,
                                                              0, m_row_count - 1);
        screen__->cursor_advanced_by_graphic_character = screen__->saved.cursor_advanced_by_graphic_character;

        m_modes_private.set_DEC_REVERSE_IMAGE(screen__->saved.reverse_mode);
        m_modes_private.set_DEC_ORIGIN(screen__->saved.origin_mode);

        m_defaults = screen__->saved.defaults;
        m_color_defaults = screen__->saved.color_defaults;
        m_character_replacements[0] = screen__->saved.character_replacements[0];
        m_character_replacements[1] = screen__->saved.character_replacements[1];
        m_character_replacement = screen__->saved.character_replacement;
}

/* Save cursor on a screen__. */
void
Terminal::save_cursor(VteScreen *screen__)
{
        screen__->saved.cursor.col = screen__->cursor.col;
        screen__->saved.cursor.row = screen__->cursor.row - screen__->insert_delta;
        screen__->saved.cursor_advanced_by_graphic_character = screen__->cursor_advanced_by_graphic_character;

        screen__->saved.reverse_mode = m_modes_private.DEC_REVERSE_IMAGE();
        screen__->saved.origin_mode = m_modes_private.DEC_ORIGIN();

        screen__->saved.defaults = m_defaults;
        screen__->saved.color_defaults = m_color_defaults;
        screen__->saved.character_replacements[0] = m_character_replacements[0];
        screen__->saved.character_replacements[1] = m_character_replacements[1];
        screen__->saved.character_replacement = m_character_replacement;
}

// [[gnu::always_inline]]
/* C++23 constexpr */ gunichar
Terminal::character_replacement(gunichar c) noexcept
{
        // DEC Special Character and Line Drawing Set
        //
        // References: VT525

        static constinit gunichar const line_drawing_map[32] = {
                0x0020,  /* _ => blank (space) */
                0x25c6,  /* ` => diamond */
                0x2592,  /* a => checkerboard */
                0x2409,  /* b => HT symbol */
                0x240c,  /* c => FF symbol */
                0x240d,  /* d => CR symbol */
                0x240a,  /* e => LF symbol */
                0x00b0,  /* f => degree */
                0x00b1,  /* g => plus/minus */
                0x2424,  /* h => NL symbol */
                0x240b,  /* i => VT symbol */
                0x2518,  /* j => downright corner */
                0x2510,  /* k => upright corner */
                0x250c,  /* l => upleft corner */
                0x2514,  /* m => downleft corner */
                0x253c,  /* n => cross */
                0x23ba,  /* o => scan line 1/9 */
                0x23bb,  /* p => scan line 3/9 */
                0x2500,  /* q => horizontal line (also scan line 5/9) */
                0x23bc,  /* r => scan line 7/9 */
                0x23bd,  /* s => scan line 9/9 */
                0x251c,  /* t => left t */
                0x2524,  /* u => right t */
                0x2534,  /* v => bottom t */
                0x252c,  /* w => top t */
                0x2502,  /* x => vertical line */
                0x2264,  /* y => <= */
                0x2265,  /* z => >= */
                0x03c0,  /* { => pi */
                0x2260,  /* | => not equal */
                0x00a3,  /* } => pound currency sign */
                0x00b7,  /* ~ => bullet */
        };

	/* If we've enabled the special drawing set, map the characters to
	 * Unicode. */
        if (*m_character_replacement == VTE_CHARACTER_REPLACEMENT_LINE_DRAWING) [[unlikely]] {
                if (c >= 95 && c <= 126)
                        return line_drawing_map[c - 95];
        }

        [[likely]] return c;
}

int
Terminal::character_width(gunichar c) noexcept
{
        return _vte_unichar_width(c, m_utf8_ambiguous_width);
}

/* Insert a single character into the stored data array.
 *
 * Note that much of this method is duplicated below in insert_single_width_chars().
 * Make sure to keep the two in sync! */
void
Terminal::insert_char(gunichar c,
                      bool invalidate_now)
{
	VteCellAttr attr;
	VteRowData *row;
	long col;
	int columns, i;
        gunichar c_unmapped = c;

        /* DEC Special Character and Line Drawing Set.  VT100 and higher (per XTerm docs). */
        static const gunichar line_drawing_map[32] = {
                0x0020,  /* _ => blank (space) */
                0x25c6,  /* ` => diamond */
                0x2592,  /* a => checkerboard */
                0x2409,  /* b => HT symbol */
                0x240c,  /* c => FF symbol */
                0x240d,  /* d => CR symbol */
                0x240a,  /* e => LF symbol */
                0x00b0,  /* f => degree */
                0x00b1,  /* g => plus/minus */
                0x2424,  /* h => NL symbol */
                0x240b,  /* i => VT symbol */
                0x2518,  /* j => downright corner */
                0x2510,  /* k => upright corner */
                0x250c,  /* l => upleft corner */
                0x2514,  /* m => downleft corner */
                0x253c,  /* n => cross */
                0x23ba,  /* o => scan line 1/9 */
                0x23bb,  /* p => scan line 3/9 */
                0x2500,  /* q => horizontal line (also scan line 5/9) */
                0x23bc,  /* r => scan line 7/9 */
                0x23bd,  /* s => scan line 9/9 */
                0x251c,  /* t => left t */
                0x2524,  /* u => right t */
                0x2534,  /* v => bottom t */
                0x252c,  /* w => top t */
                0x2502,  /* x => vertical line */
                0x2264,  /* y => <= */
                0x2265,  /* z => >= */
                0x03c0,  /* { => pi */
                0x2260,  /* | => not equal */
                0x00a3,  /* } => pound currency sign */
                0x00b7,  /* ~ => bullet */
        };

	/* If we've enabled the special drawing set, map the characters to
	 * Unicode. */
        if (G_UNLIKELY (*m_character_replacement == VTE_CHARACTER_REPLACEMENT_LINE_DRAWING)) {
                if (c >= 95 && c <= 126)
                        c = line_drawing_map[c - 95];
        }

	/* Figure out how many columns this character should occupy. */
        columns = _vte_unichar_width(c, m_utf8_ambiguous_width);

	/* If we're autowrapping here, do it. */
        col = m_screen->cursor.col;
        if (G_UNLIKELY (columns && (
                        /* no room at the terminal's right edge */
                        (col + columns > m_column_count) ||
                        /* cursor is just beyond the DECSLRM right margin, moved there by printing a letter */
                        (col == m_scrolling_region.right() + 1 && m_screen->cursor_advanced_by_graphic_character) ||
                        /* wide character is printed, cursor would cross the DECSLRM right margin */
                        (col <= m_scrolling_region.right() && col + columns > m_scrolling_region.right() + 1)))) {
		if (m_modes_private.DEC_AUTOWRAP()) {
			_vte_debug_print(VTE_DEBUG_ADJ,
					"Autowrapping before character\n");
			/* Wrap. */
			/* XXX clear to the end of line */
                        col = m_screen->cursor.col = m_scrolling_region.left();
			/* Mark this line as soft-wrapped. */
			row = ensure_row();
                        set_soft_wrapped(m_screen->cursor.row);
                        /* Handle bce (background color erase) differently from xterm:
                         * only fill the new row with the background color if scrolling happens due
                         * to an explicit escape sequence, not due to autowrapping (i.e. not here).
                         * See https://gitlab.gnome.org/GNOME/vte/-/issues/2219 for details. */
                        cursor_down_with_scrolling(false);
                        ensure_row();
                        apply_bidi_attributes(m_screen->cursor.row, row->attr.bidi_flags, VTE_BIDI_FLAG_ALL);
		} else {
                        /* Don't wrap, stay at the rightmost column or at the right margin.
                         * Note that we slighly differ from xterm. Xterm swallows wide characters
                         * that do not fit, we retreat the cursor to fit them.
                         */
                        if (/* cursor is just beyond the DECSLRM right margin, moved there by printing a letter */
                            (col == m_scrolling_region.right() + 1 && m_screen->cursor_advanced_by_graphic_character) ||
                            /* wide character is printed, cursor would cross the DECSLRM right margin */
                            (col <= m_scrolling_region.right() && col + columns > m_scrolling_region.right() + 1)) {
                                col = m_screen->cursor.col = m_scrolling_region.right() + 1 - columns;
                        } else {
                                col = m_screen->cursor.col = m_column_count - columns;
                        }
		}
	}

	_vte_debug_print(VTE_DEBUG_PARSER,
			"Inserting U+%04X '%lc' (colors %" G_GUINT64_FORMAT ") (%ld+%d, %ld), delta = %ld; ",
                         (unsigned int)c, g_unichar_isprint(c) ? c : 0xfffd,
                         m_color_defaults.attr.colors(),
                        col, columns, (long)m_screen->cursor.row,
			(long)m_screen->insert_delta);

        //FIXMEchpe
        if (G_UNLIKELY(c == 0))
                goto not_inserted;

	if (G_UNLIKELY (columns == 0)) {

		/* It's a combining mark */

		long row_num;
		VteCell *cell;

		_vte_debug_print(VTE_DEBUG_PARSER, "combining U+%04X", c);

                row_num = m_screen->cursor.row;
		row = NULL;
		if (G_UNLIKELY (col == 0)) {
			/* We are at first column.  See if the previous line softwrapped.
			 * If it did, move there.  Otherwise skip inserting. */

			if (G_LIKELY (row_num > 0)) {
				row_num--;
				row = find_row_data_writable(row_num);

				if (row) {
					if (!row->attr.soft_wrapped)
						row = NULL;
					else
						col = _vte_row_data_length (row);
				}
			}
		} else {
			row = find_row_data_writable(row_num);
		}

		if (G_UNLIKELY (!row || !col))
			goto not_inserted;

		/* Combine it on the previous cell */

		col--;
		cell = _vte_row_data_get_writable (row, col);

		if (G_UNLIKELY (!cell))
			goto not_inserted;

		/* Find the previous cell */
		while (cell && cell->attr.fragment() && col > 0)
			cell = _vte_row_data_get_writable (row, --col);
		if (G_UNLIKELY (!cell || cell->c == '\t'))
			goto not_inserted;

		/* Combine the new character on top of the cell string */
		c = _vte_unistr_append_unichar (cell->c, c);

		/* And set it */
		columns = cell->attr.columns();
		for (i = 0; i < columns; i++) {
			cell = _vte_row_data_get_writable (row, col++);
			cell->c = c;
		}

		goto done;
        } else {
                m_last_graphic_character = c_unmapped;
	}

	/* Make sure we have enough rows to hold this data. */
	row = ensure_cursor();
	g_assert(row != NULL);

        if (m_modes_ecma.IRM() &&
            m_screen->cursor.col >= m_scrolling_region.left() &&
            m_screen->cursor.col <= m_scrolling_region.right()) {
                /* Like ICH's handler: Scroll right in a custom region: only the cursor's row, from the cursor to the DECSLRM right margin. */
                auto scrolling_region = m_scrolling_region;
                scrolling_region.set_vertical(get_xterm_cursor_row(), get_xterm_cursor_row());
                scrolling_region.set_horizontal(m_screen->cursor.col, scrolling_region.right());
                scroll_text_right(scrolling_region, columns, false /* no fill */);
	} else {
                cleanup_fragments(col, col + columns);
		_vte_row_data_fill (row, &basic_cell, col);
		_vte_row_data_expand (row, col + columns);
	}

        attr = m_defaults.attr;
	attr.set_columns(columns);

	{
		VteCell *pcell = _vte_row_data_get_writable (row, col);
		pcell->c = c;
		pcell->attr = attr;
		col++;
	}

	/* insert wide-char fragments */
	attr.set_fragment(true);
	for (i = 1; i < columns; i++) {
		VteCell *pcell = _vte_row_data_get_writable (row, col);
		pcell->c = c;
		pcell->attr = attr;
		col++;
	}
	if (_vte_row_data_length (row) > m_column_count)
		cleanup_fragments(m_column_count, _vte_row_data_length (row));
	_vte_row_data_shrink (row, m_column_count);

        m_screen->cursor.col = col;

done:
        /* Signal that this part of the window needs drawing. */
        if (G_UNLIKELY (invalidate_now)) {
                invalidate_row_and_context(m_screen->cursor.row);
        }

        m_screen->cursor_advanced_by_graphic_character = true;

	/* We added text, so make a note of it. */
	m_text_inserted_flag = TRUE;

not_inserted:
	_vte_debug_print(VTE_DEBUG_ADJ|VTE_DEBUG_PARSER,
			"insertion delta => %ld.\n",
			(long)m_screen->insert_delta);
}

/* Inserts each character of the string.
 * The passed string MUST consist of single-width printable characters only.
 * It performs the equivalent of calling insert_char(..., false) on each of them,
 * but is usually much faster.
 *
 * Note that much of this method is duplicated above in insert_char().
 * Make sure to keep the two in sync! */
void
Terminal::insert_single_width_chars(gunichar const *p, int len)
{
        if (m_scrolling_region.is_restricted() ||
            (*m_character_replacement == VTE_CHARACTER_REPLACEMENT_LINE_DRAWING) ||
            !m_modes_private.DEC_AUTOWRAP() ||
            m_modes_ecma.IRM()) {
                /* There is some special unusual circumstance.
                 * Resort to inserting the characters one by one. */
                while (len--) {
                        insert_char(*p++, false);
                }
                return;
        }

        /* The usual circumstances, that is, no custom margins,
         * no box drawing mode, autowrapping enabled, no insert mode.
         * Much of this code is duplicated from insert_char(), with
         * the checks for the unnecessary conditions stripped off.
         * Also, don't check for wrapping after each character; rather,
         * fill up runs within a single row as quickly as we can.
         * Furthermore, don't clean up CJK fragments at each character.
         * All these combined result in significantly faster operation. */
        while (len) {
                VteRowData *row;
                long col;

                /* If we're autowrapping here, do it. */
                col = m_screen->cursor.col;
                if (G_UNLIKELY (col >= m_column_count)) {
                        _vte_debug_print(VTE_DEBUG_ADJ,
                                        "Autowrapping before character\n");
                        /* Wrap. */
                        /* XXX clear to the end of line */
                        col = m_screen->cursor.col = 0;
                        /* Mark this line as soft-wrapped. */
                        row = ensure_row();
                        set_soft_wrapped(m_screen->cursor.row);
                        /* Handle bce (background color erase) differently from xterm:
                         * only fill the new row with the background color if scrolling happens due
                         * to an explicit escape sequence, not due to autowrapping (i.e. not here).
                         * See https://gitlab.gnome.org/GNOME/vte/-/issues/2219 for details. */
                        cursor_down_with_scrolling(false);
                        ensure_row();
                        apply_bidi_attributes(m_screen->cursor.row, row->attr.bidi_flags, VTE_BIDI_FLAG_ALL);
                }

                /* The number of cells we can populate in this row. */
                int run = MIN(len, m_column_count - col);
                vte_assert_cmpint(run, >=, 1);

                _VTE_DEBUG_IF(VTE_DEBUG_PARSER) {
                        gchar *utf8 = g_ucs4_to_utf8(p, run, NULL, NULL, NULL);
                        _vte_debug_print(VTE_DEBUG_PARSER,
                                         "Inserting string of %d single-width characters \"%s\" (colors %" G_GUINT64_FORMAT ") (%ld+%d, %ld), delta = %ld; ",
                                         run, utf8, m_color_defaults.attr.colors(),
                                         col, run, (long)m_screen->cursor.row,
                                         (long)m_screen->insert_delta);
                        g_free(utf8);
                }

                /* Make sure we have enough rows to hold this data. */
                row = ensure_cursor();
                g_assert(row != NULL);

                cleanup_fragments(col, col + run);
                _vte_row_data_fill (row, &basic_cell, col);
                _vte_row_data_expand (row, col + run);

                len -= run;
                while (run--) {
                        VteCell *pcell = _vte_row_data_get_writable (row, col);
                        pcell->c = *p;
                        pcell->attr = m_defaults.attr;
                        p++;
                        col++;
                }

                if (_vte_row_data_length (row) > m_column_count)
                        cleanup_fragments(m_column_count, _vte_row_data_length (row));
                _vte_row_data_shrink (row, m_column_count);

                m_screen->cursor.col = col;

                m_last_graphic_character = *(p - 1);
                m_screen->cursor_advanced_by_graphic_character = true;

                /* We added text, so make a note of it. */
                m_text_inserted_flag = TRUE;

                _vte_debug_print(VTE_DEBUG_ADJ|VTE_DEBUG_PARSER,
                                 "insertion delta => %ld.\n",
                                 (long)m_screen->insert_delta);
        }
}

guint8
Terminal::get_bidi_flags() const noexcept
{
        return (m_modes_ecma.BDSM() ? VTE_BIDI_FLAG_IMPLICIT : 0) |
               (m_bidi_rtl ? VTE_BIDI_FLAG_RTL : 0) |
               (m_modes_private.VTE_BIDI_AUTO() ? VTE_BIDI_FLAG_AUTO : 0) |
               (m_modes_private.VTE_BIDI_BOX_MIRROR() ? VTE_BIDI_FLAG_BOX_MIRROR : 0);
}

/* Apply the specified BiDi parameters on the paragraph beginning at the specified line. */
void
Terminal::apply_bidi_attributes(vte::grid::row_t start, guint8 bidi_flags, guint8 bidi_flags_mask)
{
        vte::grid::row_t row = start;
        VteRowData *rowdata;

        bidi_flags &= bidi_flags_mask;

        _vte_debug_print(VTE_DEBUG_BIDI,
                         "Applying BiDi parameters from row %ld.\n", row);

        rowdata = m_screen->row_data->index_writable(row);
        if (rowdata == nullptr || (rowdata->attr.bidi_flags & bidi_flags_mask) == bidi_flags) {
                _vte_debug_print(VTE_DEBUG_BIDI,
                                 "BiDi parameters didn't change for this paragraph.\n");
                return;
        }

        while (true) {
                rowdata->attr.bidi_flags &= ~bidi_flags_mask;
                rowdata->attr.bidi_flags |= bidi_flags;

                if (!rowdata->attr.soft_wrapped)
                        break;

                rowdata = m_screen->row_data->index_writable(row + 1);
                if (rowdata == nullptr)
                        break;
                row++;
        }

        _vte_debug_print(VTE_DEBUG_BIDI,
                         "Applied BiDi parameters to rows %ld..%ld.\n", start, row);

        m_ringview.invalidate();
        invalidate_rows(start, row);
}

/* Apply the current BiDi parameters covered by bidi_flags_mask on the current paragraph
 * if the cursor is at the first position of this paragraph. */
void
Terminal::maybe_apply_bidi_attributes(guint8 bidi_flags_mask)
{
        _vte_debug_print(VTE_DEBUG_BIDI,
                         "Maybe applying BiDi parameters on current paragraph.\n");

        if (m_screen->cursor.col != 0) {
                _vte_debug_print(VTE_DEBUG_BIDI,
                                 "No, cursor not in first column.\n");
                return;
        }

        auto row = m_screen->cursor.row;

        if (row > (long)m_screen->row_data->delta()) {
                const VteRowData *rowdata = m_screen->row_data->index(row - 1);
                if (rowdata != nullptr && rowdata->attr.soft_wrapped) {
                        _vte_debug_print(VTE_DEBUG_BIDI,
                                         "No, we're not after a hard wrap.\n");
                        return;
                }
        }

        _vte_debug_print(VTE_DEBUG_BIDI,
                         "Yes, applying.\n");

        apply_bidi_attributes (row, get_bidi_flags(), bidi_flags_mask);
}

static void
reaper_child_exited_cb(VteReaper *reaper,
                       int ipid,
                       int status,
                       vte::terminal::Terminal* that) noexcept
try
{
        that->child_watch_done(pid_t{ipid}, status);
        // @that might be destroyed at this point
}
catch (...)
{
        vte::log_exception();
}

void
Terminal::child_watch_done(pid_t pid,
                           int status)
{
	if (pid != m_pty_pid)
                return;

        _VTE_DEBUG_IF (VTE_DEBUG_LIFECYCLE) {
                g_printerr ("Child[%d] exited with status %d\n",
                            pid, status);
#ifdef HAVE_SYS_WAIT_H
                if (WIFEXITED (status)) {
                        g_printerr ("Child[%d] exit code %d.\n",
                                    pid, WEXITSTATUS (status));
                } else if (WIFSIGNALED (status)) {
                        g_printerr ("Child[%d] dies with signal %d.\n",
                                    pid, WTERMSIG (status));
                }
#endif
        }

        /* Disconnect from the reaper */
        if (m_reaper) {
                g_signal_handlers_disconnect_by_func(m_reaper,
                                                     (gpointer)reaper_child_exited_cb,
                                                     this);
                g_object_unref(m_reaper);
                m_reaper = nullptr;
        }

        m_pty_pid = -1;

        /* If we still have a PTY, or data to process, defer emitting the signals
         * until we have EOF on the PTY, so that we can process all pending data.
         */
        if (pty()) {
                /* Read and process about 64k synchronously, up to EOF or EAGAIN
                 * or other error, to make sure we consume the child's output.
                 * See https://gitlab.gnome.org/GNOME/vte/-/issues/2627 */
                pty_io_read(pty()->fd(), G_IO_IN, 65536);
                if (!m_incoming_queue.empty()) {
                        process_incoming();
                }

                /* Stop processing data. Optional. Keeping processing data from grandchildren and
                 * other writers would also be a reasonable choice. It makes a difference if the
                 * terminal is held open after the child exits. */
                unset_pty();
        }

        if (widget())
                widget()->emit_child_exited(status);
}

static void
mark_input_source_invalid_cb(vte::terminal::Terminal* that)
{
	_vte_debug_print (VTE_DEBUG_IO, "Removed PTY input source\n");
	that->m_pty_input_source = 0;
}

/* Read and handle data from the child. */
static gboolean
io_read_cb(int fd,
           GIOCondition condition,
           vte::terminal::Terminal* that)
{
        return that->pty_io_read(fd, condition);
}

void
Terminal::connect_pty_read()
{
	if (m_pty_input_source != 0 || !pty())
		return;

        _vte_debug_print (VTE_DEBUG_IO, "Adding PTY input source\n");

        m_pty_input_source = g_unix_fd_add_full(VTE_CHILD_INPUT_PRIORITY,
                                                pty()->fd(),
                                                (GIOCondition)(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR),
                                                (GUnixFDSourceFunc)io_read_cb,
                                                this,
                                                (GDestroyNotify)mark_input_source_invalid_cb);
}

static void
mark_output_source_invalid_cb(vte::terminal::Terminal* that)
{
	_vte_debug_print (VTE_DEBUG_IO, "Removed PTY output source\n");
	that->m_pty_output_source = 0;
}

/* Send locally-encoded characters to the child. */
static gboolean
io_write_cb(int fd,
            GIOCondition condition,
            vte::terminal::Terminal* that)
{
        return that->pty_io_write(fd, condition);
}

void
Terminal::connect_pty_write()
{
        if (m_pty_output_source != 0 || !pty())
                return;

        g_warn_if_fail(m_input_enabled);

        /* Anything to write? */
        if (_vte_byte_array_length(m_outgoing) == 0)
                return;

        /* Do one write. FIXMEchpe why? */
        if (!pty_io_write (pty()->fd(), G_IO_OUT))
                return;

        _vte_debug_print (VTE_DEBUG_IO, "Adding PTY output source\n");

        m_pty_output_source = g_unix_fd_add_full(VTE_CHILD_OUTPUT_PRIORITY,
                                                 pty()->fd(),
                                                 G_IO_OUT,
                                                 (GUnixFDSourceFunc)io_write_cb,
                                                 this,
                                                 (GDestroyNotify)mark_output_source_invalid_cb);
}

void
Terminal::disconnect_pty_read()
{
	if (m_pty_input_source != 0) {
		_vte_debug_print (VTE_DEBUG_IO, "Removing PTY input source\n");
		g_source_remove(m_pty_input_source);
                // FIXMEchpe the destroy notify should already have done this!
		m_pty_input_source = 0;
	}
}

void
Terminal::disconnect_pty_write()
{
	if (m_pty_output_source != 0) {
		_vte_debug_print (VTE_DEBUG_IO, "Removing PTY output source\n");
		g_source_remove(m_pty_output_source);
                // FIXMEchpe the destroy notify should already have done this!
		m_pty_output_source = 0;
	}
}

void
Terminal::pty_termios_changed()
{
        _vte_debug_print(VTE_DEBUG_IO, "Termios changed\n");
}

void
Terminal::pty_scroll_lock_changed(bool locked)
{
        _vte_debug_print(VTE_DEBUG_IO, "Output %s (^%c)\n",
                         locked ? "stopped" : "started",
                         locked ? 'Q' : 'S');
}

/*
 * Terminal::watch_child:
 * @child_pid: a #pid_t
 *
 * Watches @child_pid. When the process exists, the #VteTerminal::child-exited
 * signal will be called with the child's exit status.
 *
 * Prior to calling this function, a #VtePty must have been set in @terminal
 * using vte_terminal_set_pty().
 * When the child exits, the terminal's #VtePty will be set to %NULL.
 *
 * Note: g_child_watch_add() or g_child_watch_add_full() must not have
 * been called for @child_pid, nor a #GSource for it been created with
 * g_child_watch_source_new().
 *
 * Note: when using the g_spawn_async() family of functions,
 * the %G_SPAWN_DO_NOT_REAP_CHILD flag MUST have been passed.
 */
void
Terminal::watch_child (pid_t child_pid)
{
        // FIXMEchpe: support passing child_pid = -1 to remove the wathch
        g_assert(child_pid != -1);
        if (!pty())
                return;

        auto const freezer = vte::glib::FreezeObjectNotify{m_terminal};

        /* Set this as the child's pid. */
        m_pty_pid = child_pid;

        /* Catch a child-exited signal from the child pid. */
        auto reaper = vte_reaper_ref();
        vte_reaper_add_child(child_pid);
        if (reaper != m_reaper) {
                if (m_reaper) {
                        g_signal_handlers_disconnect_by_func(m_reaper,
                                                             (gpointer)reaper_child_exited_cb,
                                                             this);
                        g_object_unref(m_reaper);
                }
                m_reaper = reaper; /* adopts */
                g_signal_connect(m_reaper, "child-exited",
                                 G_CALLBACK(reaper_child_exited_cb),
                                 this);
        } else {
                g_object_unref(reaper);
        }

        /* FIXMEchpe: call set_size() here? */
}

/* Reset the input method context. */
void
Terminal::im_reset()
{
        if (widget())
                widget()->im_reset();

        im_preedit_reset();
}

void
Terminal::process_incoming()
{
        _vte_debug_print(VTE_DEBUG_IO,
                         "Handler processing %" G_GSIZE_FORMAT " bytes over %" G_GSIZE_FORMAT " chunks.\n",
                         m_input_bytes,
                         m_incoming_queue.size());
        _vte_debug_print (VTE_DEBUG_WORK, "(");

        /* We should only be called when there's data to process. */
        g_assert(!m_incoming_queue.empty());

        auto bytes_processed = ssize_t{0};

        auto context = ProcessingContext{*this};

        while (!m_incoming_queue.empty()) {
                auto& chunk = m_incoming_queue.front();

                assert((bool)chunk);

                auto const start = chunk->begin_reading();

                _VTE_DEBUG_IF(VTE_DEBUG_IO) {
                        _vte_debug_print(VTE_DEBUG_IO,
                                         "Processing data syntax %d chunk %p starting at offset %u\n",
                                         (int)current_data_syntax(),
                                         (void*)chunk.get(),
                                         unsigned(chunk->begin_reading() - chunk->data()));

                        _vte_debug_hexdump("Incoming buffer",
                                           chunk->begin_reading(),
                                           chunk->size_reading());
                }

                switch (current_data_syntax()) {
                case DataSyntax::ECMA48_UTF8:
                        process_incoming_utf8(context, *chunk);
                        break;

#if WITH_ICU
                case DataSyntax::ECMA48_PCTERM:
                        process_incoming_pcterm(context, *chunk);
                        break;
#endif

                default:
                        g_assert_not_reached();
                        break;
                }

                bytes_processed += size_t(chunk->begin_reading() - start);

                _vte_debug_print(VTE_DEBUG_IO, "read %d bytes, chunk %s, data syntax now %d\n",
                                 int(chunk->begin_reading() - start),
                                 chunk->has_reading()?"has more":"finished",
                                 (int)current_data_syntax());
                // If all data from this chunk has been processed, go to the next one
                if (!chunk->has_reading())
                        m_incoming_queue.pop();
        }

#if VTE_DEBUG
        /* Some safety checks: ensure the visible parts of the buffer
         * are all in the buffer. */
        vte_assert_cmpint(m_screen->insert_delta, >=, m_screen->row_data->delta());

        /* The cursor shouldn't be above or below the addressable
         * part of the display buffer. */
        vte_assert_cmpint(m_screen->cursor.row, >=, m_screen->insert_delta);
#endif

        if (context.m_modified) {
                /* Keep the cursor on-screen if we scroll on output, or if
                 * we're currently at the bottom of the buffer.
                 * Also make sure the alternate screen is correctly positioned. */
                update_insert_delta();
                if (m_scroll_on_output || context.m_bottom || m_screen == &m_alternate_screen) {
                        scroll_to_bottom();
                }
                /* Deselect the current selection if its contents are changed
                 * by this insertion. */
                if (!m_selection_resolved.empty()) {
                        //FIXMEchpe: this is atrocious
                        auto selection = g_string_new(nullptr);
                        get_selected_text(selection);
                        if ((selection == nullptr) ||
                            (m_selection[vte::to_integral(vte::platform::ClipboardType::PRIMARY)] == nullptr) ||
                            (strcmp(selection->str, m_selection[vte::to_integral(vte::platform::ClipboardType::PRIMARY)]->str) != 0)) {
                                deselect_all();
                        }
                        g_string_free(selection, TRUE);
                }
        }

        if (context.m_modified || (m_screen != context.m_saved_screen)) {
                m_ringview.invalidate();
                /* Signal that the visible contents changed. */
                queue_contents_changed();
        }

        emit_pending_signals();

        if (context.m_invalidated_text) {
                invalidate_rows_and_context(context.m_bbox_top, context.m_bbox_bottom);
        }

        if ((context.m_saved_cursor.col != m_screen->cursor.col) ||
            (context.m_saved_cursor.row != m_screen->cursor.row)) {
                /* invalidate the old and new cursor positions */
                if (context.m_saved_cursor_visible)
                        invalidate_row(context.m_saved_cursor.row);
                invalidate_cursor_once();
                check_cursor_blink();
                /* Signal that the cursor moved. */
                queue_cursor_moved();
        } else if ((context.m_saved_cursor_visible != m_modes_private.DEC_TEXT_CURSOR()) ||
                   (context.m_saved_cursor_style != m_cursor_style)) {
                invalidate_row(context.m_saved_cursor.row);
                check_cursor_blink();
        }

        /* Tell the input method where the cursor is. */
        im_update_cursor();

        /* After processing some data, do a hyperlink GC. The multiplier is totally arbitrary, feel free to fine tune. */
        m_screen->row_data->hyperlink_maybe_gc(bytes_processed * 8);

        _vte_debug_print (VTE_DEBUG_WORK, ")");
        _vte_debug_print (VTE_DEBUG_IO,
                          "%" G_GSIZE_FORMAT " bytes in %" G_GSIZE_FORMAT " chunks left to process.\n",
                          m_input_bytes,
                          m_incoming_queue.size());
}

/* Note that this code is mostly copied to process_incoming_pcterm() below; any non-charset-decoding
 * related changes made here need to be made there, too.
 */
void
Terminal::process_incoming_utf8(ProcessingContext& context,
                                vte::base::Chunk& chunk)
{
        auto seq = vte::parser::Sequence{m_parser};

        auto const iend = chunk.end_reading();
        auto ip = chunk.begin_reading();

        /* Chunk size (k_chunk_size) is around 8kB, so single_width_chars is at most 32kB, fine on the stack. */
        static_assert(vte::base::Chunk::max_size() <= 8 * 1024);
        gunichar *single_width_chars = g_newa(gunichar, iend - ip);
        int single_width_chars_count;

        while (ip < iend) [[likely]] {

                switch (m_utf8_decoder.decode(*(ip++))) {
                case vte::base::UTF8Decoder::REJECT_REWIND:
                        /* Rewind the stream.
                         * Note that this will never lead to a loop, since in the
                         * next round this byte *will* be consumed.
                         */
                        --ip;
                        [[fallthrough]];
                case vte::base::UTF8Decoder::REJECT:
                        m_utf8_decoder.reset();
                        /* Fall through to insert the U+FFFD replacement character. */
                        [[fallthrough]];
                case vte::base::UTF8Decoder::ACCEPT: {
                        [[likely]];

                        auto rv = m_parser.feed(m_utf8_decoder.codepoint());
                        if (G_UNLIKELY(rv < 0)) {
#if VTE_DEBUG
                                uint32_t c = m_utf8_decoder.codepoint();
                                char c_buf[7];
                                g_snprintf(c_buf, sizeof(c_buf), "%lc", c);
                                char const* wp_str = g_unichar_isprint(c) ? c_buf : _vte_debug_sequence_to_string(c_buf, -1);
                                _vte_debug_print(VTE_DEBUG_PARSER, "Parser error on U+%04X [%s]!\n",
                                                 c, wp_str);
#endif /* VTE_DEBUG */
                                break;
                        }

#if VTE_DEBUG
                        if (rv != VTE_SEQ_NONE)
                                g_assert((bool)seq);
#endif

                        _VTE_DEBUG_IF(VTE_DEBUG_PARSER) {
                                if (rv != VTE_SEQ_NONE) {
                                        seq.print();
                                }
                        }

                        // FIXMEchpe this assumes that the only handler inserting
                        // a character is GRAPHIC, which isn't true (at least ICH, REP, SUB
                        // also do, and invalidate directly for now)...

                        switch (rv) {
                        case VTE_SEQ_GRAPHIC: [[likely]] {

                                context.pre_GRAPHIC();

                                // does insert_char(c, false)
                                GRAPHIC(seq);
                                _vte_debug_print(VTE_DEBUG_PARSER,
                                                 "Last graphic is now U+%04X %lc\n",
                                                 m_last_graphic_character,
                                                 g_unichar_isprint(m_last_graphic_character) ? m_last_graphic_character : 0xfffd);

                                // It's very common to get batches of printable single-width
                                // characters. So plan for that and avoid round-tripping
                                // through the main UTF-8 decoder as well as the Parser. It
                                // also allows for a single pre_GRAPHIC()/post_GRAPHIC().
                                single_width_chars_count = 0;
                                /* Super quickly process initial ASCII segment. */
                                while (ip < iend && *ip >= 0x20 && *ip < 0x7F) [[likely]] {
                                        single_width_chars[single_width_chars_count++] = *ip;
                                        ip++;
                                }
                                if (ip < iend && *ip >= 0x80) {
                                        /* Continue with UTF-8 (possibly including further ASCII) non-control chars. */
                                        /* This is just a little bit slower than the ASCII loop above. */
                                        vte::base::UTF8Decoder decoder;
                                        auto ip_lookahead = ip;
                                        while (ip_lookahead < iend) [[likely]] {
                                                auto state = decoder.decode(*ip_lookahead++);
                                                if (state == vte::base::UTF8Decoder::ACCEPT) [[likely]] {
                                                        gunichar c = decoder.codepoint();
                                                        if ((c >= 0x20 && c < 0x7F) ||
                                                            (c >= 0xA0 && _vte_unichar_width(c, context.m_terminal->m_utf8_ambiguous_width) == 1)) [[likely]] {
                                                                /* Single width char, append to the array. */
                                                                single_width_chars[single_width_chars_count++] = c;
                                                                ip = ip_lookahead;
                                                                continue;
                                                        } else if (c >= 0xA0) {
                                                                /* Zero or double width char, flush the array of single width ones and then process this. */
                                                                if (single_width_chars_count > 0) {
                                                                        insert_single_width_chars(single_width_chars, single_width_chars_count);
                                                                        single_width_chars_count = 0;
                                                                }
                                                                insert_char(c, false);
                                                                ip = ip_lookahead;
                                                                continue;
                                                        } else {
                                                                /* Control char. */
                                                                break;
                                                        }
                                                } else if (state == vte::base::UTF8Decoder::REJECT ||
                                                           state == vte::base::UTF8Decoder::REJECT_REWIND) [[unlikely]] {
                                                        /* Encoding error. */
                                                        break;
                                                }
                                                /* else: More bytes needed, continue. */
                                        }
                                }
                                /* Flush the array of single width chars. */
                                if (single_width_chars_count > 0) [[likely]] {
                                        insert_single_width_chars(single_width_chars, single_width_chars_count);
                                        _vte_debug_print(VTE_DEBUG_PARSER,
                                                         "Last graphic is now U+%04X %lc\n",
                                                         m_last_graphic_character,
                                                         g_unichar_isprint(m_last_graphic_character) ? m_last_graphic_character : 0xfffd);
                                }

                                context.post_GRAPHIC();
                                break;
                        }

                        case VTE_SEQ_NONE:
                        case VTE_SEQ_IGNORE:
                                break;

                        default: {
                                switch (seq.command()) {
#define _VTE_CMD_HANDLER(cmd)   \
                                case VTE_CMD_##cmd: cmd(seq); break;
#define _VTE_CMD_HANDLER_R(cmd) \
                                case VTE_CMD_##cmd: if (cmd(seq)) { \
                                        context.post_CMD(); \
                                        goto switched_data_syntax; \
                                        } \
                                        break;
#define _VTE_CMD_HANDLER_NOP(cmd)
#include "parser-cmd-handlers.hh"
#undef _VTE_CMD_HANDLER
#undef _VTE_CMD_HANDLER_NOP
#undef _VTE_CMD_HANDLER_R
                                default:
                                        _vte_debug_print(VTE_DEBUG_PARSER,
                                                         "Unknown parser command %d\n", seq.command());
                                        break;
                                }

                                m_last_graphic_character = 0;

                                context.post_CMD();
                                break;
                        }
                        }
                        break;
                }
                }
        }

        if (chunk.eos() && ip == iend) {
                m_eos_pending = true;
                /* If there's an unfinished character in the queue, insert a replacement character */
                if (m_utf8_decoder.flush()) {
                        insert_char(m_utf8_decoder.codepoint(), true);
                }
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-label"
switched_data_syntax:
#pragma GCC diagnostic pop

        // Update start for data consumed
        chunk.set_begin_reading(ip);
}

#if WITH_ICU

/* Note that this is mostly a copy of process_incoming_utf8() above; any non-charset-decoding
 * related changes made here need to be made there, too.
 */
void
Terminal::process_incoming_pcterm(ProcessingContext& context,
                                  vte::base::Chunk& chunk)
{
        auto seq = vte::parser::Sequence{m_parser};

        auto& decoder = m_converter->decoder();

        auto eos = bool{false};
        auto flush = bool{false};

        auto const iend = chunk.end_reading();
        auto ip = chunk.begin_reading();

 start:
        while (ip < iend || flush) {
                switch (decoder.decode(&ip, flush)) {
                case vte::base::ICUDecoder::Result::eSomething: {
                        auto rv = m_parser.feed(decoder.codepoint());
                        if (G_UNLIKELY(rv < 0)) {
#if VTE_DEBUG
                                uint32_t c = decoder.codepoint();
                                char c_buf[7];
                                g_snprintf(c_buf, sizeof(c_buf), "%lc", c);
                                char const* wp_str = g_unichar_isprint(c) ? c_buf : _vte_debug_sequence_to_string(c_buf, -1);
                                _vte_debug_print(VTE_DEBUG_PARSER, "Parser error on U+%04X [%s]!\n",
                                                 c, wp_str);
#endif
                                break;
                        }

#if VTE_DEBUG
                        if (rv != VTE_SEQ_NONE)
                                g_assert((bool)seq);
#endif

                        _VTE_DEBUG_IF(VTE_DEBUG_PARSER) {
                                if (rv != VTE_SEQ_NONE) {
                                        seq.print();
                                }
                        }

                        // FIXMEchpe this assumes that the only handler inserting
                        // a character is GRAPHIC, which isn't true (at least ICH, REP, SUB
                        // also do, and invalidate directly for now)...

                        switch (rv) {
                        case VTE_SEQ_GRAPHIC: {

                                context.pre_GRAPHIC();

                                // does insert_char(c, false)
                                GRAPHIC(seq);
                                _vte_debug_print(VTE_DEBUG_PARSER,
                                                 "Last graphic is now U+%04X %lc\n",
                                                 m_last_graphic_character,
                                                 g_unichar_isprint(m_last_graphic_character) ? m_last_graphic_character : 0xfffd);

                                context.post_GRAPHIC();
                                break;
                        }

                        case VTE_SEQ_NONE:
                        case VTE_SEQ_IGNORE:
                                break;

                        default: {
                                switch (seq.command()) {
#define _VTE_CMD_HANDLER(cmd)   \
                                case VTE_CMD_##cmd: cmd(seq); break;
#define _VTE_CMD_HANDLER_R(cmd) \
                                case VTE_CMD_##cmd: \
                                        if (cmd(seq)) { \
                                                context.post_CMD(); \
                                                goto switched_data_syntax; \
                                        } \
                                        break;
#define _VTE_CMD_HANDLER_NOP(cmd)
#include "parser-cmd-handlers.hh"
#undef _VTE_CMD_HANDLER
#undef _VTE_CMD_HANDLER_NOP
#undef _VTE_CMD_HANDLER_R
                                default:
                                        _vte_debug_print(VTE_DEBUG_PARSER,
                                                         "Unknown parser command %d\n", seq.command());
                                        break;
                                }

                                m_last_graphic_character = 0;

                                context.post_CMD();
                                break;
                        }
                        }
                        break;
                }
                case vte::base::ICUDecoder::Result::eNothing:
                        flush = false;
                        break;

                case vte::base::ICUDecoder::Result::eError:
                        // FIXMEchpe do we need ++ip here?
                        decoder.reset();
                        break;

                }
        }

        if (eos) {
                /* Done processing the last chunk */
                m_eos_pending = true;
                return;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-label"
 switched_data_syntax:
#pragma GCC diagnostic pop

        // Update start for data consumed
        chunk.set_begin_reading(ip);

        if (chunk.eos() && ip == chunk.end_reading()) {
                /* On EOS, we still need to flush the decoder before we can finish */
                eos = flush = true;
                goto start;
        }
}

#endif /* WITH_ICU */

bool
Terminal::pty_io_read(int const fd,
                      GIOCondition const condition,
                      int amount)
{
	_vte_debug_print (VTE_DEBUG_WORK, ".");
        _vte_debug_print(VTE_DEBUG_IO, "::pty_io_read condition %02x\n", condition);

        /* We need to check for EOS so that we can shut down the PTY.
         * When we get G_IO_HUP without G_IO_IN, we can process the EOF now.
         * However when we get G_IO_IN | G_IO_HUP, there is still data to be
         * read in this round, and in potentially more rounds; read the data
         * now, do *not* process the EOS (unless the read returns EIO, which
         * does happen and appears to mean that despite G_IO_IN no data was
         * actually available to be read, not even the cpkt header), and
         * otherwise wait for further calls which will have G_IO_HUP (and
         * possibly G_IO_IN again).
         */
        auto eos = bool{condition == G_IO_HUP};

        /* There is data to read */
	auto err = int{0};
        auto again = bool{true};
        vte::base::Chunk* chunk{nullptr};
	if (condition & (G_IO_IN | G_IO_PRI)) {
		guchar *bp;
		int rem, len;
		guint bytes, max_bytes;

		bytes = m_input_bytes;
                if (G_LIKELY (amount < 0)) {
                        max_bytes = m_max_input_bytes;
                } else {
                        /* 'amount' explicitly specified. Try to read this much on top
                         * of what we might already have read and not yet processed,
                         * but stop at EAGAIN or EOS.
                         * See https://gitlab.gnome.org/GNOME/vte/-/issues/2627 */
                        max_bytes = bytes + amount;
                }

                /* If possible, try adding more data to the chunk at the back of the queue */
                if (!m_incoming_queue.empty())
                        chunk = m_incoming_queue.back().get();

		do {
                        /* No chunk, chunk sealed or at least ¾ full? Get a new chunk */
			if (!chunk ||
                            chunk->sealed() ||
                            chunk->capacity_writing() < chunk->capacity() / 4) {
                                m_incoming_queue.push(vte::base::Chunk::get(chunk));
                                chunk = m_incoming_queue.back().get();
			}

			rem = chunk->capacity_writing();
			bp = chunk->begin_writing();
			len = 0;
			do {
#if defined(TIOCPKT)
                                /* We'd like to read (fd, bp, rem); but due to TIOCPKT mode
                                 * there's an extra input byte returned at the beginning.
                                 * We need to see what that byte is, but otherwise drop it
                                 * and write continuously to chunk->data.
                                 */
                                auto const save = bp[-1];
                                errno = 0;
                                ssize_t ret;
                                do {
                                        ret = read(fd, bp - 1, rem + 1);
                                } while (ret == -1 && errno == EINTR);
                                auto const pkt_header = bp[-1];
                                bp[-1] = save;

				switch (ret){
					case -1:
						err = errno;
						goto out;
					case 0:
						eos = true;
						goto out;
					default:
                                                ret--;

                                                if (pkt_header == TIOCPKT_DATA) {
                                                        bp += ret;
                                                        rem -= ret;
                                                        len += ret;
                                                } else {
                                                        if (pkt_header & TIOCPKT_IOCTL) {
                                                                /* We'd like to always be informed when the termios change,
                                                                 * so we can e.g. detect when no-echo is en/disabled and
                                                                 * change the cursor/input method/etc., but unfortunately
                                                                 * the kernel only sends this flag when (old or new) 'local flags'
                                                                 * include EXTPROC, which is not used often, and due to its side
                                                                 * effects, cannot be enabled by vte by default.
                                                                 *
                                                                 * FIXME: improve the kernel! see discussion in bug 755371
                                                                 * starting at comment 12
                                                                 */
                                                                pty_termios_changed();
                                                        }
                                                        if (pkt_header & TIOCPKT_STOP) {
                                                                pty_scroll_lock_changed(true);
                                                        }
                                                        if (pkt_header & TIOCPKT_START) {
                                                                pty_scroll_lock_changed(false);
                                                        }
                                                }
						break;
				}
#elif defined(__sun) && defined(HAVE_STROPTS_H)
				static unsigned char ctl_s[128];
				struct strbuf ctlbuf, databuf;
				int ret, flags = 0;
				bool have_data = false;

				ctlbuf.buf = (caddr_t)ctl_s;
				ctlbuf.maxlen = sizeof(ctl_s);
				databuf.buf = (caddr_t)bp;
				databuf.maxlen = rem;

                                do {
                                        ret = getmsg(fd, &ctlbuf, &databuf, &flags);
                                } while (ret == -1 && errno == EINTR);
				if (ret == -1) {
					err = errno;
					goto out;
				} else if (ctlbuf.len == 1) {
					switch (ctl_s[0]) {
					case M_IOCTL:
						pty_termios_changed();
						break;
					case M_STOP:
						pty_scroll_lock_changed(true);
						break;
					case M_START:
						pty_scroll_lock_changed(false);
						break;
					case M_DATA:
						have_data = true;
						break;
					}
				} else if (ctlbuf.len == -1 && databuf.len != -1) {
					// MOREDATA
					have_data = true;
				}

				if (have_data) {
					if (databuf.len == 0) {
						eos = true;
						goto out;
					}
					bp += databuf.len;
					rem -= databuf.len;
					len += databuf.len;
				}
#else /* neither TIOCPKT nor STREAMS pty */
                                ssize_t ret;
                                do {
                                        ret = read(fd, bp, rem);
                                } while (ret == -1 && errno == EINTR);
				switch (ret) {
					case -1:
						err = errno;
						goto out;
					case 0:
						eos = true;
						goto out;
					default:
						bp += ret;
						rem -= ret;
						len += ret;
						break;
				}
#endif /* */
			} while (rem);
out:
			chunk->add_size(len);
			bytes += len;
		} while (bytes < max_bytes &&
                         // This means that either a read into a not-yet-¾-full
                         // chunk used up all the available capacity, so
                         // let's assume that we can read more and thus
                         // we'll get a new chunk in the loop above and
                         // continue on (see commit 49a0cdf11); or a short read
                         // occurred in which case we also keep looping, it's
                         // important in order to consume all the data after the
                         // child quits, see
                         // https://gitlab.gnome.org/GNOME/vte/-/issues/2627
                         // Note also that on EOS or error, this condition
                         // is false (since there was capacity, but it wasn't
                         // used up).
		         chunk->capacity_writing() == 0);

                /* We may have an empty chunk at the back of the queue, but
                 * that doesn't matter, we'll fill it next time.
                 */

		if (!is_processing()) {
			add_process_timeout(this);
		}
		m_pty_input_active = len != 0;
		m_input_bytes = bytes;
		again = bytes < max_bytes;

		_vte_debug_print (VTE_DEBUG_IO, "read %d/%d bytes, again? %s, active? %s\n",
				bytes, max_bytes,
				again ? "yes" : "no",
				m_pty_input_active ? "yes" : "no");
	}

        if (condition & G_IO_ERR)
                err = EIO;

	/* Error? */
	switch (err) {
        case 0: /* no error */
                break;
        case EIO: /* EOS */
                eos = true;
                break;
        case EAGAIN:
        case EBUSY: /* do nothing */
                break;
        default:
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print (VTE_DEBUG_IO, "Error reading from child: %s",
                                  g_strerror(errsv));
                break;
	}

        if (eos) {
		_vte_debug_print(VTE_DEBUG_IO, "got PTY EOF\n");

                /* Make a note of the EOS; but do not process it since there may be data
                 * to be processed first in the incoming queue.
                 */
                if (!chunk || chunk->sealed()) {
                        m_incoming_queue.push(vte::base::Chunk::get(chunk));
                        chunk = m_incoming_queue.back().get();
                }

                chunk->set_sealed();
                chunk->set_eos();

                /* Need to process the EOS */
		if (!is_processing()) {
			add_process_timeout(this);
		}

                again = false;
        }

	return again;
}

/*
 * Terminal::feed:
 * @data: data
 *
 * Interprets @data as if it were data received from a child process.
 */
void
Terminal::feed(std::string_view const& data,
               bool start_processing_)
{
        auto length = data.size();
        auto ptr = data.data();

        vte::base::Chunk* chunk = nullptr;
        if (!m_incoming_queue.empty()) {
                auto& achunk = m_incoming_queue.back();
                if (length < achunk->capacity_writing() && !achunk->sealed())
                        chunk = achunk.get();
        }
        if (chunk == nullptr) {
                m_incoming_queue.push(vte::base::Chunk::get(nullptr));
                chunk = m_incoming_queue.back().get();
        }

        /* Break the incoming data into chunks. */
        do {
                auto rem = chunk->capacity_writing();
                auto len = std::min(length, rem);
                memcpy (chunk->begin_writing(), ptr, len);
                chunk->add_size(len);
                length -= len;
                if (length == 0)
                        break;

                ptr += len;

                /* Get another chunk for the remaining data */
                m_incoming_queue.push(vte::base::Chunk::get(chunk));
                chunk = m_incoming_queue.back().get();
        } while (true);

        if (start_processing_)
                start_processing();
}

bool
Terminal::pty_io_write(int const fd,
                       GIOCondition const condition)
{
        auto const count = write(fd,
                                 m_outgoing->data,
                                 _vte_byte_array_length(m_outgoing));
	if (count != -1) {
		_VTE_DEBUG_IF (VTE_DEBUG_IO) {
                        _vte_debug_hexdump("Outgoing buffer written",
                                           (uint8_t const*)m_outgoing->data,
                                           count);
		}
		_vte_byte_array_consume(m_outgoing, count);
	}

        /* Run again if there are more bytes to write */
        return _vte_byte_array_length(m_outgoing) != 0;
}

/* Send some UTF-8 data to the child. */
void
Terminal::send_child(std::string_view const& data)
{
        // FIXMEchpe remove
        if (!m_input_enabled)
                return;

        /* Note that for backward compatibility, we need to emit the
         * ::commit signal even if there is no PTY. See issue vte#222.
         *
         * We use the primary data syntax to decide on the format.
         */

        switch (primary_data_syntax()) {
        case DataSyntax::ECMA48_UTF8:
                emit_commit(data);
                if (pty())
                        _vte_byte_array_append(m_outgoing, data.data(), data.size());
                break;

#if WITH_ICU
        case DataSyntax::ECMA48_PCTERM: {
                auto converted = m_converter->convert(data);

                emit_commit(converted);
                if (pty())
                        _vte_byte_array_append(m_outgoing, converted.data(), converted.size());
                break;
        }
#endif

        default:
                g_assert_not_reached();
                return;
        }

        /* If we need to start waiting for the child pty to
         * become available for writing, set that up here. */
        connect_pty_write();
}

/*
 * VteTerminal::feed_child:
 * @str: data to send to the child
 *
 * Sends UTF-8 text to the child as if it were entered by the user
 * at the keyboard.
 *
 * Does nothing if input is disabled.
 */
void
Terminal::feed_child(std::string_view const& str)
{
        if (!m_input_enabled)
                return;

        send_child(str);
}

/*
 * Terminal::feed_child_binary:
 * @data: data to send to the child
 *
 * Sends a block of binary data to the child.
 *
 * Does nothing if input is disabled.
 */
void
Terminal::feed_child_binary(std::string_view const& data)
{
        if (!m_input_enabled)
                return;

        /* If there's a place for it to go, add the data to the
         * outgoing buffer. */
        if (!pty())
                return;

        emit_commit(data);
        _vte_byte_array_append(m_outgoing, data.data(), data.size());

        /* If we need to start waiting for the child pty to
         * become available for writing, set that up here. */
        connect_pty_write();
}

void
Terminal::send(vte::parser::u8SequenceBuilder const& builder,
                         bool c1,
                         vte::parser::u8SequenceBuilder::Introducer introducer,
                         vte::parser::u8SequenceBuilder::ST st) noexcept
{
        std::string str;
        builder.to_string(str, c1, -1, introducer, st);
        feed_child(str);
}

void
Terminal::send(vte::parser::Sequence const& seq,
               vte::parser::u8SequenceBuilder const& builder) noexcept
{
        // FIXMEchpe always take c1 & ST from @seq?
        if (seq.type() == VTE_SEQ_OSC &&
            builder.type() == VTE_SEQ_OSC) {
                /* If we reply to a BEL-terminated OSC, reply with BEL-terminated OSC
                 * as well, see https://bugzilla.gnome.org/show_bug.cgi?id=722446 and
                 * https://gitlab.gnome.org/GNOME/vte/issues/65 .
                 */
                send(builder, false,
                     vte::parser::u8SequenceBuilder::Introducer::DEFAULT,
                     seq.st() == 0x7 ? vte::parser::u8SequenceBuilder::ST::BEL
                     : vte::parser::u8SequenceBuilder::ST::DEFAULT);
        } else {
                send(builder, false);
        }
}

void
Terminal::send(unsigned int type,
                         std::initializer_list<int> params) noexcept
{
        // FIXMEchpe take c1 & ST from @seq
        send(vte::parser::ReplyBuilder{type, params}, false);
}

void
Terminal::reply(vte::parser::Sequence const& seq,
                          unsigned int type,
                          std::initializer_list<int> params) noexcept
{
        send(seq, vte::parser::ReplyBuilder{type, params});
}

#if 0
void
Terminal::reply(vte::parser::Sequence const& seq,
                          unsigned int type,
                          std::initializer_list<int> params,
                          std::string const& str) noexcept
{
        vte::parser::ReplyBuilder reply_builder{type, params};
        reply_builder.set_string(str);
        send(seq, reply_builder);
}
#endif

void
Terminal::reply(vte::parser::Sequence const& seq,
                          unsigned int type,
                          std::initializer_list<int> params,
                          vte::parser::ReplyBuilder const& builder) noexcept
{
        std::string str;
        builder.to_string(str, true, -1,
                          vte::parser::ReplyBuilder::Introducer::NONE,
                          vte::parser::ReplyBuilder::ST::NONE);

        vte::parser::ReplyBuilder reply_builder{type, params};
        reply_builder.set_string(std::move(str));
        send(seq, reply_builder);
}

void
Terminal::reply(vte::parser::Sequence const& seq,
                          unsigned int type,
                          std::initializer_list<int> params,
                          char const* format,
                          ...) noexcept
{
        char buf[8192];
        va_list vargs;

        va_start(vargs, format);
        G_GNUC_UNUSED auto len = g_vsnprintf(buf, sizeof(buf), format, vargs);
        va_end(vargs);
        vte_assert_cmpint(len, <, sizeof(buf));

        vte::parser::ReplyBuilder builder{type, params};
        builder.set_string(std::string{buf});

        send(seq, builder);
}

void
Terminal::im_commit(std::string_view const& str)
{
        if (!m_input_enabled)
                return;

        _vte_debug_print(VTE_DEBUG_EVENTS,
                         "Input method committed `%s'.\n", std::string{str}.c_str());
        send_child(str);

	/* Committed text was committed because the user pressed a key, so
	 * we need to obey the scroll-on-keystroke setting. */
        if (m_scroll_on_keystroke && m_input_enabled) {
                scroll_to_bottom();
	}
}

void
Terminal::im_preedit_set_active(bool active) noexcept
{
	m_im_preedit_active = active;
}

void
Terminal::im_preedit_reset() noexcept
{
        m_im_preedit.clear();
        m_im_preedit.shrink_to_fit();
        m_im_preedit_cursor = 0;
        m_im_preedit_attrs.reset();
}

void
Terminal::im_preedit_changed(std::string_view const& str,
                             int cursorpos,
                             vte::Freeable<PangoAttrList> attrs) noexcept
{
	/* Queue the area where the current preedit string is being displayed
	 * for repainting. */
	invalidate_cursor_once();

        im_preedit_reset();
	m_im_preedit = str;
        m_im_preedit_attrs = std::move(attrs);
        m_im_preedit_cursor = cursorpos;

        /* Invalidate again with the new cursor position */
	invalidate_cursor_once();

        /* And tell the input method where the cursor is on the screen */
        im_update_cursor();

        if (m_scroll_on_keystroke && m_input_enabled) {
                scroll_to_bottom();
        }
}

bool
Terminal::im_retrieve_surrounding()
{
        /* FIXME: implement this! Bug #726191 */
        return false;
}

bool
Terminal::im_delete_surrounding(int offset,
                                int n_chars)
{
        /* FIXME: implement this! Bug #726191 */
        return false;
}

void
Terminal::im_update_cursor()
{
	if (!widget_realized())
                return;

        cairo_rectangle_int_t rect;
        rect.x = m_screen->cursor.col * m_cell_width + m_border.left +
                 get_preedit_width(true) * m_cell_width;
        rect.width = m_cell_width; // FIXMEchpe: if columns > 1 ?
        rect.y = row_to_pixel(m_screen->cursor.row) + m_border.top;
        rect.height = m_cell_height;
        m_real_widget->im_set_cursor_location(&rect);
}

bool
Terminal::set_style_border(GtkBorder const& border) noexcept
{
        auto const changing = memcmp(&border, &m_style_border, sizeof(border)) != 0;
        m_style_border = border;
        return changing;
}

void
Terminal::set_cursor_aspect(float aspect)
{
        if (_vte_double_equal(aspect, m_cursor_aspect_ratio))
                return;

        m_cursor_aspect_ratio = aspect;
        invalidate_cursor_once();
}

void
Terminal::widget_style_updated()
{
        // FIXMEchpe: remove taking font info from the widget style
        update_font_desc();
}

void
Terminal::add_cursor_timeout()
{
	if (m_cursor_blink_timer)
		return; /* already added */

	m_cursor_blink_time_ms = 0;
        m_cursor_blink_timer.schedule(m_cursor_blink_cycle_ms,
                                      vte::glib::Timer::Priority::eLOW);
}

void
Terminal::remove_cursor_timeout()
{
	if (!m_cursor_blink_timer)
		return; /* already removed */

        m_cursor_blink_timer.abort();
        if (!m_cursor_blink_state) {
                invalidate_cursor_once();
                m_cursor_blink_state = true;
        }
}

/* Activates / disactivates the cursor blink timer to reduce wakeups */
void
Terminal::check_cursor_blink()
{
	if (m_has_focus &&
	    m_cursor_blinks &&
	    m_modes_private.DEC_TEXT_CURSOR())
		add_cursor_timeout();
	else
		remove_cursor_timeout();
}

void
Terminal::beep()
{
	if (m_audible_bell)
                m_real_widget->beep();
}

bool
Terminal::widget_key_press(vte::platform::KeyEvent const& event)
{
        auto handled = false;
	char *normal = NULL;
	gsize normal_length = 0;
	struct termios tio;
	gboolean scrolled = FALSE, steal = FALSE, modifier = FALSE,
		 suppress_alt_esc = FALSE, add_modifiers = FALSE;
	guint keyval = 0;
	gunichar keychar = 0;
	char keybuf[VTE_UTF8_BPC];

	/* If it's a keypress, record that we got the event, in case the
	 * input method takes the event from us. */
        // FIXMEchpe this is ::widget_key_press; what other event type could it even be!?
	if (event.is_key_press()) {
		/* Store a copy of the key. */
                keyval = event.keyval();
                m_modifiers = event.modifiers();

                // FIXMEchpe?
		if (m_cursor_blink_timer) {
			remove_cursor_timeout();
			add_cursor_timeout();
		}

		/* Determine if this is just a modifier key. */
		modifier = _vte_keymap_key_is_modifier(keyval);

		/* Unless it's a modifier key, hide the pointer. */
		if (!modifier) {
                        set_pointer_autohidden(true);
		}

		/* We steal many keypad keys here. */
		if (!m_im_preedit_active) {
			switch (keyval) {
			case GDK_KEY_KP_Add:
			case GDK_KEY_KP_Subtract:
			case GDK_KEY_KP_Multiply:
			case GDK_KEY_KP_Divide:
			case GDK_KEY_KP_Enter:
				steal = TRUE;
				break;
			default:
				break;
			}
			if (m_modifiers & VTE_ALT_MASK) {
				steal = TRUE;
			}
			switch (keyval) {
                        case GDK_KEY_ISO_Lock:
                        case GDK_KEY_ISO_Level2_Latch:
                        case GDK_KEY_ISO_Level3_Shift:
                        case GDK_KEY_ISO_Level3_Latch:
                        case GDK_KEY_ISO_Level3_Lock:
                        case GDK_KEY_ISO_Level5_Shift:
                        case GDK_KEY_ISO_Level5_Latch:
                        case GDK_KEY_ISO_Level5_Lock:
                        case GDK_KEY_ISO_Group_Shift:
                        case GDK_KEY_ISO_Group_Latch:
                        case GDK_KEY_ISO_Group_Lock:
                        case GDK_KEY_ISO_Next_Group:
                        case GDK_KEY_ISO_Next_Group_Lock:
                        case GDK_KEY_ISO_Prev_Group:
                        case GDK_KEY_ISO_Prev_Group_Lock:
                        case GDK_KEY_ISO_First_Group:
                        case GDK_KEY_ISO_First_Group_Lock:
                        case GDK_KEY_ISO_Last_Group:
                        case GDK_KEY_ISO_Last_Group_Lock:
			case GDK_KEY_Multi_key:
			case GDK_KEY_Codeinput:
			case GDK_KEY_SingleCandidate:
			case GDK_KEY_MultipleCandidate:
			case GDK_KEY_PreviousCandidate:
			case GDK_KEY_Kanji:
			case GDK_KEY_Muhenkan:
                        case GDK_KEY_Henkan_Mode:
                        /* case GDK_KEY_Henkan: is GDK_KEY_Henkan_Mode */
			case GDK_KEY_Romaji:
			case GDK_KEY_Hiragana:
			case GDK_KEY_Katakana:
			case GDK_KEY_Hiragana_Katakana:
			case GDK_KEY_Zenkaku:
			case GDK_KEY_Hankaku:
			case GDK_KEY_Zenkaku_Hankaku:
			case GDK_KEY_Touroku:
			case GDK_KEY_Massyo:
			case GDK_KEY_Kana_Lock:
			case GDK_KEY_Kana_Shift:
			case GDK_KEY_Eisu_Shift:
			case GDK_KEY_Eisu_toggle:
                        /* case GDK_KEY_Kanji_Bangou: is GDK_KEY_Codeinput */
                        /* case GDK_KEY_Zen_Koho: is GDK_KEY_MultipleCandidate */
                        /* case GDK_KEY_Mae_Koho: is GDK_KEY_PreviousCandidate */
                        /* case GDK_KEY_kana_switch: is GDK_KEY_ISO_Group_Shift */
                        case GDK_KEY_Hangul:
                        case GDK_KEY_Hangul_Start:
                        case GDK_KEY_Hangul_End:
                        case GDK_KEY_Hangul_Hanja:
                        case GDK_KEY_Hangul_Jamo:
                        case GDK_KEY_Hangul_Romaja:
                        /* case GDK_KEY_Hangul_Codeinput: is GDK_KEY_Codeinput */
                        case GDK_KEY_Hangul_Jeonja:
                        case GDK_KEY_Hangul_Banja:
                        case GDK_KEY_Hangul_PreHanja:
                        case GDK_KEY_Hangul_PostHanja:
                        /* case GDK_KEY_Hangul_SingleCandidate: is GDK_KEY_SingleCandidate */
                        /* case GDK_KEY_Hangul_MultipleCandidate: is GDK_KEY_MultipleCandidate */
                        /* case GDK_KEY_Hangul_PreviousCandidate: is GDK_KEY_PreviousCandidate */
                        case GDK_KEY_Hangul_Special:
                        /* case GDK_KEY_Hangul_switch: is GDK_KEY_ISO_Group_Shift */

				steal = FALSE;
				break;
			default:
				break;
			}
		}
	}

	/* Let the input method at this one first. */
	if (!steal && m_input_enabled) {
                // FIXMEchpe FIXMEgtk4: update IM position? im_set_cursor_location()
                if (m_real_widget->im_filter_keypress(event)) {
			_vte_debug_print(VTE_DEBUG_EVENTS,
					"Keypress taken by IM.\n");
			return true;
		}
	}

        // Try showing the context menu
        if (!handled &&
            (event.matches(GDK_KEY_Menu, 0) ||
             event.matches(GDK_KEY_F10, GDK_SHIFT_MASK))) {
                _vte_debug_print(VTE_DEBUG_EVENTS, "Showing context menu\n");
                handled = widget()->show_context_menu(vte::platform::EventContext{event});
        }

	/* Now figure out what to send to the child. */
	if (event.is_key_press() && !modifier && !handled) {
		/* Map the key to a sequence name if we can. */
		switch (event.keyval()) {
		case GDK_KEY_BackSpace:
			switch (m_backspace_binding) {
			case EraseMode::eASCII_BACKSPACE:
				normal = g_strdup("");
				normal_length = 1;
				suppress_alt_esc = FALSE;
				break;
			case EraseMode::eASCII_DELETE:
				normal = g_strdup("");
				normal_length = 1;
				suppress_alt_esc = FALSE;
				break;
			case EraseMode::eDELETE_SEQUENCE:
                                normal = g_strdup("\e[3~");
                                normal_length = 4;
                                add_modifiers = TRUE;
				suppress_alt_esc = TRUE;
				break;
			case EraseMode::eTTY:
				if (pty() &&
				    tcgetattr(pty()->fd(), &tio) != -1)
				{
					normal = g_strdup_printf("%c", tio.c_cc[VERASE]);
					normal_length = 1;
				}
				suppress_alt_esc = FALSE;
				break;
			case EraseMode::eAUTO:
			default:
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE '\0'
#endif
				if (pty() &&
				    tcgetattr(pty()->fd(), &tio) != -1 &&
				    tio.c_cc[VERASE] != _POSIX_VDISABLE)
				{
					normal = g_strdup_printf("%c", tio.c_cc[VERASE]);
					normal_length = 1;
				}
				else
				{
					normal = g_strdup("");
					normal_length = 1;
					suppress_alt_esc = FALSE;
				}
				suppress_alt_esc = FALSE;
				break;
			}
                        /* Toggle ^H vs ^? if Ctrl is pressed */
                        if (normal_length == 1 && m_modifiers & GDK_CONTROL_MASK) {
                                if (normal[0] == '\010')
                                        normal[0] = '\177';
                                else if (normal[0] == '\177')
                                        normal[0] = '\010';
                        }
			handled = TRUE;
			break;
		case GDK_KEY_KP_Delete:
		case GDK_KEY_Delete:
			switch (m_delete_binding) {
			case EraseMode::eASCII_BACKSPACE:
				normal = g_strdup("\010");
				normal_length = 1;
				break;
			case EraseMode::eASCII_DELETE:
				normal = g_strdup("\177");
				normal_length = 1;
				break;
			case EraseMode::eTTY:
				if (pty() &&
				    tcgetattr(pty()->fd(), &tio) != -1)
				{
					normal = g_strdup_printf("%c", tio.c_cc[VERASE]);
					normal_length = 1;
				}
				suppress_alt_esc = FALSE;
				break;
			case EraseMode::eDELETE_SEQUENCE:
			case EraseMode::eAUTO:
			default:
                                normal = g_strdup("\e[3~");
                                normal_length = 4;
                                add_modifiers = TRUE;
				break;
			}
			handled = TRUE;
                        /* FIXMEchpe: why? this overrides the FALSE set above? */
			suppress_alt_esc = TRUE;
			break;
		case GDK_KEY_KP_Insert:
		case GDK_KEY_Insert:
			if (m_modifiers & GDK_SHIFT_MASK) {
				if (m_modifiers & GDK_CONTROL_MASK) {
                                        emit_paste_clipboard();
					handled = TRUE;
					suppress_alt_esc = TRUE;
				} else {
                                        widget()->clipboard_request_text(vte::platform::ClipboardType::PRIMARY);
					handled = TRUE;
					suppress_alt_esc = TRUE;
				}
			} else if (m_modifiers & GDK_CONTROL_MASK) {
                                emit_copy_clipboard();
				handled = TRUE;
				suppress_alt_esc = TRUE;
			}
			break;
		/* Keypad/motion keys. */
		case GDK_KEY_KP_Up:
		case GDK_KEY_Up:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_CONTROL_MASK &&
                            m_modifiers & GDK_SHIFT_MASK) {
				scroll_lines(-1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_alt_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Down:
		case GDK_KEY_Down:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_CONTROL_MASK &&
                            m_modifiers & GDK_SHIFT_MASK) {
				scroll_lines(1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_alt_esc = TRUE;
			}
			break;
                case GDK_KEY_KP_Left:
                case GDK_KEY_Left:
                        if (m_screen == &m_normal_screen &&
                            m_modifiers & GDK_CONTROL_MASK &&
                            m_modifiers & GDK_SHIFT_MASK) {
                                scroll_to_previous_prompt();
                                scrolled = TRUE;
                                handled = TRUE;
                                suppress_alt_esc = TRUE;
                        }
                        break;
                case GDK_KEY_KP_Right:
                case GDK_KEY_Right:
                        if (m_screen == &m_normal_screen &&
                            m_modifiers & GDK_CONTROL_MASK &&
                            m_modifiers & GDK_SHIFT_MASK) {
                                scroll_to_next_prompt();
                                scrolled = TRUE;
                                handled = TRUE;
                                suppress_alt_esc = TRUE;
                        }
                        break;
		case GDK_KEY_KP_Page_Up:
		case GDK_KEY_Page_Up:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_SHIFT_MASK) {
				scroll_pages(-1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_alt_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Page_Down:
		case GDK_KEY_Page_Down:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_SHIFT_MASK) {
				scroll_pages(1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_alt_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Home:
		case GDK_KEY_Home:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_SHIFT_MASK) {
                                scroll_to_top();
				scrolled = TRUE;
				handled = TRUE;
			}
			break;
		case GDK_KEY_KP_End:
		case GDK_KEY_End:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_SHIFT_MASK) {
                                scroll_to_bottom();
				scrolled = TRUE;
				handled = TRUE;
			}
			break;
		/* Let Shift +/- tweak the font, like XTerm does. */
		case GDK_KEY_KP_Add:
		case GDK_KEY_KP_Subtract:
			if (m_modifiers & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
				switch (keyval) {
				case GDK_KEY_KP_Add:
					emit_increase_font_size();
					handled = TRUE;
					suppress_alt_esc = TRUE;
					break;
				case GDK_KEY_KP_Subtract:
					emit_decrease_font_size();
					handled = TRUE;
					suppress_alt_esc = TRUE;
					break;
				}
			}
			break;
		default:
			break;
		}
		/* If the above switch statement didn't do the job, try mapping
		 * it to a literal or capability name. */
                if (handled == FALSE) {
                        if (G_UNLIKELY (m_enable_bidi &&
                                        m_modes_private.VTE_BIDI_SWAP_ARROW_KEYS() &&
                                        (keyval == GDK_KEY_Left ||
                                         keyval == GDK_KEY_Right ||
                                         keyval == GDK_KEY_KP_Left ||
                                         keyval == GDK_KEY_KP_Right))) {
                                /* In keyboard arrow swapping mode, the left and right arrows need swapping
                                 * if the cursor stands inside a (possibly autodetected) RTL paragraph. */
                                ensure_row();
                                VteRowData const *row_data = find_row_data(m_screen->cursor.row);
                                bool rtl;
                                if ((row_data->attr.bidi_flags & (VTE_BIDI_FLAG_IMPLICIT | VTE_BIDI_FLAG_AUTO))
                                                              == (VTE_BIDI_FLAG_IMPLICIT | VTE_BIDI_FLAG_AUTO)) {
                                        /* Implicit paragraph with autodetection. Need to run the BiDi algorithm
                                         * to get the autodetected direction.
                                         * m_ringview is for the onscreen contents and the cursor may be offscreen.
                                         * Better leave that alone and use a temporary ringview for the cursor's row. */
                                        vte::base::RingView ringview;
                                        ringview.set_ring(m_screen->row_data);
                                        ringview.set_rows(m_screen->cursor.row, 1);
                                        ringview.set_width(m_column_count);
                                        ringview.update();
                                        rtl = ringview.get_bidirow(m_screen->cursor.row)->base_is_rtl();
                                } else {
                                        /* Not an implicit paragraph with autodetection, no autodetection
                                         * is required. Take the direction straight from the stored data. */
                                        rtl = !!(row_data->attr.bidi_flags & VTE_BIDI_FLAG_RTL);
                                }
                                if (rtl) {
                                        switch (keyval) {
                                        case GDK_KEY_Left:
                                                keyval = GDK_KEY_Right;
                                                break;
                                        case GDK_KEY_Right:
                                                keyval = GDK_KEY_Left;
                                                break;
                                        case GDK_KEY_KP_Left:
                                                keyval = GDK_KEY_KP_Right;
                                                break;
                                        case GDK_KEY_KP_Right:
                                                keyval = GDK_KEY_KP_Left;
                                                break;
                                        }
                                }
                        }

			_vte_keymap_map(keyval, m_modifiers,
                                        m_modes_private.DEC_APPLICATION_CURSOR_KEYS(),
                                        m_modes_private.DEC_APPLICATION_KEYPAD(),
					&normal,
					&normal_length);
			/* If we found something this way, suppress
			 * escape-on-alt. */
                        if (normal != NULL && normal_length > 0) {
				suppress_alt_esc = TRUE;
			}
		}

		/* Shall we do this here or earlier?  See bug 375112 and bug 589557 */
		if (m_modifiers & GDK_CONTROL_MASK && widget())
                        keyval = widget()->key_event_translate_ctrlkey(event);

		/* If we didn't manage to do anything, try to salvage a
		 * printable string. */
		if (handled == FALSE && normal == NULL) {

			/* Convert the keyval to a gunichar. */
			keychar = gdk_keyval_to_unicode(keyval);
			normal_length = 0;
			if (keychar != 0) {
				/* Convert the gunichar to a string. */
				normal_length = g_unichar_to_utf8(keychar,
								  keybuf);
				if (normal_length != 0) {
					normal = (char *)g_malloc(normal_length + 1);
					memcpy(normal, keybuf, normal_length);
					normal[normal_length] = '\0';
				} else {
					normal = NULL;
				}
			}
			if ((normal != NULL) &&
			    (m_modifiers & GDK_CONTROL_MASK)) {
				/* Replace characters which have "control"
				 * counterparts with those counterparts. */
				for (size_t i = 0; i < normal_length; i++) {
					if ((((guint8)normal[i]) >= 0x40) &&
					    (((guint8)normal[i]) <  0x80)) {
						normal[i] &= (~(0x60));
					}
				}
			}
			_VTE_DEBUG_IF (VTE_DEBUG_EVENTS) {
				if (normal) g_printerr(
						"Keypress, modifiers=0x%x, "
						"keyval=0x%x, cooked string=`%s'.\n",
						m_modifiers,
						keyval, normal);
			}
		}
		/* If we got normal characters, send them to the child. */
		if (normal != NULL) {
                        if (add_modifiers) {
                                _vte_keymap_key_add_key_modifiers(keyval,
                                                                  m_modifiers,
                                                                  m_modes_private.DEC_APPLICATION_CURSOR_KEYS(),
                                                                  &normal,
                                                                  &normal_length);
                        }
			if (m_modes_private.XTERM_META_SENDS_ESCAPE() &&
			    !suppress_alt_esc &&
			    (normal_length > 0) &&
			    (m_modifiers & VTE_ALT_MASK)) {
				feed_child(_VTE_CAP_ESC, 1);
			}
			if (normal_length > 0) {
				send_child({normal, normal_length});
			}
			g_free(normal);
		}
		/* Keep the cursor on-screen. */
		if (!scrolled && !modifier &&
                    m_scroll_on_keystroke && m_input_enabled) {
                        scroll_to_bottom();
		}
		handled = true;
	}

	return handled;
}

bool
Terminal::widget_key_release(vte::platform::KeyEvent const& event)
{
        m_modifiers = event.modifiers();

	if (m_input_enabled &&
            m_real_widget->im_filter_keypress(event))
                return true;

        return false;
}

#if VTE_GTK == 4

bool
Terminal::widget_key_modifiers(unsigned modifiers)
{
        m_modifiers = modifiers;
        return true;
}

#endif /* VTE_GTK == 4 */

static const guint8 word_char_by_category[] = {
        [G_UNICODE_CONTROL]             = 2,
        [G_UNICODE_FORMAT]              = 2,
        [G_UNICODE_UNASSIGNED]          = 2,
        [G_UNICODE_PRIVATE_USE]         = 0,
        [G_UNICODE_SURROGATE]           = 2,
        [G_UNICODE_LOWERCASE_LETTER]    = 1,
        [G_UNICODE_MODIFIER_LETTER]     = 1,
        [G_UNICODE_OTHER_LETTER]        = 1,
        [G_UNICODE_TITLECASE_LETTER]    = 1,
        [G_UNICODE_UPPERCASE_LETTER]    = 1,
        [G_UNICODE_SPACING_MARK]        = 0,
        [G_UNICODE_ENCLOSING_MARK]      = 0,
        [G_UNICODE_NON_SPACING_MARK]    = 0,
        [G_UNICODE_DECIMAL_NUMBER]      = 1,
        [G_UNICODE_LETTER_NUMBER]       = 1,
        [G_UNICODE_OTHER_NUMBER]        = 1,
        [G_UNICODE_CONNECT_PUNCTUATION] = 0,
        [G_UNICODE_DASH_PUNCTUATION]    = 0,
        [G_UNICODE_CLOSE_PUNCTUATION]   = 0,
        [G_UNICODE_FINAL_PUNCTUATION]   = 0,
        [G_UNICODE_INITIAL_PUNCTUATION] = 0,
        [G_UNICODE_OTHER_PUNCTUATION]   = 0,
        [G_UNICODE_OPEN_PUNCTUATION]    = 0,
        [G_UNICODE_CURRENCY_SYMBOL]     = 0,
        [G_UNICODE_MODIFIER_SYMBOL]     = 0,
        [G_UNICODE_MATH_SYMBOL]         = 0,
        [G_UNICODE_OTHER_SYMBOL]        = 0,
        [G_UNICODE_LINE_SEPARATOR]      = 2,
        [G_UNICODE_PARAGRAPH_SEPARATOR] = 2,
        [G_UNICODE_SPACE_SEPARATOR]     = 2,
};

/*
 * Terminal::is_word_char:
 * @c: a candidate Unicode code point
 *
 * Checks if a particular character is considered to be part of a word or not.
 *
 * Returns: %TRUE if the character is considered to be part of a word
 */
bool
Terminal::is_word_char(gunichar c) const
{
        const guint8 v = word_char_by_category[g_unichar_type(c)];

        if (v)
                return v == 1;

        /* Do we have an exception? */
        return std::find(std::begin(m_word_char_exceptions), std::end(m_word_char_exceptions), char32_t(c)) != std::end(m_word_char_exceptions);
}

/* Check if the characters in the two given locations are in the same class
 * (word vs. non-word characters).
 * Note that calling this method may invalidate the return value of
 * a previous find_row_data() call. */
bool
Terminal::is_same_class(vte::grid::column_t acol,
                                  vte::grid::row_t arow,
                                  vte::grid::column_t bcol,
                                  vte::grid::row_t brow) const
{
	VteCell const* pcell = nullptr;
	bool word_char;
	if ((pcell = find_charcell(acol, arow)) != nullptr && pcell->c != 0) {
                /* Group together if they're fragments of the very same character (not just character value) */
                if (arow == brow) {
                        auto a2 = acol, b2 = bcol;
                        while (a2 > 0 && find_charcell(a2, arow)->attr.fragment()) a2--;
                        while (b2 > 0 && find_charcell(b2, brow)->attr.fragment()) b2--;
                        if (a2 == b2)
                                return true;
                }

		word_char = is_word_char(_vte_unistr_get_base(pcell->c));

		/* Lets not group non-wordchars together (bug #25290) */
		if (!word_char)
			return false;

		pcell = find_charcell(bcol, brow);
		if (pcell == NULL || pcell->c == 0) {
			return false;
		}
		if (word_char != is_word_char(_vte_unistr_get_base(pcell->c))) {
			return false;
		}
		return true;
	}
	return false;
}

/*
 * Convert the mouse click or drag location (left or right half of a cell) into a selection endpoint
 * (a boundary between characters), extending the selection according to the current mode, in the
 * direction given in the @after parameter.
 *
 * All four selection modes require different strategies.
 *
 * In char mode, what matters is which vertical character boundary is closer, taking multi-cell characters
 * (CJKs, TABs) into account. Given the string "abcdef", if the user clicks on the boundary between "a"
 * and "b" (perhaps on the right half of "a", perhaps on the left half of "b"), and moves the mouse to the
 * boundary between "e" and "f" (perhaps a bit over "e", perhaps a bit over "f"), the selection should be
 * "bcde". By dragging the mouse back to approximately the click location, it is possible to select the
 * empty string. This is the common sense behavior impemented by basically every graphical toolkit
 * (unfortunately not by many terminal emulators), and also the one we go for.
 *
 * Word mode is the trickiest one. Many implementations have weird corner case bugs (e.g. don't highlight
 * a word if you double click on the second half of its last letter, or even highlight it if you click on
 * the first half of the following space). I think it is expected that double-clicking anywhere over a
 * word (including the first half of its first letter, or the last half of its last letter), but over no
 * other character, selects this entire word. By dragging the mouse it's not possible to select nothing,
 * the word (or non-word character) initially clicked on is always part of the selection. (An exception
 * is when clicking occurred over the margin, or an unused cell in a soft-wrapped row (due to CJK
 * wrapping).) Also, for symmetry reasons, the word (or non-word character) under the current mouse
 * location is also always selected.
 *
 * Line (paragraph) mode is conceptually quite similar to word mode (the cell, thus the entire row under
 * the click's location is always included), but is much easier to implement.
 *
 * In block mode, similarly to char mode, we care about vertical character boundary. (This is somewhat
 * debatable, as results in asymmetrical behavior along the two axes: a rectangle can disappear by
 * becoming zero wide, but not zero high.) We cannot take care of CJKs at the endpoints now because CJKs
 * can cross the boundary in any included row. Taking care of them needs to go to cell_is_selected_*().
 * We don't care about used vs. unused cells either. The event coordinate is simply rounded to the
 * nearest vertical cell boundary.
 */
vte::grid::coords
Terminal::resolve_selection_endpoint(vte::grid::halfcoords const& rowcolhalf, bool after) const
{
        auto row = rowcolhalf.row();
        auto col = rowcolhalf.halfcolumn().column();  /* Points to an actual cell now. At the end of this
                                                         method it'll point to a boundary. */
        auto half = rowcolhalf.halfcolumn().half();  /* 0 for left half, 1 for right half of the cell. */
        VteRowData const* rowdata;
        VteCell const* cell;
        int len;

        if (m_selection_block_mode) {
                /* Just find the nearest cell boundary within the line, not caring about CJKs, unused
                 * cells, or wrapping at EOL. The @after parameter is unused in this mode. */
                col += half;
                col = std::clamp (col, 0L, m_column_count);
        } else {
                switch (m_selection_type) {
                case SelectionType::eCHAR:
                        /* Find the nearest actual character boundary, taking CJKs and TABs into account.
                         * If at least halfway through the first unused cell, or over the right margin
                         * then wrap to the beginning of the next line.
                         * The @after parameter is unused in this mode. */
                        if (col < 0) {
                                col = 0;
                        } else if (col >= m_column_count) {
                                /* If on the right padding, select the entire line including a possible
                                 * newline character. This way if a line is fully filled and ends in a
                                 * newline, there's only a half cell width for which the line is selected
                                 * without the newline, but at least there's a way to include the newline
                                 * by moving the mouse to the right (bug 724253). */
                                col = 0;
                                row++;
                        } else {
                                vte::grid::column_t char_begin, char_end;  /* cell boundaries */
                                rowdata = find_row_data(row);
                                if (rowdata && col < _vte_row_data_nonempty_length(rowdata)) {
                                        /* Clicked over a used cell. Check for multi-cell characters. */
                                        char_begin = col;
                                        while (char_begin > 0) {
                                                cell = _vte_row_data_get (rowdata, char_begin);
                                                if (!cell->attr.fragment())
                                                        break;
                                                char_begin--;
                                        }
                                        cell = _vte_row_data_get (rowdata, char_begin);
                                        char_end = char_begin + cell->attr.columns();
                                } else {
                                        /* Clicked over unused area. Just go with cell boundaries. */
                                        char_begin = col;
                                        char_end = char_begin + 1;
                                }
                                /* Which boundary is closer? */
                                if (col * 2 + half < char_begin + char_end)
                                        col = char_begin;
                                else
                                        col = char_end;

                                /* Maybe wrap to the beginning of the next line. */
                                if (col > (rowdata ? _vte_row_data_nonempty_length(rowdata) : 0)) {
                                        col = 0;
                                        row++;
                                }
                        }
                        break;

                case SelectionType::eWORD:
                        /* Initialization for the cumbersome cases where the click didn't occur over an actual used cell. */
                        rowdata = find_row_data(row);
                        if (col < 0) {
                                /* Clicked over the left margin.
                                 * - If within a word (that is, the first letter in this row, and the last
                                 *   letter of the previous row belong to the same word) then select the
                                 *   letter according to the direction and continue expanding.
                                 * - Otherwise stop, the boundary is here before the first letter. */
                                if (row > 0 &&
                                    (rowdata = find_row_data(row - 1)) != nullptr &&
                                    rowdata->attr.soft_wrapped &&
                                    (len = _vte_row_data_nonempty_length(rowdata)) > 0 &&
                                    is_same_class(len - 1, row - 1, 0, row) /* invalidates rowdata! */) {
                                        if (!after) {
                                                col = len - 1;
                                                row--;
                                        } else {
                                                col = 0;
                                        }
                                        /* go on with expanding */
                                } else {
                                        col = 0;  /* end-exclusive */
                                        break;  /* done, don't expand any more */
                                }
                        } else if (col >= (rowdata ? _vte_row_data_nonempty_length(rowdata) : 0)) {
                                /* Clicked over the right margin, or right unused area.
                                 * - If within a word (that is, the last letter in this row, and the first
                                 *   letter of the next row belong to the same word) then select the letter
                                 *   according to the direction and continue expanding.
                                 * - Otherwise, if the row is soft-wrapped and we're over the unused area
                                 *   (which can happen if a CJK wrapped) or over the right margin, then
                                 *   stop, the boundary is wrapped to the beginning of the next row.
                                 * - Otherwise select the newline only and stop. */
                                if (rowdata != nullptr &&
                                    rowdata->attr.soft_wrapped) {
                                        if ((len = _vte_row_data_nonempty_length(rowdata)) > 0 &&
                                            is_same_class(len - 1, row, 0, row + 1) /* invalidates rowdata! */) {
                                                if (!after) {
                                                        col = len - 1;
                                                } else {
                                                        col = 0;
                                                        row++;
                                                }
                                                /* go on with expanding */
                                        } else {
                                                col = 0;  /* end-exclusive */
                                                row++;
                                                break;  /* done, don't expand any more */
                                        }
                                } else {
                                        if (!after) {
                                                col = rowdata ? _vte_row_data_nonempty_length(rowdata) : 0;  /* end-exclusive */
                                        } else {
                                                col = 0;  /* end-exclusive */
                                                row++;
                                        }
                                        break;  /* done, don't expand any more */
                                }
                        }

                        /* Expand in the given direction. */
                        if (!after) {
                                /* Keep selecting to the left (and then up) as long as the next character
                                 * we look at is of the same class as the current start point. */
                                while (true) {
                                        /* Back up within the row. */
                                        for (; col > 0; col--) {
                                                if (!is_same_class(col - 1, row, col, row)) {
                                                        break;
                                                }
                                        }
                                        if (col > 0) {
                                                /* We hit a stopping point, so stop. */
                                                break;
                                        }
                                        if (row == 0) {
                                                /* At the very beginning. */
                                                break;
                                        }
                                        rowdata = find_row_data(row - 1);
                                        if (!rowdata || !rowdata->attr.soft_wrapped) {
                                                /* Reached a hard newline. */
                                                break;
                                        }
                                        len = _vte_row_data_nonempty_length(rowdata);
                                        /* len might be smaller than m_column_count if a CJK wrapped */
                                        if (!is_same_class(len - 1, row - 1, col, row) /* invalidates rowdata! */) {
                                                break;
                                        }
                                        /* Move on to the previous line. */
                                        col = len - 1;
                                        row--;
                                }
                        } else {
                                /* Keep selecting to the right (and then down) as long as the next character
                                 * we look at is of the same class as the current end point. */
                                while (true) {
                                        rowdata = find_row_data(row);
                                        if (!rowdata) {
                                                break;
                                        }
                                        len = _vte_row_data_nonempty_length(rowdata);
                                        bool soft_wrapped = rowdata->attr.soft_wrapped;
                                        /* Move forward within the row. */
                                        for (; col < len - 1; col++) {
                                                if (!is_same_class(col, row, col + 1, row) /* invalidates rowdata! */) {
                                                        break;
                                                }
                                        }
                                        if (col < len - 1) {
                                                /* We hit a stopping point, so stop. */
                                                break;
                                        }
                                        if (!soft_wrapped) {
                                                /* Reached a hard newline. */
                                                break;
                                        }
                                        if (!is_same_class(col, row, 0, row + 1)) {
                                                break;
                                        }
                                        /* Move on to the next line. */
                                        col = 0;
                                        row++;
                                }
                                col++;  /* col points to an actual cell, we need end-exclusive instead. */
                        }
                        break;

                case SelectionType::eLINE:
                        if (!after) {
                                /* Back up as far as we can go. */
                                while (row > 0 &&
                                       m_screen->row_data->contains(row - 1) &&
                                       m_screen->row_data->is_soft_wrapped(row - 1)) {
                                        row--;
                                }
                        } else {
                                /* Move forward as far as we can go. */
                                while (m_screen->row_data->contains(row) &&
                                       m_screen->row_data->is_soft_wrapped(row)) {
                                        row++;
                                }
                                row++;  /* One more row, since the column is 0. */
                        }
                        col = 0;
                        break;
                }
        }

        return { row, col };
}

/*
 * Creates the selection's span from the origin and last coordinates.
 *
 * The origin and last points might be in reverse order; in block mode they might even point to the
 * two other corners of the rectangle than the ones we're interested in.
 * The resolved span will contain the endpoints in the proper order.
 *
 * In word & line (paragraph) modes it extends the selection accordingly.
 *
 * Also makes sure to invalidate the regions that changed, and update m_selecting_had_delta.
 *
 * FIXMEegmont it always resolves both endpoints. With a bit of extra bookkeeping it could usually
 * just resolve the moving one.
 */
void
Terminal::resolve_selection()
{
        if (m_selection_origin.row() < 0 || m_selection_last.row() < 0) {
                invalidate (m_selection_resolved);
                m_selection_resolved.clear();
                _vte_debug_print(VTE_DEBUG_SELECTION, "Selection resolved to %s.\n", m_selection_resolved.to_string());
                return;
        }

        auto m_selection_resolved_old = m_selection_resolved;

        if (m_selection_block_mode) {
                auto top    = std::min (m_selection_origin.row(), m_selection_last.row());
                auto bottom = std::max (m_selection_origin.row(), m_selection_last.row());
                auto left   = std::min (m_selection_origin.halfcolumn(), m_selection_last.halfcolumn());
                auto right  = std::max (m_selection_origin.halfcolumn(), m_selection_last.halfcolumn());

                auto topleft     = resolve_selection_endpoint ({ top,    left  }, false);
                auto bottomright = resolve_selection_endpoint ({ bottom, right }, true);

                if (topleft.column() == bottomright.column()) {
                        m_selection_resolved.clear();
                } else {
                        m_selection_resolved.set (topleft, bottomright);
                }
        } else {
                auto start = std::min (m_selection_origin, m_selection_last);
                auto end   = std::max (m_selection_origin, m_selection_last);

                m_selection_resolved.set (resolve_selection_endpoint (start, false),
                                          resolve_selection_endpoint (end,   true));
        }

        if (!m_selection_resolved.empty())
                m_selecting_had_delta = true;

        _vte_debug_print(VTE_DEBUG_SELECTION, "Selection resolved to %s.\n", m_selection_resolved.to_string());

        invalidate_symmetrical_difference (m_selection_resolved_old, m_selection_resolved, m_selection_block_mode);
}

void
Terminal::modify_selection (vte::view::coords const& pos)
{
        g_assert (m_selecting);

        /* Need to ensure the ringview is updated. */
        ringview_update();

        auto current = selection_grid_halfcoords_from_view_coords (pos);

        if (current == m_selection_last)
                return;

        _vte_debug_print(VTE_DEBUG_SELECTION,
                         "Selection dragged to %s.\n",
                         current.to_string());

        m_selection_last = current;
        resolve_selection();
}

/* Check if a cell is selected or not. BiDi: the coordinate is logical. */
bool
Terminal::_cell_is_selected_log(vte::grid::column_t lcol,
                               vte::grid::row_t row) const
{
        // Callers need to update the ringview. However, don't assert, just
        // return out-of-view coords. FIXME: may want to throw instead
        if (!m_ringview.is_updated())
                [[unlikely]] return false;

        if (m_selection_block_mode) {
                /* In block mode, make sure CJKs and TABs aren't cut in half. */
                while (lcol > 0) {
                        VteCell const* cell = find_charcell(lcol, row);
                        if (!cell || !cell->attr.fragment())
                                break;
                        lcol--;
                }
                /* Convert to visual. */
                vte::base::BidiRow const* bidirow = m_ringview.get_bidirow(row);
                vte::grid::column_t vcol = bidirow->log2vis(lcol);
                return m_selection_resolved.box_contains ({ row, vcol });
        } else {
                /* In normal modes, resolve_selection() made sure to generate such boundaries for m_selection_resolved. */
                return m_selection_resolved.contains ({ row, lcol });
        }
}

/* Check if a cell is selected or not. BiDi: the coordinate is visual. */
bool
Terminal::cell_is_selected_vis(vte::grid::column_t vcol,
                               vte::grid::row_t row) const
{
        // Callers need to update the ringview. However, don't assert, just
        // return out-of-view coords. FIXME: may want to throw instead
        if (!m_ringview.is_updated())
                [[unlikely]] return false;

        /* Convert to logical column. */
        vte::base::BidiRow const* bidirow = m_ringview.get_bidirow(row);
        vte::grid::column_t lcol = bidirow->vis2log(vcol);

        return cell_is_selected_log(lcol, row);
}

void
Terminal::widget_paste(std::string_view const& data)
{
        if (!m_input_enabled)
                return;

        feed_child(vte::terminal::pastify_string(data,
                                                 m_modes_private.XTERM_READLINE_BRACKETED_PASTE(),
                                                 false /* C1 */));

        if (m_scroll_on_insert) {
                scroll_to_bottom();
	}
}

bool
Terminal::feed_mouse_event(vte::grid::coords const& rowcol /* confined */,
                                     int button,
                                     bool is_drag,
                                     bool is_release)
{
        unsigned char cb;

        /* Don't send events on scrollback contents: bug 755187. */
        if (grid_coords_in_scrollback(rowcol))
                return false;

	/* Make coordinates 1-based. */
	auto cx = rowcol.column() + 1;
	auto cy = rowcol.row() - m_screen->insert_delta + 1;

	/* Encode the button information in cb. */
	switch (button) {
        case 0:                 /* No button, just dragging. */
                cb = 3;
                break;
        case 1:                 /* Left. */
        case 2:                 /* Middle. */
        case 3:                 /* Right. */
                cb = button - 1;
                break;
        case 4:                 /* Scroll up. */
        case 5:                 /* Scroll down. */
        case 6:                 /* Scroll left. */
        case 7:                 /* Scroll right. */
                cb = button - 4 + 64;
                break;
        case 8:                 /* Back. */
        case 9:                 /* Forward. */
        case 10:                /* ? */
        case 11:                /* ? */
                cb = button - 8 + 128;
                break;
        case 12:                /* ? */
        case 13:                /* ? */
        case 14:                /* ? */
        case 15:                /* ? */
                cb = button - 12 + 192;
                break;
        default:
                return false;
	}

	/* With the exception of the 1006 mode, button release is also encoded here. */
	/* Note that if multiple extensions are enabled, the 1006 is used, so it's okay to check for only that. */
	if (is_release && !m_modes_private.XTERM_MOUSE_EXT_SGR()) {
		cb = 3;
	}

	/* Encode the modifiers. */
        if (m_mouse_tracking_mode >= MouseTrackingMode::eSEND_XY_ON_BUTTON) {
                if (m_modifiers & GDK_SHIFT_MASK) {
                        cb |= 4;
                }
                if (m_modifiers & VTE_ALT_MASK) {
                        cb |= 8;
                }
                if (m_modifiers & GDK_CONTROL_MASK) {
                        cb |= 16;
                }
        }

	/* Encode a drag event. */
	if (is_drag) {
		cb |= 32;
	}

	/* Check the extensions in decreasing order of preference. Encoding the release event above assumes that 1006 comes first. */
	if (m_modes_private.XTERM_MOUSE_EXT_SGR()) {
		/* xterm's extended mode (1006) */
                send(is_release ? VTE_REPLY_XTERM_MOUSE_EXT_SGR_REPORT_BUTTON_RELEASE
                                : VTE_REPLY_XTERM_MOUSE_EXT_SGR_REPORT_BUTTON_PRESS,
                     {cb, (int)cx, (int)cy});
        } else if (cb <= 223 && cx <= 223 && cy <= 223) {
		/* legacy mode */
                char buf[8];
                size_t len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "M%c%c%c", 32 + cb, 32 + (guchar)cx, 32 + (guchar)cy);

                /* Send event direct to the child, this is binary not text data */
                feed_child_binary({buf, len});
	}

        return true;
}

void
Terminal::feed_focus_event(bool in)
{
        send(in ? VTE_REPLY_XTERM_FOCUS_IN : VTE_REPLY_XTERM_FOCUS_OUT, {});
}

void
Terminal::feed_focus_event_initial()
{
        /* We immediately send the terminal a focus event, since otherwise
         * it has no way to know the current status.
         */
        feed_focus_event(m_has_focus);
}

void
Terminal::maybe_feed_focus_event(bool in)
{
        if (m_modes_private.XTERM_FOCUS())
                feed_focus_event(in);
}

/*
 * Terminal::maybe_send_mouse_button:
 * @terminal:
 * @event:
 *
 * Sends a mouse button click or release notification to the application,
 * if the terminal is in mouse tracking mode.
 *
 * Returns: %TRUE iff the event was consumed
 */
bool
Terminal::maybe_send_mouse_button(vte::grid::coords const& unconfined_rowcol,
                                  vte::platform::MouseEvent const& event)
{
	switch (event.type()) {
        case vte::platform::EventBase::Type::eMOUSE_PRESS:
                if (m_mouse_tracking_mode < MouseTrackingMode::eSEND_XY_ON_CLICK) {
			return false;
		}
		break;
        case vte::platform::EventBase::Type::eMOUSE_RELEASE:
                if (m_mouse_tracking_mode < MouseTrackingMode::eSEND_XY_ON_BUTTON) {
			return false;
		}
		break;
	default:
		return false;
	}

        auto rowcol = confine_grid_coords(unconfined_rowcol);
        return feed_mouse_event(rowcol,
                                event.button_value(),
                                false /* not drag */,
                                event.is_mouse_release());
}

/*
 * Terminal::maybe_send_mouse_drag:
 * @terminal:
 * @event:
 *
 * Sends a mouse motion notification to the application,
 * if the terminal is in mouse tracking mode.
 *
 * Returns: %TRUE iff the event was consumed
 */
bool
Terminal::maybe_send_mouse_drag(vte::grid::coords const& unconfined_rowcol,
                                vte::platform::MouseEvent const& event)
{
        /* Need to ensure the ringview is updated. */
        ringview_update();

        auto rowcol = confine_grid_coords(unconfined_rowcol);

	/* First determine if we even want to send notification. */
        switch (event.type()) {
        case vte::platform::EventBase::Type::eMOUSE_MOTION:
		if (m_mouse_tracking_mode < MouseTrackingMode::eCELL_MOTION_TRACKING)
			return false;

		if (m_mouse_tracking_mode < MouseTrackingMode::eALL_MOTION_TRACKING) {

                        if (m_mouse_pressed_buttons == 0) {
				return false;
			}
			/* The xterm doc is not clear as to whether
			 * all-tracking also sends degenerate same-cell events;
                         * we don't.
                         */
                        if (rowcol == confined_grid_coords_from_view_coords(m_mouse_last_position))
				return false;
		}
		break;
	default:
		return false;
	}

        /* As per xterm, report the lowest pressed button - if any. */
        int button = ffs(m_mouse_pressed_buttons);

        return feed_mouse_event(rowcol,
                                button,
                                true /* drag */,
                                false /* not release */);
}

/*
 * Terminal::hyperlink_invalidate_and_get_bbox
 *
 * Invalidates cells belonging to the non-zero hyperlink idx, in order to
 * stop highlighting the previously hovered hyperlink or start highlighting
 * the new one. Optionally stores the coordinates of the bounding box.
 */
void
Terminal::hyperlink_invalidate_and_get_bbox(vte::base::Ring::hyperlink_idx_t idx,
                                                      GdkRectangle *bbox)
{
        auto first_row = first_displayed_row();
        auto end_row = last_displayed_row() + 1;
        vte::grid::row_t row, top = LONG_MAX, bottom = -1;
        vte::grid::column_t col, left = LONG_MAX, right = -1;
        const VteRowData *rowdata;

        g_assert (idx != 0);

        for (row = first_row; row < end_row; row++) {
                rowdata = m_screen->row_data->index(row);
                if (rowdata != NULL) {
                        bool do_invalidate_row = false;
                        for (col = 0; col < rowdata->len; col++) {
                                if (G_UNLIKELY (rowdata->cells[col].attr.hyperlink_idx == idx)) {
                                        do_invalidate_row = true;
                                        top = MIN(top, row);
                                        bottom = MAX(bottom, row);
                                        left = MIN(left, col);
                                        right = MAX(right, col);
                                }
                        }
                        if (G_UNLIKELY (do_invalidate_row)) {
                                invalidate_row(row);
                        }
                }
        }

        if (bbox == NULL)
                return;

        /* If bbox != NULL, we're looking for the new hovered hyperlink which always has onscreen bits. */
        g_assert (top != LONG_MAX && bottom != -1 && left != LONG_MAX && right != -1);

        auto allocation = get_allocated_rect();
        bbox->x = allocation.x + m_border.left + left * m_cell_width;
        bbox->y = allocation.y + m_border.top + row_to_pixel(top);
        bbox->width = (right - left + 1) * m_cell_width;
        bbox->height = (bottom - top + 1) * m_cell_height;
        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                          "Hyperlink bounding box: x=%d y=%d w=%d h=%d\n",
                          bbox->x, bbox->y, bbox->width, bbox->height);
}

/*
 * Terminal::hyperlink_hilite_update:
 *
 * Checks the coordinates for hyperlink. Updates m_hyperlink_hover_idx
 * and m_hyperlink_hover_uri, and schedules to update the highlighting.
 */
void
Terminal::hyperlink_hilite_update()
{
        const VteRowData *rowdata;
        bool do_check_hilite;
        vte::grid::coords rowcol;
        vte::base::Ring::hyperlink_idx_t new_hyperlink_hover_idx = 0;
        GdkRectangle bbox;
        const char *separator;

        if (!m_allow_hyperlink)
                return;

        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                         "hyperlink_hilite_update\n");

        /* Need to ensure the ringview is updated. */
        ringview_update();

        /* m_mouse_last_position contains the current position, see bug 789536 comment 24. */
        auto pos = m_mouse_last_position;

        /* Whether there's any chance we'd highlight something */
        do_check_hilite = view_coords_visible(pos) &&
                          m_mouse_cursor_over_widget &&
                          !(m_mouse_autohide && m_mouse_cursor_autohidden) &&
                          !m_selecting;
        if (do_check_hilite) {
                rowcol = grid_coords_from_view_coords(pos);
                rowdata = find_row_data(rowcol.row());
                if (rowdata && rowcol.column() < rowdata->len) {
                        new_hyperlink_hover_idx = rowdata->cells[rowcol.column()].attr.hyperlink_idx;
                }
        }

        if (new_hyperlink_hover_idx == m_hyperlink_hover_idx) {
                _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                  "hyperlink did not change\n");
                return;
        }

        /* Invalidate cells of the old hyperlink. */
        if (m_hyperlink_hover_idx != 0) {
                hyperlink_invalidate_and_get_bbox(m_hyperlink_hover_idx, NULL);
        }

        /* This might be different from new_hyperlink_hover_idx. If in the stream, that one contains
         * the pseudo idx VTE_HYPERLINK_IDX_TARGET_IN_STREAM and now a real idx is allocated.
         * Plus, the ring's internal belief of the hovered hyperlink is also updated. */
        if (do_check_hilite) {
                m_hyperlink_hover_idx = m_screen->row_data->get_hyperlink_at_position(rowcol.row(), rowcol.column(), true, &m_hyperlink_hover_uri);
        } else {
                m_hyperlink_hover_idx = 0;
                m_hyperlink_hover_uri = nullptr;
        }

        /* Invalidate cells of the new hyperlink. Get the bounding box. */
        if (m_hyperlink_hover_idx != 0) {
                /* URI is after the first semicolon */
                separator = strchr(m_hyperlink_hover_uri, ';');
                g_assert(separator != NULL);
                m_hyperlink_hover_uri = separator + 1;

                hyperlink_invalidate_and_get_bbox(m_hyperlink_hover_idx, &bbox);
                g_assert(bbox.width > 0 && bbox.height > 0);
        }
        _vte_debug_print(VTE_DEBUG_HYPERLINK,
                         "Hover idx: %d \"%s\"\n",
                         m_hyperlink_hover_idx,
                         m_hyperlink_hover_uri);

        /* Underlining hyperlinks has precedence over regex matches. So when the hovered hyperlink changes,
         * the regex match might need to become or stop being underlined. */
        if (regex_match_has_current())
                invalidate_match_span();

        apply_mouse_cursor();

        emit_hyperlink_hover_uri_changed(m_hyperlink_hover_idx != 0 ? &bbox : NULL);
}

/*
 * Terminal::match_hilite_clear:
 *
 * Reset match variables and invalidate the old match region if highlighted.
 */
void
Terminal::match_hilite_clear()
{
        if (regex_match_has_current())
                invalidate_match_span();

        m_match_span.clear();
        m_match_current = nullptr;

        g_free(m_match);
        m_match = nullptr;
}

/* This is only used by the dingu matching code, so no need to extend the area. */
void
Terminal::invalidate_match_span()
{
        _vte_debug_print(VTE_DEBUG_EVENTS,
                         "Invalidating match span %s\n", m_match_span.to_string());
        invalidate(m_match_span);
}

/*
 * Terminal::match_hilite_update:
 *
 * Checks the coordinates for dingu matches, setting m_match_span to
 * the match region or the no-matches region, and if there is a match,
 * sets it to display highlighted.
 */
void
Terminal::match_hilite_update()
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Match hilite update\n");

        /* Need to ensure the ringview is updated. */
        ringview_update();

        /* m_mouse_last_position contains the current position, see bug 789536 comment 24. */
        auto pos = m_mouse_last_position;

        glong col = pos.x / m_cell_width;
        glong row = pixel_to_row(pos.y);

        /* BiDi: convert to logical column. */
        vte::base::BidiRow const* bidirow = m_ringview.get_bidirow(confine_grid_row(row));
        col = bidirow->vis2log(col);

        /* Whether there's any chance we'd highlight something */
        bool do_check_hilite = view_coords_visible(pos) &&
                               m_mouse_cursor_over_widget &&
                               !(m_mouse_autohide && m_mouse_cursor_autohidden) &&
                               !m_selecting;
        if (!do_check_hilite) {
                if (regex_match_has_current())
                         match_hilite_clear();
                return;
        }

        if (m_match_span.contains(row, col)) {
                /* Already highlighted. */
                return;
        }

        /* Reset match variables and invalidate the old match region if highlighted */
        match_hilite_clear();

        /* Check for matches. */
	gsize start, end;
        auto new_match = match_check_internal(col,
                                              row,
                                              &m_match_current,
                                              &start,
                                              &end);

	/* Read the new locations. */
	if (start < vte_char_attr_list_get_size(&m_match_attributes) &&
            end < vte_char_attr_list_get_size(&m_match_attributes)) {
                struct _VteCharAttributes const *sa, *ea;
		sa = vte_char_attr_list_get(&m_match_attributes, start);
                ea = vte_char_attr_list_get(&m_match_attributes, end);

                /* convert from inclusive to exclusive (a.k.a. boundary) ending, taking a possible last CJK character into account */
                m_match_span = vte::grid::span(sa->row, sa->column, ea->row, ea->column + ea->columns);
	}

        g_assert(!m_match); /* from match_hilite_clear() above */
	m_match = new_match;

	if (m_match) {
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Matched %s.\n", m_match_span.to_string());
                invalidate_match_span();
        } else {
		_vte_debug_print(VTE_DEBUG_EVENTS,
                                 "No matches %s.\n", m_match_span.to_string());
	}

        apply_mouse_cursor();
}

void
Terminal::widget_clipboard_data_clear(vte::platform::Clipboard const& clipboard)
{
        if (m_changing_selection)
                return;

        switch (clipboard.type()) {
        case vte::platform::ClipboardType::PRIMARY:
		if (m_selection_owned[vte::to_integral(vte::platform::ClipboardType::PRIMARY)] &&
                    !m_selection_resolved.empty()) {
			_vte_debug_print(VTE_DEBUG_SELECTION, "Lost selection.\n");
			deselect_all();
		}

                [[fallthrough]];
        case vte::platform::ClipboardType::CLIPBOARD:
                m_selection_owned[vte::to_integral(clipboard.type())] = false;
                break;
        }
}

std::optional<std::string_view>
Terminal::widget_clipboard_data_get(vte::platform::Clipboard const& clipboard,
                                    vte::platform::ClipboardFormat format)
{
        auto const sel = vte::to_integral(clipboard.type());

        if (m_selection[sel] == nullptr)
                return std::nullopt;

        _VTE_DEBUG_IF(VTE_DEBUG_SELECTION) {
                g_printerr("Setting selection %d (%" G_GSIZE_FORMAT " UTF-8 bytes.) for target %s\n",
                           sel,
                           m_selection[sel]->len,
                           format == vte::platform::ClipboardFormat::HTML ? "HTML" : "TEXT");
                _vte_debug_hexdump("Selection data", (uint8_t const*)m_selection[sel]->str, m_selection[sel]->len);
        }

        return std::string_view{m_selection[sel]->str, m_selection[sel]->len};
}

/* Convert the internal color code (either index or RGB) into RGB. */
template <unsigned int redbits,
          unsigned int greenbits,
          unsigned int bluebits>
void
Terminal::rgb_from_index(guint index,
                                   vte::color::rgb& color) const
{
        bool dim = false;
        if (!(index & VTE_RGB_COLOR_MASK(redbits, greenbits, bluebits)) && (index & VTE_DIM_COLOR)) {
                index &= ~VTE_DIM_COLOR;
                dim = true;
        }

	if (index >= VTE_LEGACY_COLORS_OFFSET && index < VTE_LEGACY_COLORS_OFFSET + VTE_LEGACY_FULL_COLOR_SET_SIZE)
		index -= VTE_LEGACY_COLORS_OFFSET;
	if (index < VTE_PALETTE_SIZE) {
                color = *get_color(index);
                if (dim) {
                        /* magic formula taken from xterm */
                        color.red = color.red * 2 / 3;
                        color.green = color.green * 2 / 3;
                        color.blue = color.blue * 2 / 3;
                }
	} else if (index & VTE_RGB_COLOR_MASK(redbits, greenbits, bluebits)) {
                color.red   = VTE_RGB_COLOR_GET_COMPONENT(index, greenbits + bluebits, redbits) * 0x101U;
                color.green = VTE_RGB_COLOR_GET_COMPONENT(index, bluebits, greenbits) * 0x101U;
                color.blue  = VTE_RGB_COLOR_GET_COMPONENT(index, 0, bluebits) * 0x101U;
	} else {
		g_assert_not_reached();
	}
}

void
Terminal::get_text(vte::grid::row_t start_row,
                   vte::grid::column_t start_col,
                   vte::grid::row_t end_row,
                   vte::grid::column_t end_col,
                   bool block,
                   bool preserve_empty,
                   GString *string,
                   VteCharAttrList *attributes)
{
	const VteCell *pcell = NULL;
	struct _VteCharAttributes attr;
	vte::color::rgb fore, back;
        std::unique_ptr<vte::base::RingView> ringview;
        vte::base::BidiRow const *bidirow = nullptr;
        vte::grid::column_t vcol;

	if (attributes)
                vte_char_attr_list_set_size (attributes, 0);

        if (string->len > 0)
                g_string_truncate(string, 0);

	memset(&attr, 0, sizeof(attr));

        if (start_col < 0)
                start_col = 0;

        if (m_enable_bidi && block) {
                /* Rectangular selection operates on the visual contents, not the logical.
                 * m_ringview corresponds to the currently onscreen bits, therefore does not
                 * necessarily include the entire selection. Also we want m_ringview's size
                 * to be limited, even if the user selects a giant rectangle.
                 * So use a new ringview for the selection. */
                ringview = std::make_unique<vte::base::RingView>();
                ringview->set_ring(m_screen->row_data);
                ringview->set_rows(start_row, end_row - start_row + 1);
                ringview->set_width(m_column_count);
                ringview->update();
        }

        vte::grid::column_t lcol = block ? 0 : start_col;
        vte::grid::row_t row;
        for (row = start_row; row < end_row + 1; row++, lcol = 0) {
		VteRowData const* row_data = find_row_data(row);
                gsize last_empty, last_nonempty;
                vte::grid::column_t last_emptycol, last_nonemptycol;
                vte::grid::column_t line_last_column = (!block && row == end_row) ? end_col : m_column_count;

                last_empty = last_nonempty = string->len;
                last_emptycol = last_nonemptycol = -1;

		attr.row = row;
                attr.column = lcol;
		pcell = NULL;
		if (row_data != NULL) {
                        bidirow = ringview ? ringview->get_bidirow(row) : nullptr;
                        while (lcol < line_last_column &&
                               (pcell = _vte_row_data_get (row_data, lcol))) {

                                /* In block mode, we scan each row from its very beginning to its very end in logical order,
                                 * and here filter out the characters that are visually outside of the block. */
                                if (bidirow) {
                                        vcol = bidirow->log2vis(lcol);
                                        if (vcol < start_col || vcol >= end_col) {
                                                lcol++;
                                                continue;
                                        }
                                }

                                attr.column = lcol;

				/* If it's not part of a multi-column character,
				 * and passes the selection criterion, add it to
				 * the selection. */
				if (!pcell->attr.fragment()) {
					/* Store the attributes of this character. */
                                        // FIXMEchpe shouldn't this use determine_colors?
                                        uint32_t fg, bg, dc;
                                        vte_color_triple_get(pcell->attr.colors(), &fg, &bg, &dc);
                                        rgb_from_index<8, 8, 8>(fg, fore);
                                        rgb_from_index<8, 8, 8>(bg, back);
					attr.fore.red = fore.red;
					attr.fore.green = fore.green;
					attr.fore.blue = fore.blue;
					attr.back.red = back.red;
					attr.back.green = back.green;
					attr.back.blue = back.blue;
					attr.underline = (pcell->attr.underline() == 1);
					attr.strikethrough = pcell->attr.strikethrough();
                                        attr.columns = pcell->attr.columns();

					/* Store the cell string */
					if (pcell->c == 0) {
                                                /* Empty cells of nondefault background color are
                                                 * stored as NUL characters. Treat them as spaces
                                                 * unless 'preserve_empty' is set,
                                                 * but make a note of the last occurrence. */
                                                g_string_append_c (string, preserve_empty ? 0 : ' ');
                                                last_empty = string->len;
                                                last_emptycol = lcol;
					} else {
						_vte_unistr_append_to_string (pcell->c, string);
                                                last_nonempty = string->len;
                                                last_nonemptycol = lcol;
					}

					/* If we added text to the string, record its
					 * attributes, one per byte. */
					if (attributes) {
                                                vte_char_attr_list_fill(attributes, &attr, string->len);
					}
				}

                                lcol++;
			}
		}

                /* Empty cells of nondefault background color can appear anywhere in a line,
                 * not just at the end, e.g. between "foo" and "bar" here:
                 *   echo -e '\e[46mfoo\e[K\e[7Gbar\e[m'
                 * Strip off the trailing ones, preserve the middle ones. */
                if (last_empty > last_nonempty) {

                        lcol = last_emptycol + 1;

                        if (row_data != NULL) {
                                while ((pcell = _vte_row_data_get (row_data, lcol))) {
                                        lcol++;

                                        if (pcell->attr.fragment())
                                                continue;

                                        if (pcell->c != 0)
                                                break;
                                }
                        }
                        if (pcell == NULL) {
                                g_string_truncate(string, last_nonempty);
                                if (attributes)
                                        vte_char_attr_list_set_size(attributes, string->len);
                                attr.column = last_nonemptycol;
                        }
                }

		/* Adjust column, in case we want to append a newline */
                //FIXMEchpe MIN ?
		attr.column = MAX(m_column_count, attr.column + 1);

		/* Add a newline in block mode. */
		if (block) {
			g_string_append_c(string, '\n');
		}
		/* Else, if the last visible column on this line was in range and
		 * not soft-wrapped, append a newline. */
		else if (row < end_row) {
			/* If we didn't softwrap, add a newline. */
			/* XXX need to clear row->soft_wrap on deletion! */
                        if (!m_screen->row_data->is_soft_wrapped(row)) {
				g_string_append_c(string, '\n');
			}
		}

		/* Make sure that the attributes array is as long as the string. */
		if (attributes) {
                        vte_char_attr_list_fill(attributes, &attr, string->len);
		}
	}

	/* Sanity check. */
        if (attributes != nullptr)
                vte_assert_cmpuint(string->len, ==, vte_char_attr_list_get_size(attributes));
}

void
Terminal::get_text_displayed(GString *string,
                             VteCharAttrList* attributes)
{
        get_text(first_displayed_row(), 0,
                 last_displayed_row() + 1, 0,
                 false /* block */,
                 false /* preserve_empty */,
                 string,
                 attributes);
}

/* This is distinct from just using first/last_displayed_row since a11y
 * doesn't know about sub-row displays.
 */
void
Terminal::get_text_displayed_a11y(GString *string,
                                  VteCharAttrList* attributes)
{
        return get_text(m_screen->scroll_delta, 0,
                        m_screen->scroll_delta + m_row_count - 1 + 1, 0,
                        false /* block */,
                        false /* preserve_empty */,
                        string,
                        attributes);
}

void
Terminal::get_selected_text(GString *string,
                            VteCharAttrList* attributes)
{
        return get_text(m_selection_resolved.start_row(),
                        m_selection_resolved.start_column(),
                        m_selection_resolved.end_row(),
                        m_selection_resolved.end_column(),
                        m_selection_block_mode,
                        false /* preserve_empty */,
                        string,
                        attributes);
}

#if VTE_DEBUG
unsigned int
Terminal::checksum_area(vte::grid_rect rect)
{
        vte::grid::row_t const start_row = rect.top() + m_screen->insert_delta;
        vte::grid::row_t const end_row = rect.bottom() + m_screen->insert_delta;
        vte::grid::column_t const start_col = rect.left();
        vte::grid::column_t const end_col = rect.right() + 1;

        unsigned int checksum = 0;
        VteCharAttrList attributes;
        const VteCellAttr *attr;

        vte_char_attr_list_init(&attributes);
        auto text = g_string_new(nullptr);
        get_text(start_row, start_col, end_row, end_col,
                             true /* block */,
                             true /* preserve_empty */,
                             text,
                             &attributes);
        if (text == nullptr) {
                vte_char_attr_list_clear(&attributes);
                return checksum;
        }

        vte_assert_cmpuint(text->len, ==, vte_char_attr_list_get_size(&attributes));
        char const* end = (char const*)text->str + text->len;
        for (char const *p = text->str; p < end; p = g_utf8_next_char(p)) {
                auto const c = g_utf8_get_char(p);
                if (c == '\n')
                        continue;
                checksum += c;
                attr = char_to_cell_attr(vte_char_attr_list_get(&attributes, p - text->str));

                if (attr->invisible())
                        checksum += 0x08;
                if (attr->underline())
                        checksum += 0x10;
                if (attr->reverse())
                        checksum += 0x20;
                if (attr->blink())
                        checksum += 0x40;
                if (attr->bold())
                        checksum += 0x80;
        }
        vte_char_attr_list_clear(&attributes);
        g_string_free(text, true);

        checksum = -checksum;
        return checksum & 0xffff;
}
#endif /* VTE_DEBUG */

/*
 * Compares the visual attributes of a VteCellAttr for equality, but ignores
 * attributes that tend to change from character to character or are otherwise
 * strange (in particular: fragment, columns).
 */
// FIXMEchpe: make VteCellAttr a class with operator==
static bool
vte_terminal_cellattr_equal(VteCellAttr const* attr1,
                            VteCellAttr const* attr2)
{
        //FIXMEchpe why exclude DIM here?
	return (((attr1->attr ^ attr2->attr) & VTE_ATTR_ALL_MASK) == 0 &&
                attr1->colors()       == attr2->colors()   &&
                attr1->hyperlink_idx  == attr2->hyperlink_idx);
}

/*
 * Wraps a given string according to the VteCellAttr in HTML tags. Used
 * old-style HTML (and not CSS) for better compatibility with, for example,
 * evolution's mail editor component.
 */
char *
Terminal::cellattr_to_html(VteCellAttr const* attr,
                                     char const* text) const
{
	GString *string;
        guint fore, back, deco;

	string = g_string_new(text);

        determine_colors(attr, false, false, &fore, &back, &deco);

	if (attr->bold()) {
		g_string_prepend(string, "<b>");
		g_string_append(string, "</b>");
	}
	if (attr->italic()) {
		g_string_prepend(string, "<i>");
		g_string_append(string, "</i>");
	}
        /* <u> should be inside <font> so that it inherits its color by default */
        if (attr->underline() != 0) {
                static const char styles[][7] = {"", "solid", "double", "wavy", "dotted", "dashed"};
                char *tag, *colorattr;

                if (deco != VTE_DEFAULT_FG) {
                        vte::color::rgb color;

                        rgb_from_index<4, 5, 4>(deco, color);
                        colorattr = g_strdup_printf(";text-decoration-color:#%02X%02X%02X",
                                                    color.red >> 8,
                                                    color.green >> 8,
                                                    color.blue >> 8);
                } else {
                        colorattr = g_strdup("");
                }

                tag = g_strdup_printf("<u style=\"text-decoration-style:%s%s\">",
                                      styles[attr->underline()],
                                      colorattr);
                g_string_prepend(string, tag);
                g_free(tag);
                g_free(colorattr);
                g_string_append(string, "</u>");
        }
	if (fore != VTE_DEFAULT_FG || attr->reverse()) {
		vte::color::rgb color;
                char *tag;

                rgb_from_index<8, 8, 8>(fore, color);
		tag = g_strdup_printf("<font color=\"#%02X%02X%02X\">",
                                      color.red >> 8,
                                      color.green >> 8,
                                      color.blue >> 8);
		g_string_prepend(string, tag);
		g_free(tag);
		g_string_append(string, "</font>");
	}
	if (back != VTE_DEFAULT_BG || attr->reverse()) {
		vte::color::rgb color;
                char *tag;

                rgb_from_index<8, 8, 8>(back, color);
		tag = g_strdup_printf("<span style=\"background-color:#%02X%02X%02X\">",
                                      color.red >> 8,
                                      color.green >> 8,
                                      color.blue >> 8);
		g_string_prepend(string, tag);
		g_free(tag);
		g_string_append(string, "</span>");
	}
	if (attr->strikethrough()) {
		g_string_prepend(string, "<strike>");
		g_string_append(string, "</strike>");
	}
	if (attr->overline()) {
		g_string_prepend(string, "<span style=\"text-decoration-line:overline\">");
		g_string_append(string, "</span>");
	}
	if (attr->blink()) {
		g_string_prepend(string, "<blink>");
		g_string_append(string, "</blink>");
	}
	/* reverse and invisible are not supported */

	return g_string_free(string, FALSE);
}

/*
 * Similar to find_charcell(), but takes a VteCharAttribute for
 * indexing and returns the VteCellAttr.
 */
VteCellAttr const*
Terminal::char_to_cell_attr(VteCharAttributes const* attr) const
{
	VteCell const* cell = find_charcell(attr->column, attr->row);
	if (cell)
		return &cell->attr;
	return nullptr;
}

/*
 * Terminal::attributes_to_html:
 * @text: A string as returned by the vte_terminal_get_* family of functions.
 * @attrs: (array) (element-type Vte.CharAttributes): text attributes, as created by vte_terminal_get_*
 *
 * Marks the given text up according to the given attributes, using HTML <span>
 * commands, and wraps the string in a <pre> element. The attributes have to be
 * "fresh" in the sense that the terminal must not have changed since they were
 * obtained using the vte_terminal_get* function.
 *
 * Returns: (transfer full): a newly allocated text string, or %NULL.
 */
GString*
Terminal::attributes_to_html(GString* text_string,
                             VteCharAttrList* attrs)
{
	GString *string;
	guint from,to;
	const VteCellAttr *attr;
	char *escaped, *marked;

        char const* text = text_string->str;
        auto len = text_string->len;
        vte_assert_cmpuint(len, ==, vte_char_attr_list_get_size(attrs));

	/* Initial size fits perfectly if the text has no attributes and no
	 * characters that need to be escaped
         */
	string = g_string_sized_new (len + 11);

	g_string_append(string, "<pre>");
	/* Find streches with equal attributes. Newlines are treated specially,
	 * so that the <span> do not cover multiple lines.
         */
	from = to = 0;
	while (text[from] != '\0') {
		g_assert(from == to);
		if (text[from] == '\n') {
			g_string_append_c(string, '\n');
			from = ++to;
		} else {
			attr = char_to_cell_attr(vte_char_attr_list_get(attrs, from));
			while (text[to] != '\0' && text[to] != '\n' &&
			       vte_terminal_cellattr_equal(attr,
                                                           char_to_cell_attr(vte_char_attr_list_get(attrs, to))))
			{
				to++;
			}
			escaped = g_markup_escape_text(text + from, to - from);
			marked = cellattr_to_html(attr, escaped);
			g_string_append(string, marked);
			g_free(escaped);
			g_free(marked);
			from = to;
		}
	}
	g_string_append(string, "</pre>");

	return string;
}

/* Place the selected text onto the clipboard.  Do this asynchronously so that
 * we get notified when the selection we placed on the clipboard is replaced. */
void
Terminal::widget_copy(vte::platform::ClipboardType type,
                      vte::platform::ClipboardFormat format)
{
        /* Only put HTML on the CLIPBOARD, not PRIMARY */
        assert(type == vte::platform::ClipboardType::CLIPBOARD ||
               format == vte::platform::ClipboardFormat::TEXT);

	/* Chuck old selected text and retrieve the newly-selected text. */
        VteCharAttrList attributes;
        vte_char_attr_list_init(&attributes);
        GString *selection = g_string_new(nullptr);
        get_selected_text(selection, &attributes);

        auto const sel = vte::to_integral(type);
        if (m_selection[sel]) {
                g_string_free(m_selection[sel], TRUE);
                m_selection[sel] = nullptr;
        }

        if (selection == nullptr) {
                vte_char_attr_list_clear(&attributes);
                m_selection_owned[sel] = false;
                return;
        }

        if (format == vte::platform::ClipboardFormat::HTML) {
                m_selection[sel] = attributes_to_html(selection, &attributes);
                g_string_free(selection, TRUE);
        } else {
                m_selection[sel] = selection;
        }

        vte_char_attr_list_clear(&attributes);

	/* Place the text on the clipboard. */
        _vte_debug_print(VTE_DEBUG_SELECTION,
                         "Assuming ownership of selection.\n");

        m_selection_owned[sel] = true;
        m_selection_format[sel] = format;

        m_changing_selection = true;
        widget()->clipboard_offer_data(type, format);
        m_changing_selection = false;
}

/* Confine coordinates into the visible area. Padding is already subtracted. */
void
Terminal::confine_coordinates(long *xp,
                                        long *yp)
{
	long x = *xp;
	long y = *yp;
        long y_stop;

        /* Allow to use the bottom extra padding only if there's content there. */
        y_stop = MIN(m_view_usable_extents.height(),
                     row_to_pixel(m_screen->insert_delta + m_row_count));

	if (y < 0) {
		y = 0;
		if (!m_selection_block_mode)
			x = 0;
        } else if (y >= y_stop) {
                y = y_stop - 1;
		if (!m_selection_block_mode)
			x = m_column_count * m_cell_width - 1;
	}
	if (x < 0) {
		x = 0;
	} else if (x >= m_column_count * m_cell_width) {
		x = m_column_count * m_cell_width - 1;
	}

	*xp = x;
	*yp = y;
}

/* Start selection at the location of the event.
 * In case of regular selection, this is called with the original click's location
 * once the mouse has moved by the gtk drag threshold.
 */
void
Terminal::start_selection (vte::view::coords const& pos,
                           SelectionType type)
{
	if (m_selection_block_mode)
		type = SelectionType::eCHAR;

        /* Need to ensure the ringview is updated. */
        ringview_update();

        m_selection_origin = m_selection_last = selection_grid_halfcoords_from_view_coords(pos);

	/* Record the selection type. */
	m_selection_type = type;
	m_selecting = TRUE;
        m_selecting_had_delta = false;  /* resolve_selection() below will most likely flip it to true. */
        m_will_select_after_threshold = false;

	_vte_debug_print(VTE_DEBUG_SELECTION,
                         "Selection started at %s.\n",
                         m_selection_origin.to_string());

        /* Take care of updating the display. */
        resolve_selection();

	/* Temporarily stop caring about input from the child. */
	disconnect_pty_read();
}

bool
Terminal::maybe_end_selection()
{
	if (m_selecting) {
		/* Copy only if something was selected. */
                if (!m_selection_resolved.empty() &&
		    m_selecting_had_delta) {
                        widget_copy(vte::platform::ClipboardType::PRIMARY,
                                    vte::platform::ClipboardFormat::TEXT);
			emit_selection_changed();
		}
                stop_autoscroll();  /* Required before setting m_selecting to false, see #105. */
		m_selecting = false;

		/* Reconnect to input from the child if we paused it. */
		connect_pty_read();

		return true;
	}

        if (m_will_select_after_threshold)
                return true;

        return false;
}

/*
 * Terminal::select_all:
 *
 * Selects all text within the terminal (including the scrollback buffer).
 */
void
Terminal::select_all()
{
	deselect_all();

	m_selecting_had_delta = TRUE;

        m_selection_resolved.set ({ (vte::grid::row_t)m_screen->row_data->delta(), 0 },
                                  { (vte::grid::row_t)m_screen->row_data->next(),  0 });

	_vte_debug_print(VTE_DEBUG_SELECTION, "Selecting *all* text.\n");

        widget_copy(vte::platform::ClipboardType::PRIMARY,
                    vte::platform::ClipboardFormat::TEXT);
	emit_selection_changed();

	invalidate_all();
}

bool
Terminal::mouse_autoscroll_timer_callback()
{
	bool extend = false;
	long x, y, xmax, ymax;
	glong adj;

        auto again = bool{true};

	/* Provide an immediate effect for mouse wigglers. */
	if (m_mouse_last_position.y < 0) {
                /* Try to scroll up by one line. */
                adj = m_screen->scroll_delta - 1;
                queue_adjustment_value_changed_clamped(adj);
                extend = true;

		_vte_debug_print(VTE_DEBUG_EVENTS, "Autoscrolling down.\n");
	}
	if (m_mouse_last_position.y >= m_view_usable_extents.height()) {
                /* Try to scroll up by one line. */
                adj = m_screen->scroll_delta + 1;
                queue_adjustment_value_changed_clamped(adj);
                extend = true;

		_vte_debug_print(VTE_DEBUG_EVENTS, "Autoscrolling up.\n");
	}
	if (extend) {
                // FIXMEchpe use confine_view_coords here
		/* Don't select off-screen areas.  That just confuses people. */
		xmax = m_column_count * m_cell_width;
		ymax = m_row_count * m_cell_height;

		x = CLAMP(m_mouse_last_position.x, 0, xmax);
		y = CLAMP(m_mouse_last_position.y, 0, ymax);
		/* If we clamped the Y, mess with the X to get the entire
		 * lines. */
		if (m_mouse_last_position.y < 0 && !m_selection_block_mode) {
			x = 0;
		}
		if (m_mouse_last_position.y >= ymax && !m_selection_block_mode) {
			x = m_column_count * m_cell_width;
		}
		/* Extend selection to cover the newly-scrolled area. */
                modify_selection(vte::view::coords(x, y));
	} else {
		/* Stop autoscrolling. */
                again = false;
	}
	return again;
}

/* Start autoscroll. */
void
Terminal::start_autoscroll()
{
	if (m_mouse_autoscroll_timer)
                return;

        m_mouse_autoscroll_timer.schedule(666 / m_row_count, // FIXME WTF?
                                          vte::glib::Timer::Priority::eLOW);
}

bool
Terminal::widget_mouse_motion(vte::platform::MouseEvent const& event)
{
        /* Need to ensure the ringview is updated. */
        ringview_update();

        auto pos = view_coords_from_event(event);
        auto rowcol = grid_coords_from_view_coords(pos);

	_vte_debug_print(VTE_DEBUG_EVENTS,
                         "Motion grid %s\n", rowcol.to_string());

        m_modifiers = event.modifiers();

        if (m_will_select_after_threshold) {
                if (!gtk_drag_check_threshold(m_widget,
                                              m_mouse_last_position.x,
                                              m_mouse_last_position.y,
                                              pos.x, pos.y))
                        return true;

                start_selection(vte::view::coords(m_mouse_last_position.x, m_mouse_last_position.y),
                                SelectionType::eCHAR);
        }

        auto handled = bool{false};
        if (m_selecting &&
            (m_mouse_handled_buttons & 1) != 0) {
                _vte_debug_print(VTE_DEBUG_EVENTS, "Mousing drag 1.\n");
                modify_selection(pos);

                /* Start scrolling if we need to. */
                if (pos.y < 0 || pos.y >= m_view_usable_extents.height()) {
                        /* Give mouse wigglers something. */
                        stop_autoscroll();
                        mouse_autoscroll_timer_callback();
                        start_autoscroll();
                }

                handled = true;
        }

        if (!handled && m_input_enabled)
                maybe_send_mouse_drag(rowcol, event);

        if (pos != m_mouse_last_position) {
                m_mouse_last_position = pos;

                set_pointer_autohidden(false);
                hyperlink_hilite_update();
                match_hilite_update();
        }

	return handled;
}

bool
Terminal::widget_mouse_press(vte::platform::MouseEvent const& event)
{
	bool handled = false;
	gboolean start_selecting = FALSE, extend_selecting = FALSE;

        /* Need to ensure the ringview is updated. */
        ringview_update();

        /* Reset IM (like GtkTextView does) here */
        if (event.press_count() == 1)
                widget()->im_reset();

        auto pos = view_coords_from_event(event);
        auto rowcol = grid_coords_from_view_coords(pos);

        _vte_debug_print(VTE_DEBUG_EVENTS,
                         "Click gesture pressed button=%d at grid %s\n",
                         event.button_value(),
                         rowcol.to_string());

        m_modifiers = event.modifiers();

        switch (event.press_count()) {
        case 1: /* single click */
		/* Handle this event ourselves. */
                switch (event.button()) {
                case vte::platform::MouseEvent::Button::eLEFT:
			_vte_debug_print(VTE_DEBUG_EVENTS,
					"Handling click ourselves.\n");
			/* Grab focus. */
			if (!m_has_focus)
                                widget()->grab_focus();

                        // FIXMEchpe FIXMEgtk do im_reset() here

			/* If we're in event mode, and the user held down the
			 * shift key, we start selecting. */
			if (m_mouse_tracking_mode != MouseTrackingMode::eNONE) {
				if (m_modifiers & GDK_SHIFT_MASK) {
					start_selecting = TRUE;
				}
			} else {
				/* If the user hit shift, then extend the
				 * selection instead. */
				if ((m_modifiers & GDK_SHIFT_MASK) &&
                                    !m_selection_resolved.empty()) {
					extend_selecting = TRUE;
				} else {
					start_selecting = TRUE;
				}
			}
			if (start_selecting) {
				deselect_all();
                                m_will_select_after_threshold = true;
                                m_selection_block_mode = !!(m_modifiers & GDK_CONTROL_MASK);
				handled = true;
			}
			if (extend_selecting) {
				/* The whole selection code needs to be
				 * rewritten.  For now, put this here to
				 * fix bug 614658 */
				m_selecting = TRUE;
                                selection_maybe_swap_endpoints (pos);
                                modify_selection(pos);
				handled = true;
			}
			break;
		/* Paste if the user pressed shift or we're not sending events
		 * to the app. */
                case vte::platform::MouseEvent::Button::eMIDDLE:
			if ((m_modifiers & GDK_SHIFT_MASK) ||
			    m_mouse_tracking_mode == MouseTrackingMode::eNONE) {
                                if (widget()->primary_paste_enabled()) {
                                        widget()->clipboard_request_text(vte::platform::ClipboardType::PRIMARY);
                                        handled = true;
                                }
			}
			break;
                case vte::platform::MouseEvent::Button::eRIGHT:
                        // If we get a Shift+Right-Clickt, don't send the
                        // event to the app but instead first try to popup
                        // the context menu
                        if ((m_modifiers & (GDK_SHIFT_MASK |
                                            GDK_CONTROL_MASK |
#if VTE_GTK == 3
                                            GDK_MOD1_MASK |
#elif VTE_GTK == 4
                                            GDK_ALT_MASK |
#endif
                                            GDK_SUPER_MASK |
                                            GDK_HYPER_MASK |
                                            GDK_META_MASK)) == GDK_SHIFT_MASK) {
                                _vte_debug_print(VTE_DEBUG_EVENTS, "Showing context menu\n");
                                handled = widget()->show_context_menu(vte::platform::EventContext{event});
                        }
                        break;
		default:
			break;
		}
                if (event.button_value() >= 1 && event.button_value() <= 15) {
                        if (handled)
                                m_mouse_handled_buttons |= (1 << (event.button_value() - 1));
                        else
                                m_mouse_handled_buttons &= ~(1 << (event.button_value() - 1));
                }
		/* If we haven't done anything yet, try sending the mouse
		 * event to the app. */
		if (handled == FALSE) {
                        handled = maybe_send_mouse_button(rowcol, event);
		}
		break;
        case 2: /* double click */
                switch (event.button()) {
                case vte::platform::MouseEvent::Button::eLEFT:
                        if (m_will_select_after_threshold) {
                                start_selection(pos,
                                                SelectionType::eCHAR);
				handled = true;
			}
                        if ((m_mouse_handled_buttons & 1) != 0) {
                                start_selection(pos,
                                                SelectionType::eWORD);
				handled = true;
			}
			break;
                case vte::platform::MouseEvent::Button::eMIDDLE:
                case vte::platform::MouseEvent::Button::eRIGHT:
		default:
			break;
		}
#if VTE_GTK == 4
                /* If we haven't done anything yet, try sending the mouse
                 * event to the app. */
                if (handled == FALSE) {
                        handled = maybe_send_mouse_button(rowcol, event);
                }
#endif
		break;
        case 3: /* triple click */
                switch (event.button()) {
                case vte::platform::MouseEvent::Button::eLEFT:
                        if ((m_mouse_handled_buttons & 1) != 0) {
                                start_selection(pos,
                                                SelectionType::eLINE);
				handled = true;
			}
			break;
                case vte::platform::MouseEvent::Button::eMIDDLE:
                case vte::platform::MouseEvent::Button::eRIGHT:
		default:
			break;
		}
#if VTE_GTK == 4
                /* If we haven't done anything yet, try sending the mouse
                 * event to the app. */
                if (handled == FALSE) {
                        handled = maybe_send_mouse_button(rowcol, event);
                }
#endif
	default:
		break;
	}

        // If we haven't handled the event yet, and it's a right button,
        // with no modifiers, try showing the context menu.
        if (!handled &&
            ((event.button() == vte::platform::MouseEvent::Button::eRIGHT) &&
             ((m_modifiers & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK)) == 0) &&
             (m_modifiers & (GDK_SHIFT_MASK |
                             GDK_CONTROL_MASK |
#if VTE_GTK == 3
                             GDK_MOD1_MASK |
#elif VTE_GTK == 4
                             GDK_ALT_MASK |
#endif
                             GDK_SUPER_MASK |
                             GDK_HYPER_MASK |
                             GDK_META_MASK)) == 0)) {
                _vte_debug_print(VTE_DEBUG_EVENTS, "Showing context menu\n");
                handled = widget()->show_context_menu(vte::platform::EventContext{event});
        }

	/* Save the pointer state for later use. */
        if (event.button_value() >= 1 && event.button_value() <= 15)
                m_mouse_pressed_buttons |= (1 << (event.button_value() - 1));

	m_mouse_last_position = pos;

        set_pointer_autohidden(false);
        hyperlink_hilite_update();
        match_hilite_update();

	return handled;
}

bool
Terminal::widget_mouse_release(vte::platform::MouseEvent const& event)
{
	bool handled = false;

        /* Need to ensure the ringview is updated. */
        ringview_update();

        auto pos = view_coords_from_event(event);
        auto rowcol = grid_coords_from_view_coords(pos);

        _vte_debug_print(VTE_DEBUG_EVENTS,
                         "Click gesture released button=%d at grid %s\n",
                         event.button_value(), rowcol.to_string());

	stop_autoscroll();

        m_modifiers = event.modifiers();

        switch (event.type()) {
        case vte::platform::EventBase::Type::eMOUSE_RELEASE:
                switch (event.button()) {
                case vte::platform::MouseEvent::Button::eLEFT:
                        if (!m_selecting)
                                m_real_widget->im_activate_osk();
                        if ((m_mouse_handled_buttons & 1) != 0)
                                handled = maybe_end_selection();
			break;
                case vte::platform::MouseEvent::Button::eMIDDLE:
                        handled = (m_mouse_handled_buttons & 2) != 0;
                        m_mouse_handled_buttons &= ~2;
			break;
                case vte::platform::MouseEvent::Button::eRIGHT:
		default:
			break;
		}
		if (!handled && m_input_enabled) {
                        handled = maybe_send_mouse_button(rowcol, event);
		}
		break;
	default:
		break;
	}

	/* Save the pointer state for later use. */
        if (event.button_value() >= 1 && event.button_value() <= 15)
                m_mouse_pressed_buttons &= ~(1 << (event.button_value() - 1));

	m_mouse_last_position = pos;
        m_will_select_after_threshold = false;

        set_pointer_autohidden(false);
        hyperlink_hilite_update();
        match_hilite_update();

	return handled;
}

void
Terminal::widget_focus_in()
{
        m_has_focus = true;

#if VTE_GTK == 3
        widget()->grab_focus();
#endif

	/* We only have an IM context when we're realized, and there's not much
	 * point to painting the cursor if we don't have a window. */
	if (widget_realized()) {
		m_cursor_blink_state = TRUE;

                /* If blinking gets enabled now, do a full repaint.
                 * If blinking gets disabled, only repaint if there's blinking stuff present
                 * (we could further optimize by checking its current phase). */
                if (m_text_blink_mode == TextBlinkMode::eFOCUSED ||
                    (m_text_blink_mode == TextBlinkMode::eUNFOCUSED && m_text_blink_timer)) {
                        invalidate_all();
                }

		check_cursor_blink();

                m_real_widget->im_focus_in();
		invalidate_cursor_once();
                maybe_feed_focus_event(true);
	}
}

void
Terminal::widget_focus_out()
{
	/* We only have an IM context when we're realized, and there's not much
	 * point to painting ourselves if we don't have a window. */
	if (widget_realized()) {
                maybe_feed_focus_event(false);

		maybe_end_selection();

                /* If blinking gets enabled now, do a full repaint.
                 * If blinking gets disabled, only repaint if there's blinking stuff present
                 * (we could further optimize by checking its current phase). */
                if (m_text_blink_mode == TextBlinkMode::eUNFOCUSED ||
                    (m_text_blink_mode == TextBlinkMode::eFOCUSED && m_text_blink_timer)) {
                        invalidate_all();
                }

                m_real_widget->im_focus_out();
		invalidate_cursor_once();

                m_mouse_pressed_buttons = 0;
                m_mouse_handled_buttons = 0;
	}

	m_has_focus = false;
	check_cursor_blink();
}

void
Terminal::widget_mouse_enter(vte::platform::MouseEvent const& event)
{
        auto pos = view_coords_from_event(event);

        // FIXMEchpe read event modifiers here
        // FIXMEgtk4 or maybe not since there is no event to read them from on gtk4

	_vte_debug_print(VTE_DEBUG_EVENTS, "Motion enter at grid %s\n", pos.to_string());

        m_mouse_cursor_over_widget = TRUE;
        m_mouse_last_position = pos;

        set_pointer_autohidden(false);
        hyperlink_hilite_update();
        match_hilite_update();
        apply_mouse_cursor();
}

void
Terminal::widget_mouse_leave(vte::platform::MouseEvent const& event)
{
#if VTE_GTK == 3
        auto pos = view_coords_from_event(event);

        // FIXMEchpe read event modifiers here
        // FIXMEgtk4 or maybe not since there is no event to read them from on gtk4

	_vte_debug_print(VTE_DEBUG_EVENTS, "Motion leave at grid %s\n", pos.to_string());

        m_mouse_cursor_over_widget = FALSE;
        m_mouse_last_position = pos;
#elif VTE_GTK == 4
        // FIXMEgtk4 !!!
        m_mouse_cursor_over_widget = false;
        // keep m_mouse_last_position since the event here has no position
#endif

        // FIXMEchpe: also set m_mouse_scroll_delta to 0 here?

        hyperlink_hilite_update();
        match_hilite_update();
        apply_mouse_cursor();
}

/* Apply the changed metrics, and queue a resize if need be.
 *
 * The cell's height consists of 4 parts, from top to bottom:
 * - char_spacing.top: half of the extra line spacing,
 * - char_ascent: the font's ascent,
 * - char_descent: the font's descent,
 * - char_spacing.bottom: the other half of the extra line spacing.
 * Extra line spacing is typically 0, beef up cell_height_scale to get actual pixels
 * here. Similarly, increase cell_width_scale to get nonzero char_spacing.{left,right}.
 */
void
Terminal::apply_font_metrics(int cell_width_unscaled,
                                       int cell_height_unscaled,
                                       int cell_width,
                                       int cell_height,
                                       int char_ascent,
                                       int char_descent,
                                       GtkBorder char_spacing)
{
        int char_height;
	bool resize = false, cresize = false;

	/* Sanity check for broken font changes. */
        cell_width_unscaled = MAX(cell_width_unscaled, 1);
        cell_height_unscaled = MAX(cell_height_unscaled, 2);
        cell_width = MAX(cell_width, 1);
        cell_height = MAX(cell_height, 2);
        char_ascent = MAX(char_ascent, 1);
        char_descent = MAX(char_descent, 1);

        /* For convenience only. */
        char_height = char_ascent + char_descent;

	/* Change settings, and keep track of when we've changed anything. */
        if (cell_width_unscaled != m_cell_width_unscaled) {
                cresize = true;
                m_cell_width_unscaled = cell_width_unscaled;
	}
        if (cell_height_unscaled != m_cell_height_unscaled) {
                cresize = true;
                m_cell_height_unscaled = cell_height_unscaled;
	}
        if (cell_width != m_cell_width) {
		resize = cresize = true;
                m_cell_width = cell_width;
	}
        if (cell_height != m_cell_height) {
		resize = cresize = true;
                m_cell_height = cell_height;
	}
        if (char_ascent != m_char_ascent) {
		resize = true;
                m_char_ascent = char_ascent;
	}
        if (char_descent != m_char_descent) {
		resize = true;
                m_char_descent = char_descent;
	}
        if (memcmp(&char_spacing, &m_char_padding, sizeof(GtkBorder)) != 0) {
                resize = true;
                m_char_padding = char_spacing;
        }
        m_line_thickness = MAX (MIN (char_descent / 2, char_height / 14), 1);
        /* FIXME take these from pango_font_metrics_get_{underline,strikethrough}_{position,thickness} */
        m_underline_thickness = m_line_thickness;
        m_underline_position = MIN (char_spacing.top + char_ascent + m_line_thickness, cell_height - m_underline_thickness);
        m_double_underline_thickness = m_line_thickness;
        /* FIXME make sure this doesn't reach the baseline (switch to thinner lines, or one thicker line in that case) */
        m_double_underline_position = MIN (char_spacing.top + char_ascent + m_line_thickness, cell_height - 3 * m_double_underline_thickness);
        m_undercurl_thickness = m_line_thickness;
        m_undercurl_position = MIN (char_spacing.top + char_ascent + m_line_thickness, cell_height - _vte_draw_get_undercurl_height(cell_width, m_undercurl_thickness));
        m_strikethrough_thickness = m_line_thickness;
        m_strikethrough_position = char_spacing.top + char_ascent - char_height / 4;
        m_overline_thickness = m_line_thickness;
        m_overline_position = char_spacing.top;  /* FIXME */
        m_regex_underline_thickness = 1;  /* FIXME */
        m_regex_underline_position = char_spacing.top + char_height - m_regex_underline_thickness;  /* FIXME */

	/* Queue a resize if anything's changed. */
	if (resize) {
		if (widget_realized()) {
#if VTE_GTK == 3
			gtk_widget_queue_resize_no_redraw(m_widget);
#elif VTE_GTK == 4
                        gtk_widget_queue_resize(m_widget); // FIXMEgtk4?
#endif
		}
	}
	/* Emit a signal that the font changed. */
	if (cresize) {
                if (pty()) {
                        /* Update pixel size of PTY. */
                        pty()->set_size(m_row_count,
                                        m_column_count,
                                        m_cell_height_unscaled,
                                        m_cell_width_unscaled);
                }

                if (widget())
                        widget()->notify_char_size_changed(m_cell_width, m_cell_height);
	}
	/* Repaint. */
	invalidate_all();
}

void
Terminal::ensure_font()
{
	{
		/* Load default fonts, if no fonts have been loaded. */
		if (!m_has_fonts) {
                        update_font_desc();
		}
		if (m_fontdirty) {
                        int cell_width_unscaled, cell_height_unscaled;
                        int cell_width, cell_height;
                        int char_ascent, char_descent;
                        GtkBorder char_spacing;
			m_fontdirty = false;

                        if (!_vte_double_equal(m_font_scale, 1.)) {
                                m_draw.set_text_font(
                                                     m_widget,
                                                     m_unscaled_font_desc.get(),
                                                     m_font_options.get(),
                                                     m_cell_width_scale,
                                                     m_cell_height_scale);
                                m_draw.get_text_metrics(
                                                        &cell_width_unscaled, &cell_height_unscaled,
                                                        nullptr, nullptr, nullptr);
                        }

			m_draw.set_text_font(
                                                 m_widget,
                                                 m_fontdesc.get(),
                                                 m_font_options.get(),
                                                 m_cell_width_scale,
                                                 m_cell_height_scale);
			m_draw.get_text_metrics(
                                                    &cell_width, &cell_height,
                                                    &char_ascent, &char_descent,
                                                    &char_spacing);

                        if (_vte_double_equal(m_font_scale, 1.)) {
                                cell_width_unscaled = cell_width;
                                cell_height_unscaled = cell_height;
                        }

                        apply_font_metrics(cell_width_unscaled, cell_height_unscaled,
                                           cell_width, cell_height,
                                           char_ascent, char_descent,
                                           char_spacing);
		}
	}
}

void
Terminal::update_font()
{
        /* We'll get called again later */
        if (!m_unscaled_font_desc)
                return;

        auto desc = vte::take_freeable(pango_font_description_copy(m_unscaled_font_desc.get()));

        double size = pango_font_description_get_size(desc.get());
        if (pango_font_description_get_size_is_absolute(desc.get())) {
                pango_font_description_set_absolute_size(desc.get(), m_font_scale * size);
        } else {
                pango_font_description_set_size(desc.get(), m_font_scale * size);
        }

        m_fontdesc = std::move(desc);
        m_fontdirty = true;
        m_has_fonts = true;

        /* Set the drawing font. */
        if (widget_realized()) {
                ensure_font();
        }
}

/*
 * Terminal::set_font_desc:
 * @font_desc: (allow-none): a #PangoFontDescription for the desired font, or %nullptr
 *
 * Sets the font used for rendering all text displayed by the terminal,
 * overriding any fonts set using gtk_widget_modify_font().  The terminal
 * will immediately attempt to load the desired font, retrieve its
 * metrics, and attempt to resize itself to keep the same number of rows
 * and columns.  The font scale is applied to the specified font.
 */
bool
Terminal::set_font_desc(vte::Freeable<PangoFontDescription> font_desc)
{
        m_api_font_desc = std::move(font_desc);
        return update_font_desc();
}

bool
Terminal::update_font_desc()
{
#if VTE_GTK == 3
        auto desc = vte::Freeable<PangoFontDescription>{};

        auto context = gtk_widget_get_style_context(m_widget);
        gtk_style_context_save(context);
        gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
        gtk_style_context_get(context, GTK_STATE_FLAG_NORMAL, "font",
                              &vte::get_freeable(desc),
                              nullptr);
        gtk_style_context_restore(context);
#elif VTE_GTK == 4
        // FIXMEgtk4
        // This is how gtktextview does it, but the APIs are private... thanks, gtk4!
        // desc = vte::take_freeable
        //          (gtk_css_style_get_pango_font(gtk_style_context_lookup_style(context));

        auto context = gtk_widget_get_pango_context(m_widget);
        auto context_desc = pango_context_get_font_description(context);
        auto desc = vte::take_freeable(pango_font_description_copy(context_desc));
#endif /* VTE_GTK */

	pango_font_description_set_family_static(desc.get(), "monospace");

	if (m_api_font_desc) {
		pango_font_description_merge(desc.get(), m_api_font_desc.get(), true);
		_VTE_DEBUG_IF(VTE_DEBUG_MISC) {
			if (desc) {
				char *tmp;
				tmp = pango_font_description_to_string(desc.get());
				g_printerr("Using pango font \"%s\".\n", tmp);
				g_free (tmp);
			}
		}
	} else {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Using default monospace font.\n");
	}

        /* Sanitise the font description.
         *
         * Gravity makes no sense in vte.
         * Style needs to be default, and weight needs to allow bolding,
         * since those are set via SGR attributes.
         *
         * Allowing weight <= medium does not absolutely guarantee that bold
         * actually is bolder than the specified weight, but should be good enough
         * until we can check that the actually loaded bold font is weightier than
         * the normal font (FIXME!).
         *
         * As a special exception, allow any weight if bold-is-bright is enabled.
         */
        pango_font_description_unset_fields(desc.get(),
                                            PangoFontMask(PANGO_FONT_MASK_GRAVITY |
                                                          PANGO_FONT_MASK_STYLE));

        auto const max_weight = 1000 - VTE_FONT_WEIGHT_BOLDENING;
        if ((pango_font_description_get_set_fields(desc.get()) & PANGO_FONT_MASK_WEIGHT) &&
            (pango_font_description_get_weight(desc.get()) > max_weight) &&
            !m_bold_is_bright) {
                    pango_font_description_set_weight(desc.get(), PangoWeight(max_weight));
        }

        bool const same_desc = m_unscaled_font_desc &&
                pango_font_description_equal(m_unscaled_font_desc.get(), desc.get());

	/* Note that we proceed to recreating the font even if the description
	 * are the same.  This is because maybe screen
	 * font options were changed, or new fonts installed.  Those will be
	 * detected at font creation time and respected.
	 */

        m_unscaled_font_desc = std::move(desc);
        update_font();

        return !same_desc;
}

bool
Terminal::set_font_scale(gdouble scale)
{
        /* FIXME: compare old and new scale in pixel space */
        if (_vte_double_equal(scale, m_font_scale))
                return false;

        m_font_scale = scale;
        update_font();

        return true;
}

bool
Terminal::set_font_options(vte::Freeable<cairo_font_options_t> font_options)
{
        if ((m_font_options &&
             font_options &&
             cairo_font_options_equal(m_font_options.get(), font_options.get())) ||
            (!m_font_options &&
             !font_options))
                return false;

        m_font_options = std::move(font_options);
        update_font();

        return true;
}

bool
Terminal::set_cell_width_scale(double scale)
{
        /* FIXME: compare old and new scale in pixel space */
        if (_vte_double_equal(scale, m_cell_width_scale))
                return false;

        m_cell_width_scale = scale;
        /* Set the drawing font. */
        m_fontdirty = true;
        if (widget_realized()) {
                ensure_font();
        }

        return true;
}

bool
Terminal::set_cell_height_scale(double scale)
{
        /* FIXME: compare old and new scale in pixel space */
        if (_vte_double_equal(scale, m_cell_height_scale))
                return false;

        m_cell_height_scale = scale;
        /* Set the drawing font. */
        m_fontdirty = true;
        if (widget_realized()) {
                ensure_font();
        }

        return true;
}

/* Read and refresh our perception of the size of the PTY. */
void
Terminal::refresh_size()
{
        if (!pty())
                return;

	int rows, columns;
        if (!pty()->get_size(&rows, &columns)) {
                /* Error reading PTY size, use defaults */
                rows = VTE_ROWS;
                columns = VTE_COLUMNS;
	}

        if (m_row_count == rows &&
            m_column_count == columns)
                return;

        m_row_count = rows;
        m_column_count = columns;
        m_tabstops.resize(columns);
}

/* Resize the given screen (normal or alternate) of the terminal. */
void
Terminal::screen_set_size(VteScreen *screen_,
                                    long old_columns,
                                    long old_rows,
                                    bool do_rewrap)
{
	auto ring = screen_->row_data;
	VteVisualPosition cursor_saved_absolute;
	VteVisualPosition below_viewport;
	VteVisualPosition below_current_paragraph;
        VteVisualPosition selection_start, selection_end;
	VteVisualPosition *markers[7];
        gboolean was_scrolled_to_top = (long(ceil(screen_->scroll_delta)) == long(ring->delta()));
        gboolean was_scrolled_to_bottom = ((long) screen_->scroll_delta == screen_->insert_delta);
	glong old_top_lines;
	double new_scroll_delta;

        if (m_selection_block_mode && do_rewrap && old_columns != m_column_count)
                deselect_all();

	_vte_debug_print(VTE_DEBUG_RESIZE,
			"Resizing %s screen_\n"
			"Old  insert_delta=%ld  scroll_delta=%f\n"
                        "     cursor (absolute)  row=%ld  col=%ld\n"
			"     cursor_saved (relative to insert_delta)  row=%ld  col=%ld\n",
			screen_ == &m_normal_screen ? "normal" : "alternate",
			screen_->insert_delta, screen_->scroll_delta,
                        screen_->cursor.row, screen_->cursor.col,
                        screen_->saved.cursor.row, screen_->saved.cursor.col);

        cursor_saved_absolute.row = screen_->saved.cursor.row + screen_->insert_delta;
        cursor_saved_absolute.col = screen_->saved.cursor.col;
	below_viewport.row = screen_->scroll_delta + old_rows;
	below_viewport.col = 0;
        below_current_paragraph.row = screen_->cursor.row + 1;
	while (below_current_paragraph.row < long(ring->next())
	    && ring->index(below_current_paragraph.row - 1)->attr.soft_wrapped) {
		below_current_paragraph.row++;
	}
	below_current_paragraph.col = 0;
        memset(&markers, 0, sizeof(markers));
        markers[0] = &cursor_saved_absolute;
        markers[1] = &below_viewport;
        markers[2] = &below_current_paragraph;
        markers[3] = &screen_->cursor;
        if (!m_selection_resolved.empty()) {
                selection_start.row = m_selection_resolved.start_row();
                selection_start.col = m_selection_resolved.start_column();
                selection_end.row = m_selection_resolved.end_row();
                selection_end.col = m_selection_resolved.end_column();
                markers[4] = &selection_start;
                markers[5] = &selection_end;
	}

	old_top_lines = below_current_paragraph.row - screen_->insert_delta;

	if (do_rewrap && old_columns != m_column_count)
		ring->rewrap(m_column_count, markers);

	if (long(ring->length()) > m_row_count) {
		/* The content won't fit without scrollbars. Before figuring out the position, we might need to
		   drop some lines from the ring if the cursor is not at the bottom, as XTerm does. See bug 708213.
		   This code is really tricky, see ../doc/rewrap.txt for details! */
		glong new_top_lines, drop1, drop2, drop3, drop;
		screen_->insert_delta = ring->next() - m_row_count;
		new_top_lines = below_current_paragraph.row - screen_->insert_delta;
		drop1 = ring->length() - m_row_count;
		drop2 = ring->next() - below_current_paragraph.row;
		drop3 = old_top_lines - new_top_lines;
		drop = MIN(MIN(drop1, drop2), drop3);
		if (drop > 0) {
			int new_ring_next = screen_->insert_delta + m_row_count - drop;
			_vte_debug_print(VTE_DEBUG_RESIZE,
					"Dropping %ld [== MIN(%ld, %ld, %ld)] rows at the bottom\n",
					drop, drop1, drop2, drop3);
			ring->shrink(new_ring_next - ring->delta());
		}
	}

        if (!m_selection_resolved.empty()) {
                m_selection_resolved.set ({ selection_start.row, selection_start.col },
                                          { selection_end.row, selection_end.col });
	}

	/* Figure out new insert and scroll deltas */
	if (long(ring->length()) <= m_row_count) {
		/* Everything fits without scrollbars. Align at top. */
		screen_->insert_delta = ring->delta();
		new_scroll_delta = screen_->insert_delta;
		_vte_debug_print(VTE_DEBUG_RESIZE,
				"Everything fits without scrollbars\n");
	} else {
		/* Scrollbar required. Can't afford unused lines at bottom. */
		screen_->insert_delta = ring->next() - m_row_count;
		if (was_scrolled_to_bottom) {
			/* Was scrolled to bottom, keep this way. */
			new_scroll_delta = screen_->insert_delta;
			_vte_debug_print(VTE_DEBUG_RESIZE,
					"Scroll to bottom\n");
		} else if (was_scrolled_to_top) {
			/* Was scrolled to top, keep this way. Not sure if this special case is worth it. */
			new_scroll_delta = ring->delta();
			_vte_debug_print(VTE_DEBUG_RESIZE,
					"Scroll to top\n");
		} else {
			/* Try to scroll so that the bottom visible row stays.
			   More precisely, the character below the bottom left corner stays in that
			   (invisible) row.
			   So if the bottom of the screen_ was at a hard line break then that hard
			   line break will stay there.
			   TODO: What would be the best behavior if the bottom of the screen_ is a
			   soft line break, i.e. only a partial line is visible at the bottom? */
			new_scroll_delta = below_viewport.row - m_row_count;
			/* Keep the old fractional part. */
			new_scroll_delta += screen_->scroll_delta - floor(screen_->scroll_delta);
			_vte_debug_print(VTE_DEBUG_RESIZE,
					"Scroll so bottom row stays\n");
		}
	}

	/* Don't clamp, they'll be clamped when restored. Until then remember off-screen_ values
	   since they might become on-screen_ again on subsequent resizes. */
        screen_->saved.cursor.row = cursor_saved_absolute.row - screen_->insert_delta;
        screen_->saved.cursor.col = cursor_saved_absolute.col;

	_vte_debug_print(VTE_DEBUG_RESIZE,
			"New  insert_delta=%ld  scroll_delta=%f\n"
                        "     cursor (absolute)  row=%ld  col=%ld\n"
			"     cursor_saved (relative to insert_delta)  row=%ld  col=%ld\n\n",
			screen_->insert_delta, new_scroll_delta,
                        screen_->cursor.row, screen_->cursor.col,
                        screen_->saved.cursor.row, screen_->saved.cursor.col);

	if (screen_ == m_screen)
		queue_adjustment_value_changed(new_scroll_delta);
	else
		screen_->scroll_delta = new_scroll_delta;
}

void
Terminal::set_size(long columns,
                   long rows,
                   bool allocating)
{
	glong old_columns, old_rows;

        update_insert_delta();  /* addresses https://gitlab.gnome.org/GNOME/vte/-/issues/2258 */

	_vte_debug_print(VTE_DEBUG_RESIZE,
			"Setting PTY size to %ldx%ld.\n",
			columns, rows);

	old_rows = m_row_count;
	old_columns = m_column_count;

	if (pty()) {
		/* Try to set the terminal size, and read it back,
		 * in case something went awry.
                 */
		if (!pty()->set_size(rows,
                                     columns,
                                     m_cell_height_unscaled,
                                     m_cell_width_unscaled)) {
                        // nothing we can do here
                }
		refresh_size();
	} else {
		m_row_count = rows;
		m_column_count = columns;
                m_tabstops.resize(columns);
	}
	if (old_rows != m_row_count || old_columns != m_column_count) {
                reset_scrolling_region();
                m_modes_private.set_DEC_ORIGIN(false);

                m_normal_screen.row_data->set_visible_rows(m_row_count);
                m_alternate_screen.row_data->set_visible_rows(m_row_count);

		/* Resize the normal screen and (if rewrapping is enabled) rewrap it even if the alternate screen is visible: bug 415277 */
		screen_set_size(&m_normal_screen, old_columns, old_rows, m_rewrap_on_resize);
		/* Resize the alternate screen if it's the current one, but never rewrap it: bug 336238 comment 60 */
		if (m_screen == &m_alternate_screen)
			screen_set_size(&m_alternate_screen, old_columns, old_rows, false);

                /* Ensure scrollback buffers cover the screen. */
                set_scrollback_lines(m_scrollback_lines);

                /* Ensure the cursor is valid */
                m_screen->cursor.row = std::clamp(m_screen->cursor.row,
                                                  long(m_screen->row_data->delta()),
                                                  std::max(long(m_screen->row_data->delta()),
                                                           long(m_screen->row_data->next()) - 1));

		adjust_adjustments_full();
#if VTE_GTK == 3
		gtk_widget_queue_resize_no_redraw(m_widget);
#elif VTE_GTK == 4
		if (!allocating)
			gtk_widget_queue_resize(m_widget); // FIXMEgtk4?
#endif
	}

        /* The visible bits might have changed even if the dimension in characters didn't,
         * so call these unconditionally: https://gitlab.gnome.org/GNOME/vte/-/issues/2829 */
        m_ringview.invalidate();
        invalidate_all();
        match_contents_clear();
        /* Our visible text changed. */
        emit_text_modified();
}

void
Terminal::set_scroll_value(double value)
{
        auto const lower = m_screen->row_data->delta();
        auto const upper_minus_row_count = m_screen->insert_delta;

        value = std::clamp(value,
                           double(lower),
                           double(std::max(long(lower), upper_minus_row_count)));

        /* Save the difference. */
        auto const dy = value - m_screen->scroll_delta;

        m_screen->scroll_delta = value;

        /* Sanity checks. */
        if (G_UNLIKELY(!widget_realized()))
                return;

        /* FIXME: do this check in pixel space */
        if (!_vte_double_equal(dy, 0)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Scrolling by %f\n", dy);

                invalidate_all();
                match_contents_clear();
                emit_text_scrolled(dy);
                queue_contents_changed();
        } else {
                _vte_debug_print(VTE_DEBUG_ADJ, "Not scrolling\n");
        }
}

Terminal::Terminal(vte::platform::Widget* w,
                   VteTerminal *t) :
        m_real_widget(w),
        m_terminal(t),
        m_widget(&t->widget),
        m_normal_screen(VTE_SCROLLBACK_INIT, true),
        m_alternate_screen(VTE_ROWS, false),
        m_screen(&m_normal_screen),
        m_termprop_values(vte::terminal::n_registered_termprops()),
        m_termprops_dirty(vte::terminal::n_registered_termprops())
{
        assert(m_termprop_values.size() == vte::terminal::n_registered_termprops());
        assert(m_termprops_dirty.size() == vte::terminal::n_registered_termprops());

        /* Inits allocation to 1x1 @ -1,-1 */
        cairo_rectangle_int_t allocation;
        gtk_widget_get_allocation(m_widget, &allocation);
        set_allocated_rect(allocation);

	/* NOTE! We allocated zeroed memory, just fill in non-zero stuff. */

        // FIXMEegmont make this store row indices only, maybe convert to a bitmap
#if VTE_GTK == 3
        m_update_rects = g_array_sized_new(FALSE /* zero terminated */,
                                           FALSE /* clear */,
                                           sizeof(cairo_rectangle_int_t),
                                           32 /* preallocated size */);
#endif

	/* Set up dummy metrics, value != 0 to avoid division by 0 */
        // FIXMEchpe this is wrong. These values must not be used before
        // the view has been set up, so if they are, that's a bug
	m_cell_width = 1;
	m_cell_height = 1;
	m_char_ascent = 1;
	m_char_descent = 1;
	m_line_thickness = 1;
	m_underline_position = 1;
        m_double_underline_position = 1;
        m_undercurl_position = 1.;
	m_strikethrough_position = 1;
        m_overline_position = 1;
        m_regex_underline_position = 1;

        vte_char_attr_list_init(&m_search_attrs);
        vte_char_attr_list_init(&m_match_attributes);

        m_match_contents = g_string_new(nullptr);

        m_defaults = m_color_defaults = basic_cell;

	/* Set up the desired palette. */
	set_colors_default();
	for (auto i = 0; i < VTE_PALETTE_SIZE; i++)
		m_palette[i].sources[vte::to_integral(color_palette::ColorSource::Escape)].is_set = false;

	/* Set up I/O encodings. */
	m_outgoing = _vte_byte_array_new();

	/* Setting the terminal type and size requires the PTY master to
	 * be set up properly first. */
        set_size(VTE_COLUMNS, VTE_ROWS, false);
        reset_scrolling_region();

        /* Default is 0, forces update in vte_terminal_set_scrollback_lines */
	set_scrollback_lines(VTE_SCROLLBACK_INIT);

        /* Initialize the saved cursor. */
        save_cursor(&m_normal_screen);
        save_cursor(&m_alternate_screen);

	/* Matching data. */
        m_match_span.clear(); // FIXMEchpe unnecessary
	match_hilite_clear(); // FIXMEchpe unnecessary

        /* Word chars */
        set_word_char_exceptions(WORD_CHAR_EXCEPTIONS_DEFAULT);

        update_view_extents();

#if VTE_DEBUG
        if (g_test_flags != 0) {
                feed("\e[1m\e[31mWARNING:\e[39m Test mode enabled. This is insecure!\e[0m\n\e[G"sv, false);
        }
#endif

#if !WITH_GNUTLS
        std::string str{"\e[1m\e[31m"};
        str.append(_("WARNING"));
        str.append(":\e[39m ");
        str.append(_("GnuTLS not enabled; data will be written to disk unencrypted!"));
        str.append("\e[0m\n\e[G");

        feed(str, false);
#endif
}

void
Terminal::widget_measure_width(int *minimum_width,
                               int *natural_width) noexcept
{
	ensure_font();

        refresh_size();

	*minimum_width = m_cell_width * VTE_MIN_GRID_WIDTH;
        *natural_width = m_cell_width * m_column_count;

#if VTE_GTK == 3
        *minimum_width += m_style_border.left + m_style_border.right;
        *natural_width += m_style_border.left + m_style_border.right;
#endif

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] minimum_width=%d, natural_width=%d for %ldx%ld cells (padding %d,%d;%d,%d).\n",
                        m_terminal,
			*minimum_width, *natural_width,
			m_column_count,
                         m_row_count,
                         m_style_border.left, m_style_border.right,
                         m_style_border.top, m_style_border.bottom);
}

void
Terminal::widget_measure_height(int *minimum_height,
                                int *natural_height) noexcept
{
	ensure_font();

        refresh_size();

	*minimum_height = m_cell_height * VTE_MIN_GRID_HEIGHT;
        *natural_height = m_cell_height * m_row_count;

#if VTE_GTK == 3
        *minimum_height += m_style_border.top + m_style_border.bottom;
        *natural_height += m_style_border.top + m_style_border.bottom;
#endif

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] minimum_height=%d, natural_height=%d for %ldx%ld cells (padding %d,%d;%d,%d).\n",
                        m_terminal,
			*minimum_height, *natural_height,
			m_column_count,
                         m_row_count,
                         m_style_border.left, m_style_border.right,
                         m_style_border.top, m_style_border.bottom);
}

void
Terminal::widget_size_allocate(
#if VTE_GTK == 3
                               int allocation_x,
                               int allocation_y,
#endif /* VTE_GTK == 3 */
                               int allocation_width,
                               int allocation_height,
                               int allocation_baseline,
                               Alignment xalign,
                               Alignment yalign,
                               bool xfill,
                               bool yfill) noexcept
{
        /* On gtk3, the style border is part of the widget's allocation;
         * on gtk4, is is not.
         */
#if VTE_GTK == 3
        auto width = allocation_width - (m_style_border.left + m_style_border.right);
        auto height = allocation_height - (m_style_border.top + m_style_border.bottom);
#elif VTE_GTK == 4
        auto width = allocation_width;
        auto height = allocation_height;
#endif

        auto grid_width = int(width / m_cell_width);
        auto grid_height = int(height / m_cell_height);

        width -= grid_width * m_cell_width;
        height -= grid_height * m_cell_height;
        /* assert(width >= 0); assert(height >= 0); */

        /* Distribute extra space according to alignment */
        /* xfill doesn't have any effect */
        auto lpad = 0, rpad = 0;
        switch (xalign) {
        default:
        case Alignment::START:  lpad = 0; rpad = width; break;
        case Alignment::CENTRE: lpad = width / 2; rpad = width - lpad; break;
        case Alignment::END:    lpad = width; rpad = 0; break;
        }

        /* yfill is only applied to START */
        auto tpad = 0, bpad = 0;
        switch (yalign) {
        default:
        case Alignment::START:
                tpad = 0;
                bpad = yfill ? 0 : height;
                break;

        case Alignment::CENTRE:      tpad = height / 2; bpad = height - tpad; break;
        case Alignment::END:         tpad = height; bpad = 0; break;
        }

#if VTE_GTK == 3
        m_border = m_style_border;
#elif VTE_GTK == 4
        m_border = {};
#endif

        m_border.left   += lpad;
        m_border.right  += rpad;
        m_border.top    += tpad;
        m_border.bottom += bpad;

        /* The minimum size returned from  ::widget_measure_width/height()
         * is VTE_MIN_GRID_WIDTH/HEIGHT, but let's be extra safe.
         */
        grid_width = std::max(grid_width, VTE_MIN_GRID_WIDTH);
        grid_height = std::max(grid_height, VTE_MIN_GRID_HEIGHT);

        _vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
                         "[Terminal %p] Sizing window to %dx%d (%dx%d, effective border %d,%d;%d,%d).\n",
                         m_terminal,
                         allocation_width, allocation_height,
                         grid_width, grid_height,
                         m_border.left, m_border.right, m_border.top, m_border.bottom);

        auto const current_allocation = get_allocated_rect();
        auto const repaint = current_allocation.width != allocation_width ||
                current_allocation.height != allocation_height;
        /* FIXME: remove this */
        auto const update_scrollback = current_allocation.height != allocation_height;

#if VTE_GTK == 3
        set_allocated_rect({allocation_x, allocation_y, allocation_width, allocation_height});
#elif VTE_GTK == 4
        set_allocated_rect({0, 0, allocation_width, allocation_height});
#endif

        if (grid_width != m_column_count ||
            grid_height != m_row_count ||
            update_scrollback) {
                /* Set the size of the pseudo-terminal. */
                set_size(grid_width, grid_height, true);

		/* Notify viewers that the contents have changed. */
		queue_contents_changed();
	}

        /* Force a repaint if we were resized. */
        if (widget_realized() && repaint) {
                reset_update_rects();
                invalidate_all();
        }
}

void
Terminal::widget_unmap()
{
        m_ringview.pause();
}

void
Terminal::widget_unrealize()
{
	/* Deallocate the cursors. */
        m_mouse_cursor_over_widget = FALSE;

	match_hilite_clear();

	m_im_preedit_active = FALSE;

	/* Drop font cache */
        m_draw.clear_font_cache();
	m_fontdirty = true;

        /* Remove the cursor blink timeout function. */
	remove_cursor_timeout();

        /* Remove the contents blink timeout function. */
        m_text_blink_timer.abort();

	/* Cancel any pending redraws. */
	stop_processing(this);

	/* Cancel any pending signals */
	m_contents_changed_pending = FALSE;
	m_cursor_moved_pending = FALSE;
	m_text_modified_flag = FALSE;
	m_text_inserted_flag = FALSE;
	m_text_deleted_flag = FALSE;

	/* Clear modifiers. */
	m_modifiers = 0;

	/* Free any selected text, but if we currently own the selection,
	 * throw the text onto the clipboard without an owner so that it
	 * doesn't just disappear. */
        for (auto sel_type : {vte::platform::ClipboardType::CLIPBOARD,
                              vte::platform::ClipboardType::PRIMARY}) {
                auto const sel = vte::to_integral(sel_type);
		if (m_selection[sel] != nullptr) {
			if (m_selection_owned[sel]) {
                                // FIXMEchpe we should check m_selection_format[sel]
                                // and also put text/html on if it's HTML format
                                widget()->clipboard_set_text(sel_type,
                                                             m_selection[sel]->str,
                                                             m_selection[sel]->len);
			}
			g_string_free(m_selection[sel], TRUE);
                        m_selection[sel] = nullptr;
		}
	}
}

void
Terminal::set_blink_settings(bool blink,
                             int blink_time_ms,
                             int blink_timeout_ms) noexcept
{
        m_cursor_blinks = m_cursor_blinks_system = blink;
        m_cursor_blink_cycle_ms = std::max(blink_time_ms / 2, VTE_MIN_CURSOR_BLINK_CYCLE);
        m_cursor_blink_timeout_ms = std::max(blink_timeout_ms, VTE_MIN_CURSOR_BLINK_TIMEOUT);

        update_cursor_blinks();

        /* Misuse gtk-cursor-blink-time for text blinking as well. This might change in the future. */
        m_text_blink_cycle_ms = m_cursor_blink_cycle_ms;
        if (m_text_blink_timer) {
                /* The current phase might have changed, and an already installed
                 * timer to blink might fire too late. So remove the timer and
                 * repaint the contents (which will install a correct new timer). */
                m_text_blink_timer.abort();
                invalidate_all();
        }
}

Terminal::~Terminal()
{
        /* Make sure not to change selection while in destruction. See issue vte#89. */
        m_changing_selection = true;

        terminate_child();
        unset_pty(false /* don't notify widget */);

        /* Stop processing input. */
        stop_processing(this);

	/* Free matching data. */
        vte_char_attr_list_clear(&m_match_attributes);
        g_string_free(m_match_contents, TRUE);

        vte_char_attr_list_clear(&m_search_attrs);

	/* Disconnect from autoscroll requests. */
	stop_autoscroll();

	/* Cancel pending adjustment change notifications. */
	m_adjustment_changed_pending = false;

        /* Stop listening for child-exited signals. */
        if (m_reaper) {
                g_signal_handlers_disconnect_by_func(m_reaper,
                                                     (gpointer)reaper_child_exited_cb,
                                                     this);
                g_object_unref(m_reaper);
        }

	/* Discard any pending data. */
	_vte_byte_array_free(m_outgoing);
        m_outgoing = nullptr;

#if VTE_GTK == 3
        /* Update rects */
        g_array_free(m_update_rects, TRUE /* free segment */);
#endif
}

void
Terminal::widget_realize()
{
        m_mouse_cursor_over_widget = FALSE;  /* We'll receive an enter_notify_event if the window appears under the cursor. */

	m_im_preedit_active = FALSE;

	/* Clear modifiers. */
	m_modifiers = 0;

        // Create the font cache
	ensure_font();
}

// FIXMEchpe probably @attr should be passed by ref
void
Terminal::determine_colors(VteCellAttr const* attr,
                                     bool is_selected,
                                     bool is_cursor,
                                     guint *pfore,
                                     guint *pback,
                                     guint *pdeco) const
{
        guint fore, back, deco;

        g_assert(attr);

	/* Start with cell colors */
        vte_color_triple_get(attr->colors(), &fore, &back, &deco);

	/* Reverse-mode switches default fore and back colors */
        if (m_modes_private.DEC_REVERSE_IMAGE()) [[unlikely]] {
		if (fore == VTE_DEFAULT_FG)
			fore = VTE_DEFAULT_BG;
		if (back == VTE_DEFAULT_BG)
			back = VTE_DEFAULT_FG;
	}

	/* Handle bold by using set bold color or brightening */
        if (attr->bold()) [[unlikely]] {
                if (fore == VTE_DEFAULT_FG && get_color(VTE_BOLD_FG) != NULL) {
			fore = VTE_BOLD_FG;
                } else if (m_bold_is_bright &&
                           fore >= VTE_LEGACY_COLORS_OFFSET &&
                           fore < VTE_LEGACY_COLORS_OFFSET + VTE_LEGACY_COLOR_SET_SIZE) {
			fore += VTE_COLOR_BRIGHT_OFFSET;
		}
	}

        /* Handle dim colors.  Only apply to palette colors, dimming direct RGB wouldn't make sense.
         * Apply to the foreground color only, but do this before handling reverse/highlight so that
         * those can be used to dim the background instead. */
        if (attr->dim() && !(fore & VTE_RGB_COLOR_MASK(8, 8, 8))) [[unlikely]] {
	        fore |= VTE_DIM_COLOR;
        }

	/* Reverse cell? */
	if (attr->reverse()) [[unlikely]] {
                using std::swap;
                swap(fore, back);
	}

	/* Selection: use hightlight back/fore, or inverse */
	if (is_selected) [[unlikely]] {
		/* XXX what if hightlight back is same color as current back? */
		bool do_swap = true;
		if (get_color(VTE_HIGHLIGHT_BG) != NULL) {
			back = VTE_HIGHLIGHT_BG;
			do_swap = false;
		}
		if (get_color(VTE_HIGHLIGHT_FG) != NULL) {
			fore = VTE_HIGHLIGHT_FG;
			do_swap = false;
		}
		if (do_swap) {
                        using std::swap;
                        swap(fore, back);
                }
	}

	/* Cursor: use cursor back, or inverse */
	if (is_cursor) [[unlikely]] {
		/* XXX what if cursor back is same color as current back? */
                bool do_swap = true;
                if (get_color(VTE_CURSOR_BG) != NULL) {
                        back = VTE_CURSOR_BG;
                        do_swap = false;
                }
                if (get_color(VTE_CURSOR_FG) != NULL) {
                        fore = VTE_CURSOR_FG;
                        do_swap = false;
                }
                if (do_swap) {
                        using std::swap;
                        swap(fore, back);
                }
	}

	*pfore = fore;
	*pback = back;
        *pdeco = deco;
}

void
Terminal::determine_colors(VteCell const* cell,
                                     bool highlight,
                                     guint *fore,
                                     guint *back,
                                     guint *deco) const
{
	determine_colors(cell ? &cell->attr : &basic_cell.attr,
                         highlight, false /* not cursor */,
                         fore, back, deco);
}

void
Terminal::determine_cursor_colors(VteCell const* cell,
                                            bool highlight,
                                            guint *fore,
                                            guint *back,
                                            guint *deco) const
{
	determine_colors(cell ? &cell->attr : &basic_cell.attr,
                         highlight, true /* cursor */,
                         fore, back, deco);
}

void
Terminal::resolve_normal_colors(VteCell const* cell,
                                unsigned* pfore,
                                unsigned* pback,
                                vte::color::rgb& fg,
                                vte::color::rgb& bg)
{
        auto deco = unsigned{};
        determine_colors(cell, false, pfore, pback, &deco);
        rgb_from_index<8, 8, 8>(*pfore, fg);
        rgb_from_index<8, 8, 8>(*pback, bg);
}

// FIXMEchpe this constantly removes and reschedules the timer. improve this!
bool
Terminal::text_blink_timer_callback()
{
        invalidate_all();
        return false; /* don't run again */
}

/* Draw a string of characters with similar attributes. */
void
Terminal::draw_cells(vte::view::DrawingContext::TextRequest* items,
                               gssize n,
                               uint32_t fore,
                               uint32_t back,
                               uint32_t deco,
                               bool clear,
                               bool draw_default_bg,
                               uint32_t attr,
                               bool hyperlink,
                               bool hilite,
                               int column_width,
                               int row_height)
{
        int i, xl, xr, y;
	gint columns = 0;
        vte::color::rgb fg, bg, dc;

	g_assert(n > 0);
#if 0
	_VTE_DEBUG_IF(VTE_DEBUG_CELLS) {
		GString *str = g_string_new (NULL);
		gchar *tmp;
		for (i = 0; i < n; i++) {
			g_string_append_unichar (str, items[i].c);
		}
		tmp = g_string_free (str, FALSE);
                g_printerr ("draw_cells('%s', fore=%d, back=%d, deco=%d, bold=%d,"
                                " ul=%d, strike=%d, ol=%d, blink=%d,"
                                " hyperlink=%d, hilite=%d, boxed=%d)\n",
                                tmp, fore, back, deco, bold,
                                underline, strikethrough, overline, blink,
                                hyperlink, hilite, boxed);
		g_free (tmp);
	}
#endif

        rgb_from_index<8, 8, 8>(fore, fg);
        rgb_from_index<8, 8, 8>(back, bg);
        // FIXMEchpe defer resolving deco color until we actually need to draw an underline?
        if (deco == VTE_DEFAULT_FG)
                dc = fg;
        else
                rgb_from_index<4, 5, 4>(deco, dc);

        if (clear && (draw_default_bg || back != VTE_DEFAULT_BG)) {
                /* Paint the background. */
                i = 0;
                while (i < n) {
                        xl = items[i].x;
                        xr = items[i].x + items[i].columns * column_width;
                        y = items[i].y;
                        /* Items are not necessarily contiguous in LTR order.
                         * Combine as long as they form a single visual run. */
                        for (i++; i < n && items[i].y == y; i++) {
                                if (G_LIKELY (items[i].x == xr)) {
                                        xr += items[i].columns * column_width;  /* extend to the right */
                                } else if (items[i].x + items[i].columns * column_width == xl) {
                                        xl = items[i].x;                        /* extend to the left */
                                } else {
                                        break;                                  /* break the run */
                                }
                        }

                        m_draw.fill_rectangle(xl,
                                              y,
                                              xr - xl, row_height,
                                              &bg);
                }
        }

        if (attr & VTE_ATTR_BLINK) {
                /* Notify the caller that cells with the "blink" attribute were encountered (regardless of
                 * whether they're actually painted or skipped now), so that the caller can set up a timer
                 * to make them blink if it wishes to. */
                m_text_to_blink = true;

                /* This is for the "off" state of blinking text. Invisible text could also be handled here,
                 * but it's not, it's handled outside by not even calling this method.
                 * Setting fg = bg and painting the text would not work for two reasons: it'd be opaque
                 * even if the background is translucent, and this method can be called with a continuous
                 * run of identical fg, yet different bg colored cells. So we simply bail out. */
                if (!m_text_blink_state)
                        return;
        }

        /* Draw whatever SFX are required. Do this before drawing the letters,
         * so that if the descent of a letter crosses an underline of a different color,
         * it's the letter's color that wins. Other kinds of decorations always have the
         * same color as the text, so the order is irrelevant there. */
        if ((attr & (VTE_ATTR_UNDERLINE_MASK |
                     VTE_ATTR_STRIKETHROUGH_MASK |
                     VTE_ATTR_OVERLINE_MASK |
                     VTE_ATTR_BOXED_MASK)) |
            hyperlink | hilite) {
		i = 0;
                while (i < n) {
                        xl = items[i].x;
                        xr = items[i].x + items[i].columns * column_width;
                        columns = items[i].columns;
			y = items[i].y;
                        /* Items are not necessarily contiguous in LTR order.
                         * Combine as long as they form a single visual run. */
                        for (i++; i < n && items[i].y == y; i++) {
                                if (G_LIKELY (items[i].x == xr)) {
                                        xr += items[i].columns * column_width;  /* extend to the right */
                                        columns += items[i].columns;
                                } else if (items[i].x + items[i].columns * column_width == xl) {
                                        xl = items[i].x;                        /* extend to the left */
                                        columns += items[i].columns;
                                } else {
                                        break;                                  /* break the run */
                                }
			}
                        switch (vte_attr_get_value(attr, VTE_ATTR_UNDERLINE_VALUE_MASK, VTE_ATTR_UNDERLINE_SHIFT)) {
                        case 1:  /* single underline */
                                m_draw.draw_line(
                                                    xl,
                                                    y + m_underline_position,
                                                    xr - 1,
                                                    y + m_underline_position + m_underline_thickness - 1,
                                                    VTE_LINE_WIDTH,
                                                    &dc);
                                break;
                        case 2:  /* double underline */
                                m_draw.draw_line(
                                                    xl,
                                                    y + m_double_underline_position,
                                                    xr - 1,
                                                    y + m_double_underline_position + m_double_underline_thickness - 1,
                                                    VTE_LINE_WIDTH,
                                                    &dc);
                                m_draw.draw_line(
                                                    xl,
                                                    y + m_double_underline_position + 2 * m_double_underline_thickness,
                                                    xr - 1,
                                                    y + m_double_underline_position + 3 * m_double_underline_thickness - 1,
                                                    VTE_LINE_WIDTH,
                                                    &dc);
                                break;
                        case 3:  /* curly underline */
                                m_draw.draw_undercurl(
                                                         xl,
                                                         y + m_undercurl_position,
                                                         m_undercurl_thickness,
                                                         columns,
                                                         widget()->scale_factor(),
                                                         &dc);
                                break;
                        case 4:  /* dotted underline */
                                for (int j = 0; j < columns; j++) {
                                        for (int k = 0; k < column_width - m_underline_thickness; k += 2 * m_underline_thickness) {
                                                m_draw.draw_line(
                                                                    xl + j * column_width + k,
                                                                    y + m_underline_position,
                                                                    xl + j * column_width + k + m_underline_thickness - 1,
                                                                    y + m_underline_position + m_underline_thickness - 1,
                                                                    VTE_LINE_WIDTH,
                                                                    &dc);
                                        }
                                }
                                break;
                        case 5:  /* dashed underline */
                                for (int j = 0; j < columns; j++) {
                                        /* left quarter */
                                        m_draw.draw_line(
                                                            xl + j * column_width,
                                                            y + m_underline_position,
                                                            xl + j * column_width + MAX(column_width / 4, 1) - 1,
                                                            y + m_underline_position + m_underline_thickness - 1,
                                                            VTE_LINE_WIDTH,
                                                            &dc);
                                        /* right quarter */
                                        m_draw.draw_line(
                                                            xl + j * column_width + MAX(column_width * 3 / 4, 1) - 1,
                                                            y + m_underline_position,
                                                            xl + (j + 1) * column_width - 1,
                                                            y + m_underline_position + m_underline_thickness - 1,
                                                            VTE_LINE_WIDTH,
                                                            &dc);
                                }
                                break;
			}
			if (attr & VTE_ATTR_STRIKETHROUGH) {
                                m_draw.draw_line(
                                                    xl,
                                                    y + m_strikethrough_position,
                                                    xr - 1,
                                                    y + m_strikethrough_position + m_strikethrough_thickness - 1,
                                                    VTE_LINE_WIDTH,
                                                    &fg);
			}
                        if (attr & VTE_ATTR_OVERLINE) {
                                m_draw.draw_line(
                                                    xl,
                                                    y + m_overline_position,
                                                    xr - 1,
                                                    y + m_overline_position + m_overline_thickness - 1,
                                                    VTE_LINE_WIDTH,
                                                    &fg);
                        }
			if (hilite) {
                                m_draw.draw_line(
                                                    xl,
                                                    y + m_regex_underline_position,
                                                    xr - 1,
                                                    y + m_regex_underline_position + m_regex_underline_thickness - 1,
                                                    VTE_LINE_WIDTH,
                                                    &fg);
                        } else if (hyperlink) {
                                for (double j = 1.0 / 6.0; j < columns; j += 0.5) {
                                        m_draw.fill_rectangle(
                                                                 xl + j * column_width,
                                                                 y + m_regex_underline_position,
                                                                 MAX(column_width / 6.0, 1.0),
                                                                 m_regex_underline_thickness,
                                                                 &fg);
                                }
                        }
			if (attr & VTE_ATTR_BOXED) {
                                m_draw.draw_rectangle(
                                                         xl,
                                                         y,
                                                         xr - xl,
                                                         row_height,
                                                         &fg);
			}
                }
	}

        m_draw.draw_text(
                       items, n,
                       attr,
                       &fg);
}

/* FIXME: we don't have a way to tell GTK+ what the default text attributes
 * should be, so for now at least it's assuming white-on-black is the norm and
 * is using "black-on-white" to signify "inverse".  Pick up on that state and
 * fix things.  Do this here, so that if we suddenly get red-on-black, we'll do
 * the right thing. */
void
Terminal::fudge_pango_colors(GSList *attributes,
                                       VteCell *cells,
                                       gsize n)
{
	gsize i, sumlen = 0;
	struct _fudge_cell_props{
		gboolean saw_fg, saw_bg;
		vte::color::rgb fg, bg;
		guint index;
	}*props = g_newa (struct _fudge_cell_props, n);

	for (i = 0; i < n; i++) {
		gchar ubuf[7];
		gint len = g_unichar_to_utf8 (cells[i].c, ubuf);
		props[i].index = sumlen;
		props[i].saw_fg = props[i].saw_bg = FALSE;
		sumlen += len;
	}

	while (attributes != NULL) {
		PangoAttribute *attr = (PangoAttribute *)attributes->data;
		PangoAttrColor *color;
		switch (attr->klass->type) {
		case PANGO_ATTR_FOREGROUND:
			for (i = 0; i < n; i++) {
				if (props[i].index < attr->start_index) {
					continue;
				}
				if (props[i].index >= attr->end_index) {
					break;
				}
				props[i].saw_fg = TRUE;
				color = (PangoAttrColor*) attr;
				props[i].fg = color->color;
			}
			break;
		case PANGO_ATTR_BACKGROUND:
			for (i = 0; i < n; i++) {
				if (props[i].index < attr->start_index) {
					continue;
				}
				if (props[i].index >= attr->end_index) {
					break;
				}
				props[i].saw_bg = TRUE;
				color = (PangoAttrColor*) attr;
				props[i].bg = color->color;
			}
			break;
		default:
			break;
		}
		attributes = g_slist_next(attributes);
	}

	for (i = 0; i < n; i++) {
		if (props[i].saw_fg && props[i].saw_bg &&
				(props[i].fg.red == 0xffff) &&
				(props[i].fg.green == 0xffff) &&
				(props[i].fg.blue == 0xffff) &&
				(props[i].bg.red == 0) &&
				(props[i].bg.green == 0) &&
				(props[i].bg.blue == 0)) {
                        cells[i].attr.copy_colors(m_color_defaults.attr);
			cells[i].attr.set_reverse(true);
		}
	}
}

/* Apply the attribute given in the PangoAttribute to the list of cells. */
void
Terminal::apply_pango_attr(PangoAttribute *attr,
                                     VteCell *cells,
                                     gsize n_cells)
{
	guint i, ival;
	PangoAttrInt *attrint;
	PangoAttrColor *attrcolor;

	switch (attr->klass->type) {
	case PANGO_ATTR_FOREGROUND:
	case PANGO_ATTR_BACKGROUND:
		attrcolor = (PangoAttrColor*) attr;
                ival = VTE_RGB_COLOR(8, 8, 8,
                                     ((attrcolor->color.red & 0xFF00) >> 8),
                                     ((attrcolor->color.green & 0xFF00) >> 8),
                                     ((attrcolor->color.blue & 0xFF00) >> 8));
		for (i = attr->start_index;
		     i < attr->end_index && i < n_cells;
		     i++) {
			if (attr->klass->type == PANGO_ATTR_FOREGROUND) {
                                cells[i].attr.set_fore(ival);
			}
			if (attr->klass->type == PANGO_ATTR_BACKGROUND) {
                                cells[i].attr.set_back(ival);
			}
		}
		break;
	case PANGO_ATTR_UNDERLINE_COLOR:
		attrcolor = (PangoAttrColor*) attr;
                ival = VTE_RGB_COLOR(4, 5, 4,
                                     ((attrcolor->color.red & 0xFF00) >> 8),
                                     ((attrcolor->color.green & 0xFF00) >> 8),
                                     ((attrcolor->color.blue & 0xFF00) >> 8));
		for (i = attr->start_index;
		     i < attr->end_index && i < n_cells;
		     i++) {
			if (attr->klass->type == PANGO_ATTR_UNDERLINE) {
                                cells[i].attr.set_deco(ival);
			}
		}
		break;
	case PANGO_ATTR_STRIKETHROUGH:
		attrint = (PangoAttrInt*) attr;
		ival = attrint->value;
		for (i = attr->start_index;
		     (i < attr->end_index) && (i < n_cells);
		     i++) {
			cells[i].attr.set_strikethrough(ival != FALSE);
		}
		break;
	case PANGO_ATTR_UNDERLINE:
		attrint = (PangoAttrInt*) attr;
		ival = attrint->value;
		for (i = attr->start_index;
		     (i < attr->end_index) && (i < n_cells);
		     i++) {
                        unsigned int underline = 0;
                        switch (ival) {
                        case PANGO_UNDERLINE_SINGLE:
                                underline = 1;
                                break;
                        case PANGO_UNDERLINE_DOUBLE:
                                underline = 2;
                                break;
                        case PANGO_UNDERLINE_ERROR:
                                underline = 3; /* wavy */
                                break;
                        case PANGO_UNDERLINE_NONE:
                        case PANGO_UNDERLINE_LOW: /* FIXME */
                                underline = 0;
                                break;
                        }
			cells[i].attr.set_underline(underline);
		}
		break;
	case PANGO_ATTR_WEIGHT:
		attrint = (PangoAttrInt*) attr;
		ival = attrint->value;
		for (i = attr->start_index;
		     (i < attr->end_index) && (i < n_cells);
		     i++) {
			cells[i].attr.set_bold(ival >= PANGO_WEIGHT_BOLD);
		}
		break;
	case PANGO_ATTR_STYLE:
		attrint = (PangoAttrInt*) attr;
		ival = attrint->value;
		for (i = attr->start_index;
		     (i < attr->end_index) && (i < n_cells);
		     i++) {
			cells[i].attr.set_italic(ival != PANGO_STYLE_NORMAL);
		}
		break;
	default:
		break;
	}
}

/* Convert a PangoAttrList and a location in that list to settings in a
 * charcell structure.  The cells array is assumed to contain enough items
 * so that all ranges in the attribute list can be mapped into the array, which
 * typically means that the cell array should have the same length as the
 * string (byte-wise) which the attributes describe. */
void
Terminal::translate_pango_cells(PangoAttrList *attrs,
                                          VteCell *cells,
                                          gsize n_cells)
{
	PangoAttribute *attr;
	PangoAttrIterator *attriter;
	GSList *list, *listiter;
	guint i;

	for (i = 0; i < n_cells; i++) {
                cells[i] = m_color_defaults;
	}

	attriter = pango_attr_list_get_iterator(attrs);
	if (attriter != NULL) {
		do {
			list = pango_attr_iterator_get_attrs(attriter);
			if (list != NULL) {
				for (listiter = list;
				     listiter != NULL;
				     listiter = g_slist_next(listiter)) {
					attr = (PangoAttribute *)listiter->data;
					apply_pango_attr(attr, cells, n_cells);
				}
				attr = (PangoAttribute *)list->data;
				fudge_pango_colors(
								 list,
								 cells +
								 attr->start_index,
								 MIN(n_cells, attr->end_index) -
								 attr->start_index);
				g_slist_free_full(list, (GDestroyNotify)pango_attribute_destroy);
			}
		} while (pango_attr_iterator_next(attriter) == TRUE);
		pango_attr_iterator_destroy(attriter);
	}
}

/* Draw the listed items using the given attributes.  Tricky because the
 * attribute string is indexed by byte in the UTF-8 representation of the string
 * of characters.  Because we draw a character at a time, this is slower. */
void
Terminal::draw_cells_with_attributes(vte::view::DrawingContext::TextRequest* items,
                                               gssize n,
                                               PangoAttrList *attrs,
                                               bool draw_default_bg,
                                               gint column_width,
                                               gint height)
{
        int i, j, cell_count;
	VteCell *cells;
	char scratch_buf[VTE_UTF8_BPC];
        guint fore, back, deco;

	/* Note: since this function is only called with the pre-edit text,
	 * all the items contain gunichar only, not vteunistr. */
        // FIXMEchpe is that really true for all input methods?

        uint32_t const attr_mask = m_allow_bold ? ~0 : ~VTE_ATTR_BOLD_MASK;

	for (i = 0, cell_count = 0; i < n; i++) {
		cell_count += g_unichar_to_utf8(items[i].c, scratch_buf);
	}
	cells = g_new(VteCell, cell_count);
	translate_pango_cells(attrs, cells, cell_count);
	for (i = 0, j = 0; i < n; i++) {
                determine_colors(&cells[j], false, &fore, &back, &deco);
		draw_cells(items + i, 1,
					fore,
					back,
                                        deco,
					TRUE, draw_default_bg,
					cells[j].attr.attr & attr_mask,
                                        m_allow_hyperlink && cells[j].attr.hyperlink_idx != 0,
					FALSE, column_width, height);
		j += g_unichar_to_utf8(items[i].c, scratch_buf);
	}
	g_free(cells);
}

void
Terminal::ringview_update()
{
        auto first_row = first_displayed_row();
        auto last_row = last_displayed_row();
        if (cursor_is_onscreen())
                last_row = std::max(last_row, m_screen->cursor.row);

        m_ringview.set_ring (m_screen->row_data);
        m_ringview.set_rows (first_row, last_row - first_row + 1);
        m_ringview.set_width (m_column_count);
        m_ringview.set_enable_bidi (m_enable_bidi);
        m_ringview.set_enable_shaping (m_enable_shaping);
        m_ringview.update ();
}

/* Paint the contents of a given row at the given location.  Take advantage
 * of multiple-draw APIs by finding runs of characters with identical
 * attributes and bundling them together. */
void
Terminal::draw_rows(VteScreen *screen_,
                    cairo_region_t const* region,
                    vte::grid::row_t start_row,
                    vte::grid::row_t end_row,
                    gint start_y, /* must be the start of a row */
                    gint column_width,
                    gint row_height)
{
        vte::grid::row_t row;
        vte::grid::column_t i, j, lcol, vcol;
        guint fore = VTE_DEFAULT_FG, nfore, back = VTE_DEFAULT_BG, nback, deco = VTE_DEFAULT_FG, ndeco;
        gboolean hyperlink = FALSE, nhyperlink;  /* non-hovered explicit hyperlink, needs dashed underlining */
        gboolean hilite = FALSE, nhilite;        /* hovered explicit hyperlink or regex match, needs continuous underlining */
        gboolean selected;
        gboolean nrtl = FALSE, rtl;  /* for debugging */
        uint32_t attr = 0, nattr;
	guint item_count;
	const VteCell *cell;
	VteRowData const* row_data;
        vte::base::BidiRow const* bidirow;

        auto const column_count = m_column_count;
        uint32_t const attr_mask = m_allow_bold ? ~0 : ~VTE_ATTR_BOLD_MASK;

        /* Need to ensure the ringview is updated. */
        ringview_update();

        auto items = g_newa(vte::view::DrawingContext::TextRequest, column_count);

        /* Paint the background.
         * Do it first for all the cells we're about to paint, before drawing the glyphs,
         * so that overflowing bits of a glyph (to the right or downwards) won't be
         * chopped off by another cell's background, not even across changes of the
         * background or any other attribute.
         * Process each row independently. */
#if VTE_GTK == 3
        int const rect_width = get_allocated_width();
#elif VTE_GTK == 4
        int const rect_width = get_allocated_width() + m_style_border.left + m_style_border.right;
#endif

        auto bg_rect = vte::view::Rectangle{0,
                                            start_y,
                                            int(column_count * column_width),
                                            int(row_height * (end_row - start_row))};
        m_draw.begin_background(bg_rect, column_count, end_row - start_row);

        /* The rect contains the area of the row, and is moved row-wise in the loop. */
#if VTE_GTK == 3
        auto crect = vte::view::Rectangle{-m_border.left, start_y, rect_width, row_height};
#endif
        for (row = start_row;
             row < end_row;
             row++
#if VTE_GTK == 3
                     , crect.advance_y(row_height)
#endif
             ) {
#if VTE_GTK == 3
                /* Check whether we need to draw this row at all */
                if (cairo_region_contains_rectangle(region, crect.cairo()) == CAIRO_REGION_OVERLAP_OUT)
                        continue;

                auto const y = crect.cairo()->y;
#endif

		row_data = find_row_data(row);
                bidirow = m_ringview.get_bidirow(row);

#if VTE_GTK == 3
                _VTE_DEBUG_IF (VTE_DEBUG_BIDI) {
                        /* Debug: Highlight the paddings of RTL rows with a slightly different background. */
                        if (bidirow->base_is_rtl()) {
                                vte::color::rgb bg;
                                rgb_from_index<8, 8, 8>(VTE_DEFAULT_BG, bg);
                                /* Go halfway towards #C0C0C0. */
                                bg.red   = (bg.red   + 0xC000) / 2;
                                bg.green = (bg.green + 0xC000) / 2;
                                bg.blue  = (bg.blue  + 0xC000) / 2;
                                m_draw.fill_rectangle(
                                                          -m_border.left,
                                                          y,
                                                          m_border.left,
                                                          row_height,
                                                          &bg);
                                m_draw.fill_rectangle(
                                                          column_count * column_width,
                                                          y,
                                                          rect_width - m_border.left - column_count * column_width,
                                                          row_height,
                                                          &bg);
                        }
                }
#endif // VTE_GTK == 3

                i = j = 0;
                /* Walk the line.
                 * Locate runs of identical bg colors within a row, and paint each run as a single rectangle. */
                do {
                        /* Get the first cell's contents. */
                        cell = row_data ? _vte_row_data_get (row_data, bidirow->vis2log(i)) : nullptr;
                        /* Find the colors for this cell. */
                        selected = cell_is_selected_vis(i, row);
                        determine_colors(cell, selected, &fore, &back, &deco);
                        rtl = bidirow->vis_is_rtl(i);

                        while (++j < column_count) {
                                /* Retrieve the next cell. */
                                cell = row_data ? _vte_row_data_get (row_data, bidirow->vis2log(j)) : nullptr;
                                /* Resolve attributes to colors where possible and
                                 * compare visual attributes to the first character
                                 * in this chunk. */
                                selected = cell_is_selected_vis(j, row);
                                determine_colors(cell, selected, &nfore, &nback, &ndeco);
                                nrtl = bidirow->vis_is_rtl(j);
                                if (nback != back || (_vte_debug_on (VTE_DEBUG_BIDI) && nrtl != rtl)) {
                                        break;
                                }
                        }
                        if (back != VTE_DEFAULT_BG) {
                                vte::color::rgb bg;
                                rgb_from_index<8, 8, 8>(back, bg);
                                m_draw.fill_cell_background(i, row - start_row, (j - i), &bg);
                        }

#if VTE_GTK == 3
                        _VTE_DEBUG_IF (VTE_DEBUG_BIDI) {
                                /* Debug: Highlight RTL letters and RTL rows with a slightly different background. */
                                vte::color::rgb bg;
                                rgb_from_index<8, 8, 8>(back, bg);
                                /* Go halfway towards #C0C0C0. */
                                bg.red   = (bg.red   + 0xC000) / 2;
                                bg.green = (bg.green + 0xC000) / 2;
                                bg.blue  = (bg.blue  + 0xC000) / 2;
                                int y1 = y + round(row_height / 8.);
                                int y2 = y + row_height - round(row_height / 8.);
                                /* Paint the top and bottom eighth of the cell with this more gray background
                                 * if the paragraph has a resolved RTL base direction. */
                                if (bidirow->base_is_rtl()) {
                                        m_draw.fill_rectangle(
                                                                  i * column_width,
                                                                  y,
                                                                  (j - i) * column_width,
                                                                  y1 - y,
                                                                  &bg);
                                        m_draw.fill_rectangle(
                                                                  i * column_width,
                                                                  y2,
                                                                  (j - i) * column_width,
                                                                  y + row_height - y2,
                                                                  &bg);
                                }
                                /* Paint the middle three quarters of the cell with this more gray background
                                 * if the current character has a resolved RTL direction. */
                                if (rtl) {
                                        m_draw.fill_rectangle(
                                                                  i * column_width,
                                                                  y1,
                                                                  (j - i) * column_width,
                                                                  y2 - y1,
                                                                  &bg);
                                }
                        }
#endif // VTE_GTK == 3

                        /* We'll need to continue at the first cell which didn't
                         * match the first one in this set. */
                        i = j;
                } while (i < column_count);
        }

        m_draw.flush_background(bg_rect);

        /* Render the text.
         * The rect contains the area of the row (enlarged a bit at the top and bottom
         * to allow the text to overdraw a bit), and is moved row-wise in the loop.
         */
        auto rect = vte::view::Rectangle{-m_border.left,
                                         start_y - cell_overflow_top(),
                                         rect_width,
                                         row_height + cell_overflow_top() + cell_overflow_bottom()};

        int y;
        for (row = start_row, y = start_y;
             row < end_row;
             row++, y += row_height, rect.advance_y(row_height)) {
#if VTE_GTK == 3
                /* Check whether we need to draw this row at all */
                if (cairo_region_contains_rectangle(region, rect.cairo()) == CAIRO_REGION_OVERLAP_OUT)
                        continue;
#endif

                row_data = find_row_data(row);
                if (row_data == NULL)
                        continue; /* Skip row. */

                /* Ensure that drawing is restricted to the cell (plus the overdraw area) */
                _vte_draw_autoclip_t clipper{m_draw, &rect};

                bidirow = m_ringview.get_bidirow(row);

                /* Walk the line in logical order.
                 * Locate runs of identical attributes within a row, and draw each run using a single draw_cells() call. */
                item_count = 0;
                // FIXME No need for the "< column_count" safety cap once bug 135 is addressed.
                for (lcol = 0; lcol < row_data->len && lcol < column_count; ) {
                        vcol = bidirow->log2vis(lcol);

                        /* Get the character cell's contents. */
                        cell = _vte_row_data_get (row_data, lcol);
                        g_assert(cell != nullptr);

                        nhyperlink = (m_allow_hyperlink && cell->attr.hyperlink_idx != 0);
                        nhilite = (nhyperlink && cell->attr.hyperlink_idx == m_hyperlink_hover_idx) ||
                                  (!nhyperlink && regex_match_has_current() && m_match_span.contains(row, lcol));
                        if (cell->c == 0 ||
                            ((cell->c == ' ' || cell->c == '\t') &&  // FIXME '\t' is newly added now, double check
                             cell->attr.has_none(VTE_ATTR_UNDERLINE_MASK |
                                                 VTE_ATTR_STRIKETHROUGH_MASK |
                                                 VTE_ATTR_OVERLINE_MASK) &&
                             !nhyperlink &&
                             !nhilite) ||
                            cell->attr.fragment() ||
                            cell->attr.invisible()) {
                                /* Skip empty or fragment cell, but erase on ' ' and '\t', since
                                 * it may be overwriting an image. */
                                lcol++;
                                continue;
                        }

                        /* Find the colors for this cell. */
                        nattr = cell->attr.attr;
                        selected = cell_is_selected_log(lcol, row);
                        determine_colors(cell, selected, &nfore, &nback, &ndeco);

                        /* See if it no longer fits the run. */
                        if (item_count > 0 &&
                                   (((attr ^ nattr) & (VTE_ATTR_BOLD_MASK |
                                                       VTE_ATTR_ITALIC_MASK |
                                                       VTE_ATTR_UNDERLINE_MASK |
                                                       VTE_ATTR_STRIKETHROUGH_MASK |
                                                       VTE_ATTR_OVERLINE_MASK |
                                                       VTE_ATTR_BLINK_MASK |
                                                       VTE_ATTR_INVISIBLE_MASK)) ||  // FIXME or just simply "attr != nattr"?
                                    fore != nfore ||
                                    back != nback ||
                                    deco != ndeco ||
                                    hyperlink != nhyperlink ||
                                    hilite != nhilite)) {
                                /* Draw the completed run of cells and start a new one. */
                                draw_cells(items, item_count,
                                           fore, back, deco, FALSE, FALSE,
                                           attr & attr_mask,
                                           hyperlink, hilite,
                                           column_width, row_height);
                                item_count = 0;
                        }

                        /* Combine with subsequent spacing marks. */
                        vteunistr c = cell->c;
                        j = lcol + cell->attr.columns();
                        if (G_UNLIKELY (lcol == 0 && g_unichar_ismark (_vte_unistr_get_base (cell->c)))) {
                                /* A rare special case: the first cell contains a spacing mark.
                                 * Place on top of a NBSP, along with additional spacing marks if any,
                                 * and display beginning at offscreen column -1.
                                 * Additional spacing marks, if any, will be combined by the loop below. */
                                c = _vte_unistr_append_unistr (0x00A0, cell->c);
                                lcol = -1;
                        }
                        // FIXME No need for the "< column_count" safety cap once bug 135 is addressed.
                        while (j < row_data->len && j < column_count) {
                                /* Combine with subsequent spacing marks. */
                                cell = _vte_row_data_get (row_data, j);
                                if (cell && !cell->attr.fragment() && g_unichar_ismark (_vte_unistr_get_base (cell->c))) {
                                        c = _vte_unistr_append_unistr (c, cell->c);
                                        j += cell->attr.columns();
                                } else {
                                        break;
                                }
                        }

                        attr = nattr;
                        fore = nfore;
                        back = nback;
                        deco = ndeco;
                        hyperlink = nhyperlink;
                        hilite = nhilite;

                        vte_assert_cmpint (item_count, <, column_count);
                        items[item_count].c = bidirow->vis_get_shaped_char(vcol, c);
                        items[item_count].columns = j - lcol;
                        items[item_count].x = (vcol - (bidirow->vis_is_rtl(vcol) ? items[item_count].columns - 1 : 0)) * column_width;
                        items[item_count].y = y;
                        items[item_count].mirror = bidirow->vis_is_rtl(vcol);
                        items[item_count].box_mirror = !!(row_data->attr.bidi_flags & VTE_BIDI_FLAG_BOX_MIRROR);
                        item_count++;

                        vte_assert_cmpint (j, >, lcol);
                        lcol = j;
                }

                /* Draw the last run of cells in the row. */
                if (item_count > 0) {
                        draw_cells(items, item_count,
                                   fore, back, deco, FALSE, FALSE,
                                   attr & attr_mask,
                                   hyperlink, hilite,
                                   column_width, row_height);
                }
        }
}

// Returns the rectangle the cursor would be drawn if a block cursor,
// or a zero-size rect at the top left corner if the cursor is not
// on-screen.
vte::view::Rectangle
Terminal::cursor_rect()
{
        // This mostly replicates the code below in ::paint_cursor(), but
        // it would be too complicated to try to extract that into a common
        // method.

        auto lcol = m_screen->cursor.col;
        auto const drow = m_screen->cursor.row;
        auto const width = m_cell_width;
	auto const height = m_cell_height;

        if (!cursor_is_onscreen() ||
            std::clamp(long(lcol), long(0), long(m_column_count - 1)) != lcol)
		return {};

        /* Need to ensure the ringview is updated. */
        ringview_update();

        /* Find the first cell of the character "under" the cursor.
         * This is for CJK.  For TAB, paint the cursor where it really is. */
        vte::base::BidiRow const* bidirow = m_ringview.get_bidirow(drow);

        auto cell = find_charcell(lcol, drow);
        while (cell && cell->attr.fragment() && cell->c != '\t' && lcol > 0) {
                --lcol;
                cell = find_charcell(lcol, drow);
	}

        auto vcol = bidirow->log2vis(lcol);
        auto const c = (cell && cell->c) ? bidirow->vis_get_shaped_char(vcol, cell->c) : ' ';
	auto const columns = c == '\t' ? 1 : cell ? cell->attr.columns() : 1;
        auto const x = (vcol - ((cell && bidirow->vis_is_rtl(vcol)) ? cell->attr.columns() - 1 : 0)) * width;
	auto const y = row_to_pixel(drow);

        auto cursor_width = columns * width;

        // Include the spacings in the cursor, see bug 781479 comments 39-44.
        // Make the cursor even wider if the glyph is wider.
        if (cell && cell->c != 0 && cell->c != ' ' && cell->c != '\t') {

                auto const attr = cell && cell->c ? cell->attr.attr : 0;
                int l, r;
                m_draw.get_char_edges(cell->c, cell->attr.columns(), attr, l /* unused */, r);
                cursor_width = cursor_width >= r ? cursor_width : r;
        }

        return {int(x), int(y), int(cursor_width), int(height)};
}

void
Terminal::paint_cursor()
{
        vte::view::DrawingContext::TextRequest item;
        vte::grid::row_t drow;
        vte::grid::column_t lcol, vcol;
        int width, height, cursor_width;
        guint fore, back, deco;
	vte::color::rgb bg;
	int x, y;
	gboolean blink, selected, focus;

        //FIXMEchpe this should already be reflected in the m_cursor_blink_state below
	if (!m_modes_private.DEC_TEXT_CURSOR())
		return;

        if (m_im_preedit_active)
                return;

	focus = m_has_focus;
	blink = m_cursor_blink_state;

	if (focus && !blink)
		return;

        lcol = m_screen->cursor.col;
        drow = m_screen->cursor.row;
	width = m_cell_width;
	height = m_cell_height;

        if (!cursor_is_onscreen())
                return;
        if (CLAMP(lcol, 0, m_column_count - 1) != lcol)
		return;

        /* Need to ensure the ringview is updated. */
        ringview_update();

        /* Find the first cell of the character "under" the cursor.
         * This is for CJK.  For TAB, paint the cursor where it really is. */
        VteRowData const *row_data = find_row_data(drow);
        vte::base::BidiRow const *bidirow = m_ringview.get_bidirow(drow);

        auto cell = find_charcell(lcol, drow);
        while (cell != NULL && cell->attr.fragment() && cell->c != '\t' && lcol > 0) {
                lcol--;
                cell = find_charcell(lcol, drow);
	}

	/* Draw the cursor. */
        vcol = bidirow->log2vis(lcol);
        item.c = (cell && cell->c) ? bidirow->vis_get_shaped_char(vcol, cell->c) : ' ';
	item.columns = item.c == '\t' ? 1 : cell ? cell->attr.columns() : 1;
        item.x = (vcol - ((cell && bidirow->vis_is_rtl(vcol)) ? cell->attr.columns() - 1 : 0)) * width;
	item.y = row_to_pixel(drow);
        item.mirror = bidirow->vis_is_rtl(vcol);
        item.box_mirror = (row_data && (row_data->attr.bidi_flags & VTE_BIDI_FLAG_BOX_MIRROR));
        auto const attr = cell && cell->c ? cell->attr.attr : 0;

        selected = cell_is_selected_log(lcol, drow);
        determine_cursor_colors(cell, selected, &fore, &back, &deco);
        rgb_from_index<8, 8, 8>(back, bg);

	x = item.x;
	y = item.y;

        switch (decscusr_cursor_shape()) {

		case CursorShape::eIBEAM: {
                        /* Draw at the very left of the cell (before the spacing), even in case of CJK.
                         * IMO (egmont) not overrunning the letter improves readability, vertical movement
                         * looks good (no zigzag even when a somewhat wider glyph that starts filling up
                         * the left spacing, or CJK that begins further to the right is encountered),
                         * and also this is where it looks good if background colors change, including
                         * Shift+arrows highlighting experience in some editors. As per the behavior of
                         * word processors, don't increase the height by the line spacing. */
                        int stem_width;

                        stem_width = (int) (((float) (m_char_ascent + m_char_descent)) * m_cursor_aspect_ratio + 0.5);
                        stem_width = CLAMP (stem_width, VTE_LINE_WIDTH, m_cell_width);

                        /* The I-beam goes to the right edge of the cell if its character has RTL resolved direction. */
                        if (bidirow->vis_is_rtl(vcol))
                                x += item.columns * m_cell_width - stem_width;

                        m_draw.fill_rectangle(
                                                 x, y + m_char_padding.top, stem_width, m_char_ascent + m_char_descent,
                                                 &bg);

                        /* Show the direction of the current character if the paragraph contains a mixture
                         * of directions.
                         * FIXME Do this for the other cursor shapes, too. Need to find a good visual design. */
                        if (focus && bidirow->has_foreign())
                                m_draw.fill_rectangle(
                                                         bidirow->vis_is_rtl(vcol) ? x - stem_width : x + stem_width,
                                                         y + m_char_padding.top,
                                                         stem_width, stem_width,
                                                         &bg);
			break;
                }

		case CursorShape::eUNDERLINE: {
                        /* The width is at least the overall width of the cell (or two cells) minus the two
                         * half spacings on the two edges. That is, underlines under a CJK are more than twice
                         * as wide as narrow characters in case of letter spacing. Plus, if necessary, the width
                         * is increased to span under the entire glyph. Vertical position is not affected by
                         * line spacing. */

                        int line_height, left, right;

			/* use height (not width) so underline and ibeam will
			 * be equally visible */
                        line_height = (int) (((float) (m_char_ascent + m_char_descent)) * m_cursor_aspect_ratio + 0.5);
                        line_height = CLAMP (line_height, VTE_LINE_WIDTH, m_char_ascent + m_char_descent);

                        left = m_char_padding.left;
                        right = item.columns * m_cell_width - m_char_padding.right;

                        if (cell && cell->c != 0 && cell->c != ' ' && cell->c != '\t') {
                                int l, r;
                                m_draw.get_char_edges(cell->c, cell->attr.columns(), attr, l, r);
                                left = MIN(left, l);
                                right = MAX(right, r);
                        }

                        m_draw.fill_rectangle(
                                                 x + left, y + m_cell_height - m_char_padding.bottom - line_height,
                                                 right - left, line_height,
                                                 &bg);
			break;
                }

		case CursorShape::eBLOCK:
                        /* Include the spacings in the cursor, see bug 781479 comments 39-44.
                         * Make the cursor even wider if the glyph is wider. */

                        cursor_width = item.columns * width;
                        if (cell && cell->c != 0 && cell->c != ' ' && cell->c != '\t') {
                                int l, r;
                                m_draw.get_char_edges(cell->c, cell->attr.columns(), attr, l /* unused */, r);
                                cursor_width = MAX(cursor_width, r);
			}

                        uint32_t const attr_mask = m_allow_bold ? ~0 : ~VTE_ATTR_BOLD_MASK;

			if (focus) {
				/* just reverse the character under the cursor */
                                m_draw.fill_rectangle(
							     x, y,
                                                         cursor_width, height,
                                                         &bg);

                                if (cell && cell->c != 0 && cell->c != ' ' && cell->c != '\t') {
                                        draw_cells(
                                                        &item, 1,
                                                        fore, back, deco, TRUE, FALSE,
                                                        cell->attr.attr & attr_mask,
                                                        m_allow_hyperlink && cell->attr.hyperlink_idx != 0,
                                                        FALSE,
                                                        width,
                                                        height);
				}

			} else {
				/* draw a box around the character */
                                m_draw.draw_rectangle(
							     x - VTE_LINE_WIDTH,
							     y - VTE_LINE_WIDTH,
							     cursor_width + 2*VTE_LINE_WIDTH,
                                                         height + 2*VTE_LINE_WIDTH,
                                                         &bg);
			}

			break;
	}
}

void
Terminal::paint_im_preedit_string()
{
        int vcol, columns;
        long row;
	long width, height;
	int i, len;

	if (m_im_preedit.empty())
		return;

        /* Need to ensure the ringview is updated. */
        ringview_update();

        /* Get the row's BiDi information. */
        row = m_screen->cursor.row;
        if (row < first_displayed_row() || row > last_displayed_row())
                return;
        vte::base::BidiRow const *bidirow = m_ringview.get_bidirow(row);

	/* Keep local copies of rendering information. */
	width = m_cell_width;
	height = m_cell_height;

	/* Find out how many columns the pre-edit string takes up. */
	columns = get_preedit_width(false);
	len = get_preedit_length(false);

	/* If the pre-edit string won't fit on the screen if we start
	 * drawing it at the cursor's position, move it left. */
        vcol = bidirow->log2vis(m_screen->cursor.col);
        if (vcol + columns > m_column_count) {
                vcol = MAX(0, m_column_count - columns);
	}

	/* Draw the preedit string, boxed. */
	if (len > 0) {
		const char *preedit = m_im_preedit.c_str();
		int preedit_cursor;

                auto items = g_new0(vte::view::DrawingContext::TextRequest, len);
		for (i = columns = 0; i < len; i++) {
			items[i].c = g_utf8_get_char(preedit);
                        items[i].columns = _vte_unichar_width(items[i].c,
                                                              m_utf8_ambiguous_width);
                        items[i].x = (vcol + columns) * width;
			items[i].y = row_to_pixel(m_screen->cursor.row);
			columns += items[i].columns;
			preedit = g_utf8_next_char(preedit);
		}
                if (G_LIKELY(m_clear_background)) {
                        m_draw.clear(
                                        vcol * width,
                                        row_to_pixel(m_screen->cursor.row),
                                        width * columns,
                                        height,
                                        get_color(ColorPaletteIndex::default_bg()), m_background_alpha);
                }

		draw_cells_with_attributes(
							items, len,
							m_im_preedit_attrs.get(),
							TRUE,
							width, height);
		preedit_cursor = m_im_preedit_cursor;

		if (preedit_cursor >= 0 && preedit_cursor < len) {
                        uint32_t fore, back, deco;
                        vte_color_triple_get(m_color_defaults.attr.colors(), &fore, &back, &deco);

			/* Cursored letter in reverse. */
			draw_cells(
						&items[preedit_cursor], 1,
                                                fore, back, deco,
                                                TRUE,  /* clear */
                                                TRUE,  /* draw_default_bg */
                                                VTE_ATTR_NONE | VTE_ATTR_BOXED,
                                                FALSE, /* hyperlink */
                                                FALSE, /* hilite */
						width, height);
		}
		g_free(items);
	}
}

#if VTE_GTK == 3

void
Terminal::widget_draw(cairo_t* cr) noexcept
{
#if VTE_DEBUG
        _VTE_DEBUG_IF(VTE_DEBUG_LIFECYCLE | VTE_DEBUG_WORK | VTE_DEBUG_UPDATES) do {
                auto clip_rect = cairo_rectangle_int_t{};
                if (!gdk_cairo_get_clip_rectangle (cr, &clip_rect))
                        break;

                _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_draw()\n");
                _vte_debug_print (VTE_DEBUG_WORK, "+");
                _vte_debug_print (VTE_DEBUG_UPDATES, "Draw (%d,%d)x(%d,%d)\n",
                                  clip_rect.x, clip_rect.y,
                                  clip_rect.width, clip_rect.height);
        } while (0);
#endif /* VTE_DEBUG */

        m_draw.set_cairo(cr);
        m_draw.translate(m_border.left, m_border.top);
        m_draw.set_scale_factor(widget()->scale_factor());

        /* Both cr and region should be in view coordinates now.
         * No need to further translation.
         */

        auto region = vte_cairo_get_clip_region(cr);
        if (region != nullptr)
                draw(region.get());

        m_draw.untranslate();
        m_draw.set_cairo(nullptr);
}

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4

void
Terminal::widget_snapshot(GtkSnapshot* snapshot_object) noexcept
{
        _vte_debug_print(VTE_DEBUG_DRAW, "Widget snapshot\n");

        m_draw.set_snapshot(snapshot_object);
        m_draw.translate(m_border.left, m_border.top);
        m_draw.set_scale_factor(widget()->scale_factor());

        draw(nullptr);

        m_draw.untranslate();
        m_draw.set_snapshot(nullptr);
}

#endif /* VTE_GTK == 4 */

void
Terminal::draw(cairo_region_t const* region) noexcept
{
        int allocated_width, allocated_height;
        int extra_area_for_cursor;
        bool text_blink_enabled_now;
        auto now_ms = int64_t{0};

        allocated_width = get_allocated_width();
        allocated_height = get_allocated_height();

        /* Note: @cr's origin is at the top left of the view area; the left/top border
         * is entirely to the left/top of this point.
         */

        if (G_LIKELY(m_clear_background)) {
                m_draw.clear(
#if VTE_GTK == 3
                             -m_border.left,
                             -m_border.top,
                             allocated_width,
                             allocated_height,
#elif VTE_GTK == 4
                             -m_border.left - m_style_border.left,
                             -m_border.top - m_style_border.top,
                             allocated_width + m_style_border.left + m_style_border.right,
                             allocated_height + m_style_border.top + m_style_border.bottom,
#endif
                             get_color(ColorPaletteIndex::default_bg()), m_background_alpha);
        }

        /* Clip vertically, for the sake of smooth scrolling. We want the top and bottom paddings to be unused.
         * Don't clip horizontally so that antialiasing can legally overflow to the right padding. */
        auto const vert_clip = vte::view::Rectangle{
#if VTE_GTK == 3
                                                    -m_border.left,
#elif VTE_GTK == 4
                                                    -m_style_border.left - m_border.left,
#endif
                                                    0,
#if VTE_GTK == 3
                                                    allocated_width,
#elif VTE_GTK == 4
                                                    allocated_width + m_style_border.left + m_style_border.right,
#endif
                                                    allocated_height - m_border.top - m_border.bottom};
        m_draw.clip_border(&vert_clip);

        /* Whether blinking text should be visible now */
        m_text_blink_state = true;
        text_blink_enabled_now = (unsigned)m_text_blink_mode & (unsigned)(m_has_focus ? TextBlinkMode::eFOCUSED : TextBlinkMode::eUNFOCUSED);
        if (text_blink_enabled_now) {
                now_ms = g_get_monotonic_time() / 1000;
                if (now_ms % (m_text_blink_cycle_ms * 2) >= m_text_blink_cycle_ms)
                        m_text_blink_state = false;
        }
        /* Painting will flip this if it encounters any cell with blink attribute */
        m_text_to_blink = false;

        /* and now paint them */
        auto const first_row = first_displayed_row();
        draw_rows(m_screen,
                  region,
                  first_row,
                  last_displayed_row() + 1,
                  row_to_pixel(first_row),
                  m_cell_width,
                  m_cell_height);

	paint_im_preedit_string();

        m_draw.unclip_border();

        /* Re-clip, allowing VTE_LINE_WIDTH more pixel rows for the outline cursor. */
        /* TODOegmont: It's really ugly to do it here. */
        extra_area_for_cursor = (decscusr_cursor_shape() == CursorShape::eBLOCK && !m_has_focus) ? VTE_LINE_WIDTH : 0;
        auto const reclip = vte::view::Rectangle{
#if VTE_GTK == 3
                                                 -m_border.left,
#elif VTE_GTK == 4
                                                 -m_style_border.left - m_border.left,
#endif
                                                 -extra_area_for_cursor,
#if VTE_GTK == 3
                                                 allocated_width,
#elif VTE_GTK == 4
                                                 allocated_width + m_style_border.left + m_style_border.right,
#endif
                                                 allocated_height - m_border.top - m_border.bottom + 2 * extra_area_for_cursor};

        m_draw.clip_border(&reclip);
	paint_cursor();
        m_draw.unclip_border();

        /* If painting encountered any cell with blink attribute, we might need to set up a timer.
         * Blinking is implemented using a one-shot (not repeating) timer that keeps getting reinstalled
         * here as long as blinking cells are encountered during (re)painting. This way there's no need
         * for an explicit step to stop the timer when blinking cells are no longer present, this happens
         * implicitly by the timer not getting reinstalled anymore (often after a final unnecessary but
         * harmless repaint). */
        if (G_UNLIKELY (m_text_to_blink && text_blink_enabled_now && !m_text_blink_timer))
                m_text_blink_timer.schedule(m_text_blink_cycle_ms - now_ms % m_text_blink_cycle_ms,
                                            vte::glib::Timer::Priority::eLOW);

        m_invalidated_all = FALSE;
}

#if VTE_GTK == 3

/* Handle an expose event by painting the exposed area. */
static vte::Freeable<cairo_region_t>
vte_cairo_get_clip_region(cairo_t *cr)
{
        auto list = vte::take_freeable(cairo_copy_clip_rectangle_list(cr));
        if (list->status == CAIRO_STATUS_CLIP_NOT_REPRESENTABLE) {

                auto clip_rect = cairo_rectangle_int_t{};
                if (!gdk_cairo_get_clip_rectangle(cr, &clip_rect))
                        return nullptr;

                return vte::take_freeable(cairo_region_create_rectangle(&clip_rect));
        }

        auto region = vte::take_freeable(cairo_region_create());
        for (auto i = list->num_rectangles - 1; i >= 0; --i) {
                auto rect = &list->rectangles[i];

                cairo_rectangle_int_t clip_rect;
                clip_rect.x = floor (rect->x);
                clip_rect.y = floor (rect->y);
                clip_rect.width = ceil (rect->x + rect->width) - clip_rect.x;
                clip_rect.height = ceil (rect->y + rect->height) - clip_rect.y;

                if (cairo_region_union_rectangle(region.get(), &clip_rect) != CAIRO_STATUS_SUCCESS) {
                        region.reset();
                        break;
                }
        }

        return region;
}

#endif /* VTE_GTK == 3 */

bool
Terminal::widget_mouse_scroll(vte::platform::ScrollEvent const& event)
{
	gdouble v;
        gint cnt_x, cnt_y, i;
	int button;

        m_modifiers = event.modifiers();
        m_mouse_smooth_scroll_x_delta += event.dx();
        m_mouse_smooth_scroll_y_delta += event.dy();

        /* If we're running a mouse-aware application, map the scroll event to button presses on buttons 4-7. */
	if (m_mouse_tracking_mode != MouseTrackingMode::eNONE) {
                cnt_x = m_mouse_smooth_scroll_x_delta;
                cnt_y = m_mouse_smooth_scroll_y_delta;
                if (cnt_x == 0 && cnt_y == 0)
			return true;

                /* Need to ensure the ringview is updated. */
                ringview_update();

                m_mouse_smooth_scroll_x_delta -= cnt_x;
                m_mouse_smooth_scroll_y_delta -= cnt_y;
		_vte_debug_print(VTE_DEBUG_EVENTS,
                                 "Scroll application by %d lines, %d columns, smooth scroll delta set back to y=%f, x=%f\n",
                                 cnt_y, cnt_x, m_mouse_smooth_scroll_y_delta, m_mouse_smooth_scroll_x_delta);

                button = cnt_y > 0 ? 5 : 4;
                if (cnt_y < 0)
                        cnt_y = -cnt_y;
                for (i = 0; i < cnt_y; i++) {
                        /* Encode the parameters and send them to the app. */
                        feed_mouse_event(confined_grid_coords_from_view_coords(m_mouse_last_position),
                                         button,
                                         false /* not drag */,
                                         false /* not release */);
                }

                button = cnt_x > 0 ? 7 : 6;
                if (cnt_x < 0)
                        cnt_x = -cnt_x;
                for (i = 0; i < cnt_x; i++) {
                        /* Encode the parameters and send them to the app. */
                        feed_mouse_event(confined_grid_coords_from_view_coords(m_mouse_last_position),
                                         button,
                                         false /* not drag */,
                                         false /* not release */);
                }
		return true;
	}

        /* The modes below only care about vertical scrolling. Don't accumulate horizontal noise. */
        m_mouse_smooth_scroll_x_delta = 0;

        v = MAX (1., ceil (m_row_count /* page increment */ / 10.));
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Scroll speed is %d lines per non-smooth scroll unit\n",
			(int) v);
	if (m_screen == &m_alternate_screen &&
            m_modes_private.XTERM_ALTBUF_SCROLL()) {
		char *normal;
		gsize normal_length;

                cnt_y = v * m_mouse_smooth_scroll_y_delta;
                if (cnt_y == 0)
			return true;
                m_mouse_smooth_scroll_y_delta -= cnt_y / v;
		_vte_debug_print(VTE_DEBUG_EVENTS,
                                "Scroll by %d lines, smooth scroll y_delta set back to %f\n",
                                cnt_y, m_mouse_smooth_scroll_y_delta);

                /* In the alternate screen there is no scrolling, so fake a few cursor keystrokes (only vertically). */
		_vte_keymap_map (
                                cnt_y > 0 ? GDK_KEY_Down : GDK_KEY_Up,
				m_modifiers,
                                m_modes_private.DEC_APPLICATION_CURSOR_KEYS(),
                                m_modes_private.DEC_APPLICATION_KEYPAD(),
				&normal,
				&normal_length);
                if (cnt_y < 0)
                        cnt_y = -cnt_y;
                for (i = 0; i < cnt_y; i++) {
			send_child({normal, normal_length});
		}
		g_free (normal);
                return true;
	} else if (m_fallback_scrolling) {
		/* Perform a history scroll. */
                double dcnt = m_screen->scroll_delta + v * m_mouse_smooth_scroll_y_delta;
		queue_adjustment_value_changed_clamped(dcnt);
                m_mouse_smooth_scroll_y_delta = 0;
                return true;
	}
        return false;
}

bool
Terminal::set_audible_bell(bool setting)
{
        if (setting == m_audible_bell)
                return false;

	m_audible_bell = setting;
        return true;
}

bool
Terminal::set_text_blink_mode(TextBlinkMode setting)
{
        if (setting == m_text_blink_mode)
                return false;

        m_text_blink_mode = setting;
        invalidate_all();

        return true;
}

bool
Terminal::set_enable_a11y(bool setting)
{
        if (setting == m_enable_a11y)
                return false;

        m_enable_a11y = setting;

        return true;
}

bool
Terminal::set_enable_bidi(bool setting)
{
        if (setting == m_enable_bidi)
                return false;

        m_enable_bidi = setting;
        m_ringview.invalidate();
        invalidate_all();

        /* Chances are that we can free up some BiDi/shaping buffers that we
         * won't need for a while. */
        if (!setting)
                m_ringview.pause();

        return true;
}

bool
Terminal::set_enable_shaping(bool setting)
{
        if (setting == m_enable_shaping)
                return false;

        m_enable_shaping = setting;
        m_ringview.invalidate();
        invalidate_all();

        /* Chances are that we can free up some BiDi/shaping buffers that we
         * won't need for a while. */
        if (!setting)
                m_ringview.pause();

        return true;
}

bool
Terminal::set_allow_bold(bool setting)
{
        if (setting == m_allow_bold)
                return false;

	m_allow_bold = setting;
	invalidate_all();

        return true;
}

bool
Terminal::set_bold_is_bright(bool setting)
{
        if (setting == m_bold_is_bright)
                return false;

	m_bold_is_bright = setting;

        /* Need to re-sanitise the font description to ensure bold is distinct. */
        update_font_desc();

	invalidate_all();

        return true;
}

bool
Terminal::set_allow_hyperlink(bool setting)
{
        if (setting == m_allow_hyperlink)
                return false;

        if (setting == false) {
                m_hyperlink_hover_idx = m_screen->row_data->get_hyperlink_at_position(-1, -1, true, NULL);
                g_assert (m_hyperlink_hover_idx == 0);
                m_hyperlink_hover_uri = NULL;
                emit_hyperlink_hover_uri_changed(NULL);  /* FIXME only emit if really changed */
                m_defaults.attr.hyperlink_idx = m_screen->row_data->get_hyperlink_idx(NULL);
                g_assert (m_defaults.attr.hyperlink_idx == 0);
        }

        m_allow_hyperlink = setting;
        invalidate_all();

        return true;
}

bool
Terminal::set_fallback_scrolling(bool set)
{
        if (set == m_fallback_scrolling)
                return false;

        m_fallback_scrolling = set;
        return true;
}

bool
Terminal::set_scroll_on_insert(bool scroll)
{
        if (scroll == m_scroll_on_insert)
                return false;

        m_scroll_on_insert = scroll;
        return true;
}

bool
Terminal::set_scroll_on_output(bool scroll)
{
        if (scroll == m_scroll_on_output)
                return false;

        m_scroll_on_output = scroll;
        return true;
}

bool
Terminal::set_scroll_on_keystroke(bool scroll)
{
        if (scroll == m_scroll_on_keystroke)
                return false;

        m_scroll_on_keystroke = scroll;
        return true;
}

bool
Terminal::set_rewrap_on_resize(bool rewrap)
{
        if (rewrap == m_rewrap_on_resize)
                return false;

        m_rewrap_on_resize = rewrap;
        return true;
}

void
Terminal::update_cursor_blinks()
{
        auto blink = false;

        switch (decscusr_cursor_blink()) {
        case CursorBlinkMode::eSYSTEM:
                blink = m_cursor_blinks_system;
                break;
        case CursorBlinkMode::eON:
                blink = true;
                break;
        case CursorBlinkMode::eOFF:
                blink = false;
                break;
        }

	if (m_cursor_blinks == blink)
		return;

	m_cursor_blinks = blink;
	check_cursor_blink();
}

bool
Terminal::set_cursor_blink_mode(CursorBlinkMode mode)
{
        if (mode == m_cursor_blink_mode)
                return false;

        m_cursor_blink_mode = mode;
        update_cursor_blinks();

        return true;
}

bool
Terminal::set_cursor_shape(CursorShape shape)
{
        if (shape == m_cursor_shape)
                return false;

        m_cursor_shape = shape;
	invalidate_cursor_once();

        return true;
}

/* DECSCUSR set cursor style */
bool
Terminal::set_cursor_style(CursorStyle style)
{
        if (m_cursor_style == style)
                return false;

        m_cursor_style = style;
        update_cursor_blinks();
        /* and this will also make cursor shape match the DECSCUSR style */
        invalidate_cursor_once();

        return true;
}

/*
 * Terminal::decscusr_cursor_blink:
 *
 * Returns the cursor blink mode set by DECSCUSR. If DECSCUSR was never
 * called, or it set the blink mode to terminal default, this returns the
 * value set via API or in dconf. Internal use only.
 *
 * Return value: cursor blink mode
 */
Terminal::CursorBlinkMode
Terminal::decscusr_cursor_blink() const noexcept
{
        switch (m_cursor_style) {
        default:
        case CursorStyle::eTERMINAL_DEFAULT:
                return m_cursor_blink_mode;
        case CursorStyle::eBLINK_BLOCK:
        case CursorStyle::eBLINK_UNDERLINE:
        case CursorStyle::eBLINK_IBEAM:
                return CursorBlinkMode::eON;
        case CursorStyle::eSTEADY_BLOCK:
        case CursorStyle::eSTEADY_UNDERLINE:
        case CursorStyle::eSTEADY_IBEAM:
                return CursorBlinkMode::eOFF;
        }
}

/*
 * Terminal::decscusr_cursor_shape:
 * @terminal: a #VteTerminal
 *
 * Returns the cursor shape set by DECSCUSR. If DECSCUSR was never called,
 * or it set the cursor shape to terminal default, this returns the value
 * set via API. Internal use only.
 *
 * Return value: cursor shape
 */
Terminal::CursorShape
Terminal::decscusr_cursor_shape() const noexcept
{
        switch (m_cursor_style) {
        default:
        case CursorStyle::eTERMINAL_DEFAULT:
                return m_cursor_shape;
        case CursorStyle::eBLINK_BLOCK:
        case CursorStyle::eSTEADY_BLOCK:
                return CursorShape::eBLOCK;
        case CursorStyle::eBLINK_UNDERLINE:
        case CursorStyle::eSTEADY_UNDERLINE:
                return CursorShape::eUNDERLINE;
        case CursorStyle::eBLINK_IBEAM:
        case CursorStyle::eSTEADY_IBEAM:
                return CursorShape::eIBEAM;
        }
}

bool
Terminal::set_scrollback_lines(long lines)
{
        glong low, high, next;
        double scroll_delta;
	VteScreen *scrn;

	if (lines < 0)
		lines = G_MAXLONG;

#if 0
        /* FIXME: this breaks the scrollbar range, bug #562511 */
        if (lines == m_scrollback_lines)
                return false;
#endif

	_vte_debug_print (VTE_DEBUG_MISC,
			"Setting scrollback lines to %ld\n", lines);

	m_scrollback_lines = lines;

        /* The main screen gets the full scrollback buffer. */
        scrn = &m_normal_screen;
        lines = std::max (lines, m_row_count);
        next = std::max (m_screen->cursor.row + 1,
                         long(scrn->row_data->next()));
        scrn->row_data->resize(lines);
        low = scrn->row_data->delta();
        high = lines + MIN (G_MAXLONG - lines, low - m_row_count + 1);
        scrn->insert_delta = CLAMP (scrn->insert_delta, low, high);
        scrn->scroll_delta = CLAMP (scrn->scroll_delta, low, scrn->insert_delta);
        next = MIN (next, scrn->insert_delta + m_row_count);
        if (long(scrn->row_data->next()) > next){
                scrn->row_data->shrink(next - low);
        }

        /* The alternate scrn isn't allowed to scroll at all. */
        scrn = &m_alternate_screen;
        scrn->row_data->resize(m_row_count);
        scrn->scroll_delta = scrn->row_data->delta();
        scrn->insert_delta = scrn->row_data->delta();
        if (long(scrn->row_data->next()) > scrn->insert_delta + m_row_count){
                scrn->row_data->shrink(m_row_count);
        }

	/* Adjust the scrollbar to the new location. */
	/* Hack: force a change in scroll_delta even if the value remains, so that
	   vte_term_q_adj_val_changed() doesn't shortcut to no-op, see bug 676075. */
        scroll_delta = m_screen->scroll_delta;
	m_screen->scroll_delta = -1;
	queue_adjustment_value_changed(scroll_delta);
	adjust_adjustments_full();

        m_ringview.invalidate();
        invalidate_all();
        match_contents_clear();

        return true;
}

bool
Terminal::set_backspace_binding(EraseMode binding)
{
        if (binding == m_backspace_binding)
                return false;

	m_backspace_binding = binding;
        return true;
}

bool
Terminal::set_delete_binding(EraseMode binding)
{
        if (binding == m_delete_binding)
                return false;

	m_delete_binding = binding;
        return true;
}

bool
Terminal::set_mouse_autohide(bool autohide)
{
        if (autohide == m_mouse_autohide)
                return false;

	m_mouse_autohide = autohide;

        if (m_mouse_cursor_autohidden) {
                hyperlink_hilite_update();
                match_hilite_update();
                apply_mouse_cursor();
        }
        return true;
}

void
Terminal::reset_decoder()
{
        switch (primary_data_syntax()) {
        case DataSyntax::ECMA48_UTF8:
                m_utf8_decoder.reset();
                break;

#if WITH_ICU
        case DataSyntax::ECMA48_PCTERM:
                m_converter->decoder().reset();
                break;
#endif

        default:
                g_assert_not_reached();
        }
}

void
Terminal::reset_data_syntax()
{
        if (current_data_syntax() == primary_data_syntax())
                return;

        switch (current_data_syntax()) {
        default:
                break;
        }

        pop_data_syntax();
}

/*
 * Terminal::reset:
 * @clear_tabstops: whether to reset tabstops
 * @clear_history: whether to empty the terminal's scrollback buffer
 *
 * Resets as much of the terminal's internal state as possible, discarding any
 * unprocessed input data, resetting character attributes, cursor state,
 * national character set state, status line, terminal modes (insert/delete),
 * selection state, and encoding.
 *
 */
void
Terminal::reset(bool clear_tabstops,
                          bool clear_history,
                          bool from_api)
{
        if (from_api && !m_input_enabled)
                return;

        auto const freezer = vte::glib::FreezeObjectNotify{m_terminal};

        m_bell_pending = false;

	/* Clear the output buffer. */
	_vte_byte_array_clear(m_outgoing);

	/* Reset charset substitution state. */

        /* Reset decoder */
        reset_decoder();

        /* Reset parser */
        reset_data_syntax();
        m_parser.reset();
        m_last_graphic_character = 0;

        /* Reset modes */
        m_modes_ecma.reset();
        m_modes_private.clear_saved();
        m_modes_private.reset();
        m_decsace_is_rectangle = false; // DECSACE defaults to STREAM

        /* Reset tabstops */
        if (clear_tabstops) {
                m_tabstops.reset();
        }

        /* Window title stack */
        if (clear_history) {
                m_window_title_stack.clear();
        }

        update_mouse_protocol();

	/* Reset the color palette. Only the 256 indexed colors, not the special ones, as per xterm. */
	for (int i = 0; i < 256; i++)
		m_palette[i].sources[vte::to_integral(color_palette::ColorSource::Escape)].is_set = false;
	/* Reset the default attributes.  Reset the alternate attribute because
	 * it's not a real attribute, but we need to treat it as one here. */
        m_defaults = m_color_defaults = basic_cell;
        /* Reset charset modes. */
        m_character_replacements[0] = VTE_CHARACTER_REPLACEMENT_NONE;
        m_character_replacements[1] = VTE_CHARACTER_REPLACEMENT_NONE;
        m_character_replacement = &m_character_replacements[0];
	/* Clear the scrollback buffers and reset the cursors. Switch to normal screen. */
	if (clear_history) {
                m_screen = &m_normal_screen;
                m_normal_screen.scroll_delta = m_normal_screen.insert_delta =
                        m_normal_screen.row_data->reset();
                m_normal_screen.cursor.row = m_normal_screen.insert_delta;
                m_normal_screen.cursor.col = 0;
                m_normal_screen.cursor_advanced_by_graphic_character = false;
                m_alternate_screen.scroll_delta = m_alternate_screen.insert_delta =
                        m_alternate_screen.row_data->reset();
                m_alternate_screen.cursor.row = m_alternate_screen.insert_delta;
                m_alternate_screen.cursor.col = 0;
                m_alternate_screen.cursor_advanced_by_graphic_character = false;
                /* Adjust the scrollbar to the new location. */
                /* Hack: force a change in scroll_delta even if the value remains, so that
                   vte_term_q_adj_val_changed() doesn't shortcut to no-op, see bug 730599. */
                m_screen->scroll_delta = -1;
                queue_adjustment_value_changed(m_screen->insert_delta);
		adjust_adjustments_full();
	}
        /* DECSCUSR cursor style */
        set_cursor_style(CursorStyle::eTERMINAL_DEFAULT);
	/* Reset restricted scrolling regions, leave insert mode, make
	 * the cursor visible again. */
        reset_scrolling_region();
        /* Reset the visual bits of selection on hard reset, see bug 789954. */
        if (clear_history) {
                deselect_all();
                stop_autoscroll();  /* Required before setting m_selecting to false, see #105. */
                m_selecting = FALSE;
                m_selecting_had_delta = FALSE;
                m_selection_origin = m_selection_last = { -1, -1, 1 };
                m_selection_resolved.clear();
        }

	/* Reset mouse motion events. */
        m_mouse_pressed_buttons = 0;
        m_mouse_handled_buttons = 0;
	m_mouse_last_position = vte::view::coords(-1, -1);
        m_mouse_smooth_scroll_x_delta = 0.;
        m_mouse_smooth_scroll_y_delta = 0.;
	/* Clear modifiers. */
	m_modifiers = 0;

        /* Reset the saved cursor. */
        save_cursor(&m_normal_screen);
        save_cursor(&m_alternate_screen);
        /* BiDi */
        m_bidi_rtl = FALSE;
	/* Cause everything to be redrawn (or cleared). */
	m_ringview.invalidate();
	invalidate_all();
	match_contents_clear();

        /* Reset XTerm window controls */
        m_xterm_wm_iconified = false;

        // Reset termprops
        reset_termprops();
}

void
Terminal::unset_pty(bool notify_widget)
{
        /* This may be called from inside or from widget,
         * and must notify the widget if not called from it.
         */

        disconnect_pty_read();
        disconnect_pty_write();

        /* Clear incoming and outgoing queues */
        m_input_bytes = 0;
        m_incoming_queue = {};
        _vte_byte_array_clear(m_outgoing);

        stop_processing(this); // FIXMEchpe only if m_incoming_queue.empty() !!!

        reset_decoder();

        m_pty.reset();

        if (notify_widget && widget())
                widget()->unset_pty();
}

bool
Terminal::set_pty(vte::base::Pty *new_pty)
{
        if (pty().get() == new_pty)
                return false;

        if (pty()) {
                unset_pty(false /* don't notify widget */);
        }

        m_pty = vte::base::make_ref(new_pty);
        if (!new_pty)
                return true;

        set_size(m_column_count, m_row_count, false);

        if (!pty()->set_utf8(primary_data_syntax() == DataSyntax::ECMA48_UTF8)) {
                // nothing we can do here
        }

        /* Open channels to listen for input on. */
        connect_pty_read();

        return true;
}

bool
Terminal::terminate_child() noexcept
{
	if (m_pty_pid == -1)
                return false;

        auto pgrp = getpgid(m_pty_pid);
        if (pgrp != -1 && pgrp != getpgid(getpid())) {
                kill(-pgrp, SIGHUP);
        }

        kill(m_pty_pid, SIGHUP);
        m_pty_pid = -1;

        return true;
}

void
Terminal::select_text(vte::grid::column_t start_col,
                                vte::grid::row_t start_row,
                                vte::grid::column_t end_col,
                                vte::grid::row_t end_row)
{
	deselect_all();

	m_selection_type = SelectionType::eCHAR;
	m_selecting_had_delta = true;
        m_selection_resolved.set ({ start_row, start_col },
                                  { end_row, end_col });
        widget_copy(vte::platform::ClipboardType::PRIMARY,
                    vte::platform::ClipboardFormat::TEXT);
	emit_selection_changed();

        invalidate_rows(start_row, end_row);
}

void
Terminal::select_empty(vte::grid::column_t col,
                                 vte::grid::row_t row)
{
        select_text(col, row, col, row);
}

void
Terminal::reset_update_rects()
{
#if VTE_GTK == 3
        g_array_set_size(m_update_rects, 0);
#endif
	m_invalidated_all = false;
}

static void
stop_processing(vte::terminal::Terminal* that)
{
	that->reset_update_rects();

        if (that->m_scheduler != nullptr) {
                _vte_scheduler_remove_callback (that->m_widget, that->m_scheduler);
                that->m_scheduler = nullptr;
        }
}

static void
add_process_timeout(vte::terminal::Terminal* that)
{
        if (that->m_scheduler == nullptr)
                that->m_scheduler = _vte_scheduler_add_callback (
                        that->m_widget, process_timeout, that);
}

void
Terminal::start_processing()
{
	if (!is_processing())
		add_process_timeout(this);
}

void
Terminal::emit_pending_signals()
{
        auto const freezer = vte::glib::FreezeObjectNotify{m_terminal};

	emit_adjustment_changed();

	if (m_pending_changes & vte::to_integral(PendingChanges::TERMPROPS)) {
                auto const n_props = m_termprops_dirty.size();
                auto changed_props = g_newa(int, n_props);

                auto n_changed_props = 0;
                auto any_ephemeral = false;
                for (auto i = 0u; i < n_props; ++i) {
                        if (m_termprops_dirty[i]) {
                                changed_props[n_changed_props++] = int(i);

                                if (unsigned(get_termprop_info(i)->flags()) &
                                    unsigned(vte::terminal::TermpropFlags::EPHEMERAL)) {
                                        any_ephemeral = true;
                                } else {
                                        m_termprops_dirty[i] = false;
                                }
                        }
                }

                widget()->notify_termprops_changed(changed_props, n_changed_props);

                // If there was (at least) an epehmeral termprop in this set,
                // reset its value(s).
                if (any_ephemeral) {
                        for (auto i = 0u; i < n_props; ++i) {
                                if (m_termprops_dirty[i]) {
                                        m_termprop_values[i] = {};
                                        m_termprops_dirty[i] = false;
                                }
                        }
                }
        }

        if (!m_no_legacy_signals) {
                // Emit deprecated signals and notify:: for deprecated properties,

                if (m_pending_changes & vte::to_integral(PendingChanges::TITLE)) {
                        _vte_debug_print(VTE_DEBUG_SIGNALS,
                                         "Emitting `window-title-changed'.\n");
                        g_signal_emit(freezer.get(), signals[SIGNAL_WINDOW_TITLE_CHANGED], 0);
                        g_object_notify_by_pspec(freezer.get(), pspecs[PROP_WINDOW_TITLE]);
                }

                if (m_pending_changes & vte::to_integral(PendingChanges::CWD)) {
                        _vte_debug_print(VTE_DEBUG_SIGNALS,
                                         "Emitting `current-directory-uri-changed'.\n");
                        g_signal_emit(freezer.get(), signals[SIGNAL_CURRENT_DIRECTORY_URI_CHANGED], 0);
                        g_object_notify_by_pspec(freezer.get(), pspecs[PROP_CURRENT_DIRECTORY_URI]);
                }

                if (m_pending_changes & vte::to_integral(PendingChanges::CWF)) {
                        _vte_debug_print(VTE_DEBUG_SIGNALS,
                                         "Emitting `current-file-uri-changed'.\n");
                        g_signal_emit(freezer.get(), signals[SIGNAL_CURRENT_FILE_URI_CHANGED], 0);
                        g_object_notify_by_pspec(freezer.get(), pspecs[PROP_CURRENT_FILE_URI]);
                }
        }

        m_pending_changes = 0;

	/* Flush any pending "inserted" signals. */

        if (m_cursor_moved_pending) {
                _vte_debug_print(VTE_DEBUG_SIGNALS,
                                 "Emitting `cursor-moved'.\n");
                g_signal_emit(freezer.get(), signals[SIGNAL_CURSOR_MOVED], 0);
                m_cursor_moved_pending = false;
        }
        if (m_text_modified_flag) {
                _vte_debug_print(VTE_DEBUG_SIGNALS,
                                 "Emitting buffered `text-modified'.\n");
                emit_text_modified();
                m_text_modified_flag = false;
        }
        if (m_text_inserted_flag) {
                _vte_debug_print(VTE_DEBUG_SIGNALS,
                                 "Emitting buffered `text-inserted'\n");
                emit_text_inserted();
                m_text_inserted_flag = false;
        }
        if (m_text_deleted_flag) {
                _vte_debug_print(VTE_DEBUG_SIGNALS,
                                 "Emitting buffered `text-deleted'\n");
                emit_text_deleted();
                m_text_deleted_flag = false;
	}
	if (m_contents_changed_pending) {
                /* Update hyperlink and dingus match set. */
		match_contents_clear();
		if (m_mouse_cursor_over_widget) {
                        hyperlink_hilite_update();
                        match_hilite_update();
		}

		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting `contents-changed'.\n");
		g_signal_emit(m_terminal, signals[SIGNAL_CONTENTS_CHANGED], 0);
		m_contents_changed_pending = false;
	}
        if (m_bell_pending) {
                auto const timestamp = g_get_monotonic_time();
                if ((timestamp - m_bell_timestamp) >= VTE_BELL_MINIMUM_TIME_DIFFERENCE) {
                        beep();
                        emit_bell();

                        m_bell_timestamp = timestamp;
                 }

                m_bell_pending = false;
        }

        if (m_eos_pending) {
                queue_eof();
                m_eos_pending = false;

                unset_pty();
        }
}

void
Terminal::time_process_incoming()
{
	g_timer_reset(process_timer);
	process_incoming();
	auto elapsed = g_timer_elapsed(process_timer, NULL) * 1000;
	gssize target = VTE_MAX_PROCESS_TIME / elapsed * m_input_bytes;
	m_max_input_bytes = (m_max_input_bytes + target) / 2;
}

bool
Terminal::process()
{
        if (pty()) {
                if (m_pty_input_active ||
                    m_pty_input_source == 0) {
                        m_pty_input_active = false;
                }
                connect_pty_read();
        }

        bool is_active = !m_incoming_queue.empty();
        if (is_active) {
                if (VTE_MAX_PROCESS_TIME) {
                        time_process_incoming();
                } else {
                        process_incoming();
                }
                m_input_bytes = 0;
        } else
                emit_pending_signals();

        return is_active;
}

static void
process_timeout (GtkWidget *widget,
                 gpointer data) noexcept
try
{
        auto that = reinterpret_cast<vte::terminal::Terminal*>(data);

        that->m_is_processing = true;
        auto is_active = that->process();
        that->m_is_processing = false;

        that->invalidate_dirty_rects_and_process_updates();
        that->emit_adjustment_changed();

        if (!is_active) {
                stop_processing(that);
                vte::base::Chunk::prune();
        }
}
catch (...)
{
        vte::log_exception();
}

bool
Terminal::invalidate_dirty_rects_and_process_updates()
{
        if (G_UNLIKELY(!widget_realized()))
                return false;

#if VTE_GTK == 3
	if (G_UNLIKELY (!m_update_rects->len))
		return false;

        auto region = cairo_region_create();
        auto n_rects = m_update_rects->len;
        for (guint i = 0; i < n_rects; i++) {
                cairo_rectangle_int_t *rect = &g_array_index(m_update_rects, cairo_rectangle_int_t, i);
                cairo_region_union_rectangle(region, rect);
	}
        g_array_set_size(m_update_rects, 0);
	m_invalidated_all = false;

        auto allocation = get_allocated_rect();
        cairo_region_translate(region,
                               allocation.x + m_border.left,
                               allocation.y + m_border.top);

	/* and perform the merge with the window visible area */
        gtk_widget_queue_draw_region(m_widget, region);
	cairo_region_destroy (region);

#elif VTE_GTK == 4
        if (G_UNLIKELY(!m_invalidated_all))
                return false;

        invalidate_all();
        gtk_widget_queue_draw(m_widget);
#endif

	return true;
}

bool
Terminal::write_contents_sync (GOutputStream *stream,
                               VteWriteFlags flags,
                               GCancellable *cancellable,
                               GError **error)
{
        return m_screen->row_data->write_contents(stream, flags, cancellable, error);
}

/*
 * Buffer search
 */

/* TODO Add properties & signals */

/*
 * Terminal::search_set_regex:
 * @regex: (allow-none): a #VteRegex, or %nullptr
 * @flags: PCRE2 match flags, or 0
 *
 * Sets the regex to search for. Unsets the search regex when passed %nullptr.
 */
bool
Terminal::search_set_regex (vte::base::RefPtr<vte::base::Regex>&& regex,
                            uint32_t flags)
{
        if (regex == m_search_regex &&
            flags == m_search_regex_match_flags)
                return false;

        m_search_regex = std::move(regex);
        m_search_regex_match_flags = flags;

	invalidate_all();

        return true;
}

bool
Terminal::search_set_wrap_around(bool wrap)
{
        if (wrap == m_search_wrap_around)
                return false;

        m_search_wrap_around = wrap;
        return true;
}

bool
Terminal::search_rows(pcre2_match_context_8 *match_context,
                      pcre2_match_data_8 *match_data,
                      vte::grid::row_t start_row,
                      vte::grid::row_t end_row,
                      bool backward)
{
	int start, end;
	long start_col, end_col;
	VteCharAttributes *ca;
        VteCharAttrList *attrs;

        auto row_text = g_string_new(nullptr);
        get_text(start_row, 0,
                 end_row, 0,
                 false /* block */,
                 false /* preserve_empty */,
                 row_text,
                 nullptr);

        int (* match_fn) (const pcre2_code_8 *,
                          PCRE2_SPTR8, PCRE2_SIZE, PCRE2_SIZE, uint32_t,
                          pcre2_match_data_8 *, pcre2_match_context_8 *);
        gsize *ovector, so, eo;
        int r;

        if (m_search_regex->jited())
                match_fn = pcre2_jit_match_8;
        else
                match_fn = pcre2_match_8;

        r = match_fn(m_search_regex->code(),
                     (PCRE2_SPTR8)row_text->str, row_text->len , /* subject, length */
                     0, /* start offset */
                     m_search_regex_match_flags |
                     PCRE2_NO_UTF_CHECK | PCRE2_NOTEMPTY | PCRE2_PARTIAL_SOFT /* FIXME: HARD? */,
                     match_data,
                     match_context);

        if (r == PCRE2_ERROR_NOMATCH) {
                g_string_free (row_text, TRUE);
                return false;
        }
        // FIXME: handle partial matches (PCRE2_ERROR_PARTIAL)
        if (r < 0) {
                g_string_free (row_text, TRUE);
                return false;
        }

        ovector = pcre2_get_ovector_pointer_8(match_data);
        so = ovector[0];
        eo = ovector[1];
        if (G_UNLIKELY(so == PCRE2_UNSET || eo == PCRE2_UNSET)) {
                g_string_free (row_text, TRUE);
                return false;
        }

        start = so;
        end = eo;

	/* Fetch text again, with attributes */
        g_string_truncate(row_text, 0);
	attrs = &m_search_attrs;
	get_text(start_row, 0,
                 end_row, 0,
                 false /* block */,
                 false /* preserve_empty */,
                 row_text,
                 attrs);

	ca = vte_char_attr_list_get(attrs, start);
	start_row = ca->row;
	start_col = ca->column;
	ca = vte_char_attr_list_get(attrs, end - 1);
	end_row = ca->row;
        end_col = ca->column + ca->columns;

	g_string_free (row_text, TRUE);

	select_text(start_col, start_row, end_col, end_row);
	/* Quite possibly the math here should not access the scroll values directly... */
        auto const value = m_screen->scroll_delta;
        auto const page_size = m_row_count;
	if (backward) {
		if (end_row < value || end_row > value + page_size - 1)
			queue_adjustment_value_changed_clamped(end_row - page_size + 1);
	} else {
		if (start_row < value || start_row > value + page_size - 1)
			queue_adjustment_value_changed_clamped(start_row);
	}

	return true;
}

bool
Terminal::search_rows_iter(pcre2_match_context_8 *match_context,
                                     pcre2_match_data_8 *match_data,
                                     vte::grid::row_t start_row,
                                     vte::grid::row_t end_row,
                                     bool backward)
{
	long iter_start_row, iter_end_row;

	if (backward) {
		iter_start_row = end_row;
		while (iter_start_row > start_row) {
			iter_end_row = iter_start_row;

			do {
				iter_start_row--;
			} while (m_screen->row_data->is_soft_wrapped(iter_start_row - 1));

			if (search_rows(match_context, match_data,
                                        iter_start_row, iter_end_row, backward))
				return true;
		}
	} else {
		iter_end_row = start_row;
		while (iter_end_row < end_row) {
			iter_start_row = iter_end_row;

			do {
				iter_end_row++;
			} while (m_screen->row_data->is_soft_wrapped(iter_end_row - 1));

			if (search_rows(match_context, match_data,
                                        iter_start_row, iter_end_row, backward))
				return true;
		}
	}

	return false;
}

bool
Terminal::search_find (bool backward)
{
        vte::grid::row_t buffer_start_row, buffer_end_row;
        vte::grid::row_t last_start_row, last_end_row;
        bool match_found = true;

        if (!m_search_regex)
                return false;

	/* TODO
	 * Currently We only find one result per extended line, and ignore columns
	 * Moreover, the whole search thing is implemented very inefficiently.
	 */

        auto match_context = create_match_context();
        auto match_data = vte::take_freeable(pcre2_match_data_create_8(256 /* should be plenty */,
                                                                       nullptr /* general context */));

	buffer_start_row = m_screen->row_data->delta();
	buffer_end_row = m_screen->row_data->next();

        if (!m_selection_resolved.empty()) {
                last_start_row = m_selection_resolved.start_row();
                last_end_row = m_selection_resolved.end_row() + 1;
	} else {
		last_start_row = m_screen->scroll_delta + m_row_count;
		last_end_row = m_screen->scroll_delta;
	}
	last_start_row = MAX (buffer_start_row, last_start_row);
	last_end_row = MIN (buffer_end_row, last_end_row);

	/* If search fails, we make an empty selection at the last searched
	 * position... */
	if (backward) {
		if (search_rows_iter(match_context.get(), match_data.get(),
                                      buffer_start_row, last_start_row, backward))
			goto found;
		if (m_search_wrap_around &&
		    search_rows_iter(match_context.get(), match_data.get(),
                                      last_end_row, buffer_end_row, backward))
			goto found;
                if (!m_selection_resolved.empty()) {
			if (m_search_wrap_around)
                            select_empty(m_selection_resolved.start_column(), m_selection_resolved.start_row());
			else
			    select_empty(-1, buffer_start_row - 1);
		}
                match_found = false;
	} else {
		if (search_rows_iter(match_context.get(), match_data.get(),
                                      last_end_row, buffer_end_row, backward))
			goto found;
		if (m_search_wrap_around &&
		    search_rows_iter(match_context.get(), match_data.get(),
                                      buffer_start_row, last_start_row, backward))
			goto found;
                if (!m_selection_resolved.empty()) {
			if (m_search_wrap_around)
                                select_empty(m_selection_resolved.end_column(), m_selection_resolved.end_row());
			else
                                select_empty(0, buffer_end_row);
		}
                match_found = false;
	}

 found:

	return match_found;
}

/*
 * Terminal::set_input_enabled:
 * @enabled: whether to enable user input
 *
 * Enables or disables user input. When user input is disabled,
 * the terminal's child will not receive any key press, or mouse button
 * press or motion events sent to it.
 *
 * Returns: %true iff the setting changed
 */
bool
Terminal::set_input_enabled (bool enabled)
{
        if (enabled == m_input_enabled)
                return false;

        m_input_enabled = enabled;

        auto context = gtk_widget_get_style_context(m_widget);

        /* FIXME: maybe hide cursor when input disabled, too? */

        if (enabled) {
                if (m_has_focus)
                        widget()->im_focus_in();

                gtk_style_context_remove_class (context, VTE_STYLE_CLASS_READ_ONLY);
        } else {
                im_reset();
                if (m_has_focus)
                        widget()->im_focus_out();

                disconnect_pty_write();
                _vte_byte_array_clear(m_outgoing);

                gtk_style_context_add_class (context, VTE_STYLE_CLASS_READ_ONLY);
        }

        return true;
}

std::optional<std::vector<char32_t>>
Terminal::process_word_char_exceptions(std::string_view str_view) const noexcept
{
        auto str = str_view.data();

        auto array = std::vector<char32_t>{};
        array.reserve(g_utf8_strlen(str, -1));

        for (auto const* p = str; *p; p = g_utf8_next_char(p)) {
                auto const c = g_utf8_get_char(p);

                /* For forward compatibility reasons, we skip
                 * characters that aren't supposed to be here,
                 * instead of erroring out.
                 */
                /* '-' must only be used*  at the start of the string */
                if (c == (gunichar)'-' && p != str)
                        continue;
                if (!g_unichar_isgraph(c))
                        continue;
                if (g_unichar_isspace(c))
                        continue;
                if (g_unichar_isalnum(c))
                        continue;

                array.push_back(c);
        }

        /* Sort the result since we want to use bsearch on it */
        std::sort(std::begin(array), std::end(array));

        /* Check that no character occurs twice */
        for (size_t i = 1; i < array.size(); ++i) {
                if (array[i-1] != array[i])
                        continue;

                return std::nullopt;
        }

#if 0
        /* Debug */
        for (auto const c : array) {
                char utf[7];
                utf[g_unichar_to_utf8(c, utf)] = '\0';
                g_printerr("Word char exception: U+%04X %s\n", c, utf);
        }
#endif

        return array;
}

/*
 * Terminal::set_word_char_exceptions:
 * @exceptions: a string of ASCII punctuation characters, or %nullptr
 *
 * With this function you can provide a set of characters which will
 * be considered parts of a word when doing word-wise selection, in
 * addition to the default which only considers alphanumeric characters
 * part of a word.
 *
 * The characters in @exceptions must be non-alphanumeric, each character
 * must occur only once, and if @exceptions contains the character
 * U+002D HYPHEN-MINUS, it must be at the start of the string.
 *
 * Use %nullptr to reset the set of exception characters to the default.
 *
 * Returns: %true if the word char exceptions changed
 */
bool
Terminal::set_word_char_exceptions(std::optional<std::string_view> stropt)
{
        if (auto array = process_word_char_exceptions(stropt ? stropt.value() : WORD_CHAR_EXCEPTIONS_DEFAULT)) {
                m_word_char_exceptions = *array;
                return true;
        }

        return false;
}

void
Terminal::set_clear_background(bool setting)
{
        if (m_clear_background == setting)
                return;

        m_clear_background = setting;
        invalidate_all();
}

} // namespace terminal
} // namespace vte

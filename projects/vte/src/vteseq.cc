/*
 * Copyright © 2001-2004 Red Hat, Inc.
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2008-2018 Christian Persch
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

#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#if __has_include(<sys/syslimits.h>)
#include <sys/syslimits.h>
#endif

#include <glib.h>

#include <vte/vte.h>
#include "vteinternal.hh"
#include "vtegtk.hh"
#include "caps.hh"
#include "debug.h"
#include "sgr.hh"

#define BEL_C0 "\007"
#define ST_C0 _VTE_CAP_ST

#include <algorithm>

using namespace std::literals;

enum {
        VTE_XTERM_WM_RESTORE_WINDOW = 1,
        VTE_XTERM_WM_MINIMIZE_WINDOW = 2,
        VTE_XTERM_WM_SET_WINDOW_POSITION = 3,
        VTE_XTERM_WM_SET_WINDOW_SIZE_PIXELS = 4,
        VTE_XTERM_WM_RAISE_WINDOW = 5,
        VTE_XTERM_WM_LOWER_WINDOW = 6,
        VTE_XTERM_WM_REFRESH_WINDOW = 7,
        VTE_XTERM_WM_SET_WINDOW_SIZE_CELLS = 8,
        VTE_XTERM_WM_MAXIMIZE_WINDOW = 9,
        VTE_XTERM_WM_FULLSCREEN_WINDOW = 10,
        VTE_XTERM_WM_GET_WINDOW_STATE = 11,
        VTE_XTERM_WM_GET_WINDOW_POSITION = 13,
        VTE_XTERM_WM_GET_WINDOW_SIZE_PIXELS = 14,
        VTE_XTERM_WM_GET_WINDOW_SIZE_CELLS = 18,
        VTE_XTERM_WM_GET_SCREEN_SIZE_CELLS = 19,
        VTE_XTERM_WM_GET_ICON_TITLE = 20,
        VTE_XTERM_WM_GET_WINDOW_TITLE = 21,
        VTE_XTERM_WM_TITLE_STACK_PUSH = 22,
        VTE_XTERM_WM_TITLE_STACK_POP = 23,
};

inline consteval int firmware_version() noexcept
{
        return (VTE_MAJOR_VERSION * 100 + VTE_MINOR_VERSION) * 100 + VTE_MICRO_VERSION;
}

void
vte::parser::Sequence::print() const noexcept
{
#if VTE_DEBUG
        auto c = m_seq != nullptr ? terminator() : 0;
        char c_buf[7];
        g_snprintf(c_buf, sizeof(c_buf), "%lc", c);
        g_printerr("%s:%s [%s]", type_string(), command_string(),
                   g_unichar_isprint(c) ? c_buf : _vte_debug_sequence_to_string(c_buf, -1));
        if (m_seq != nullptr && m_seq->n_args > 0) {
                g_printerr("[ ");
                for (unsigned int i = 0; i < m_seq->n_args; i++) {
                        if (i > 0)
                                g_print(", ");
                        g_printerr("%d", vte_seq_arg_value(m_seq->args[i]));
                }
                g_printerr(" ]");
        }
        if (m_seq->type == VTE_SEQ_OSC || m_seq->type == VTE_SEQ_DCS) {
                char* str = string_param();
                g_printerr(" \"%s\"", str);
                g_free(str);
        }
        g_printerr("\n");
#endif
}

char const*
vte::parser::Sequence::type_string() const
{
        if (G_UNLIKELY(m_seq == nullptr))
                return "(nil)";

        switch (type()) {
        case VTE_SEQ_NONE:    return "NONE";
        case VTE_SEQ_IGNORE:  return "IGNORE";
        case VTE_SEQ_GRAPHIC: return "GRAPHIC";
        case VTE_SEQ_CONTROL: return "CONTROL";
        case VTE_SEQ_ESCAPE:  return "ESCAPE";
        case VTE_SEQ_CSI:     return "CSI";
        case VTE_SEQ_DCS:     return "DCS";
        case VTE_SEQ_OSC:     return "OSC";
        case VTE_SEQ_SCI:     return "SCI";
        case VTE_SEQ_APC:     return "APC";
        case VTE_SEQ_PM:      return "PM";
        case VTE_SEQ_SOS:     return "SOS";
        default:
                g_assert(false);
                return nullptr;
        }
}

char const*
vte::parser::Sequence::command_string() const
{
        if (G_UNLIKELY(m_seq == nullptr))
                return "(nil)";

        switch (command()) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd;
#define _VTE_NOP(cmd)
#include "parser-cmd.hh"
#undef _VTE_CMD
#undef _VTE_NOP
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "NOP OR UNKOWN(%u)", command());
                return buf;
        }
}

// FIXMEchpe optimise this
std::string
vte::parser::Sequence::string_utf8() const noexcept
{
        std::string str;

        size_t len;
        auto buf = vte_seq_string_get(&m_seq->arg_str, &len);

        char u[6];
        for (size_t i = 0; i < len; ++i) {
                auto ulen = g_unichar_to_utf8(buf[i], u);
                str.append((char const*)u, ulen);
        }

        return str;
}

/* A couple are duplicated from vte.c, to keep them static... */

/* Check how long a string of unichars is.  Slow version. */
static gsize
vte_unichar_strlen(gunichar const* c)
{
	gsize i;
	for (i = 0; c[i] != 0; i++) ;
	return i;
}

/* Convert a wide character string to a multibyte string */
/* Simplified from glib's g_ucs4_to_utf8() to simply allocate the maximum
 * length instead of walking the input twice.
 */
char*
vte::parser::Sequence::ucs4_to_utf8(gunichar const* str,
                                    ssize_t len) const noexcept
{
        if (len < 0)
                len = vte_unichar_strlen(str);
        auto outlen = (len * VTE_UTF8_BPC) + 1;

        auto result = (char*)g_try_malloc(outlen);
        if (result == nullptr)
                return nullptr;

        auto end = str + len;
        auto p = result;
        for (auto i = str; i < end; i++)
                p += g_unichar_to_utf8(*i, p);
        *p = '\0';

        return result;
}

namespace vte {
namespace terminal {

using namespace vte::color_palette;
using namespace vte::osc_colors;

/* Emit a "bell" signal. */
void
Terminal::emit_bell()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `bell'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_BELL], 0);
}

/* Emit a "resize-window" signal.  (Grid size.) */
void
Terminal::emit_resize_window(guint columns,
                             guint rows)
{
        // Ignore resizes with excessive number of rows or columns,
        // see https://gitlab.gnome.org/GNOME/vte/-/issues/2786
        if (columns < VTE_MIN_GRID_WIDTH ||
            columns > 511 ||
            rows < VTE_MIN_GRID_HEIGHT ||
            rows > 511)
                return;

        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `resize-window' %d columns %d rows.\n",
                         columns, rows);
        g_signal_emit(m_terminal, signals[SIGNAL_RESIZE_WINDOW], 0, columns, rows);
}

/* Some common functions */

/* In Xterm, upon printing a character in the last column the cursor doesn't
 * advance.  It's special cased that printing the following letter will first
 * wrap to the next row.
 *
 * As a rule of thumb, escape sequences that move the cursor (e.g. cursor up)
 * or immediately update the visible contents (e.g. clear in line) disable
 * this special mode, whereas escape sequences with no immediate visible
 * effect (e.g. color change) leave this special mode on.  There are
 * exceptions of course (e.g. scroll up).
 *
 * In VTE, a different technical approach is used.  The cursor is advanced to
 * the invisible column on the right, but it's set back to the visible
 * rightmost column whenever necessary (that is, before handling any of the
 * sequences that disable the special cased mode in xterm).
 *
 * Similarly, if a right margin is set up and the cursor moved just beyond
 * that margin due to a graphic character (as opposed to a cursor moving
 * escape sequence) then set back the cursor by one column.
 *
 * See https://gitlab.gnome.org/GNOME/vte/-/issues/2108
 * and https://gitlab.gnome.org/GNOME/vte/-/issues/2677
 */
void
Terminal::maybe_retreat_cursor()
{
        m_screen->cursor.col = get_xterm_cursor_column();
        m_screen->cursor_advanced_by_graphic_character = false;
}

void
Terminal::home_cursor()
{
        set_cursor_coords(0, 0);
}

void
Terminal::clear_screen()
{
        maybe_retreat_cursor();

        auto row = get_xterm_cursor_row();
        auto initial = m_screen->row_data->next();
	/* Add a new screen's worth of rows. */
        for (auto i = 0; i < m_row_count; i++)
                ring_append(true);
	/* Move the cursor and insertion delta to the first line in the
	 * newly-cleared area and scroll if need be. */
        m_screen->insert_delta = initial;
        m_screen->cursor.row = row + m_screen->insert_delta;
        m_screen->cursor_advanced_by_graphic_character = false;
        adjust_adjustments();
	/* Redraw everything. */
        invalidate_all();
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear the current line. */
void
Terminal::clear_current_line()
{
	VteRowData *rowdata;

        maybe_retreat_cursor();

        /* If the cursor's row is covered by the ring, clear data in the row
	 * which corresponds to the cursor. */
        if (long(m_screen->row_data->next()) > m_screen->cursor.row) {
		/* Get the data for the row which the cursor points to. */
                rowdata = m_screen->row_data->index_writable(m_screen->cursor.row);
		g_assert(rowdata != NULL);
		/* Remove it. */
		_vte_row_data_shrink (rowdata, 0);
		/* Add enough cells to the end of the line to fill out the row. */
                _vte_row_data_fill (rowdata, &m_color_defaults, m_column_count);
                set_hard_wrapped(m_screen->cursor.row);
                rowdata->attr.bidi_flags = get_bidi_flags();
                /* Repaint this row's paragraph (might need to extend upwards). */
                invalidate_row_and_context(m_screen->cursor.row);
	}

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear above the current line. */
void
Terminal::clear_above_current()
{
        /* Make the line just above the writable area hard wrapped. */
        if (m_screen->insert_delta > long(m_screen->row_data->delta())) {
                set_hard_wrapped(m_screen->insert_delta - 1);
        }
        /* Clear data in all the writable rows above (excluding) the cursor's. */
        for (auto i = m_screen->insert_delta; i < m_screen->cursor.row; i++) {
                if (long(m_screen->row_data->next()) > i) {
			/* Get the data for the row we're erasing. */
                        auto rowdata = m_screen->row_data->index_writable(i);
			g_assert(rowdata != NULL);
			/* Remove it. */
			_vte_row_data_shrink (rowdata, 0);
			/* Add new cells until we fill the row. */
                        _vte_row_data_fill (rowdata, &m_color_defaults, m_column_count);
                        set_hard_wrapped(i);
                        rowdata->attr.bidi_flags = get_bidi_flags();
		}
	}
        /* Repaint the cleared area. No need to extend, set_hard_wrapped() took care of
         * invalidating the context lines if necessary. */
        invalidate_rows(m_screen->insert_delta, m_screen->cursor.row - 1);
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

void
Terminal::restore_cursor()
{
        restore_cursor(m_screen);
}

void
Terminal::save_cursor()
{
        save_cursor(m_screen);
}

/* Switch to normal screen. */
void
Terminal::switch_normal_screen()
{
        switch_screen(&m_normal_screen);
}

void
Terminal::switch_screen(VteScreen *new_screen)
{
        /* if (new_screen == m_screen) return; ? */

        /* The two screens use different hyperlink pools, so carrying on the idx
         * wouldn't make sense and could lead to crashes.
         * Ideally we'd carry the target URI itself, but I'm just lazy.
         * Also, run a GC before we switch away from that screen. */
        m_hyperlink_hover_idx = m_screen->row_data->get_hyperlink_at_position(-1, -1, true, NULL);
        g_assert (m_hyperlink_hover_idx == 0);
        m_hyperlink_hover_uri = NULL;
        emit_hyperlink_hover_uri_changed(NULL);  /* FIXME only emit if really changed */
        m_defaults.attr.hyperlink_idx = m_screen->row_data->get_hyperlink_idx(NULL);
        g_assert (m_defaults.attr.hyperlink_idx == 0);

        /* cursor.row includes insert_delta, adjust accordingly */
        auto cr = m_screen->cursor.row - m_screen->insert_delta;
        auto cc = m_screen->cursor.col;
        auto cadv = m_screen->cursor_advanced_by_graphic_character;
        m_screen = new_screen;
        m_screen->cursor.row = cr + m_screen->insert_delta;
        m_screen->cursor.col = cc;
        m_screen->cursor_advanced_by_graphic_character = cadv;

        /* Make sure the ring is large enough */
        ensure_row();
}

/* Switch to alternate screen. */
void
Terminal::switch_alternate_screen()
{
        switch_screen(&m_alternate_screen);
}

void
Terminal::set_mode_ecma(vte::parser::Sequence const& seq,
                                  bool set) noexcept
{
        auto const n_params = seq.size();
        for (unsigned int i = 0; i < n_params; i = seq.next(i)) {
                auto const param = seq.collect1(i);
                auto const mode = m_modes_ecma.mode_from_param(param);

                _vte_debug_print(VTE_DEBUG_MODES,
                                 "Mode %d (%s) %s\n",
                                 param, m_modes_ecma.mode_to_cstring(mode),
                                 set ? "set" : "reset");

                if (mode < 0)
                        continue;

                m_modes_ecma.set(mode, set);

                if (mode == m_modes_ecma.eBDSM) {
                        _vte_debug_print(VTE_DEBUG_BIDI,
                                         "BiDi %s mode\n",
                                         set ? "implicit" : "explicit");
                        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_IMPLICIT);
                }
        }
}

void
Terminal::update_mouse_protocol() noexcept
{
        if (m_modes_private.XTERM_MOUSE_ANY_EVENT())
                m_mouse_tracking_mode = MouseTrackingMode::eALL_MOTION_TRACKING;
        else if (m_modes_private.XTERM_MOUSE_BUTTON_EVENT())
                m_mouse_tracking_mode = MouseTrackingMode::eCELL_MOTION_TRACKING;
        else if (m_modes_private.XTERM_MOUSE_VT220_HIGHLIGHT())
                m_mouse_tracking_mode = MouseTrackingMode::eHILITE_TRACKING;
        else if (m_modes_private.XTERM_MOUSE_VT220())
                m_mouse_tracking_mode = MouseTrackingMode::eSEND_XY_ON_BUTTON;
        else if (m_modes_private.XTERM_MOUSE_X10())
                m_mouse_tracking_mode = MouseTrackingMode::eSEND_XY_ON_CLICK;
        else
                m_mouse_tracking_mode = MouseTrackingMode::eNONE;

        m_mouse_smooth_scroll_x_delta = 0.0;
        m_mouse_smooth_scroll_y_delta = 0.0;

        /* Mouse pointer might change */
        apply_mouse_cursor();

        _vte_debug_print(VTE_DEBUG_MODES,
                         "Mouse protocol is now %d\n", (int)m_mouse_tracking_mode);
}

void
Terminal::set_mode_private(int mode,
                           bool set) noexcept
{
        /* Pre actions */
        switch (mode) {
        default:
                break;
        }

        m_modes_private.set(mode, set);

        /* Post actions */
        switch (mode) {
        case vte::terminal::modes::Private::eDEC_132_COLUMN:
                /* DECCOLM: set/reset to 132/80 columns mode, clear screen and cursor home */
                // FIXMEchpe don't do clear screen if DECNCSM is set
                /* FIXMEchpe!!!
                 * Changing this mode resets the top, bottom, left, right margins;
                 * clears the screen (unless DECNCSM is set); resets DECLRMM; and clears
                 * the status line if host-writable.
                 */
                if (m_modes_private.XTERM_DECCOLM()) {
                        emit_resize_window(set ? 132 : 80, m_row_count);
                        m_scrolling_region.reset();
                        clear_screen();
                        home_cursor();
                }
                break;

        case vte::terminal::modes::Private::eDEC_REVERSE_IMAGE:
                invalidate_all();
                break;

        case vte::terminal::modes::Private::eDEC_ORIGIN:
                /* Reposition the cursor in its new home position. */
                home_cursor();
                break;

        case vte::terminal::modes::Private::eDEC_TEXT_CURSOR:
                /* No need to invalidate the cursor here, this is done
                 * in process_incoming().
                 */
                break;

        case vte::terminal::modes::Private::eDECLRMM:
                if (!set) {
                        m_scrolling_region.reset_horizontal();
                }
                break;

        case vte::terminal::modes::Private::eXTERM_ALTBUF:
                [[fallthrough]];
        case vte::terminal::modes::Private::eXTERM_OPT_ALTBUF:
                [[fallthrough]];
        case vte::terminal::modes::Private::eXTERM_OPT_ALTBUF_SAVE_CURSOR:
                if (set) {
                        if (mode == vte::terminal::modes::Private::eXTERM_OPT_ALTBUF_SAVE_CURSOR)
                                save_cursor();

                        switch_alternate_screen();

                        /* Clear the alternate screen */
                        if (mode == vte::terminal::modes::Private::eXTERM_OPT_ALTBUF_SAVE_CURSOR)
                                clear_screen();
                } else {
                        if (mode == vte::terminal::modes::Private::eXTERM_OPT_ALTBUF &&
                            m_screen == &m_alternate_screen)
                                clear_screen();

                        switch_normal_screen();

                        if (mode == vte::terminal::modes::Private::eXTERM_OPT_ALTBUF_SAVE_CURSOR)
                                restore_cursor();
                }

                /* Reset scrollbars and repaint everything. */
                queue_adjustment_value_changed(m_screen->scroll_delta);
                set_scrollback_lines(m_scrollback_lines);
                queue_contents_changed();
                invalidate_all();
                break;

        case vte::terminal::modes::Private::eXTERM_SAVE_CURSOR:
                if (set)
                        save_cursor();
                else
                        restore_cursor();
                break;

        case vte::terminal::modes::Private::eXTERM_MOUSE_X10:
        case vte::terminal::modes::Private::eXTERM_MOUSE_VT220:
        case vte::terminal::modes::Private::eXTERM_MOUSE_VT220_HIGHLIGHT:
        case vte::terminal::modes::Private::eXTERM_MOUSE_BUTTON_EVENT:
        case vte::terminal::modes::Private::eXTERM_MOUSE_ANY_EVENT:
        case vte::terminal::modes::Private::eXTERM_MOUSE_EXT:
        case vte::terminal::modes::Private::eXTERM_MOUSE_EXT_SGR:
                update_mouse_protocol();
                break;

        case vte::terminal::modes::Private::eXTERM_FOCUS:
                if (set)
                        feed_focus_event_initial();
                break;

        case vte::terminal::modes::Private::eVTE_BIDI_BOX_MIRROR:
                _vte_debug_print(VTE_DEBUG_BIDI,
                                 "BiDi box drawing mirroring %s\n",
                                 set ? "enabled" : "disabled");
                maybe_apply_bidi_attributes(VTE_BIDI_FLAG_BOX_MIRROR);
                break;

        case vte::terminal::modes::Private::eVTE_BIDI_AUTO:
                        _vte_debug_print(VTE_DEBUG_BIDI,
                                         "BiDi dir autodetection %s\n",
                                         set ? "enabled" : "disabled");
                maybe_apply_bidi_attributes(VTE_BIDI_FLAG_AUTO);
                break;

        default:
                break;
        }
}

void
Terminal::set_mode_private(vte::parser::Sequence const& seq,
                                     bool set) noexcept
{
        auto const n_params = seq.size();
        for (unsigned int i = 0; i < n_params; i = seq.next(i)) {
                auto const param = seq.collect1(i);
                auto const mode = m_modes_private.mode_from_param(param);

                _vte_debug_print(VTE_DEBUG_MODES,
                                 "Private mode %d (%s) %s\n",
                                 param, m_modes_private.mode_to_cstring(mode),
                                 set ? "set" : "reset");

                if (mode < 0)
                        continue;

                set_mode_private(mode, set);
        }
}

void
Terminal::save_mode_private(vte::parser::Sequence const& seq,
                                      bool save) noexcept
{
        auto const n_params = seq.size();
        for (unsigned int i = 0; i < n_params; i = seq.next(i)) {
                auto const param = seq.collect1(i);
                auto const mode = m_modes_private.mode_from_param(param);

                if (mode < 0) {
                        _vte_debug_print(VTE_DEBUG_MODES,
                                         "Saving private mode %d (%s)\n",
                                         param, m_modes_private.mode_to_cstring(mode));
                        continue;
                }

                if (save) {
                        _vte_debug_print(VTE_DEBUG_MODES,
                                         "Saving private mode %d (%s) is %s\n",
                                         param, m_modes_private.mode_to_cstring(mode),
                                         m_modes_private.get(mode) ? "set" : "reset");

                        m_modes_private.push_saved(mode);
                } else {
                        bool const set = m_modes_private.pop_saved(mode);

                        _vte_debug_print(VTE_DEBUG_MODES,
                                         "Restoring private mode %d (%s) to %s\n",
                                         param, m_modes_private.mode_to_cstring(mode),
                                         set ? "set" : "reset");

                        set_mode_private(mode, set);
                }
        }
}

void
Terminal::set_character_replacement(unsigned slot)
{
        g_assert(slot < G_N_ELEMENTS(m_character_replacements));
        m_character_replacement = &m_character_replacements[slot];
}

template<class B>
static void
append_attr_sgr_params(VteCellAttr const& attr,
                       B&& builder)
{
        // The VT520/525 manual shows an example response from DECRQSS SGR,
        // which start with 0 (reset-all).
        builder.append_param(VTE_SGR_RESET_ALL);

        if (attr.bold())
                builder.append_param(VTE_SGR_SET_BOLD);
        if (attr.dim())
                builder.append_param(VTE_SGR_SET_DIM);
        if (attr.italic())
                builder.append_param(VTE_SGR_SET_ITALIC);
        if (auto v = attr.underline()) {
                if (v == 1)
                        builder.append_param(VTE_SGR_SET_UNDERLINE);
                else if (v == 2)
                        builder.append_param(VTE_SGR_SET_UNDERLINE_DOUBLE);
                else
                        builder.append_subparams({VTE_SGR_SET_UNDERLINE, int(v)});
        }
        if (attr.blink())
                builder.append_param(VTE_SGR_SET_BLINK);
        if (attr.reverse())
                builder.append_param(VTE_SGR_SET_REVERSE);
        if (attr.invisible())
                builder.append_param(VTE_SGR_SET_INVISIBLE);
        if (attr.strikethrough())
                builder.append_param(VTE_SGR_SET_STRIKETHROUGH);
        if (attr.overline())
                builder.append_param(VTE_SGR_SET_OVERLINE);

        auto append_color = [&](uint32_t cidx,
                                unsigned default_cidx,
                                int sgr,
                                int legacy_sgr_first,
                                int legacy_sgr_last,
                                int legacy_sgr_bright_first,
                                int legacy_sgr_bright_last,
                                int redbits,
                                int greenbits,
                                int bluebits) constexpr noexcept -> void {
                if (cidx == default_cidx)
                        return;

                if (cidx & VTE_RGB_COLOR_MASK(redbits, greenbits, bluebits)) {
                        // Truecolour
                        auto const red   = VTE_RGB_COLOR_GET_COMPONENT(cidx, greenbits + bluebits, redbits);
                        auto const green = VTE_RGB_COLOR_GET_COMPONENT(cidx, bluebits, greenbits);
                        auto const blue  = VTE_RGB_COLOR_GET_COMPONENT(cidx, 0, bluebits);

                        builder.append_subparams({sgr,
                                        vte::parser::detail::VTE_SGR_COLOR_SPEC_RGB,
                                        -1 /* colourspace */,
                                        int(red),
                                        int(green),
                                        int(blue)});
                        return;
                }

                if (cidx & VTE_DIM_COLOR)
                        cidx &= ~VTE_DIM_COLOR;

                if (cidx & VTE_LEGACY_COLORS_OFFSET) {
                        // Legacy colour

                        cidx -= VTE_LEGACY_COLORS_OFFSET;
                        if (cidx < unsigned(legacy_sgr_last - legacy_sgr_first + 1)) {
                                builder.append_param(legacy_sgr_first + cidx);
                                return;
                        }
                        if (cidx >= VTE_COLOR_BRIGHT_OFFSET) {
                                cidx -= VTE_COLOR_BRIGHT_OFFSET;
                                if (cidx < unsigned(legacy_sgr_bright_last - legacy_sgr_bright_first + 1)) {
                                        builder.append_param(legacy_sgr_bright_first + cidx);
                                        return;
                                }
                        }

                        return;
                }

                // Palette colour

                if (cidx < 256) {
                        builder.append_subparams({sgr,
                                        vte::parser::detail::VTE_SGR_COLOR_SPEC_LEGACY,
                                        int(cidx)});
                        return;
                }
        };

        append_color(attr.fore(),
                     VTE_DEFAULT_FG,
                     VTE_SGR_SET_FORE_SPEC,
                     VTE_SGR_SET_FORE_LEGACY_START,
                     VTE_SGR_SET_FORE_LEGACY_END,
                     VTE_SGR_SET_FORE_LEGACY_BRIGHT_START,
                     VTE_SGR_SET_FORE_LEGACY_BRIGHT_END,
                     8, 8, 8);
        append_color(attr.back(),
                     VTE_DEFAULT_BG,
                     VTE_SGR_SET_BACK_SPEC,
                     VTE_SGR_SET_BACK_LEGACY_START,
                     VTE_SGR_SET_BACK_LEGACY_END,
                     VTE_SGR_SET_BACK_LEGACY_BRIGHT_START,
                     VTE_SGR_SET_BACK_LEGACY_BRIGHT_END,
                     8, 8, 8);
        append_color(attr.deco(),
                     VTE_DEFAULT_FG,
                     VTE_SGR_SET_DECO_SPEC,
                     -1, -1, -1, -1,
                     4, 5, 5);
}

template<class B>
static void
append_attr_decsgr_params(VteCellAttr const& attr,
                          B&& builder)
{
        // The VT520/525 manual shows an example response from DECRQSS SGR,
        // which start with 0 (reset-all); do the same for DECSGR.
        builder.append_param(VTE_DECSGR_RESET_ALL);

        if (attr.overline())
                builder.append_param(VTE_DECSGR_SET_OVERLINE);
}

/* Clear from the cursor position (inclusive!) to the beginning of the line. */
void
Terminal::clear_to_bol()
{
        maybe_retreat_cursor();

	/* Get the data for the row which the cursor points to. */
	auto rowdata = ensure_row();
        /* Clean up Tab/CJK fragments. */
        cleanup_fragments(0, m_screen->cursor.col + 1);
	/* Clear the data up to the current column with the default
	 * attributes.  If there is no such character cell, we need
	 * to add one. */
        vte::grid::column_t i;
        for (i = 0; i <= m_screen->cursor.col; i++) {
                if (i < (glong) _vte_row_data_length (rowdata)) {
			/* Muck with the cell in this location. */
                        auto pcell = _vte_row_data_get_writable(rowdata, i);
                        *pcell = m_color_defaults;
		} else {
			/* Add new cells until we have one here. */
                        _vte_row_data_append (rowdata, &m_color_defaults);
		}
	}
        /* Repaint this row's paragraph. */
        invalidate_row_and_context(m_screen->cursor.row);

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear to the right of the cursor and below the current line. */
void
Terminal::clear_below_current()
{
        maybe_retreat_cursor();

	/* If the cursor is actually on the screen, clear the rest of the
	 * row the cursor is on and all of the rows below the cursor. */
        VteRowData *rowdata;
        auto i = m_screen->cursor.row;
	if (i < long(m_screen->row_data->next())) {
		/* Get the data for the row we're clipping. */
                rowdata = m_screen->row_data->index_writable(i);
                /* Clean up Tab/CJK fragments. */
                if ((glong) _vte_row_data_length(rowdata) > m_screen->cursor.col)
                        cleanup_fragments(m_screen->cursor.col, _vte_row_data_length(rowdata));
		/* Clear everything to the right of the cursor. */
		if (rowdata)
                        _vte_row_data_shrink(rowdata, m_screen->cursor.col);
	}
	/* Now for the rest of the lines. */
        for (i = m_screen->cursor.row + 1;
	     i < long(m_screen->row_data->next());
	     i++) {
		/* Get the data for the row we're removing. */
		rowdata = m_screen->row_data->index_writable(i);
		/* Remove it. */
		if (rowdata)
			_vte_row_data_shrink (rowdata, 0);
	}
	/* Now fill the cleared areas. */
        bool const not_default_bg = (m_color_defaults.attr.back() != VTE_DEFAULT_BG);

        for (i = m_screen->cursor.row;
	     i < m_screen->insert_delta + m_row_count;
	     i++) {
		/* Retrieve the row's data, creating it if necessary. */
		if (m_screen->row_data->contains(i)) {
			rowdata = m_screen->row_data->index_writable(i);
			g_assert(rowdata != NULL);
		} else {
			rowdata = ring_append(false);
		}
		/* Pad out the row. */
                if (not_default_bg) {
                        _vte_row_data_fill(rowdata, &m_color_defaults, m_column_count);
		}
                set_hard_wrapped(i);
                if (i > m_screen->cursor.row)
                        rowdata->attr.bidi_flags = get_bidi_flags();
	}
        /* Repaint the cleared area (might need to extend upwards). */
        invalidate_rows_and_context(m_screen->cursor.row, m_screen->insert_delta + m_row_count - 1);

	/* We've modified the display.  Make a note of it. */
	m_text_deleted_flag = TRUE;
}

/* Clear from the cursor position to the end of the line. */
void
Terminal::clear_to_eol()
{
	/* If we were to strictly emulate xterm, we'd ensure the cursor is onscreen.
	 * But due to https://bugzilla.gnome.org/show_bug.cgi?id=740789 we intentionally
	 * deviate and do instead what konsole does. This way emitting a \e[K doesn't
	 * influence the text flow, and serves as a perfect workaround against a new line
	 * getting painted with the active background color (except for a possible flicker).
	 */
        /* maybe_retreat_cursor(); */

	/* Get the data for the row which the cursor points to. */
        auto rowdata = ensure_cursor();
	g_assert(rowdata != NULL);
        if ((glong) _vte_row_data_length(rowdata) > m_screen->cursor.col) {
                /* Clean up Tab/CJK fragments. */
                cleanup_fragments(m_screen->cursor.col, _vte_row_data_length(rowdata));
                /* Remove the data at the end of the array until the current column
                 * is the end of the array. */
                _vte_row_data_shrink(rowdata, m_screen->cursor.col);
		/* We've modified the display.  Make a note of it. */
		m_text_deleted_flag = TRUE;
	}
        bool const not_default_bg = (m_color_defaults.attr.back() != VTE_DEFAULT_BG);

        if (not_default_bg) {
		/* Add enough cells to fill out the row. */
                _vte_row_data_fill(rowdata, &m_color_defaults, m_column_count);
	}
        set_hard_wrapped(m_screen->cursor.row);
        /* Repaint this row's paragraph. */
        invalidate_row_and_context(m_screen->cursor.row);
}

/*
 * Terminal::set_cursor_column:
 * @col: the column. 0-based from 0 to m_column_count - 1
 *
 * Sets the cursor column to @col.
 *
 * @col is relative relative to the DECSLRM scrolling region iff origin mode (DECOM) is enabled.
 */
void
Terminal::set_cursor_column(vte::grid::column_t col)
{
	_vte_debug_print(VTE_DEBUG_PARSER,
                         "Moving cursor to column %ld.\n", col);

        vte::grid::column_t left_col, right_col;
        if (m_modes_private.DEC_ORIGIN()) {
                left_col = m_scrolling_region.left();
                right_col = m_scrolling_region.right();
        } else {
                left_col = 0;
                right_col = m_column_count - 1;
        }
        col += left_col;
        col = CLAMP(col, left_col, right_col);

        m_screen->cursor.col = col;
        m_screen->cursor_advanced_by_graphic_character = false;
}

void
Terminal::set_cursor_column1(vte::grid::column_t col)
{
        set_cursor_column(col - 1);
}

/*
 * Terminal::set_cursor_row:
 * @row: the row. 0-based
 *
 * Sets the cursor row to @row.
 *
 * @row is relative to the scrolling DECSTBM scrolling region iff origin mode (DECOM) is enabled.
 */
void
Terminal::set_cursor_row(vte::grid::row_t row)
{
        _vte_debug_print(VTE_DEBUG_PARSER,
                         "Moving cursor to row %ld.\n", row);

        vte::grid::row_t top_row, bottom_row;
        if (m_modes_private.DEC_ORIGIN()) {
                top_row = m_scrolling_region.top();
                bottom_row = m_scrolling_region.bottom();
        } else {
                top_row = 0;
                bottom_row = m_row_count - 1;
        }
        row += top_row;
        row = CLAMP(row, top_row, bottom_row);

        m_screen->cursor.row = row + m_screen->insert_delta;
        m_screen->cursor_advanced_by_graphic_character = false;
}

void
Terminal::set_cursor_row1(vte::grid::row_t row)
{
        set_cursor_row(row - 1);
}

/*
 * Terminal::set_cursor_coords:
 * @row: the row. 0-based
 * @col: the column. 0-based from 0 to m_column_count - 1
 *
 * Sets the cursor row to @row.
 *
 * Sets the cursor column to @col, clamped to the range 0..m_column_count-1.
 *
 * @row and @col are relative to the scrolling DECSTBM / DECSLRM scrolling region
 * iff origin mode (DECOM) is enabled.
 */
void
Terminal::set_cursor_coords(vte::grid::row_t row,
                                      vte::grid::column_t column)
{
        set_cursor_column(column);
        set_cursor_row(row);
}

void
Terminal::set_cursor_coords1(vte::grid::row_t row,
                                      vte::grid::column_t column)
{
        set_cursor_column1(column);
        set_cursor_row1(row);
}

void
Terminal::erase_characters(long count,
                           bool use_basic)
{
	VteCell *cell;
	long col, i;

        maybe_retreat_cursor();

        count = CLAMP(count, 1, m_column_count - m_screen->cursor.col);

	/* Clear out the given number of characters. */
	auto rowdata = ensure_row();
        if (long(m_screen->row_data->next()) > m_screen->cursor.row) {
		g_assert(rowdata != NULL);
                /* Clean up Tab/CJK fragments. */
                cleanup_fragments(m_screen->cursor.col, m_screen->cursor.col + count);
		/* Write over the characters.  (If there aren't enough, we'll
		 * need to create them.) */
                _vte_row_data_fill (rowdata, &basic_cell, m_screen->cursor.col);
		for (i = 0; i < count; i++) {
                        col = m_screen->cursor.col + i;
			if (col >= 0) {
				if (col < (glong) _vte_row_data_length (rowdata)) {
					/* Replace this cell with the current
					 * defaults. */
					cell = _vte_row_data_get_writable (rowdata, col);
                                        *cell = use_basic ? basic_cell : m_color_defaults;
				} else {
					/* Add new cells until we have one here. */
                                        _vte_row_data_fill (rowdata, use_basic ? &basic_cell : &m_color_defaults, col + 1);
				}
			}
		}
                /* Repaint this row's paragraph. */
                invalidate_row_and_context(m_screen->cursor.row);
	}

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

void
Terminal::copy_rect(grid_rect source_rect,
                    grid_point dest) noexcept
try
{
        // Copies the rectangle of cells denoted by @source_rect to the
        // destination rect which is @source_rect translatecd to
        // dest_top, dest_left. If the destination rect is partially
        // off-screen, the operation is clipped.
        //
        // @source_rect is inclusive, @source_rect and @dest are 0-based
        //
        // @source_rect and @dest_dect must be entirely inside the screen.

        auto dest_rect = source_rect.clone().move_to(dest);
        if (dest_rect.empty())
                return;

        auto const screen_rect = grid_rect{0, 0, int(m_column_count) - 1, int(m_row_count) - 1};
        if (!screen_rect.contains(source_rect) ||
            !screen_rect.contains(dest_rect))
                return;

        auto const dest_width = dest_rect.right() - dest_rect.left() + 1;

        // Ensure all used rows exist
        // auto const first_row = std::min(source_rect.top(), dest_rect.top());
        auto const last_row = std::max(source_rect.bottom(), dest_rect.bottom());
        auto rowdelta = m_screen->insert_delta + last_row - long(m_screen->row_data->next()) + 1;
        if (rowdelta > 0) [[unlikely]] {
                do {
                        ring_append(false);
                } while (--rowdelta);

                adjust_adjustments();
        }

        // Buffer to simplify copying when source and dest overlap
        auto vec = std::vector<VteCell>{};
        vec.reserve(dest_width);

        auto copy_row = [&](auto srow,
                            auto drow) -> void
        {
                auto srowdata = m_screen->row_data->index_writable(srow);
                if (!srowdata)
                        return;

                if (!_vte_row_data_ensure_len(srowdata, source_rect.right() + 1))
                        return;

                vec.clear();
                auto col = source_rect.left();
                if (auto cell = _vte_row_data_get(srowdata, col)) {
                        // there is at least some data in this row to copy

                        // If we start with a fragment, need to fill with defaults first
                        while (col < int(srowdata->len) &&
                               col <= source_rect.right() &&
                               cell->attr.fragment()) {
                                vec.push_back(basic_cell); // or m_defaults?
                                ++cell;
                                ++col;
                        }

                        // Now copy non-fragment cells, if any
                        while (col < int(srowdata->len) &&
                               col + int(cell->attr.columns()) <= source_rect.right() + 1) {
                                auto const cols = cell->attr.columns();
                                for (auto j = 0u; j < cols; ++j, ++cell)
                                        vec.push_back(*cell);

                                col += cols;
                        }

                        // Fill left-over space (if any) with attributes from source
                        // but erased character content
                        for (;
                             col < int(srowdata->len) && col <= source_rect.right();
                             ++col, ++cell) {
                                auto erased_cell = VteCell{.c = 0, .attr = cell->attr};
                                erased_cell.attr.set_fragment(false);
                                vec.push_back(erased_cell);
                        }
                }

                // Fill left-over space (if any) with erased default attributes
                for (; col <= source_rect.right(); ++col)
                        vec.push_back(m_defaults); // or basic_cell ??

                assert(vec.size() == size_t(dest_width));

                auto drowdata = m_screen->row_data->index_writable(drow);
                if (!drowdata)
                        return;

                if (!_vte_row_data_ensure_len(drowdata, dest_rect.right() + 1))
                        return;

                cleanup_fragments(drowdata, drow, dest_rect.left(), dest_rect.right() + 1);
                _vte_row_data_fill_cells(drowdata,
                                         dest_rect.left(),
                                         &basic_cell, // or m_defaults ?
                                         vec.data(),
                                         vec.size());

                // FIXME: truncate row if only erased cells at end?
        };

        if (dest_rect.top() < source_rect.top() ||
            ((dest_rect.top() == source_rect.top()) &&
             (dest_rect.left() < source_rect.left()))) {
                // Copy from top to bottom and left-to-right
                auto drow = m_screen->insert_delta + dest_rect.top();
                for (auto srow = m_screen->insert_delta + source_rect.top();
                     srow <= m_screen->insert_delta + source_rect.bottom();
                     ++srow, ++drow) {
                        copy_row(srow, drow);
                }
        } else {
                // Copy from bottom to top (would need to copy right-
                // to-left if not using the buffer)
                auto drow = m_screen->insert_delta + dest_rect.bottom();
                for (auto srow = m_screen->insert_delta + source_rect.bottom();
                     srow >= m_screen->insert_delta + source_rect.top();
                     --srow, --drow) {
                        copy_row(srow, drow);
                }
        }

        /* We modified the display, so make a note of it for completeness. */
        m_text_modified_flag = true;

        emit_text_modified();
        invalidate_all();
}
catch (...)
{
}

void
Terminal::fill_rect(grid_rect rect,
                    char32_t c,
                    VteCellAttr attr) noexcept
try
{
        // Fills the rectangle of cells denoted by @rect with character @c
        // and attribute @attr.
        // Note that the bottom and right parameters in @rect are inclusive.

        auto const cw = character_width(c);
        if (cw == 0) [[unlikely]]
                return; // ignore

        // Build an array of VteCell to copy to the rows
        auto const rect_width = rect.right() - rect.left() + 1;
        auto vec = std::vector<VteCell>{};
        vec.reserve(rect_width);

        auto cell = VteCell{.c = c, .attr = attr};
        cell.attr.set_columns(cw);

        auto frag_cell = cell;
        frag_cell.attr.set_fragment(true);

        // Fill cells with character
        auto col = 0;
        while (col + cw <= rect_width) {
                vec.push_back(cell);
                for (auto f = 1; f < cw; ++f)
                        vec.push_back(frag_cell);

                col+= cw;
        }

        // Fill the rest with erased cells
        cell.c = 0;
        cell.attr.set_columns(1);
        cell.attr.set_fragment(false);
        while (col++ < rect_width) {
                vec.push_back(cell);
        }

        assert(vec.size() == size_t(rect_width));

        // Ensure all used rows exist
        auto rowdelta = m_screen->insert_delta + rect.bottom() - long(m_screen->row_data->next()) + 1;
        if (rowdelta > 0) [[unlikely]] {
                do {
                        ring_append(false);
                } while (--rowdelta);

                adjust_adjustments();
        }

        // Now copy the cells into the ring

        for (auto row = m_screen->insert_delta + rect.top();
             row <= m_screen->insert_delta + rect.bottom();
             ++row) {
                auto rowdata = m_screen->row_data->index_writable(row);
                if (!rowdata)
                        continue;

                cleanup_fragments(rowdata, row, rect.left(), rect.right() + 1);
                _vte_row_data_fill_cells(rowdata,
                                         rect.left(),
                                         &basic_cell,
                                         vec.data(),
                                         vec.size());

                // FIXME: truncate row if only erased cells at end?
        }

        /* We modified the display, so make a note of it for completeness. */
        m_text_modified_flag = true;

        emit_text_modified();
        invalidate_all();
}
catch (...)
{
}

template<class P>
void
Terminal::rewrite_rect(grid_rect rect,
                       bool as_rectangle,
                       bool only_attrs,
                       P&& pen) noexcept
try
{
        // Visit the rectangle of cells (either as a rectangle, or a stream
        // of cells) denoted by @rect and calls @pen on each cell.
        // Note that the bottom and right parameters in @rect are inclusive.

        // Ensure all used rows exist
        auto rowdelta = m_screen->insert_delta + rect.bottom() - long(m_screen->row_data->next()) + 1;
        if (rowdelta > 0) [[unlikely]] {
                do {
                        ring_append(false);
                } while (--rowdelta);

                adjust_adjustments();
        }


        // If the pen will only write visual attrs, we don't need to cleanup
        // fragments. However we do need to make sure it's not writing only
        // the attrs for half a double-width character. If the pen does write
        // character data, it may only write width 1 characters (unless this
        // function is fixed to allow for that).

        auto visit_row = [&](auto rownum,
                             int left /* inclusive */,
                             int right /* exclusive */) -> void {
                auto rowdata = m_screen->row_data->index_writable(rownum);
                if (!rowdata)
                        return;

                // Note that in RECTANGLE mode, changes apply to all cells in the
                // rectangle, while in STREAM mode, changes should only be applied
                // to non-erased cells. In the latter case, don't extend the line
                // and make sure below to check for erased cells, as per
                // https://gitlab.gnome.org/GNOME/vte/-/issues/2783#note_2164294
                if (as_rectangle) {
                        if (!_vte_row_data_ensure_len(rowdata, right))
                                return;

                        _vte_row_data_fill(rowdata, &basic_cell, left);

                        auto fill = VteCell{.c = ' ', .attr = m_defaults.attr};
                        fill.attr.set_columns(1);
                        fill.attr.set_fragment(false);
                        _vte_row_data_fill(rowdata, &fill, right);
                } else {
                        if (int(rowdata->len) <= left)
                                return; // nothing to do

                        right = std::min(right, int(rowdata->len));
                }

                if (!only_attrs)
                        cleanup_fragments(rowdata, rownum, left, right);

                auto cell = &rowdata->cells[left];
                if (as_rectangle) {
                        for (auto col = left; col < right; ++col, ++cell) {
                                if (only_attrs &&
                                    !cell->attr.fragment() &&
                                    (col + int(cell->attr.columns()) > right)) [[unlikely]]
                                        break;

                                // When not writing character content, need to
                                // occupy erased cells.
                                if (cell->c == 0 && only_attrs) {
                                        cell->c = ' '; // SPACE
                                        cell->attr.set_fragment(false);
                                }

                                pen(cell);

                        }

                        _vte_row_data_expand(rowdata, right);
                } else {
                        for (auto col = left; col < right; ++col, ++cell) {
                                if (cell->c == 0) // erased? skip this cell
                                        continue;

                                if (only_attrs &&
                                    !cell->attr.fragment() &&
                                    (col + int(cell->attr.columns()) > right)) [[unlikely]]
                                        break;

                                pen(cell);
                        }
                }
        };

        if (as_rectangle || rect.top() == rect.bottom()) { // as rectangle
                for (auto row = m_screen->insert_delta + rect.top();
                     row <= m_screen->insert_delta + rect.bottom();
                     ++row) {
                        visit_row(row, rect.left(), rect.right() + 1);
                }
        } else { // as stream (see DECSACE)
                auto row = m_screen->insert_delta + rect.top();
                visit_row(row++, rect.left(), m_column_count);
                for (;
                     row < m_screen->insert_delta + rect.bottom();
                     ++row) {
                        visit_row(row, 0, m_column_count);
                }
                visit_row(row, 0, rect.right() + 1);
        }

        /* We modified the display, so make a note of it for completeness. */
        m_text_modified_flag = true;

        emit_text_modified();
        invalidate_all();
}
catch (...)
{
}

/* Terminal::move_cursor_up:
 * Cursor up by n rows (respecting the DECSTBM / DECSLRM scrolling region).
 *
 * See the "CUU, CUD, CUB, CUF" picture in ../doc/scrolling-region.txt.
 *
 * DEC STD 070 says not to move further if the cursor hits the margin outside of the scrolling area.
 * Xterm follows this, and so do we. Reportedly (#2526) DEC terminals move the cursor despite their doc.
 */
void
Terminal::move_cursor_up(vte::grid::row_t rows)
{
        // FIXMEchpe allow 0 as no-op?
        rows = CLAMP(rows, 1, m_row_count);

        //FIXMEchpe why not do this afterward?
        maybe_retreat_cursor();

        vte::grid::row_t top;
        if (m_screen->cursor.row >= m_screen->insert_delta + m_scrolling_region.top()) {
                top = m_screen->insert_delta + m_scrolling_region.top();
	} else {
		top = m_screen->insert_delta;
	}

        m_screen->cursor.row = MAX(m_screen->cursor.row - rows, top);
        m_screen->cursor_advanced_by_graphic_character = false;
}

/* Terminal::move_cursor_down:
 * Cursor down by n rows (respecting the DECSTBM / DECSLRM scrolling region).
 *
 * See the "CUU, CUD, CUB, CUF" picture in ../doc/scrolling-region.txt.
 *
 * DEC STD 070 says not to move further if the cursor hits the margin outside of the scrolling area.
 * Xterm follows this, and so do we. Reportedly (#2526) DEC terminals move the cursor despite their doc.
 */
void
Terminal::move_cursor_down(vte::grid::row_t rows)
{
        rows = CLAMP(rows, 1, m_row_count);

        // FIXMEchpe why not do this afterwards?
        maybe_retreat_cursor();

        vte::grid::row_t bottom;
        if (m_screen->cursor.row <= m_screen->insert_delta + m_scrolling_region.bottom()) {
                bottom = m_screen->insert_delta + m_scrolling_region.bottom();
	} else {
                bottom = m_screen->insert_delta + m_row_count - 1;
	}

        m_screen->cursor.row = MIN(m_screen->cursor.row + rows, bottom);
        m_screen->cursor_advanced_by_graphic_character = false;
}

/* Terminal::move_cursor_backward:
 * Cursor left by n columns (respecting the DECSTBM / DECSLRM scrolling region).
 *
 * See the "CUU, CUD, CUB, CUF" picture in ../doc/scrolling-region.txt.
 *
 * DEC STD 070 says not to move further if the cursor hits the margin outside of the scrolling area.
 * Xterm follows this, and so do we. Reportedly (#2526) DEC terminals move the cursor despite their doc.
 */
void
Terminal::move_cursor_backward(vte::grid::column_t columns)
{
        columns = CLAMP(columns, 1, m_column_count);

        maybe_retreat_cursor();

        vte::grid::column_t left;
        if (m_screen->cursor.col >= m_scrolling_region.left()) {
                left = m_scrolling_region.left();
        } else {
                left = 0;
        }

        m_screen->cursor.col = MAX(m_screen->cursor.col - columns, left);
        m_screen->cursor_advanced_by_graphic_character = false;
}

/* Terminal::move_cursor_forward:
 * Cursor right by n columns (respecting the DECSTBM / DECSLRM scrolling region).
 *
 * See the "CUU, CUD, CUB, CUF" picture in ../doc/scrolling-region.txt.
 *
 * DEC STD 070 says not to move further if the cursor hits the margin outside of the scrolling area.
 * Xterm follows this, and so do we. Reportedly (#2526) DEC terminals move the cursor despite their doc.
 */
void
Terminal::move_cursor_forward(vte::grid::column_t columns)
{
        columns = CLAMP(columns, 1, m_column_count);

        maybe_retreat_cursor();

        vte::grid::column_t right;
        if (m_screen->cursor.col <= m_scrolling_region.right()) {
                right = m_scrolling_region.right();
        } else {
                right = m_column_count - 1;
        }

        m_screen->cursor.col = MIN(m_screen->cursor.col + columns, right);
        m_screen->cursor_advanced_by_graphic_character = false;
}

void
Terminal::move_cursor_tab_backward(int count)
{
        if (count == 0)
                return;

        auto const col = get_xterm_cursor_column();

        /* Find the count'th previous tabstop, but don't cross the left margin.
         * The exact desired behavior is debated, though.
         * See https://gitlab.gnome.org/GNOME/vte/-/issues/2526#note_1879956 */
        auto const stop = col >= m_scrolling_region.left() ? m_scrolling_region.left() : 0;
        auto const newcol = m_tabstops.get_previous(col, count, stop);

        m_screen->cursor.col = newcol;
        m_screen->cursor_advanced_by_graphic_character = false;
}

void
Terminal::move_cursor_tab_forward(int count)
{
        if (count == 0)
                return;

        auto const col = get_xterm_cursor_column();

        /* If a printable character would wrap then a TAB does nothing;
         * most importantly, does not snap back the cursor.
         * https://gitlab.gnome.org/GNOME/gnome-terminal/-/issues/3461 */
        if (col < m_screen->cursor.col)
                return;

        /* Find the count'th next tabstop, but don't cross the right margin.
         * The exact desired behavior is debated, though.
         * See https://gitlab.gnome.org/GNOME/vte/-/issues/2526#note_1879956 */
        auto const stop = col <= m_scrolling_region.right() ? m_scrolling_region.right() : m_column_count - 1;
        auto const newcol = m_tabstops.get_next(col, count, stop);

        /* If the cursor didn't advance then nothing left to do. */
        vte_assert_cmpint((int)newcol, >=, col);
        if ((int)newcol == col)
                return;

        /* Smart tab handling: bug 353610
         *
         * If we currently don't have any cells in the space this
         * tab creates, we try to make the tab character copyable,
         * by appending a single tab char with lots of fragment
         * cells following it.
         *
         * Otherwise, just append empty cells that will show up
         * as a space each.
         */

        VteRowData *rowdata = ensure_row();
        auto const old_len = _vte_row_data_length (rowdata);
        _vte_row_data_fill (rowdata, &basic_cell, newcol);

        /* Insert smart tab if there's nothing in the line after
         * us, not even empty cells (with non-default background
         * color for example).
         *
         * Notable bugs here: 545924, 597242, 764330
         */
        if (col >= old_len && (newcol - col) <= VTE_TAB_WIDTH_MAX) {
                glong i;
                VteCell *cell = _vte_row_data_get_writable (rowdata, col);
                cell->c = '\t';
                cell->attr.set_columns(newcol - col);
                /* And adjust the fragments */
                for (i = col + 1; i < newcol; i++) {
                        cell = _vte_row_data_get_writable (rowdata, i);
                        cell->c = '\t';
                        cell->attr.set_columns(1);
                        cell->attr.set_fragment(true);
                }
        }

        /* Repaint the cursor. */
        invalidate_row(m_screen->cursor.row);
        m_screen->cursor.col = newcol;
        m_screen->cursor_advanced_by_graphic_character = false;
}

void
Terminal::carriage_return()
{
        /* Xterm and DEC STD 070 p5-58 agree that if the cursor is to the left
         * of the left margin then move it to the first column.
         * They disagree whether to stop at the left margin if the cursor is to
         * the right of the left margin, but outside of the top/bottom margins.
         * Follow Xterm's behavior for now, subject to change if needed, as per
         * the discussions at https://gitlab.gnome.org/GNOME/vte/-/issues/2526 */
        if (m_screen->cursor.col >= m_scrolling_region.left()) {
                m_screen->cursor.col = m_scrolling_region.left();
        } else {
                m_screen->cursor.col = 0;
        }
        m_screen->cursor_advanced_by_graphic_character = false;
}

void
Terminal::line_feed()
{
        maybe_retreat_cursor();
        cursor_down_with_scrolling(true);
        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_ALL);
}

void
Terminal::erase_in_display(vte::parser::Sequence const& seq)
{
        // We don't implement the protected attribute, so we can ignore selective:
        auto selective = (seq.command() == VTE_CMD_DECSED);

        switch (seq.collect1(0)) {
        case -1: /* default */
	case 0:
		/* Clear below the current line. */
                clear_below_current();
		break;
	case 1:
		/* Clear above the current line. */
                clear_above_current();
		/* Clear everything to the left of the cursor, too. */
		/* FIXME: vttest. */
                clear_to_bol();
		break;
	case 2:
		/* Clear the entire screen. */
                clear_screen();
		break;
        case 3:
                if (selective)
                        break;

                /* Drop the scrollback (only for ED) */
                drop_scrollback();
                break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

void
Terminal::erase_in_line(vte::parser::Sequence const& seq)
{
        /* We don't implement the protected attribute, so we can ignore selective:
         * bool selective = (seq.command() == VTE_CMD_DECSEL);
         */

        switch (seq.collect1(0)) {
        case -1: /* default */
	case 0:
		/* Clear to end of the line. */
                clear_to_eol();
		break;
	case 1:
		/* Clear to start of the line. */
                clear_to_bol();
		break;
	case 2:
		/* Clear the entire line. */
                clear_current_line();
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

void
Terminal::set_color(vte::parser::Sequence const& seq,
                    vte::parser::StringTokeniser::const_iterator& token,
                    vte::parser::StringTokeniser::const_iterator const& endtoken,
                    osc_colors::OSCValuedColorSequenceKind osc_kind,
                    int osc) noexcept
{
        while (token != endtoken) {
                auto const value = token.number();

                if (++token == endtoken)
                        break;

                if (!value) {
                        ++token; // skip the colour param
                        continue;
                }

                if (auto const index = OSCColorIndex::from_sequence(osc_kind, *value))
                        set_color_index(seq, token, endtoken, value, *index, osc);

                ++token;
        }
}

void
Terminal::set_color_index(vte::parser::Sequence const& seq,
                          vte::parser::StringTokeniser::const_iterator& token,
                          vte::parser::StringTokeniser::const_iterator const& endtoken,
                          std::optional<int> number,
                          OSCColorIndex index,
                          int osc) noexcept
{
        auto const str = *token;

        if (str == "?"s) {
                auto const color = resolve_reported_color(index).value_or(vte::color::rgb{0, 0, 0});

                if (number)
                        reply(seq, VTE_REPLY_OSC, {}, "%d;%d;rgb:%04x/%04x/%04x",
                              osc, *number, color.red, color.green, color.blue);
                else
                        reply(seq, VTE_REPLY_OSC, {}, "%d;rgb:%04x/%04x/%04x",
                              osc, color.red, color.green, color.blue);
        } else {
                vte::color::rgb color;

                if (index.kind() == OSCColorIndexKind::Palette &&
                    color.parse(str.data())) {
                        set_color(index.palette_index(), ColorSource::Escape, color);
                }
        }
}

auto
Terminal::resolve_reported_color(osc_colors::OSCColorIndex index) const noexcept -> std::optional<vte::color::rgb>
{
        if (index.kind() == OSCColorIndexKind::Palette) {
                if (auto const color = get_color_opt(index.palette_index()))
                        return color;
        }

        if (auto const fallback_index = index.fallback_palette_index())
                return get_color_opt(*fallback_index);

        return std::nullopt;
}

void
Terminal::set_special_color(vte::parser::Sequence const& seq,
                                      vte::parser::StringTokeniser::const_iterator& token,
                                      vte::parser::StringTokeniser::const_iterator const& endtoken,
                                      const ColorPaletteIndex index,
                                      const int osc) noexcept
{
        if (token == endtoken)
                return;

        set_color_index(seq, token, endtoken, std::nullopt, index, osc);
}

void
Terminal::reset_color(vte::parser::Sequence const& seq,
                                vte::parser::StringTokeniser::const_iterator& token,
                                vte::parser::StringTokeniser::const_iterator const& endtoken,
                                const osc_colors::OSCValuedColorSequenceKind osc_kind) noexcept
{
        /* Empty param? Reset all */
        if (token == endtoken ||
            token.size_remaining() == 0) {
                if (osc_kind == OSCValuedColorSequenceKind::XTermColor) {
                        for (auto idx = 0; idx < VTE_DEFAULT_FG; idx++)
                                reset_color(idx, ColorSource::Escape);
                }

                reset_color(ColorPaletteIndex::bold_fg(), ColorSource::Escape);
                /* Add underline/blink/reverse/italic here if/when implemented */

                return;
        }

        while (token != endtoken) {
                if (auto const value = token.number()) {
                        if (auto index = OSCColorIndex::from_sequence(osc_kind, *value))
                                if (index->kind() == OSCColorIndexKind::Palette)
                                        reset_color(index->palette_index(), ColorSource::Escape);
                }

                ++token;
        }
}

void
Terminal::set_termprop_uri(vte::parser::Sequence const& seq,
                           vte::parser::StringTokeniser::const_iterator& token,
                           vte::parser::StringTokeniser::const_iterator const& endtoken,
                           int termprop_id,
                           PendingChanges legacy_pending_change) noexcept
{
        auto const info = get_termprop_info(termprop_id);
        assert(info);

        auto set = false;
        if (token != endtoken && token.size_remaining() > 0) {
                auto const str = token.string_remaining();

                // Only parse the URI if the termprop doesn't already have the
                // same string value
                if (auto const old_value = get_termprop(*info);
                    !old_value ||
                    !std::holds_alternative<vte::terminal::TermpropURIValue>(*old_value) ||
                    std::get<vte::terminal::TermpropURIValue>(*old_value).second != str) {

                        if (auto uri = vte::take_freeable(g_uri_parse(str.c_str(),
                                                                      GUriFlags(G_URI_FLAGS_ENCODED),
                                                                      nullptr));
                            uri &&
                            g_strcmp0(g_uri_get_scheme(uri.get()), "file") == 0) {

                                set = true;
                                m_termprops_dirty.at(info->id()) = true;
                                m_termprop_values.at(info->id()) =
                                        vte::terminal::TermpropValue{std::in_place_type<vte::terminal::TermpropURIValue>,
                                                                     std::move(uri),
                                                                     str};
                        } else {
                                // invalid URI, or not a file: URI
                                set = true;
                                reset_termprop(*info);
                        }
                }
        } else {
                // Only reset the termprop if it's not already reset
                if (auto const old_value = get_termprop(*info);
                    !old_value ||
                    !std::holds_alternative<std::monostate>(*old_value)) {
                        set = true;
                        reset_termprop(*info);
                }
        }

        if (set) {
                m_pending_changes |= vte::to_integral(PendingChanges::TERMPROPS) |
                        vte::to_integral(legacy_pending_change);
        }
}

void
Terminal::set_current_hyperlink(vte::parser::Sequence const& seq,
                                          vte::parser::StringTokeniser::const_iterator& token,
                                          vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept
{
        if (token == endtoken)
                return; // FIXMEchpe or should we treat this as a reset?

        /* Handle OSC 8 hyperlinks.
         * See bug 779734 and https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
         */

        if (!m_allow_hyperlink)
                return;

        /* The hyperlink, as we carry around and store in the streams, is "id;uri" */
        std::string hyperlink;

        /* First, find the ID */
        auto tokenstr = *token;
        vte::parser::StringTokeniser subtokeniser{tokenstr, ':'};
        for (auto subtoken : subtokeniser) {
                auto const len = subtoken.size();
                if (len < 3)
                        continue;

                if (subtoken[0] != 'i' || subtoken[1] != 'd' || subtoken[2] != '=')
                        continue;

                if (len > 3 + VTE_HYPERLINK_ID_LENGTH_MAX) {
                        _vte_debug_print (VTE_DEBUG_HYPERLINK, "Overlong \"id\" ignored: \"%s\"\n",
                                          subtoken.data());
                        break;
                }

                hyperlink = subtoken.substr(3);
                break;
        }

        if (hyperlink.size() == 0) {
                /* Automatically generate a unique ID string. The colon makes sure
                 * it cannot conflict with an explicitly specified one.
                 */
                char idbuf[24];
                auto len = g_snprintf(idbuf, sizeof(idbuf), ":%ld", m_hyperlink_auto_id++);
                hyperlink.append(idbuf, len);
                _vte_debug_print (VTE_DEBUG_HYPERLINK, "Autogenerated id=\"%s\"\n", hyperlink.data());
        }

        /* Now get the URI */
        if (++token == endtoken)
                return; // FIXMEchpe or should we treat this the same as 0-length URI ?

        hyperlink.push_back(';');
        guint idx;
        auto const len = token.size_remaining();
        if (len > 0 && len <= VTE_HYPERLINK_URI_LENGTH_MAX) {
                token.append_remaining(hyperlink);

                _vte_debug_print (VTE_DEBUG_HYPERLINK, "OSC 8: id;uri=\"%s\"\n", hyperlink.data());

                idx = m_screen->row_data->get_hyperlink_idx(hyperlink.data());
        } else {
                if (G_UNLIKELY(len > VTE_HYPERLINK_URI_LENGTH_MAX))
                        _vte_debug_print (VTE_DEBUG_HYPERLINK, "Overlong URI ignored (len %" G_GSIZE_FORMAT ")\n", len);

                /* idx = 0; also remove the previous current_idx so that it can be GC'd now. */
                idx = m_screen->row_data->get_hyperlink_idx(nullptr);
        }

        m_defaults.attr.hyperlink_idx = idx;
}

void
Terminal::set_current_shell_integration_mode(vte::parser::Sequence const& seq,
                                             vte::parser::StringTokeniser::const_iterator& token,
                                             vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept
{
        if (token != endtoken && token.size_remaining() > 0) {
                std::string mode = *token;
                if (mode == "A") {
                        m_defaults.attr.set_shellintegration(ShellIntegrationMode::ePROMPT);
                } else if (mode == "B") {
                        m_defaults.attr.set_shellintegration(ShellIntegrationMode::eCOMMAND);
                } else if (mode == "C") {
                        m_defaults.attr.set_shellintegration(ShellIntegrationMode::eNORMAL);
                } else if (mode == "D") {
                        /* This deliberately doesn't start a different mode. Ignore for now. */
                } else if (mode == "L") {
                        /* Maybe insert some CR LFs, with the purpose of making sure that the
                         * shell prompt starts on its own paragraph (i.e. just after a hard wrap).
                         * See https://gitlab.gnome.org/GNOME/vte/-/issues/2681#note_1911689.
                         *
                         * (This doesn't start a new mode, so the method name is not quite accurate. Nevermind.) */
                        while (m_screen->cursor.col > 0 ||
                               m_screen->row_data->is_soft_wrapped(m_screen->cursor.row - 1)) {
                                set_cursor_column(0);
                                cursor_down_with_scrolling(true);
                        }
                        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_ALL);
                }
        }
}

#if VTE_DEBUG

void
Terminal::reply_termprop_query(vte::parser::Sequence const& seq,
                               vte::terminal::TermpropInfo const* info)
{
        // Since this is only used in test mode, we just send one
        // OSC reply per query, instead of trying to consolidate
        // multiple replies into as few OSCs as possible.

        auto str = std::string{info->name()};
        switch (info->type()) {
                using enum vte::terminal::TermpropType;
        case VALUELESS:
                if (m_termprops_dirty.at(info->id()))
                        str.push_back('!');
                break;

        default:
                if (auto const vstr =
                    vte::terminal::unparse_termprop_value(info->type(),
                                                          m_termprop_values.at(info->id()))) {
                        str.push_back('=');
                        str.append(*vstr);
                }
        }

        reply(seq, VTE_REPLY_OSC, {}, "%d;%s", VTE_OSC_VTE_TERMPROP,str.c_str());
}

#endif // VTE_DEBUG

void
Terminal::parse_termprop(vte::parser::Sequence const& seq,
                         std::string_view const& str,
                         bool& set,
                         bool& query) noexcept
try
{
        auto const pos = str.find_first_of("=?!"); // possibly str.npos
        auto const info = vte::terminal::get_termprop_info(str.substr(0, pos));

        // No-OSC termprops cannot be set via the termprop OSC, but they
        // can be queried and reset
        auto const no_osc = info &&
                (unsigned(info->flags()) & unsigned(vte::terminal::TermpropFlags::NO_OSC)) != 0;
        // Valueless termprops are special in that they can only be
        // emitted or reset, and resetting cancels the emission
        auto const is_valueless = info &&
                info->type() == vte::terminal::TermpropType::VALUELESS;

        if (pos == str.npos) {
                // Reset
                //
                // Allow reset even for no-OSC termprops
                if (info &&
                    !std::holds_alternative<std::monostate>(m_termprop_values.at(info->id()))) {
                        set = true;
                        m_termprops_dirty.at(info->id()) = !is_valueless;
                        m_termprop_values.at(info->id()) = {};
                }
        } else if (str[pos] == '=' &&
                   info &&
                   !is_valueless &&
                   !no_osc) {
                if (auto value = vte::terminal::parse_termprop_value(info->type(),
                                                                     str.substr(pos + 1))) {
                        // Set
                        if (value != m_termprop_values.at(info->id())) {
                                set = true;
                                m_termprop_values.at(info->id()) = std::move(*value);
                                m_termprops_dirty.at(info->id()) = true;
                        }
                } else {
                        // Reset
                        if (!std::holds_alternative<std::monostate>(m_termprop_values.at(info->id()))) {
                                set = true;
                                m_termprop_values.at(info->id()) = {};
                                m_termprops_dirty.at(info->id()) = true;
                        }
                }
        } else if (str[pos] == '?') {
                if ((pos + 1) == str.size()) {
                        // Query
                        //
                        // In test mode, do reply to the query. In non-test mode,
                        // just set a flag and send a single dummy reply afterwards.
                        //
                        // Allow query even for no-OSC termprops and even unregistered
                        // termprops, for forward compatibility.
#if VTE_DEBUG
                        if (info && (g_test_flags & VTE_TEST_FLAG_TERMPROP) != 0) {
                                reply_termprop_query(seq, info);
                        } else
#endif
                        query = true;
                }
        } else if (str[pos] == '!') {
                if ((pos + 1) == str.size() &&
                    info &&
                    is_valueless &&
                    !no_osc &&
                    !m_termprops_dirty.at(info->id())) {
                        // Signal
                        set = true;
                        m_termprops_dirty.at(info->id()) = true;
                        // no need to set/reset the value
                }
        }
}
catch (...)
{
        set = true; // something may have happened already
}

void
Terminal::vte_termprop(vte::parser::Sequence const& seq,
                       vte::parser::StringTokeniser::const_iterator& token,
                       vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept
try
{
        // This is a new and vte-only feature, so reject BEL-terminated OSC.
        if (seq.is_st_bel()) {
                token = endtoken;
                return;
        }

        auto set = false, query = false;
        auto queries = std::vector<int>{};
        while (token != endtoken) {
                parse_termprop(seq, *token, set, query);
                ++token;
        }

        if (set) {
                // https://gitlab.gnome.org/GNOME/vte/-/issues/2125#note_1155148
                // mentions that we may want to break out of processing input now
                // and dispatch the changed notification immediately. However,
                // (at least for now) it's better not to give that guarantee, and
                // instead make this asynchronous (and thus also automatically
                // rate-limited). Also, due to the documented prohibition of
                // calling any API on VteTerminal except the termprop value
                // retrieval functions, this should not be further limiting.

                m_pending_changes |= vte::to_integral(PendingChanges::TERMPROPS);
        }

        if (query) {
                // Reserved for future extension. Reply with an empty
                // termprop set statement for forward compatibility.

                reply(seq, VTE_REPLY_OSC, {}, "%d", VTE_OSC_VTE_TERMPROP);
        }
}
catch (...)
{
        // nothing to do here
}

void
Terminal::urxvt_extension(vte::parser::Sequence const& seq,
                          vte::parser::StringTokeniser::const_iterator& token,
                          vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept
try
{
        if (!enable_legacy_osc777())
                return;

        if (token == endtoken)
                return;

        auto maybe_set_termprop_void = [&](int prop,
                                           bool set = true) -> void {
                if (auto const info = get_termprop_info(prop);
                    info && info->type() == vte::terminal::TermpropType::VALUELESS) {
                        m_termprops_dirty.at(info->id()) = set;
                        m_termprop_values.at(info->id()) = {};
                        m_pending_changes |= vte::to_integral(PendingChanges::TERMPROPS);
                }
        };

        auto maybe_set_termprop = [&](int prop,
                                      auto&& value) -> void {
                if (auto const info = get_termprop_info(prop)) {
                        m_termprops_dirty.at(info->id()) = true;
                        m_termprop_values.at(info->id()) = std::move(value);
                        m_pending_changes |= vte::to_integral(PendingChanges::TERMPROPS);
                }
        };

        auto maybe_reset_termprop = [&](int prop) -> void {
                if (auto const info = get_termprop_info(prop)) {
                        m_termprops_dirty.at(info->id()) = true;
                        m_termprop_values.at(info->id()) = {};
                        m_pending_changes |= vte::to_integral(PendingChanges::TERMPROPS);
                }
        };

        auto const cmd = *token;
        if (cmd == "precmd") {
                maybe_set_termprop_void(VTE_PROPERTY_ID_SHELL_PRECMD);

        } else if (cmd == "preexec") {
                maybe_set_termprop_void(VTE_PROPERTY_ID_SHELL_PREEXEC);

        } else if (cmd == "notify") {
                if (++token == endtoken)
                        return;

                if (*token != "Command completed")
                        return;

                maybe_set_termprop_void(VTE_PROPERTY_ID_SHELL_POSTEXEC);

        } else if (cmd == "container") {

                if (++token == endtoken)
                        return;

                auto const subcmd = *token;
                if (subcmd != "pop" && subcmd != "push")
                        return;

                // Note: There is no stack of values anymore.

                // Reset container termprops so we don't get inconsistent
                // values with incomplete sequences below.
                maybe_reset_termprop(VTE_PROPERTY_ID_CONTAINER_NAME);
                maybe_reset_termprop(VTE_PROPERTY_ID_CONTAINER_RUNTIME);
                maybe_reset_termprop(VTE_PROPERTY_ID_CONTAINER_UID);

                if (subcmd == "push") {
                        if (++token == endtoken)
                                return;

                        maybe_set_termprop(VTE_PROPERTY_ID_CONTAINER_NAME, *token);

                        if (++token == endtoken)
                                return;

                        maybe_set_termprop(VTE_PROPERTY_ID_CONTAINER_RUNTIME, *token);

                        if (++token == endtoken)
                                return;

                        if (auto value = vte::terminal::parse_termprop_value(vte::terminal::TermpropType::UINT, *token)) {
                                maybe_set_termprop(VTE_PROPERTY_ID_CONTAINER_UID, *value);
                        }

                } else if (subcmd == "pop") {
                        // already reset above
                }
        }
}
catch (...)
{
        // nothing to do here
}

// collect_rect:
// @seq:
// @idx:
//
// Collects a rectangle from the parameters of @seq at @idx.
// @idx will be advanced to the first parameter after the rect.
//
// As per the DEC documentation for DECCRA, DECFRA, CEDERA, DECSERA, DECCARA,
// DECRARA, and DECRQCRA, the rectangle consists of 4 (final) parameters, in
// order, the coordinates of the top, left, bottom, and right edges of the
// rectangle, and are clamped to the number of lines for top, and bottom; and
// to the number of columns for left, and right.
//
// The documentation says that
// "The coordinates of the rectangular area are affected by the setting of
// Origin Mode. This control is not otherwise affected by the margins."
// which one might interpret as the rectangle not being clipped by the
// scrolling margins; however a different interpretation (and one that is
// confirmed by testing an actual VT420 terminal) is that "otherwise" refers
// to DECOM, i.e. the function is unaffected by the margins iff DECOM is reset.
// In origin mode, the coordinates are clamped to the scrolling region, so that
// a rectangle completely outside the scrolling region is brought inside the
// scrolling region as a single line and/or column. See the discussion in
// https://gitlab.gnome.org/GNOME/vte/-/issues/2783 .
//
// The parameters admit default values, which are 1 for the top and left
// parameters, the number of lines in the current page for the bottom parameter,
// and the number of columns for the right parameter.
// Top must be less or equal to bottom, and left must be less or equal to right.
//
// Returns: the (possibly empty) rectangle
//
// References: DEC STD 070 page 5-168 ff
//             DEC VT525
//
vte::grid_rect
Terminal::collect_rect(vte::parser::Sequence const& seq,
                       unsigned& idx) noexcept
{
        // Param values are 1-based; directly translate to 0-based
        auto top = seq.collect1(idx, 1, 1, m_row_count) - 1;
        idx = seq.next(idx);
        auto left = seq.collect1(idx, 1, 1, m_column_count) - 1;
        idx = seq.next(idx);
        auto bottom = seq.collect1(idx, m_row_count, 1, m_row_count) - 1;
        idx = seq.next(idx);
        auto right = seq.collect1(idx, m_column_count, 1, m_column_count) - 1;
        idx = seq.next(idx);

        auto rect = grid_rect{left, top, right, bottom};
        if (m_modes_private.DEC_ORIGIN()) {
                // Translate to and intersect with the scrolling region
                rect += m_scrolling_region.origin();
                rect.intersect_or_extend(m_scrolling_region.as_rect());
        }
        #if 0
        // unnecessary since the coords were already clipped above
        else {
                // clip to the whole screen
                rect &= grid_rect{0, 0, int(m_column_count) - 1, int(m_row_count) - 1};
        }
        #endif

        return rect;
}

/*
 * Command Handlers
 * This is the unofficial documentation of all the VTE_CMD_* definitions.
 * Each handled command has a separate function with an extensive comment on
 * the semantics of the command.
 * Note that many semantics are unknown and need to be verified. This is mostly
 * about error-handling, though. Applications rarely rely on those features.
 */

void
Terminal::NONE(vte::parser::Sequence const& seq)
{
}

void
Terminal::GRAPHIC(vte::parser::Sequence const& seq)
{
#if 0
        struct vte_char ch = VTE_CHAR_NULL;

        if (screen->state.cursor_x + 1 == screen->page->width
            && screen->flags & VTE_FLAG_PENDING_WRAP
            && screen->state.auto_wrap) {
                screen_cursor_down(screen, 1, true);
                screen_cursor_set(screen, 0, screen->state.cursor_y);
        }

        screen_cursor_clear_wrap(screen);

        ch = vte_char_merge(ch, screen_map(screen, seq->terminator));
        vte_page_write(screen->page,
                          screen->state.cursor_x,
                          screen->state.cursor_y,
                          ch,
                          1,
                          &screen->state.attr,
                          screen->age,
                          false);

        if (screen->state.cursor_x + 1 == screen->page->width)
                screen->flags |= VTE_FLAG_PENDING_WRAP;
        else
                screen_cursor_right(screen, 1);

        return 0;
#endif

        insert_char(seq.terminator(), false);
}

void
Terminal::ACK(vte::parser::Sequence const& seq)
{
        /*
         * ACK - acknowledge
         *
         * References: ECMA-48 § 8.3.1
         *             ECMA-16 § 3.1.6
         */

        m_bell_pending = true;
}

void
Terminal::ACS(vte::parser::Sequence const& seq)
{
        /* ACS - announce-code-structure
         *
         * The final byte of the sequence identifies the facility number
         * from 1 to 62 starting with 4/01.
         * DEC uses some final characters in the 3/00..3/15 range for
         * private purposes.
         *
         * References: ECMA-35 § 15.2
         *             DEC VT525
         *             DEC PPLV2
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can mostly ignore this */

        switch (int(seq.terminator()) - 0x40) {
        case -10: /* '6' */
                /* S7C1R/DECTC1 - truncate C1 controls
                 *
                 * Masks the high bit from C1 controls and then
                 * processes them as if received like that.
                 *
                 * References: DEC PPLV2
                 */
                 break;
        case -9: /* '7' */
                /* S8C1R/DECAC1 - accept C1 controls
                 *
                 * Accept both C0 and C1 controls.
                 *
                 * References: DEC PPLV2
                 */
                 break;
        case 6:
                /*
                 * This causes the terminal to start sending C1 controls as 7bit
                 * sequences instead of 8bit C1 controls.
                 * This is ignored if the terminal is below level-2 emulation mode
                 * (VT100 and below), the terminal already sends 7bit controls then.
                 *
                 * References: ECMA-35
                 *             VT525
                 */
#if 0
                if (screen->conformance_level > VTE_CONFORMANCE_LEVEL_VT100)
                        screen->flags |= VTE_FLAG_7BIT_MODE;
#endif
                break;

        case 7:
                /*
                 * This causes the terminal to start sending C1 controls as 8bit C1
                 * control instead of 7bit sequences.
                 * This is ignored if the terminal is below level-2 emulation mode
                 * (VT100 and below). The terminal always sends 7bit controls in those
                 * modes.
                 *
                 * References: ECMA-35
                 *             VT525
                 */
#if 0
                if (screen->conformance_level > VTE_CONFORMANCE_LEVEL_VT100)
                        screen->flags &= ~VTE_FLAG_7BIT_MODE;
#endif
                break;

        case 12:
                /* Use Level 1 of ECMA-43
                 *
                 * Probably not worth implementing.
                 */
                break;
        case 13:
                /* Use Level 2 of ECMA-43
                 *
                 * Probably not worth implementing.
                 *
                 * On a VTxxx, both levels 1 and 2 designate as follows:
                 * G0 = ASCII (IR #6)
                 * G1 = ISO_LATIN1_SUPPLEMENTAL
                 * with G0 mapped to GL, G1 to GR.
                 *
                 * References: VT525
                 */
                break;
        case 14:
                /* Use Level 3 of ECMA-43
                 *
                 * Probably not worth implementing.
                 *
                 * On a VTxxx, this designates as follows:
                 * G0 = ASCII (IR #6)
                 * with G0 mapped to GL.
                 *
                 *
                 * References: VT525
                 */
                break;
        }
}

void
Terminal::BEL(vte::parser::Sequence const& seq)
{
        /*
         * BEL - sound bell tone
         * This command should trigger an acoustic bell.
         *
         * References: ECMA-48 § 8.3.3
         */

        m_bell_pending = true;
}

void
Terminal::BPH(vte::parser::Sequence const& seq)
{
        /*
         * BPH - break permitted here
         *
         * References: ECMA-48 § 8.3.4
         *
         * Not worth implementing.
         */
}

void
Terminal::BS(vte::parser::Sequence const& seq)
{
        /*
         * BS - backspace
         * Move cursor one cell to the left. If already at the left margin,
         * nothing happens.
         *
         * References: ECMA-48 § 8.3.5
         */

#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_left(screen, 1);
#endif

        move_cursor_backward(1);
}

void
Terminal::CBT(vte::parser::Sequence const& seq)
{
        /*
         * CBT - cursor-backward-tabulation
         * Move the cursor @args[0] tabs backwards (to the left). The
         * current cursor cell, in case it's a tab, is not counted.
         * Furthermore, the cursor cannot be moved beyond the left margin
         * and it will stop there.
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.7
         */
#if 0
        screen_cursor_clear_wrap(screen);
#endif

        move_cursor_tab_backward(seq.collect1(0, 1));
}

void
Terminal::CCH(vte::parser::Sequence const& seq)
{
        /*
         * CCH - cancel character
         * Indicates that the CCH and the preceding graphic character
         * (including SPACE (2/0)) in the data stream should be ignored.
         * If CCH is not preceded by a graphic character but by a
         * control function instead, CCH is ignored.
         *
         * References: ECMA-48 § 8.3.8
         *
         * Not worth implementing.
         */
}

void
Terminal::CHA(vte::parser::Sequence const& seq)
{
        /*
         * CHA - cursor-horizontal-absolute
         * Move the cursor to position @args[0] in the current line
         * (presentation).
         * The cursor cannot be moved beyond the rightmost cell; it will
         * stop there.
         *
         * Arguments:
         *   args[0]: column
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.9
         */

#if 0
        unsigned int pos = 1;

        if (seq->args[0] > 0)
                pos = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, pos - 1, screen->state.cursor_y);
#endif

        auto value = seq.collect1(0, 1, 1, m_column_count);
        set_cursor_column1(value);
}

void
Terminal::CHT(vte::parser::Sequence const& seq)
{
        /*
         * CHT - cursor-horizontal-forward-tabulation
         * Move the cursor @args[0] tabs forward (to the right) (presentation).
         * The current cursor cell, in case it's a tab, is not counted.
         * Furthermore, the cursor cannot be moved beyond the right margin
         * and will stop there.
         *
         * Arguments:
         *   args[0]: count
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.10
         */
#if 0
        screen_cursor_clear_wrap(screen);
#endif

        move_cursor_tab_forward(seq.collect1(0, 1));
}

void
Terminal::CMD(vte::parser::Sequence const& seq)
{
        /*
         * CMD - coding method delimiter
         *
         * References: ECMA-35 § 15.3
         *             ECMA-48 § 8.3.11
         *
         * Not worth implementing.
         */
}

void
Terminal::CNL(vte::parser::Sequence const& seq)
{
        /*
         * CNL - cursor-next-line
         * Move the cursor @args[0] lines down, without scrolling, stopping at the bottom margin.
         * Also moves the cursor all the way to the left, stopping at the left margin.
         *
         * Arguments:
         *   args[0]: number of lines
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 §8.3.12
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif

        carriage_return();

        auto value = seq.collect1(0, 1);
        move_cursor_down(value);
}

void
Terminal::CPL(vte::parser::Sequence const& seq)
{
        /*
         * CPL - cursor-preceding-line
         * Move the cursor @args[0] lines up, without scrolling, stoppng at the top margin.
         * Also moves the cursor all the way to the left, stopping at the left margin.
         *
         * Arguments:
         *   args[0]: number of lines
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.13
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_up(screen, num, false);
#endif

        carriage_return();

        auto const value = seq.collect1(0, 1);
        move_cursor_up(value);
}

void
Terminal::CR(vte::parser::Sequence const& seq)
{
        /*
         * CR - carriage-return
         * Move the cursor to the left margin or to the left edge on the current line.
         *
         * References: ECMA-48 § 8.3.15
         */
#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, 0, screen->state.cursor_y);
#endif

        carriage_return();
}

void
Terminal::CTC(vte::parser::Sequence const& seq)
{
        /*
         * CTC - cursor tabulation control
         * Set/clear tabstops.
         *
         * For the cases @args[0] = 0, 2, 4, the effect depends on TSM mode.
         *
         * References: ECMA-48 § 8.3.17
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                /* Set tabstop at the current cursor position */
                m_tabstops.set(get_xterm_cursor_column());
                break;

        case 1:
                /* Sets line tabstop in the ative line (presentation) */
                break;

        case 2:
                /* Clear tabstop at the current cursor position */
                m_tabstops.unset(get_xterm_cursor_column());
                break;

        case 3:
                /* Clear line tabstop in the active line */
                break;

        case 4:
                /* Clear all tabstops in the active line */
                [[fallthrough]];
        case 5:
                /* Clear all tabstops */
                m_tabstops.clear();
                break;

        case 6:
                /* Clear all line tabstops */
                break;

        default:
                break;
        }
}

void
Terminal::CUB(vte::parser::Sequence const& seq)
{
        /*
         * CUB - cursor-backward
         * Move the cursor @args[0] positions to the left. The cursor stops
         * at the left-most position. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.18
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_left(screen, num);
#endif

        auto value = seq.collect1(0, 1);
        move_cursor_backward(value);
}

void
Terminal::CUD(vte::parser::Sequence const& seq)
{
        /*
         * CUD - cursor-down
         * Move the cursor @args[0] positions down. The cursor stops at the
         * bottom margin. If it was already moved further, it stops at the
         * bottom line. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.19
         *             DEC STD 070 page 5-43
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif

        auto value = seq.collect1(0, 1);
        move_cursor_down(value);
}

void
Terminal::CUF(vte::parser::Sequence const& seq)
{
        /*
         * CUF -cursor-forward
         * Move the cursor @args[0] positions to the right. The cursor stops
         * at the right-most position. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.20
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_right(screen, num);
#endif

        auto value = seq.collect1(0, 1);
        move_cursor_forward(value);
}

void
Terminal::CUP(vte::parser::Sequence const& seq)
{
        /*
         * CUP - cursor-position
         * Moves the cursor to position @args[1] x @args[0]. If either is 0, it
         * is treated as 1. The positions are subject to the origin-mode and
         * clamped to the addressable width/height. (presentation)
         *
         * Arguments:
         *   args[0]: line
         *   args[0]: column
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *
         * References: ECMA-48 § 8.3.21
         */
#if 0
        unsigned int x = 1, y = 1;

        if (seq->args[0] > 0)
                y = seq->args[0];
        if (seq->args[1] > 0)
                x = seq->args[1];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set_rel(screen, x - 1, y - 1);
#endif

        /* The first is the row, the second is the column. */
        auto rowvalue = seq.collect1(0, 1, 1, m_row_count);
        auto colvalue = seq.collect1(seq.next(0), 1, 1, m_column_count);
        set_cursor_coords1(rowvalue, colvalue);
}

void
Terminal::CUU(vte::parser::Sequence const& seq)
{
        /*
         * CUU - cursor-up
         * Move the cursor @args[0] positions up. The cursor stops at the
         * top margin. If it was already moved further, it stops at the
         * top line. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.22
         *             DEC STD 070 page 5-41
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_up(screen, num, false);
#endif

        auto const value = seq.collect1(0, 1);
        move_cursor_up(value);
}

void
Terminal::CVT(vte::parser::Sequence const& seq)
{
        /*
         * CVT - cursor line tabulation
         * Move the cursor @args[0] positions down. The cursor stops at the
         * bottom margin. If it was already moved further, it stops at the
         * bottom line. (presentation)
         *
         * Arguments:
         *   args[0]: number of positions
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.23
         */

        /* FIXME: implement this? */
}

void
Terminal::CnD(vte::parser::Sequence const& seq)
{
        /*
         * CnD - Cn-designate
         *
         * Designate a set of control functions.
         *
         * References: ECMA-35 § 14.2
         *             ISO 2375 IR
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can ignore this */
}

void
Terminal::DA1(vte::parser::Sequence const& seq)
{
        /*
         * DA1 - primary-device-attributes
         * The primary DA asks for basic terminal features. We simply return
         * a hard-coded list of features we implement.
         * Note that the primary DA asks for supported features, not currently
         * enabled features.
         *
         * Reply: DECDA1R (CSI ? 65 ; ARGS c)
         *
         * The first argument, 65, is fixed and denotes a VT520 (a Level 5
         * terminal), the last DEC-term that extended this number.
         * All following arguments denote supported features. Note
         * that at most 15 features can be sent (max CSI args). It is safe to
         * send more, but clients might not be able to parse them. This is a
         * client's problem and we shouldn't care. There is no other way to
         * send those feature lists, so we have to extend them beyond 15 in
         * those cases.
         *
         * Known modes:
         *    1: 132 column mode
         *       The 132 column mode is supported by the terminal.
         *    2: printer port
         *       A priner-port is supported and can be addressed via
         *       control-codes.
         *    3: ReGIS graphics
         *       Support for ReGIS graphics is available. The ReGIS routines
         *       provide the "remote graphics instruction set" and allow basic
         *       vector-rendering.
         *    4: Sixel
         *       Support of Sixel graphics is available. This provides access
         *       to the sixel bitmap routines.
         *    6: selective erase
         *       The terminal supports DECSCA and related selective-erase
         *       functions. This allows to protect specific cells from being
         *       erased, if specified.
         *    7: soft character set (DRCS)
         *       TODO: ?
         *    8: user-defined keys (UDKs)
         *       TODO: ?
         *    9: national-replacement character sets (NRCS)
         *       National-replacement character-sets are available.
         *   12: Serbo-Croatian (SCS)
         *       TODO: ?
         *   15: technical character set
         *       The DEC technical-character-set is available.
         *   18: windowing capability
         *       TODO: ?
         *   19: sessions capability
         *       TODO: ?
         *   21: horizontal scrolling
         *       TODO: ?
         *   22: ANSI color
         *       TODO: ?
         *   23: Greek
         *       TODO: ?
         *   24: Turkish
         *       TODO: ?
         *   28: rectangular editing
         *   29: DECterm text locator
         *       TODO: ?
         *   42: ISO Latin-2 character set
         *       TODO: ?
         *   44: PCTerm
         *       TODO: ?
         *   45: soft key mapping
         *       TODO: ?
         *   46: ASCII emulation
         *       TODO: ?
         *
         * Extensions which are implied by the level are not reported explicity
         * (e.g. 6, 8, 15 in level 5).
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.24
         *             VT525
         */

        if (seq.collect1(0, 0) != 0)
                return;

        // When testing, use level 5 (VT525); otherwise be more honest and
        // use level 1 (VT100-ish) since we don't implement some/many of the
        // things the higher level mandates.
        // See https://gitlab.gnome.org/GNOME/vte/-/issues/2724
        auto const level = g_test_flags ? 65 : 61;

        reply(seq, VTE_REPLY_DECDA1R,
              {level,
               1, // 132-column mode
               21, // horizontal scrolling
               22, // colour text
               28 // rectangular editing
              });
}

void
Terminal::DA2(vte::parser::Sequence const& seq)
{
        /*
         * DA2 - secondary-device-attributes
         * The secondary DA asks for the terminal-ID, firmware versions and
         * other non-primary attributes. All these values are
         * informational-only and should not be used by the host to detect
         * terminal features.
         *
         * Reply: DECDA2R (CSI > 65 ; FIRMWARE ; KEYBOARD [; OPTION…]* c)
         * where 65 is fixed for VT525 color terminals, the last terminal-line that
         * increased this number (64 for VT520). FIRMWARE is the firmware
         * version encoded as major/minor (20 == 2.0) and KEYBOARD is 0 for STD
         * keyboard and 1 for PC keyboards. None or more OPTION values may
         * be present, indicating which options are installed in the device.
         *
         * We replace the firmware-version with the VTE version so clients
         * can decode it again.
         *
         * References: VT525
         *             DECSTD 070 p4–24
         */

        /* Param != 0 means this is a reply, not a request */
        if (seq.collect1(0, 0) != 0)
                return;

        // When testing, use level 5 (VT525); otherwise be more honest and
        // use level 1 (VT100-ish) since we don't implement some/many of the
        // things the higher level mandates.
        // See https://gitlab.gnome.org/GNOME/vte/-/issues/2724
        auto const level = g_test_flags ? 65 : 61;

        reply(seq, VTE_REPLY_DECDA2R, {level, firmware_version(), 1});
}

void
Terminal::DA3(vte::parser::Sequence const& seq)
{
        /*
         * DA3 - tertiary-device-attributes
         * The tertiary DA is used to query the terminal-ID.
         *
         * Reply: DECRPTUI
         *   DATA: four pairs of hexadecimal digits, encoded 4 bytes.
         *   The first byte denotes the manufacturing site, the remaining
         *   three is the terminal's ID.
         *
         * We always reply with '~VTE' encoded in hex.
         */

        if (seq.collect1(0, 0) != 0)
                return;

        reply(seq, VTE_REPLY_DECRPTUI, {});
}

void
Terminal::DAQ(vte::parser::Sequence const& seq)
{
        /*
         * DAQ - define area qualification
         *
         * Arguments:
         *   args[0]: type
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.25, § 6.5.2
         */
}

void
Terminal::DC1(vte::parser::Sequence const& seq)
{
        /*
         * DC1 - device-control-1 or XON
         * This clears any previous XOFF and resumes terminal-transmission.
         *
         * References: ECMA-48 § 8.3.28
         */

        /* we do not support XON */
}

void
Terminal::DC2(vte::parser::Sequence const& seq)
{
        /*
         * DC2 - device-control-2
         *
         * References: ECMA-48 § 8.3.29
         *
         * Not implemented.
         */
}

void
Terminal::DC3(vte::parser::Sequence const& seq)
{
        /*
         * DC3 - device-control-3 or XOFF
         * Stops terminal transmission. No further characters are sent until
         * an XON is received.
         *
         * References: ECMA-48 § 8.3.30
         */

        /* we do not support XOFF */
}

void
Terminal::DC4(vte::parser::Sequence const& seq)
{
        /*
         * DC4 - device-control-4
         *
         * References: ECMA-48 § 8.3.31
         *
         * Not implemented.
         */
}

void
Terminal::DCH(vte::parser::Sequence const& seq)
{
        /*
         * DCH - delete-character
         * This deletes @argv[0] characters at the current cursor position.
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.26
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_delete_cells(screen->page,
                                 screen->state.cursor_x,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        /* If the cursor (xterm-like interpretation when about to wrap) is horizontally outside
         * the DECSLRM margins then do nothing. */
        if (cursor_col < m_scrolling_region.left() || cursor_col > m_scrolling_region.right()) {
                return;
        }

        maybe_retreat_cursor();

        auto const count = seq.collect1(0, 1);
        /* Scroll left in a custom region: only the cursor's row, from the cursor to the DECSLRM right margin. */
        auto scrolling_region{m_scrolling_region};
        scrolling_region.set_vertical(cursor_row, cursor_row);
        scrolling_region.set_horizontal(cursor_col, scrolling_region.right());
        scroll_text_left(scrolling_region, count, true /* fill */);
}

void
Terminal::DECAC(vte::parser::Sequence const& seq)
{
        /*
         * DECAC - assign color
         * Assign the color used for normal text.
         *
         * Arguments:
         *   @args[0]: item; 1 for normal text, 2 for the text in the window frame
         *   @args[1]: foreground color palette index (0..15)
         *   @args[2]: background color palette index (0..15)
         *
         * References: VT525
         */

        // FIXMEchpe maybe implement this, allowing our extended color
        // format instead of just palette colors
}

void
Terminal::DECALN(vte::parser::Sequence const& seq)
{
        /*
         * DECALN - screen-alignment-pattern
         * Resets the margins, homes the cursor, and fills the screen
         * with 'E's.
         *
         * References: VT525
         *             DEC STD 070
         */

        m_defaults = m_color_defaults = basic_cell;
        m_scrolling_region.reset();
        m_modes_private.set_DEC_ORIGIN(false);
        home_cursor();

        fill_rect({0, 0, int(m_column_count) - 1, int(m_row_count) - 1},
                  U'E',
                  m_defaults.attr);
}

void
Terminal::DECARR(vte::parser::Sequence const& seq)
{
        /*
         * DECARR - auto repeat rate
         * Sets the key autorepeat rate in from @args[0] in keys/s.
         * 0…5 are mapped to 0/s, 6…15 to 10/s, 16…30 to 30/s.
         * Other values are ignored. The default is 30.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECATC(vte::parser::Sequence const& seq)
{
        /*
         * DECATC - alternate text color
         * Assign the color used for attribute combinations text.
         *
         * Arguments:
         *   @args[0]: selects the attribute combinations from a
         *     value table (0 = normal, 1 = bold, 2 = reverse,
         *     3 = (single) underline, 4 = blink; then 5…15
         *     encode the combinations)
         *   @args[1]: foreground color palette index (0..15)
         *   @args[2]: background color palette index (0..15)
         *
         * References: VT525
         */

        // FIXMEchpe maybe implement this, allowing our extended color
        // format instead of just palette colors
}

void
Terminal::DECAUPSS(vte::parser::Sequence const& seq)
{
        /*
         * DECAUPSS - assign user preferred supplemental sets
         * Sets a supplemental charset as user preferred.
         * Arguments:
         *   @args[0]: charset designator:
         *     0 = DEC, Latin 1/2
         *     1 = Latin 5/7, ISO Cyrillic, ISO Hebrew
         *   DATA: the charset, as in a ECMA-35 charset designation
         *     sequence (sans the ESC); but only some charsets are
         *     supported.
         *
         * Default: DEC Supplemental Graphic set.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECBI(vte::parser::Sequence const& seq)
{
        /*
         * DECBI - back-index
         * This control function moves the cursor backward one column. If the
         * cursor is at the left margin, then all screen data within the margin
         * moves one column to the right. The column that shifted past the right
         * margin is lost.
         * DECBI adds a new column at the left margin with no visual attributes.
         * DECBI does not affect the margins. If the cursor is beyond the
         * left-margin at the left border, then the terminal ignores DECBI.
         */

        maybe_retreat_cursor();
        cursor_left_with_scrolling(true);
}

void
Terminal::DECCARA(vte::parser::Sequence const& seq)
{
        /*
         * DECCARA - change-attributes-in-rectangular-area
         * Change some character attributes (bold, blink, reverse,
         * (single) underline) in the specified rectangle.
         * The characters in the area are unchanged.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *   args[4:]: the character attributes to change; values as in SGR
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *   args[4:]: no defaults
         *
         * If the top > bottom or left > right, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM).
         * Current SGR defaults and cursor position are unchanged.
         * If no parameters after arg[3] are set, clears all attributes (like SGR 0).
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the start and end
         * positions.
         *
         * References: DEC STD 070 page 5-173 f
         *             VT525
         */

        auto idx = 0u;
        auto const rect = collect_rect(seq, idx);
        if (!rect)
                return;

        // Parse the SGR attributes twice, applying them first to
        // an all-unset attr, then to an all-set attr. Combining these
        // obtains a mask and a value that can be applied to each
        // cell's attrs to set them to their new value while preserving
        // any attrs not mentioned in the SGR attributes.

        auto const sgr_idx = idx; // save index
        auto empty = VteCellAttr{.attr = 0, .m_colors = 0};
        vte::parser::collect_sgr(seq, idx, empty);

        idx = sgr_idx; // restore index
        auto full = VteCellAttr{.attr = ~uint32_t{0}, .m_colors = ~uint64_t{0}};
        vte::parser::collect_sgr(seq, idx, full);

        auto const attr_mask = (full.attr & ~empty.attr & VTE_ATTR_ALL_SGR_MASK)
                | ~VTE_ATTR_ALL_SGR_MASK; // make sure not to change non-visual attrs
        auto const attr = empty.attr;
        auto const colors_mask = full.m_colors & ~empty.m_colors;
        auto const colors = empty.m_colors;

        rewrite_rect(rect,
                     m_decsace_is_rectangle,
                     true, // only writing attrs
                     [&](VteCell* cell) constexpr noexcept -> void {
                             auto& cell_attr = cell->attr;
                             cell_attr.attr &= attr_mask;
                             cell_attr.attr ^= attr;

                             cell_attr.m_colors &= colors_mask;
                             cell_attr.m_colors ^= colors;
                     });
}

void
Terminal::DECCKD(vte::parser::Sequence const& seq)
{
        /*
         * DECCKD - copy key default
         * Copy the defaults from one key to another.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECCRA(vte::parser::Sequence const& seq)
{
        /*
         * DECCRA - copy-rectangular-area
         * Copies characters and their attributes from one rectangle to
         * another.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the source rectangle (1-based)
         *   args[4]: source page
         *   args[5..6]: top, left of the target rectangle
         *   args[7]: target page
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *   args[4]: 1
         *   args[5]: 1
         *   args[6]: 1
         *   args[7]: 1
         *
         * If the top > bottom or left > right for either of the rectangles,
         * the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM).
         * Current SGR defaults and cursor position are unchanged.
         *
         * If a page value is greater than the number of available pages,
         * it is treated as the last page (instead of ignoring the whole
         * function).
         *
         * References: DEC STD 070 page 5-169
         *             VT525
         */

        auto idx = 0u;
        auto source_rect = collect_rect(seq, idx);
        if (!source_rect)
                return;

        // auto const source_page = seq.collect1(idx, 1);
        idx = seq.next(idx);

        auto dest_top = seq.collect1(idx, 1, 1, int(m_row_count)) - 1;
        idx = seq.next(idx);
        auto dest_left = seq.collect1(idx, 1, 1, int(m_column_count)) - 1;
        idx = seq.next(idx);

        // auto const dest_page = seq.collect1(idx, 1);

        // dest is subject to origin mode
        auto dest = grid_point{dest_left, dest_top};
        if (m_modes_private.DEC_ORIGIN()) {
                dest += m_scrolling_region.origin();
        }

        // Calculate the destination rect by first moving @source_rect to
        // @dest then intersecting with the scrolling region (in origin mode)
        // or clamping to the whole screen (when not in origin mode)
        auto dest_rect = source_rect.clone().move_to(dest);
        if (m_modes_private.DEC_ORIGIN()) {
                dest_rect.intersect_or_extend(m_scrolling_region.as_rect());
        } else {
                dest_rect &= grid_rect{0, 0, int(m_column_count - 1), int(m_row_count - 1)};
        }

        copy_rect(source_rect.size_to(dest_rect), dest_rect.topleft());
}

void
Terminal::DECCRTST(vte::parser::Sequence const& seq)
{
        /*
         * DECCRTST - CRT saver time
         * Sets the CRT saver timer. When DECCRTSM is set, the
         * screen blanks when the time elapsed since the last
         * keystroke or output is greater than the time set here.
         *
         * Arguments:
         *   args[0]: the time in minutes (0…60) (0 = never)
         *
         * Default: 15
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDC(vte::parser::Sequence const& seq)
{
        /*
         * DECDC - delete-column
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: VT525
         */

        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        /* If the cursor (xterm-like interpretation when about to wrap) is outside
         * the DECSTBM / DECSLRM scrolling region then do nothing. */
        if (!m_scrolling_region.contains_row_col(cursor_row, cursor_col)) {
                return;
        }

        /* As per xterm, do not clear the "about to wrap" state, so no maybe_retreat_cursor() here. */

        auto const count = seq.collect1(0, 1);
        /* Scroll left in a custom region: the left is at the cursor, the rest is according to DECSTBM / DECSLRM. */
        auto scrolling_region{m_scrolling_region};
        scrolling_region.set_horizontal(cursor_col, scrolling_region.right());
        scroll_text_left(scrolling_region, count, true /* fill */);
}

void
Terminal::DECDHL_BH(vte::parser::Sequence const& seq)
{
        /*
         * DECDHL_BH - double-width-double-height-line: bottom half
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDHL_TH(vte::parser::Sequence const& seq)
{
        /*
         * DECDHL_TH - double-width-double-height-line: top half
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDLD(vte::parser::Sequence const& seq)
{
        /*
         * DECDLD - dynamically redefinable character sets extension
         * Loads a soft font for a DRCS charset from SIXEL data
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDLDA(vte::parser::Sequence const& seq)
{
        /*
         * DECDLD - down line load allocation
         * Sets the number of DRCSes allowed per sesion
         * (monochrome terminals only).
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECDMAC(vte::parser::Sequence const& seq)
{
        /*
         * DECDMAC - define-macro
         * Define a macro that can be executed by DECINVM.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECDWL(vte::parser::Sequence const& seq)
{
        /*
         * DECDWL - double-width-single-height-line
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECEFR(vte::parser::Sequence const& seq)
{
        /*
         * DECEFR - enable-filter-rectangle
         * Defines the coordinates of a filter rectangle (top, left, bottom,
         * right as @args[0] to @args[3]) and activates it.
         * Anytime the locator is detected outside of the filter rectangle, an
         * outside rectangle event is generated and the rectangle is disabled.
         * Filter rectangles are always treated as "one-shot" events. Any
         * parameters that are omitted default to the current locator position.
         * If all parameters are omitted, any locator motion will be reported.
         * DECELR always cancels any prevous rectangle definition.
         *
         * The locator is usually associated with the mouse-cursor, but based
         * on cells instead of pixels. See DECELR how to initialize and enable
         * it. DECELR can also enable pixel-mode instead of cell-mode.
         *
         * References: VT525
         *
         * TODO: implement
         */
}

void
Terminal::DECELF(vte::parser::Sequence const& seq)
{
        /*
         * DECELF - enable-local-functions
         * Enable or disable keys to perform local functions like
         * copy/paster, panning and window resize.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECELR(vte::parser::Sequence const& seq)
{
        /*
         * DECELR - enable-locator-reporting
         * This changes the locator-reporting mode. @args[0] specifies the mode
         * to set, 0 disables locator-reporting, 1 enables it continuously, 2
         * enables it for a single report. @args[1] specifies the
         * precision-mode. 0 and 2 set the reporting to cell-precision, 1 sets
         * pixel-precision.
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * References: VT525
         *
         * TODO: implement
         */
}

void
Terminal::DECERA(vte::parser::Sequence const& seq)
{
        /*
         * DECERA - erase-rectangular-area
         * Erases characters in the specified rectangle, replacing
         * them with SPACE (2/0). Character attributes are erased
         * too, but not line attributes (DECDHL, DECDWL).
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *
         * If the top > bottom or left > right, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM).
         * Current SGR defaults and cursor position are unchanged.
         *
         * References: DEC STD 070 page 5-171
         *             VT525
         */

        auto idx = 0u;
        auto const rect = collect_rect(seq, idx);
        if (!rect)
                return; // ignore

        // Like in other erase operations, only use the colours not the other attrs
        auto const erased_cell = m_color_defaults;
        rewrite_rect(rect,
                     true, // as rectangle
                     false, // not only writing attrs
                     [&](VteCell* cell) constexpr noexcept -> void {
                             *cell = erased_cell;
                     });
}

void
Terminal::DECES(vte::parser::Sequence const& seq)
{
        /*
         * DECES - enable session
         * Makes this session active as if by the Session key;
         * that is, makes the session receiving this command the
         * session receiving keyboard input.
         *
         * References: VT525
         *
         * VTE does not support sessions.
         */
}

void
Terminal::DECFI(vte::parser::Sequence const& seq)
{
        /*
         * DECFI - forward-index
         * This control function moves the cursor forward one column. If the
         * cursor is at the right margin, then all screen data within the
         * margins moves one column to the left. The column shifted past the
         * left margin is lost.
         * DECFI adds a new column at the right margin, with no visual
         * attributes. DECFI does not affect margins. If the cursor is beyond
         * the right margin at the border of the page when the terminal
         * receives DECFI, then the terminal ignores DECFI.
         *
         * References: VT525
         */

        /* Unlike the DECBI, IND, RI counterparts, this one usually doesn't clear the
         * "about to wrap" state in xterm. However, it clears it if the cursor is at
         * the right edge of the terminal, beyond the right margin. */
        if (m_screen->cursor.col == m_column_count &&
            m_scrolling_region.right() < m_column_count - 1) {
                maybe_retreat_cursor();
        }
        cursor_right_with_scrolling(true);
}

void
Terminal::DECFNK(vte::parser::Sequence const& seq)
{
        /*
         * DECFNK - function key (or XTERM bracketed paste)
         *
         * References: VT525
         *             XTERM
         */
}

void
Terminal::DECFRA(vte::parser::Sequence const& seq)
{
        /*
         * DECFRA - fill-rectangular-area
         * Fills the specified rectangle with the specified character,
         * replacing the current characters in it. Character attributes
         * are replaced by the current default SGR. Does not change
         * line attributes (DECDHL, DECDWL).
         *
         * Arguments;
         *   args[0]: the decimal value of the replacement character (GL or GR)
         *   args[1..4]: top, left, bottom, right of the rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 32 (U+0020 SPACE)
         *   args[1]: 1
         *   args[2]: 1
         *   args[3]: height of current page
         *   args[4]: width of current page
         *
         * If the top > bottom or left > right, the command is ignored.
         * If the character is not in the GL or GR area, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?). Current SGR defaults
         * and cursor position are unchanged.
         *
         * Note: As a VTE exension, this function accepts any non-zero-width,
         *   non-combining, non-control unicode character.
         *   For characters in the BMP, just use its scalar value as-is for
         *   arg[0].
         *   For characters not in the BMP, you can either
         *   * encode it using a surrogate pair as a ':' delimited
         *     subparameter sequence as arg[0], e.g. using '55358:57240'
         *     for the UTF-16 representation 0xD83E 0xDF98 of the
         *     character U+1FB98 UPPER LEFT TO LOWER RIGHT FILL, or
         *   * encode it as a ':' delimited subparameter sequence containing
         *     the scalar value split into 16-bit chunks in big-endian
         *     order, e.g. using '1:64408' for the same U+1FB98 character.
         *
         * References: DEC STD 070 page 5-170 ff
         *             VT525
         */

        auto c = U' ';
        auto idx = 0u;
        switch (primary_data_syntax()) {
        case DataSyntax::ECMA48_UTF8: {
                if (auto const co = seq.collect_char(idx, U' '))
                        c = *co;
                else
                        return;
                break;
        }

#if WITH_ICU
        case DataSyntax::ECMA48_PCTERM: {
                auto v = seq.param(idx);
                if (v == -1 || v == 0)
                        v = 0x20;
                if (v > 0xff)
                        return;

                try {
                        // Cannot use m_converter directly since it may have saved
                        // state or pending output
                        if (!m_oneoff_decoder)
                                m_oneoff_decoder = vte::base::ICUDecoder::clone(m_converter->decoder());
                        if (!m_oneoff_decoder)
                                return;

                        m_oneoff_decoder->reset();

                        uint8_t const c8 = {uint8_t(v)};
                        auto c8ptr = &c8;
                        if (m_oneoff_decoder->decode(&c8ptr) !=
                            vte::base::ICUDecoder::Result::eSomething ||
                            m_oneoff_decoder->pending())
                                return;

                        c = m_oneoff_decoder->codepoint();
                        // The translated character must not be C0 or C1
                        if (c < 0x20 || (c >= 0x7f && c < 0xa0))
                                return;
                } catch (...) {
                        return;
                }

                break;
        }
#endif // WITH_ICU

        default:
                __builtin_unreachable();
                return;
        }

        idx = seq.next(idx);
        auto const rect = collect_rect(seq, idx);
        if (!rect)
                return; // ignore

        /* fill_rect already checks for width 0, no need to pre-check  */
        if (g_unichar_ismark(c))
                return; // ignore

        // Charset invocation applies to the fill character
        fill_rect(rect,
                  character_replacement(c),
                  m_defaults.attr);
}

void
Terminal::DECIC(vte::parser::Sequence const& seq)
{
        /*
         * DECIC - insert-column
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: VT525
         */

        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        /* If the cursor (xterm-like interpretation when about to wrap) is outside
         * the DECSTBM / DECSLRM scrolling region then do nothing. */
        if (!m_scrolling_region.contains_row_col(cursor_row, cursor_col)) {
                return;
        }

        /* As per xterm, do not clear the "about to wrap" state, so no maybe_retreat_cursor() here. */

        auto const count = seq.collect1(0, 1);
        /* Scroll right in a custom region: the left is at the cursor, the rest is according to DECSTBM / DECSLRM. */
        auto scrolling_region{m_scrolling_region};
        scrolling_region.set_horizontal(cursor_col, scrolling_region.right());
        scroll_text_right(scrolling_region, count, true /* fill */);
}

void
Terminal::DECINVM(vte::parser::Sequence const& seq)
{
        /*
         * DECINVM - invoke-macro
         * Invokes a macro defined by DECDMAC.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECKBD(vte::parser::Sequence const& seq)
{
        /*
         * DECKBD - keyboard-language-selection
         * Selects a keyboard language.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECKPAM(vte::parser::Sequence const& seq)
{
        /*
         * DECKPAM - keypad-application-mode
         * Enables the keypad-application mode. If enabled, the keypad sends
         * special characters instead of the printed characters. This way,
         * applications can detect whether a numeric key was pressed on the
         * top-row or on the keypad.
         * Default is keypad-numeric-mode.
         *
         * References: VT525
         */

        set_mode_private(vte::terminal::modes::Private::eDEC_APPLICATION_KEYPAD, true);
}

void
Terminal::DECKPNM(vte::parser::Sequence const& seq)
{
        /*
         * DECKPNM - keypad-numeric-mode
         * This disables the keypad-application-mode (DECKPAM) and returns to
         * the keypad-numeric-mode. Keypresses on the keypad generate the same
         * sequences as corresponding keypresses on the main keyboard.
         * Default is keypad-numeric-mode.
         *
         * References: VT525
         */
        set_mode_private(vte::terminal::modes::Private::eDEC_APPLICATION_KEYPAD, false);
}

void
Terminal::DECLANS(vte::parser::Sequence const& seq)
{
        /*
         * DECLANS - load answerback message
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECLBAN(vte::parser::Sequence const& seq)
{
        /*
         * DECLBAN - load banner message
         * Loads a banner message that will be displayed in double size
         * characters when the terminal powers up.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECLBD(vte::parser::Sequence const& seq)
{
        /*
         * DECLBD - locator button define
         *
         * References: VT330
         */
}

void
Terminal::DECLFKC(vte::parser::Sequence const& seq)
{
        /*
         * DECLFKC - local-function-key-control
         * Select the action for local function keys.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECLL(vte::parser::Sequence const& seq)
{
        /*
         * DECLL - load-leds
         * Sets the keyboard LEDs when in DECKLHIM mode.
         *
         * Arguments:
         *   args[0]: which LED to change to which state
         *     0: NumLock, CapsLock, ScrollLock off
         *     1, 21: NumLock on/off
         *     2, 22: CapsLock on/off
         *     3, 23: ScrollLock on/off
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECLTOD(vte::parser::Sequence const& seq)
{
        /*
         * DECLTOD - load-time-of-day
         * Sets the clock.
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECPAK(vte::parser::Sequence const& seq)
{
        /*
         * DECPAK - program alphanumeric key
         * Program alphanumeric keys to send different codes or perform actions.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECPCTERM(vte::parser::Sequence const& seq)
{
        /*
         * DECPCTERM - pcterm-mode
         * This enters/exits the PCTerm mode. Default mode is VT-mode. It can
         * also select parameters for scancode/keycode mappings in SCO mode.
         *
         * References: VT525
         *
         * Definitely not worth implementing.
         */
}

void
Terminal::DECPCTERM_OR_XTERM_RPM(vte::parser::Sequence const& seq)
{
        /*
         * There's a conflict between DECPCTERM and XTERM_RPM.
         * XTERM_RPM takes a single argument, DECPCTERM takes 2.
         * Note that since both admit default values (which may be
         * omitted at the end of the sequence), this only an approximation.
         */
        if (seq.size_final() <= 1)
                XTERM_RPM(seq);
        #ifdef PARSER_INCLUDE_NOP
        else
                DECPCTERM(seq);
        #endif
}

void
Terminal::DECPFK(vte::parser::Sequence const& seq)
{
        /*
         * DECPFK - program function key
         * Program function keys to send different codes or perform actions.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECPKA(vte::parser::Sequence const& seq)
{
        /*
         * DECPKA - program-key-action
         * Sets whether DECPFK, DECPAK, DECCD, DECUDK can reprogram keys.
         *
         * Arguments:
         *   args[0]:
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECPKFMR(vte::parser::Sequence const& seq)
{
        /*
         * DECPKFMR - program-key-free-memory-report
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECPS(vte::parser::Sequence const& seq)
{
        /*
         * DECPS - play sound
         * Plays a note. Arguments:
         *   @args[0]: the volume. 0 = off, 1…3 = low, 4…7 = high
         *   @args[1]: the duration, in multiples of 1s/32
         *   @args[2..]: the note(s); from 1 = C5, 2 = C♯5 … to 25 = C7
         *
         * Defaults:
         *   @args[0]: no default
         *   @args[1]: no default
         *   @args[2..]: no default
         *
         * Note that a VT525 is specified to store only 16 notes at a time.
         *
         * Note that while the VT520/525 programming manual documents the
         * DECPS sequence on page 5-89 with only one note, in the Setup
         * section on page 2-60 it shows the sequence taking multiple notes
         * (likely up to the maximum number or parameters the VT525
         * supports in CSI sequences, which is at least 16 as per DEC STD 070).
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECRARA(vte::parser::Sequence const& seq)
{
        /*
         * DECRARA - reverse-attributes-in-rectangular-area
         * Reverse some character attributes (bold, blink, reverse,
         * (single) underline) in the specified rectangle.
         * The characters in the area are unchanged, as are the
         * other character attributes.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *   args[4:]: the character attributes to change; values as in SGR
         *     except that only bold, blink, reverse, (single) underline are
         *     supported; 0 to reverse all of these.
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *   args[4:]: no defaults
         *
         * If the top > bottom or left > right, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM).
         * Current SGR defaults and cursor position are unchanged.
         *
         * Note: DECSACE selects whether this function operates on the
         * rectangular area or the data stream between the start and end
         * positions.
         *
         * References: DEC STD 070 page 5-175 f
         *             VT525
         */

        auto idx = 0u;
        auto const rect = collect_rect(seq, idx);
        if (!rect)
                return;

        // Without SGR params this is a no-op (instead of setting all attributes!)
        if (idx >= seq.size())
                return;


        // Note that using an an SGR attributes that unsets some attribute
        // should be ignored; e.g. a DECCARA 3;23 should be the same as a
        // DECCARA 3.

        auto mask = VteCellAttrReverseMask{};
        vte::parser::collect_sgr(seq, idx, mask);
        if (!mask)
                return; // nothing to do

        // Make sure to only change visual attributes
        mask.attr &= VTE_ATTR_ALL_SGR_MASK;

        // As per DEC STD 070, DECRARA only supports bold, underline,
        // blink, and reverse attributes unless they are part of a
        // well-defined extension. Vte provides such an extension in
        // that it allows any SGR attributes here (except colours).
        // However, specifically exclude invisible from the supported
        // attrs so that an DECRARA 0 doesn't turn all text invisible.
        mask.attr &= ~VTE_ATTR_INVISIBLE_MASK;

        rewrite_rect(rect,
                     m_decsace_is_rectangle,
                     true, // only writing attrs
                     [&](VteCell* cell) constexpr noexcept -> void {
                             // While vte has different underline styles
                             // selected by subparameters of SGR 4, reversing
                             // underline only toggles between any underline
                             // to no-underline and v.v.

                             // Need to handle attrs that occupy more than
                             // 1 bit specially by normalising their non-zero
                             // values to all-1, so that the ^ can reverse the
                             // value correctly.

                             auto& attr = cell->attr;
                             if (attr.underline() && (mask.attr & VTE_ATTR_UNDERLINE_MASK))
                                     attr.set_underline(VTE_ATTR_UNDERLINE_VALUE_MASK);

                             attr.attr ^= mask.attr;
                     });
}

void
Terminal::DECRC(vte::parser::Sequence const& seq)
{
        /*
         * DECRC - restore-cursor
         * Restores the terminal to the state saved by the save cursor (DECSC)
         * function. If there was not a previous DECSC, then this does:
         *   * Home the cursor
         *   * Resets DECOM
         *   * Resets the SGR attributes
         *   * Designates ASCII (IR #6) to GL, and DEC Supplemental Graphics to GR
         *
         * Note that the status line has its own DECSC buffer.
         *
         * References: VT525
         */
#if 0
        screen_restore_state(screen, &screen->saved);
#endif

        restore_cursor();
}

void
Terminal::DECREGIS(vte::parser::Sequence const& seq)
{
        /*
         * DECREGIS - ReGIS graphics
         *
         * References: VT330
         */
}

void
Terminal::DECREQTPARM(vte::parser::Sequence const& seq)
{
        /*
         * DECREQTPARM - request-terminal-parameters
         * The sequence DECREPTPARM is sent by the terminal controller to notify
         * the host of the status of selected terminal parameters. The status
         * sequence may be sent when requested by the host or at the terminal's
         * discretion. DECREPTPARM is sent upon receipt of a DECREQTPARM.
         *
         * If @args[0] is 0, this marks a request and the terminal is allowed
         * to send DECREPTPARM messages without request. If it is 1, the same
         * applies but the terminal should no longer send DECREPTPARM
         * unrequested.
         * 2 and 3 mark a report, but 3 is only used if the terminal answers as
         * an explicit request with @args[0] == 1.
         *
         * The other arguments are ignored in requests, but have the following
         * meaning in responses:
         *   args[1]: 1=no-parity-set 4=parity-set-and-odd 5=parity-set-and-even
         *   args[2]: 1=8bits-per-char 2=7bits-per-char
         *   args[3]: transmission-speed
         *   args[4]: receive-speed
         *   args[5]: 1=bit-rate-multiplier-is-16
         *   args[6]: This value communicates the four switch values in block 5
         *            of SETUP B, which are only visible to the user when an STP
         *            option is installed. These bits may be assigned for an STP
         *            device. The four bits are a decimal-encoded binary number.
         *            Value between 0-15.
         *
         * The transmission/receive speeds have mappings for number => bits/s
         * which are quite weird. Examples are: 96->3600, 112->9600, 120->19200
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT100
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                #if 0
                screen->flags &= ~VTE_FLAG_INHIBIT_TPARM;
                #endif
                reply(seq, VTE_REPLY_DECREPTPARM,
                      {2, 1, 1, 120, 120, 1, 0});
                break;
        case 1:
                #if 0
                screen->flags |= VTE_FLAG_INHIBIT_TPARM;
                #endif
                reply(seq, VTE_REPLY_DECREPTPARM,
                      {3, 1, 1, 120, 120, 1, 0});
                break;
        case 2:
        case 3:
                /* This is a report, not a request */
        default:
                break;
        }
}

void
Terminal::DECREQTPARM_OR_WYCDIR(vte::parser::Sequence const& seq)
{
        /*
         * There's a conflict between DECREQTPERM and WYCDIR.
         * A DECTPARM request (_not_ response!) only has at most one
         * parameter, while WYCDIR takes three. Although both
         * commands admit default values to all parameters, using
         * the number of parameters to disambiguate should be good
         * enough here.
         */
        if (seq.size_final() <= 1)
                DECREQTPARM(seq);
#ifdef PARSER_INCLUDE_NOP
        else
                WYCDIR(seq);
#endif
}

void
Terminal::DECRQCRA(vte::parser::Sequence const& seq)
{
        /*
         * DECRQCRA - request checksum of rectangular area
         * Computes a simple checksum of the characters in the rectangular
         * area. args[0] is an identifier, which the response must use.
         * args[1] is the page number; if it's 0 or default then the
         * checksum is computed over all pages; if it's greater than the
         * number of pages, then the checksum is computed only over the
         * last page. args[2]..args[5] describe the area to compute the
         * checksum from, denoting the top, left, bottom, right, resp
         * (1-based). It's required that top ≤ bottom, and left ≤ right.
         * These coordinates are interpreted according to origin mode.
         *
         * NOTE: Since this effectively allows to read the screen
         * (by using a 1x1 rectangle on each cell), we normally only
         * send a dummy reply, and only reply with the actual checksum
         * when in test mode.
         *
         * Defaults:
         *   args[0]: no default
         *   args[1]: 0
         *   args[2]: 1
         *   args[3]: 1
         *   args[4]: height of current page
         *   args[5]: width of current page
         *
         * Reply: DECCKSR
         *   @args[0]: the identifier from the request
         *   DATA: the checksum as a 4-digit hex number
         *
         * References: VT525
         *             XTERM
         */

        unsigned int idx = 0;
        int id = seq.collect1(idx);

#if !VTE_DEBUG
        /* Send a dummy reply */
        return reply(seq, VTE_REPLY_DECCKSR, {id}, "0000");
#else

        /* Not in test mode? Send a dummy reply */
        if ((g_test_flags & VTE_TEST_FLAG_DECRQCRA) == 0) {
                return reply(seq, VTE_REPLY_DECCKSR, {id}, "0000");
        }

        idx = seq.next(idx);

        /* We only support 1 'page', so ignore args[1] */
        idx = seq.next(idx);

        auto checksum = 0u;
        if (auto rect = collect_rect(seq, idx))
                checksum = checksum_area(rect);
        else
                checksum = 0; /* empty area */

        reply(seq, VTE_REPLY_DECCKSR, {id}, "%04X", checksum);
#endif /* VTE_DEBUG */
}

void
Terminal::DECRQDE(vte::parser::Sequence const& seq)
{
        /*
         * DECRQDE - request-display-extent
         * Request how much of the current page is shown on screen.
         *
         * Reply: DECRPDE
         *   Arguments:
         *     args[0]: the number of lines of page memory being displayed
         *     args[1]: the number of columns of page memory being displayed
         *     args[2]: the first column being displayed
         *     args[3]: the first line being displayed
         *     args[4]: the page being displayed
         *
         * References: DEC STD 070 p5–88
         *             VT525
         */

        reply(seq, VTE_REPLY_DECRPDE, {int(m_row_count),
                                       int(m_column_count),
                                       1, // column
                                       1, // row
                                       1}); // page
}

void
Terminal::DECRQKT(vte::parser::Sequence const& seq)
{
        /*
         * DECRQKT - request-key-type
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECRQLP(vte::parser::Sequence const& seq)
{
        /*
         * DECRQLP - request-locator-position
         * See DECELR for locator-information.
         *
         * References: VT525
         *
         * TODO: document and implement
         */
}

void
Terminal::DECRQM_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * DECRQM_ECMA - request-mode-ecma
         * The host sends this control function to find out if a particular mode
         * is set or reset. The terminal responds with a report mode function.
         * @args[0] contains the mode to query.
         *
         * Response is DECRPM with the first argument set to the mode that was
         * queried, second argument is 0 if mode is invalid, 1 if mode is set,
         * 2 if mode is not set (reset), 3 if mode is permanently set and 4 if
         * mode is permanently not set (reset):
         *   ECMA: ^[ MODE ; VALUE $ y
         *   DEC:  ^[ ? MODE ; VALUE $ y
         *
         * References: VT525
         */

        auto const param = seq.collect1(0);
        auto const mode = m_modes_ecma.mode_from_param(param);

        int value;
        switch (mode) {
        case vte::terminal::modes::ECMA::eUNKNOWN:      value = 0; break;
        case vte::terminal::modes::ECMA::eALWAYS_SET:   value = 3; break;
        case vte::terminal::modes::ECMA::eALWAYS_RESET: value = 4; break;
        default: assert(mode >= 0); value = m_modes_ecma.get(mode) ? 1 : 2; break;
        }

        _vte_debug_print(VTE_DEBUG_MODES,
                         "Reporting mode %d (%s) is %d\n",
                         param, m_modes_ecma.mode_to_cstring(mode),
                         value);

        reply(seq, VTE_REPLY_DECRPM_ECMA, {param, value});
}

void
Terminal::DECRQM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * DECRQM_DEC - request-mode-dec
         * Same as DECRQM_ECMA but for DEC modes.
         *
         * References: VT525
         */

        auto const param = seq.collect1(0);
        auto const mode = m_modes_private.mode_from_param(param);

        int value;
        switch (mode) {
        case vte::terminal::modes::Private::eUNKNOWN:      value = 0; break;
        case vte::terminal::modes::Private::eALWAYS_SET:   value = 3; break;
        case vte::terminal::modes::Private::eALWAYS_RESET: value = 4; break;
        default: assert(mode >= 0); value = m_modes_private.get(mode) ? 1 : 2; break;
        }

        _vte_debug_print(VTE_DEBUG_MODES,
                         "Reporting private mode %d (%s) is %d\n",
                         param, m_modes_private.mode_to_cstring(mode),
                         value);

        reply(seq, VTE_REPLY_DECRPM_DEC, {param, value});
}

void
Terminal::DECRQPKFM(vte::parser::Sequence const& seq)
{
        /*
         * DECRQPKFM - request-program-key-free-memory
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECRQPSR(vte::parser::Sequence const& seq)
{
        /*
         * DECRQPSR - request-presentation-state-report
         * Requests a report of the terminal state, that can later
         * be restored with DECRSPS.
         *
         * References: VT525
         *             DEC STD 070 p5–197ff
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
        default:
                /* Ignore request and send no report */
                break;

        case 1:
                /* Cursor information report. This contains:
                 *   - the cursor position, including character attributes and
                 *     character protection attribute,
                 *   - origin mode (DECOM),
                 *   - the character sets designated to the G0, G1, G2, and G3 sets.
                 *
                 * Reply: DECCIR
                 *   DATA: report in the format specified in DEC STD 070 p5–200ff
                 */
                // For now, send an error report
                reply(seq, VTE_REPLY_DECPSR, {0});
                break;

        case 2:
                /* Tabulation Stop information report.
                 *
                 * Reply: DECTABSR
                 *   DATA: report in the format specified in DEC STD 070 p5–204
                 */
                // For now, send an error report
                reply(seq, VTE_REPLY_DECPSR, {0});
                break;
        }
}

void
Terminal::DECRQSS(vte::parser::Sequence const& seq)
{
        /*
         * DECRQSS - request selection or setting
         * The DATA string contains the intermediate(s) and final
         * character of a CSI sequence that codes for which
         * selection or setting to report.
         *
         * Reply: DECRPSS
         *   @args[0]: 1 if the request was valid, otherwise 0
         *   DATA: the current value of the selection or setting
         *
         * Note that the VT525 documentation is buggy, is says it
         *   sends 0 for a valid and 1 or an invalid request; we
         *   follow the STD 070 and XTERM behaviour.
         *
         * References: VT525
         */

        /* Use a subparser to get the command from the request */
        vte::parser::Parser parser{};
        parser.feed(0x9b); /* CSI */

        int rv = VTE_SEQ_NONE;

        /* If at the end, the parser returns a VTE_SEQ_CSI sequence,
         * we interpret that; otherwise we ignore the request and
         * send only a dummy reply.
         * Note that this makes sure there is only one setting
         * requested; if there were more than one, the parser would
         * parse them as GRAPHIC and thus we reply 'invalid'.
         */
        auto const str = seq.string();
        size_t i;
        for (i = 0; i < str.size(); ++i) {
                auto const c = str[i];
                if (c < 0x20 || c >= 0x7f)
                        break;
                rv = parser.feed(c);
        }

        vte::parser::Sequence request{parser};
        /* If not the whole string was parsed, or the sequence
         * is not a CSI sequence, or it has parameters, reject
         * the request as invalid.
         */
        if (i != str.size() || rv != VTE_SEQ_CSI || request.size() > 0 /* any parameters */)
                return reply(seq, VTE_REPLY_DECRPSS, {0});

        switch (request.command()) {

        case VTE_CMD_DECSACE:
                return reply(seq, VTE_REPLY_DECRPSS, {1},
                             {VTE_REPLY_DECSACE, {m_decsace_is_rectangle ? 2 : 0 /* or 1 */}});

        case VTE_CMD_DECSCUSR:
                return reply(seq, VTE_REPLY_DECRPSS, {1}, {VTE_REPLY_DECSCUSR, {int(m_cursor_style)}});

        case VTE_CMD_DECSGR: {
                auto builder = vte::parser::ReplyBuilder(VTE_REPLY_DECSGR, {});
                append_attr_decsgr_params(m_defaults.attr, builder);
                return reply(seq, VTE_REPLY_DECRPSS, {1},
                             std::move(builder));
        }

        case VTE_CMD_DECSTBM:
                return reply(seq, VTE_REPLY_DECRPSS, {1},
                             {VTE_REPLY_DECSTBM, {m_scrolling_region.top() + 1,
                                                  m_scrolling_region.bottom() + 1}});

        case VTE_CMD_DECSLPP:
        case VTE_CMD_DECSLPP_OR_XTERM_WM:
                return reply(seq, VTE_REPLY_DECRPSS, {1},
                             {VTE_REPLY_DECSLPP, {int(m_row_count)}});

        case VTE_CMD_DECSLRM:
        case VTE_CMD_DECSLRM_OR_SCOSC:
                return reply(seq, VTE_REPLY_DECRPSS, {1},
                             {VTE_REPLY_DECSLRM, {m_scrolling_region.left() + 1,
                                                  m_scrolling_region.right() + 1}});

        case VTE_CMD_SGR: {
                auto builder = vte::parser::ReplyBuilder(VTE_REPLY_SGR, {});
                append_attr_sgr_params(m_defaults.attr, builder);
                return reply(seq, VTE_REPLY_DECRPSS, {1},
                             std::move(builder));
        }

        case VTE_CMD_DECAC:
        case VTE_CMD_DECARR:
        case VTE_CMD_DECATC:
        case VTE_CMD_DECCRTST:
        case VTE_CMD_DECDLDA:
        case VTE_CMD_DECSASD:
        case VTE_CMD_DECSCA:
        case VTE_CMD_DECSCL:
        case VTE_CMD_DECSCP:
        case VTE_CMD_DECSCPP:
        case VTE_CMD_DECSCS:
        case VTE_CMD_DECSDDT:
        case VTE_CMD_DECSDPT:
        case VTE_CMD_DECSEST:
        case VTE_CMD_DECSFC:
        case VTE_CMD_DECSKCV:
        case VTE_CMD_DECSLCK:
        case VTE_CMD_DECSMBV:
        case VTE_CMD_DECSNLS:
        case VTE_CMD_DECSPMA:
        case VTE_CMD_DECSPP:
        case VTE_CMD_DECSPPCS:
        case VTE_CMD_DECSPRTT:
        case VTE_CMD_DECSSCLS:
        case VTE_CMD_DECSSDT:
        case VTE_CMD_DECSSL:
        case VTE_CMD_DECSTGLT:
        case VTE_CMD_DECSTRL:
        case VTE_CMD_DECSWBV:
        case VTE_CMD_DECSZS:
        case VTE_CMD_DECTME:
        case VTE_CMD_XTERM_MODKEYS:
        default:
                return reply(seq, VTE_REPLY_DECRPSS, {0});
        }
}

void
Terminal::DECRQTSR(vte::parser::Sequence const& seq)
{
        /*
         * DECRQTSR - request-terminal-state-report
         * Requests a report of the terminal state, that can later
         * be restored by DECRSTS.
         *
         * References: VT525
         *             DEC STD 070 p5–206ff
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
        default:
                /* Ignore, send no report*/
                break;

        case 1:
                /* DECTSR – Terminal state request
                 *
                 * Reply: DECTSR
                 *   DATA: report in an unspecified format
                 */
                // For now, send an error report
                return reply(seq, VTE_REPLY_DECTSR, {0});

        case 2:
                /* DECCTR – Color table request
                 *
                 * Arguments:
                 *   args[1]: color coordinate system
                 *     0: invalid
                 *     1: HLS (0…360, 0…100, 0…100)
                 *     2: RGB (0…100, 0…100, 0…100) (yes, really!)
                 *
                 * Reply: DECCTR
                 *   DATA: report in an unspecified format
                 */
                // For now, send an error report
                return reply(seq, VTE_REPLY_DECTSR, {0});
        }
}

void
Terminal::DECRQUPSS(vte::parser::Sequence const& seq)
{
        /*
         * DECRQUPSS - request-user-preferred-supplemental-set
         * Requests the user-preferred supplemental set.
         *
         * Reply: DECAUPSS
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */

        // FIXMEchpe send a dummy reply?
}

void
Terminal::DECRSPS(vte::parser::Sequence const& seq)
{
        /*
         * DECRSPS - restore presentation state
         * Restores terminal state from a DECRQPSR response.
         *
         * References: VT525
         *             DEC STD 070 p5–197ff
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
        default:
                /* Error; ignore */
                break;

        case 1:
                /* Cursor information report*/
                break;

        case 2:
                /* Tabstop report */
                break;
        }
}

void
Terminal::DECRSTS(vte::parser::Sequence const& seq)
{
        /*
         * DECRSTS - restore terminal state
         * Restore terminal state from a DECRQTSR response.
         *
         * References: VT525
         *             DEC STD 070 p5–206ff
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                /* Ignore */
                break;

        case 1:
                /* Terminal state report */
                break;

        case 2:
                /* Color table report */
                break;

        default:
                break;
        }
}

void
Terminal::DECSACE(vte::parser::Sequence const& seq)
{
        /*
         * DECSACE - select-attribute-change-extent
         * Selects which positions the DECCARA and DECRAR rectangle
         * commands affects.
         *
         * Arguments:
         *   args[0]:
         *     0, 1: the stream of positions beginning at the
         *           (top, left) and ending at the (bottom, right)
         *           position
         *     2: the positions in the rectangle with corners
         *        (top, left) and (bottom, right)
         *
         * Defaults;
         *   args[0]: 0
         *
         * References: DEC STD 070 page 5-177 f
         *             VT525
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
        case 1:
                m_decsace_is_rectangle = false;
                break;
        case 2:
                m_decsace_is_rectangle = true;
                break;
        default:
                break;
        }
}

void
Terminal::DECSASD(vte::parser::Sequence const& seq)
{
        /*
         * DECSASD - select-active-status-display
         * Selects between main screen and status line.
         *
         * Arguments:
         *   args[0]:
         *     0: main screen
         *     1: status line
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSC(vte::parser::Sequence const& seq)
{
        /*
         * DECSC - save-cursor
         * Save cursor and terminal state so it can be restored later on.
         * This stores:
         *   * Cursor position
         *   * SGR attributes
         *   * Charset designations for GL and GR
         *   * Wrap flag
         *   * DECOM state
         *   * Selective erase attribute
         *   * Any SS2 or SS3 sent
         *
         * References: VT525
         */
#if 0
        screen_save_state(screen, &screen->saved);
#endif

        save_cursor();
}

void
Terminal::DECSCA(vte::parser::Sequence const& seq)
{
        /*
         * DECSCA - select character protection attribute
         * Sets whether characters inserted are protected or not.
         * Protected characters will not be erased by DECSED or DECSEL.
         * SGR attributes are unchanged.
         *
         * Arguments:
         *   args[0]:
         *     0, 2: not protected
         *     1: protected
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         */
#if 0
        unsigned int mode = 0;

        if (seq->args[0] > 0)
                mode = seq->args[0];

        switch (mode) {
        case 0:
        case 2:
                screen->state.attr.protect = 0;
                break;
        case 1:
                screen->state.attr.protect = 1;
                break;
        }
#endif
}

void
Terminal::DECSCL(vte::parser::Sequence const& seq)
{
        /*
         * DECSCL - select-conformance-level
         * Select the terminal's operating level. The factory default is
         * level 4 (VT Level 4 mode, 7-bit controls).
         * When you change the conformance level, the terminal performs a hard
         * reset (RIS).
         *
         * @args[0] defines the conformance-level, valid values are:
         *   61: Level 1 (VT100)
         *   62: Level 2 (VT200)
         *   63: Level 3 (VT300)
         *   64: Level 4 (VT400)
         * @args[1] defines the 8bit-mode, valid values are:
         *    0: 8-bit controls
         *    1: 7-bit controls
         *    2: 8-bit controls (same as 0)
         *
         * If @args[0] is 61, then @args[1] is ignored and 7bit controls are
         * enforced.
         *
         * Defaults:
         *   args[0]: 64
         *   args[1]: 0
         *
         * When not using private colour registers, this
         * must also clear (assign to black) all SIXEL
         * colour registers. (DEC PPLV2 § 5.8)
         *
         * References: VT525
         */
#if 0
        unsigned int level = 64, bit = 0;

        if (seq->n_args > 0) {
                level = seq->args[0];
                if (seq->n_args > 1)
                        bit = seq->args[1];
        }

        vte_screen_hard_reset(screen);

        switch (level) {
        case 61:
                screen->conformance_level = VTE_CONFORMANCE_LEVEL_VT100;
                screen->flags |= VTE_FLAG_7BIT_MODE;
                break;
        case 62 ... 69:
                screen->conformance_level = VTE_CONFORMANCE_LEVEL_VT400;
                if (bit == 1)
                        screen->flags |= VTE_FLAG_7BIT_MODE;
                else
                        screen->flags &= ~VTE_FLAG_7BIT_MODE;
                break;
        }
#endif
}

void
Terminal::DECSCP(vte::parser::Sequence const& seq)
{
        /*
         * DECSCP - select-communication-port
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSCPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSCPP - select-columns-per-page
         * Select columns per page. The number of rows is unaffected by this.
         * @args[0] selectes the number of columns (width), DEC only defines 80
         * and 132, but we allow any integer here. 0 is equivalent to 80.
         * Page content is *not* cleared and the cursor is left untouched.
         * However, if the page is reduced in width and the cursor would be
         * outside the visible region, it's set to the right border. Newly added
         * cells are cleared. No data is retained outside the visible region.
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * FIXMEchpe: implement this instead of deprecated DECCOLM
         */
}

void
Terminal::DECSCS(vte::parser::Sequence const& seq)
{
        /*
         * DECSCS - select-communication-speed
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSCUSR(vte::parser::Sequence const& seq)
{
        /*
         * DECSCUSR - set-cursor-style
         * This changes the style of the cursor. @args[0] can be one of:
         *   0, 1: blinking block
         *      2: steady block
         *      3: blinking underline
         *      4: steady underline
         *      5: blinking ibeam (XTERM)
         *      6: steady ibeam (XTERM)
         * Changing this setting does _not_ affect the cursor visibility itself.
         * Use DECTCEM for that.
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525 5–126
         *             XTERM
         */

        auto param = seq.collect1(0, 0);
        switch (param) {
        case 0 ... 6:
                set_cursor_style(CursorStyle(param));
                break;
        default:
                break;
        }
}

void
Terminal::DECSDDT(vte::parser::Sequence const& seq)
{
        /*
         * DECSDDT - select-disconnect-delay-time
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSDPT(vte::parser::Sequence const& seq)
{
        /*
         * DECSDPT - select-digital-printed-data-type
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSED(vte::parser::Sequence const& seq)
{
        /*
         * DECSED - selective-erase-in-display
         *
         * Erases (some or all of, depending on args[0]) the erasable
         * characters in the display, i.e. those which have the
         * Selectively Erasable attribute set. Characters written with
         * the Selectively Erasable attribute reset, and empty character
         * positions, are not affected.
         * Line attributes are not changed by this function.
         * This function is not affected by the scrolling margins.
         *
         * Arguments:
         *   args[0]: mode
         *     0 = erase from the cursor position to the end of the screen
         *         (inclusive)
         *     1 = erase from the beginning of the screen to the cursor
         *         position (inclusive)
         *     2 = erase display
         *
         * Defaults:
         *   args[0]: 0
         *
         * This function is not affected by the scrolling margins.
         *
         * References: DEC STD 070 page 5-162 ff
         *             DEC VT 525
         */

        erase_in_display(seq);
}

void
Terminal::DECSEL(vte::parser::Sequence const& seq)
{
        /*
         * DECSEL - selective-erase-in-line
         *
         * Erases (some or all of, depending on args[0]) the erasable
         * characters in the active line, i.e. those which have the
         * Selectively Erasable attribute set. Characters written with
         * the Selectively Erasable attribute reset, and empty character
         * positions, are not affected.
         * Line attributes are not changed by this function.
         * This function is not affected by the scrolling margins.
         *
         * Arguments: mode
         *   args[0]: which character positions to erase
         *     0: from the active position to the end of the line (inclusive)
         *     1: from the start of the line to the active position (inclusive)
         *     2: all positions on the active line
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: DEC STD 070 page 5-159 ff
         *             DEC VT 525
         */

        erase_in_line(seq);
}

void
Terminal::DECSERA(vte::parser::Sequence const& seq)
{
        /*
         * DECSERA - selective-erase-rectangular-area
         * Erases the erasable characters in the rectangle, i.e. those which
         * have the Selectively Erasable attribute set. Characters written
         * with the Selectively Erasable attribute reset, and empty character
         * positions, are not affected.
         * Line attributes are not changed by this function.
         * This function is not affected by the scrolling margins.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the source rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *
         * If the top > bottom or left > right the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?). Current SGR defaults
         * and cursor position are unchanged.
         *
         * References: DEC STD 070 page 5-172
         *             VT525
         */

        // Note that this function still differs from DECERA in
        // that DECERA also erases the attributes (replacing them
        // with defaults) while DECSERA only erases the characters
        // and keeps the attributes.

        auto idx = 0u;
        auto const rect = collect_rect(seq, idx);
        if (!rect)
                return; // ignore

        rewrite_rect(rect,
                     true, // as rectangle
                     false, // not only writing attrs
                     [&](VteCell* cell) constexpr noexcept -> void {
                             // We don't implement the protected attribute, so treat
                             // all cells as unprotected.

                             cell->c = ' ';
                             cell->attr.set_columns(1);
                             cell->attr.set_fragment(false);
                     });
}

void
Terminal::DECSEST(vte::parser::Sequence const& seq)
{
        /*
         * DECSEST - energy saver time
         * Sets the enerty saver timer. When DECCRTSM is set, the
         * screen switches to suspend mode when the time elapsed
         * since the last keystroke or output is greater than the
         * time set here.
         *
         * Arguments:
         *   args[0]: the time in minutes (0…60) (0 = never)
         *
         * Default: 15
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSFC(vte::parser::Sequence const& seq)
{
        /*
         * DECSFC - select-flow-control
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSGR(vte::parser::Sequence const& seq)
{
        /*
         * DECSGR - DEC select graphics rendition
         * Selects the character attributes to use for newly inserted
         * characters.
         *
         * Arguments:
         *   args[0:]: the attributes
         *     0 = reset all attributes (deprecated; same as SGR 0)
         *     4 = set superscript and reset subscript
         *     5 = set subscript and reset superscript
         *     6 = set overline (deprecated; same as SGR 53)
         *     8 = set transparency mode
         *     24 = reset superscript and subscript
         *     26 = reset overline (deprecated; same as SGR 55)
         *     28 = reset transparency mode
         *
         * Defaults:
         *   args[0]: 0 (reset all attributes)
         *
         * References: DEC PPLV2
         *             DEC LJ250
         */
        /* TODO: consider implementing sub/superscript? */

        vte::parser::collect_decsgr(seq, 0, m_defaults.attr);

        // Since DECSGR doesn't change any colours, no need to
        // copy them from m_defaults to m_color_defaults
}

void
Terminal::DECSIXEL(vte::parser::Sequence const& seq)
{
        /*
         * DECSIXEL - SIXEL graphics
         * Image data in DECSIXEL format.
         *
         * Arguments:
         *  args[0]: macro parameter (should always use 0 and use DECGRA instead)
         *    See DEC PPLV Table 5–2 in § 5.4.1.1 for more information.
         *  args[1]: background
         *    0: device default (same as 2)
         *    1: pixels with colour 0 retain the colour
         *    2: pixels with colour 0 are set to the current background
         *    5: OR mode (nonstandard NetBSD/x68k extension, see
         *       [https://qiita.com/arakiken/items/26f6c67da5a9f9f907ac])
         *  args[2]: horizontal grid size in the unit set by SSU
         *  args[3]: image ID (range 0..1024) (nonstandard RLogin extension)
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 2 (1 for printers)
         *   args[2]: no default
         *   args[3]: no default
         *
         * References: VT330
         *             DEC PPLV2 § 5.4
         */
}

void
Terminal::DECSKCV(vte::parser::Sequence const& seq)
{
        /*
         * DECSKCV - set-key-click-volume
         * Sets the key click volume.
         *
         * Arguments:
         *   args[0]: the volume setting
         *     0, 5…8: high
         *     1: off
         *     2…4: low
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSLCK(vte::parser::Sequence const& seq)
{
        /*
         * DECSLCK - set-lock-key-style
         * Allow host control of the CapsLock key
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSLE(vte::parser::Sequence const& seq)
{
        /*
         * DECSLE - select-locator-events
         *
         * References: VT330
         *
         * TODO: implement
         */
}

void
Terminal::DECSLPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSLPP - set-lines-per-page
         * Set the number of lines per page.
         *
         * Arguments:
         *   args[0]: the number of lines per page
         *
         * Defaults:
         *   args[0]: 0 (meaning 24)
         *
         * Note that VT525 only allows a limited number of choices,
         * (24, 25, 36, 41, 42, 48, 52, 53, 72); VTE is not so limited
         * and supports any value >= 24.
         *
         * Top and bottom scrolling margins are unaffected, unless their
         * current values exceed the new page size, in which case they are
         * reset to the default.
         *
         * References: VT525
         */

        auto param = seq.collect1(0);
        if (param == 0)
                param = 24;
        else if (param < 24)
                return;

        emit_resize_window(m_column_count, param);
}

void
Terminal::DECSLPP_OR_XTERM_WM(vte::parser::Sequence const& seq)
{
        /*
         * DECSLPP and XTERM_WM use the same sequence, but we can
         * distinguish between them by the parameter value.
         */
        auto const param = seq.collect1(0);
        if (param > 0 && param < 24)
                XTERM_WM(seq);
        else
                DECSLPP(seq);
}

void
Terminal::DECSLRM(vte::parser::Sequence const& seq)
{
        /*
         * DECSLRM - set left and right margins
         * Sets the left and right margins of the scrolling region.
         * This is only applicable if the vertical split-screen mode
         * (DECLRMM) is set.
         *
         * Arguments:
         *   args[0]: left margin
         *   args[1]: right margin
         *
         * Default:
         *   args[0]: 1
         *   args[2]: page width
         *
         * If the values aren't in the right order, or after clamping don't
         * define a region of at least 2 columns, the command is ignored.
         *
         * The maximum of right is the page size (set with DECSCPP).
         * Homes to cursor to (1,1) of the page (scrolling region?).
         *
         * References: VT525
         */

        auto const left = seq.collect1(0, 1, 1, m_column_count);
        auto const right = seq.collect1(seq.next(0), m_column_count, 1, m_column_count);

        /* Ignore if not at least 2 columns */
        if (right <= left)
                return;

        /* Set the right values. */
        m_scrolling_region.set_horizontal(left - 1, right - 1);
        if (m_scrolling_region.is_restricted()) {
                /* Maybe extend the ring: https://gitlab.gnome.org/GNOME/vte/-/issues/2036 */
                while (long(m_screen->row_data->next()) < m_screen->insert_delta + m_row_count)
                        m_screen->row_data->insert(m_screen->row_data->next(), get_bidi_flags());
        }

        home_cursor();
}

void
Terminal::DECSLRM_OR_SCOSC(vte::parser::Sequence const& seq)
{
        /*
         * set left and right margins or SCO restore cursor - DECSLRM or SCOSC
         * There is a conflict between SCOSC and DECSLRM that both are
         * CSI s (CSI 7/3). SCOSC has 0 parameters, and DECSLRM has 2
         * parameters which both have default values, and my reading
         * of ECMA-48 § 5.4.2h says that this allows for an empty
         * parameter string to represent them.
         *
         * While the DEC manuals say that SCOSC/SCORC only operates in
         * "SCO Console Mode" (which is entered by DECTME 13), and not in
         * "VT mode" (i.e. native mode), we instead distinguish the cases
         * by private mode DECLRMM: If DECLRMM is set, dispatch DECSLRM;
         * if it's reset, dispatch SCOSC.
         *
         * See issue #48.
         */

        if (m_modes_private.DECLRMM())
                DECSLRM(seq);
        else
                SCOSC(seq);
}

void
Terminal::DECSMBV(vte::parser::Sequence const& seq)
{
        /*
         * DECSMBV - set-margin-bell-volume
         * Sets the margin bell volume.
         *
         * Arguments:
         *   args[0]: the volume setting
         *     0, 1: off
         *     2…4: low
         *     5…8: high
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSMKR(vte::parser::Sequence const& seq)
{
        /*
         * DECSMKR - select-modifier-key-reporting
         * Make modifier keys send extended keyboard reports (DECEKBD)
         * when pressed or released in key position mode (DECKPM).
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSNLS(vte::parser::Sequence const& seq)
{
        /*
         * DECSNLS - set-lines-per-screen
         * Sets the number of lines per screen.
         * DEC only supports 26, 42, 53 lines here; but VTE has no
         * such restriction.
         *
         * Arguments:
         *   args[0]: the number of lines
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: VT525
         *
         * FIXMEchpe: implement this
         */
}

void
Terminal::DECSPMA(vte::parser::Sequence const& seq)
{
        /*
         * DECSPMA - session page memory allocation
         * Allocate pages of 25 lines to each session.
         *
         * References: VT525
         *
         * VTE does not support sessions.
         */
}

void
Terminal::DECSPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSPP - set-port-parameter
         * Sets parameters for the communications or printer port.
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSPPCS(vte::parser::Sequence const& seq)
{
        /*
         * DECSPPCS - select-pro-printer-character-set
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSPRTT(vte::parser::Sequence const& seq)
{
        /*
         * DECSPRTT - select-printer-type
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSR(vte::parser::Sequence const& seq)
{
        /*
         * DECSR - secure-reset
         * Hard reset, with confirmation.
         * Like RIS, but the terminal replies with the token.
         * [long list of things this resets]
         *
         * Arguments:
         *   args[0]: a token
         *
         * Defaults:
         *   args[0]: no default
         *
         * Reply: DECSRC
         *   args[0]: the token
         *
         * References: VT525
         */

        /* Note: reset() wipes out @seq, so we need to get the
         * param beforehand, and use send() instead of reply().
         */
        auto const token = seq.collect1(0);
	reset(true, true);
        send(VTE_REPLY_DECSRC, {token});
}

void
Terminal::DECSRFR(vte::parser::Sequence const& seq)
{
        /*
         * DECSRFR - select-refresh-rate
         * [...]
         *
         * References: VT510
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSSCLS(vte::parser::Sequence const& seq)
{
        /*
         * DECSSCLS - set-scroll-speed
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSSDT(vte::parser::Sequence const& seq)
{
        /*
         * DECSSDT - select-status-display-line-type
         * Sets the type of status line shown.
         *
         * Arguments:
         *   args[0]: the type
         *     0: no status line
         *     1: indicator status line
         *     2: host-writable status line
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Not worth implementing.
         */
}

void
Terminal::DECSSL(vte::parser::Sequence const& seq)
{
        /*
         * DECSSL - select-setup-language
         *
         * Selects set-up language
         *
         * References: VT525
         *
         * VTE does not implement a set-up.
         *
         * or:
         *
         * WYDRBX - draw a box
         *
         * References: WY370
         */
}

void
Terminal::DECST8C(vte::parser::Sequence const& seq)
{
        /*
         * DECST8C - set-tab-at-every-8-columns
         * Clear the tab-ruler and reset it to a tab at every 8th column,
         * starting at 9 (though, setting a tab at 1 is fine as it has no
         * effect).
         *
         * References: VT525
         */

        if (seq.collect1(0) != 5)
                return;

        m_tabstops.reset(8);
        m_tabstops.unset(0);
}

void
Terminal::DECSTBM(vte::parser::Sequence const& seq)
{
        /*
         * DECSTBM - set-top-and-bottom-margins
         * Sets the top and bottom scrolling margins.
         * Arguments:
         *   args[0]: the top margin
         *   args[1]: the bottom margin
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: number of lines
         *
         * If the values aren't in the right order, or after clamping don't
         * define a region of at least 2 lines, the command is ignored.
         *
         * The maximum size of the scrolling region is the whole page.
         * Homes the cursor to position (1,1) (of the scrolling region?).
         *
         * References: VT525 5–149
         */
#if 0
        unsigned int top, bottom;

        top = 1;
        bottom = screen->page->height;

        if (seq->args[0] > 0)
                top = seq->args[0];
        if (seq->args[1] > 0)
                bottom = seq->args[1];

        if (top > screen->page->height)
                top = screen->page->height;
        if (bottom > screen->page->height)
                bottom = screen->page->height;

        if (top >= bottom ||
            top > screen->page->height ||
            bottom > screen->page->height) {
                top = 1;
                bottom = screen->page->height;
        }

        vte_page_set_scroll_region(screen->page, top - 1, bottom - top + 1);
        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, 0, 0);
#endif

        auto const top = seq.collect1(0, 1, 1, m_row_count);
        auto const bottom = seq.collect1(seq.next(0), m_row_count, 1, m_row_count);

        /* Ignore if not at least 2 lines */
        if (bottom <= top)
                return;

	/* Set the right values. */
        m_scrolling_region.set_vertical(top - 1, bottom - 1);
        if (m_scrolling_region.is_restricted()) {
                /* Maybe extend the ring: https://gitlab.gnome.org/GNOME/vte/-/issues/2036 */
                while (long(m_screen->row_data->next()) < m_screen->insert_delta + m_row_count)
                        m_screen->row_data->insert(m_screen->row_data->next(), get_bidi_flags());
	}

        home_cursor();
}

void
Terminal::DECSTGLT(vte::parser::Sequence const& seq)
{
        /*
         * DECSTGLT - select color lookup table
         * Selects color mapping.
         *
         * Arguments:
         *   args[0]: mode
         *     0: Text colors are shown in monochrome or grey levels
         *     1: Text attributes (bold, blink, reverse, (single) underline,
         *        and any combinations thereof) are shown with alternate
         *        colors (defined by set-up), plus the attribute
         *     2: Like 1, but attributes are only represented by the color
         *     3: Text color as specified by SGR, and attributes
         *        as specified.
         *
         * Defaults:
         *   args[0]: 3
         *
         * Set-up default: 3
         *
         * References: VT525
         *
         * Maybe worth implementing.
         */
}

void
Terminal::DECSTR(vte::parser::Sequence const& seq)
{
        /*
         * DECSTR - soft-terminal-reset
         * Perform a soft reset to the default values.
         * [list of default values]
         *
         * When not using private colour registers, this
         * must also clear (assign to black) all SIXEL
         * colour registers. (DEC PPLV2 § 5.8)
         *
         * References: VT525
         */

	reset(false, false);
}

void
Terminal::DECSTRL(vte::parser::Sequence const& seq)
{
        /*
         * DECSTRL - set-transmit-rate-limit
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSTUI(vte::parser::Sequence const& seq)
{
        /*
         * DECSTUI - set terminal unit ID
         * Sets the terminal unit ID that DA3 reports.
         *
         * References: VT525
         *
         * VTE does not implement this.
         */
}

void
Terminal::DECSWBV(vte::parser::Sequence const& seq)
{
        /*
         * DECSWBV - set-warning-bell-volume
         * Sets the warning bell volume.
         *
         * Arguments:
         *   args[0]: the volume setting
         *     0, 5…8: high
         *     1: off
         *     2…4: low
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSWL(vte::parser::Sequence const& seq)
{
        /*
         * DECSWL - single-width-single-height-line
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECSZS(vte::parser::Sequence const& seq)
{
        /*
         * DECSZS - select zero symbol
         * Selects the zero glyph shape.
         *
         * Aguments:
         *   args[0]: shape
         *     0: oval zero
         *     1: zero with slash
         *     2: zero with dot
         *
         * Default:
         *  args[0]: 0
         *
         * References: VT525
         *
         * Maybe worth implementing; could use the opentype "zero" feature
         * to get the slashed zero.
         */
}

void
Terminal::DECTID(vte::parser::Sequence const& seq)
{
        /*
         * DECTID - select-terminal-id
         * Selects the response to DA1.
         * [...]
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DECTME(vte::parser::Sequence const& seq)
{
        /*
         * DECTME - terminal-mode-emulation
         * Selects the terminal emulation mode.
         * Available values are various VTxxx, Wyse, TVI, ADDS, SCO
         * terminals.
         * Changing the emulation mode effects a soft reset.
         *
         * References: VT525
         *
         * Not worth implementing.
         */
}

void
Terminal::DECTST(vte::parser::Sequence const& seq)
{
        /*
         * DECTST - invoke-confidence-test
         * Executes self-tests.
         *
         * Arguments:
         *   args[0]: 4
         *   args[1]: which test to perform
         *
         * References: VT525
         *
         * Not worth implementing.
         */
}

void
Terminal::DECUDK(vte::parser::Sequence const& seq)
{
        /*
         * DECUDK - user define keys
         * Loads key definitions.
         *
         * References: VT525
         *
         * For security reasons, VTE does not implement this.
         */
}

void
Terminal::DECUS(vte::parser::Sequence const& seq)
{
        /*
         * DECUS - update session
         *
         * References: VT525
         *
         * VTE does not support sessions.
         */
}

void
Terminal::DL(vte::parser::Sequence const& seq)
{
        /*
         * DL - delete-line
         * Delete lines starting from the active line (presentation).
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * Also affected by TSM and VEM modes, and the SLH and SEE
         * functions.
         *
         * Arguments:
         *  args[0]: number of lines to delete
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.32
         *             DEC STD 070 page 5-148
         *             Terminal-wg/bidi
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_delete_lines(screen->page,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        auto const cursor_row = get_xterm_cursor_row();
        auto const cursor_col = get_xterm_cursor_column();

        /* If the cursor (xterm-like interpretation when about to wrap) is outside
         * the DECSTBM / DECSLRM scrolling region then do nothing. */
        if (!m_scrolling_region.contains_row_col(cursor_row, cursor_col)) {
                return;
        }

        carriage_return();

        auto const count = seq.collect1(0, 1);
        /* Scroll up in a custom region: the top is at the cursor, the rest is according to DECSTBM / DECSLRM. */
        auto scrolling_region{m_scrolling_region};
        scrolling_region.set_vertical(cursor_row, scrolling_region.bottom());
        scroll_text_up(scrolling_region, count, true /* fill */);
}

void
Terminal::DLE(vte::parser::Sequence const& seq)
{
        /*
         * DLE - data link escape
         * Supplementary transmission control functions.
         *
         * References: ECMA-48 § 8.3.33
         *             ECMA-16 § 3.1.7
         *             ECMA-37
         *
         * Not worth implementing.
         */
}

void
Terminal::DMI(vte::parser::Sequence const& seq)
{
        /*
         * DMI - disable manual input
         *
         * References: ECMA-48 § 8.3.34
         *
         * Probably not worth implementing.
         */
}

void
Terminal::DOCS(vte::parser::Sequence const& seq)
{
        /*
         * DOCS - designate other coding systyem
         *
         * References: ECMA-35 § 15.4
         *             ISO 2375 IR
         *
         * TODO: implement (bug #787228)
         */
}

void
Terminal::DSR_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * DSR_ECMA - Device Status Report
         *
         * Reports status, or requests a status report.
         *
         * Arguments:
         *   args[0]: type
         *
         * Defaults:
         *   arg[0]: 0
         *
         * References: ECMA-48 § 8.3.35
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
                /* This is a status report */
                break;

        case 5:
                /* Request operating status report.
                 * Reply: DSR
                 *   @arg[0]: status
                 *     0 = ok
                 *     3 = malfunction
                 */
                reply(seq, VTE_REPLY_DSR, {0});
                break;

        case 6:
                /* Request cursor position report
                 * Reply: CPR
                 *   @arg[0]: line
                 *   @arg[1]: column
                 */
                vte::grid::row_t top, bottom, rowval;
                vte::grid::column_t left, right, colval;

                if (m_modes_private.DEC_ORIGIN()) {
                        top = m_scrolling_region.top();
                        bottom = m_scrolling_region.bottom();
                        left = m_scrolling_region.left();
                        right = m_scrolling_region.right();
                } else {
                        top = 0;
                        bottom = m_row_count - 1;
                        left = 0;
                        right = m_column_count - 1;
                }
                rowval = CLAMP(get_xterm_cursor_row(), top, bottom) - top;
                colval = CLAMP(get_xterm_cursor_column(), left, right) - left;

                reply(seq, VTE_REPLY_CPR, {int(rowval + 1), int(colval + 1)});
                break;

        default:
                break;
        }
}

void
Terminal::DSR_DEC(vte::parser::Sequence const& seq)
{
        /*
         * DSR_DEC - device-status-report-dec
         *
         * Reports status, or requests a status report.
         *
         * Defaults:
         *   arg[0]: 0
         *
         * References: VT525 5–173
         *             VT330
         *             XTERM
         */

        switch (seq.collect1(0)) {
        case 6:
                /* Request extended cursor position report
                 * Reply: DECXCPR
                 *   @arg[0]: line
                 *   @arg[1]: column
                 *   @arg[2]: page
                 *     Always report page 1 here (per XTERM source code).
                 */
                vte::grid::row_t top, bottom, rowval;
                vte::grid::column_t left, right, colval;

                if (m_modes_private.DEC_ORIGIN()) {
                        top = m_scrolling_region.top();
                        bottom = m_scrolling_region.bottom();
                        left = m_scrolling_region.left();
                        right = m_scrolling_region.right();
                } else {
                        top = 0;
                        bottom = m_row_count - 1;
                        left = 0;
                        right = m_column_count - 1;
                }
                rowval = CLAMP(get_xterm_cursor_row(), top, bottom) - top;
                colval = CLAMP(get_xterm_cursor_column(), left, right) - left;

                reply(seq, VTE_REPLY_DECXCPR, {int(rowval + 1), int(colval + 1), 1});
                break;

        case 15:
                /* Request printer port report
                 * Reply: DECDSR
                 *   @arg[0]: status
                 *     10 = printer ready
                 *     11 = printer not ready
                 *     13 = no printer
                 *     18 = printer busy
                 *     19 = printer assigned to another session
                 */
                reply(seq, VTE_REPLY_DECDSR, {13});
                break;

        case 25:
                /* Request user-defined keys report
                 * Reply: DECDSR
                 *   @arg[0]: locked status
                 *      20 = UDK unlocked
                 *      21 = UDK locked
                 *
                 * Since we don't do UDK, we report them as locked.
                 */
                reply(seq, VTE_REPLY_DECDSR, {21});
                break;

        case 26:
                /* Request keyboard report
                 * Reply: DECDSR
                 *   @arg[0]: 27
                 *   @arg[1]: Keyboard language
                 *     0 = undetermined
                 *     1..40
                 *
                 *   @arg[2]: Keyboard status
                 *     0 = ready
                 *     3 = no keyboard
                 *     8 = keyboard busy (used by other session)
                 *
                 *   @arg[3]: Keyboard type
                 *     0 = LK201 (XTERM response)
                 *     4 = LK411
                 *     5 = PCXAL
                 */
                reply(seq, VTE_REPLY_DECDSR, {27, 0, 0, 5});
                break;

        case 55:
                /* Request locator status report
                 * Reply: DECDSR
                 *   @arg[0]: status
                 *     50 = locator ready
                 *     53 = no locator
                 *
                 * Since we don't implement the DEC locator mode,
                 * we reply with 53.
                 */
                reply(seq, VTE_REPLY_DECDSR, {53});
                break;

        case 56:
                /* Request locator type report
                 * Reply: DECDSR
                 *   @arg[0]: 57
                 *   @arg[1]: status
                 *     0 = unknown
                 *     1 = mouse
                 *
                 * Since we don't implement the DEC locator mode,
                 * we reply with 0.
                 */
                reply(seq, VTE_REPLY_DECDSR, {57, 0});
                break;

        case 62:
                /* Request macro space report
                 * Reply: DECMSR
                 *   @arg[0]: floor((number of bytes available) / 16); we report 0
                 */
                reply(seq, VTE_REPLY_DECMSR, {0});
                break;

        case 63:
                /* Request memory checksum report
                 * Reply: DECCKSR
                 *   @arg[0]: PID
                 *   DATA: the checksum as a 4-digit hex number
                 *
                 * Reply with a dummy checksum.
                 */
                reply(seq, VTE_REPLY_DECCKSR, {seq.collect1(1)}, "0000");
                break;

        case 75:
                /* Request data integrity report
                 * Reply: DECDSR
                 *   @arg[0]: status
                 *     70 = no error, no power loss, no communication errors
                 *     71 = malfunction or communication error
                 *     73 = no data loss since last power-up
                 */
                reply(seq, VTE_REPLY_DECDSR, {70});
                break;

        case 85:
                /* Request multi-session status report
                 * Reply: DECDSR
                 *   @arg[0]: status
                 *     ...
                 *     83 = not configured
                 */
                reply(seq, VTE_REPLY_DECDSR, {83});
                break;

        default:
                break;
        }
}

void
Terminal::DTA(vte::parser::Sequence const& seq)
{
        /*
         * DTA - dimension text area
         * Set the dimension of the text area.
         *
         * Arguments:
         *  args[0]:
         *  args[1]:
         *
         * Defaults:
         *   args[0]: no default
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.36
         */
}

void
Terminal::EA(vte::parser::Sequence const& seq)
{
        /*
         * EA - erase in area
         * Erase some/all character positions in the qualified area.
         *
         * Arguments:
         *  args[0]: type
         *    0 = Erase the active position and all positions to the end
         *        of the qualified area (inclusive).
         *    1 = Erase from the beginning of the qualified area to
         *        the active position (inclusive).
         *    2 = Erase all of the qualified area.
         *
         * Defaults:
         *   args[0]: 0
         *
         * If ERM is set, erases only non-protected areas; if
         * ERM is reset, erases all areas.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.37
         *             Terminal-wg/bidi
         */

        switch (seq.collect1(0)) {
        case -1:
        case 0:
                break;
        }
}

void
Terminal::ECH(vte::parser::Sequence const& seq)
{
        /*
         * ECH - erase-character
         * Erase characters from the active position.
         *
         * DSCM mode controls whether this function operates on the
         * presentation or data position.
         * Also affected by ERM mode.
         *
         * Arguments:
         *   args[0]: number of characters to erase
         *
         * Defaults:
         *   args[0]: 1
         *
         * If ERM is set, erases only non-protected characters; if
         * ERM is reset, erases all characters.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.38
         *             Terminal-wg/bidi
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_erase(screen->page,
                          screen->state.cursor_x, screen->state.cursor_y,
                          screen->state.cursor_x + num, screen->state.cursor_y,
                          &screen->state.attr, screen->age, false);
#endif

        /* Erase characters starting at the cursor position (overwriting N with
         * spaces, but not moving the cursor). */
        auto const count = seq.collect1(0, 1);
        erase_characters(count, false);
}

void
Terminal::ED(vte::parser::Sequence const& seq)
{
        /*
         * ED - erase-in-display
         * Erases characters.
         * Line attributes of completely erased lines are reset to
         * single-width single-height, and all character attributes
         * are reset to default.
         *
         * Arguments:
         *   args[0]: mode
         *     0 = erase from the cursor position to the end of the screen
         *     1 = erase from the beginning of the screen to the cursor
         *         position (inclusive)
         *     2 = erase display
         *     3 = erase scrollback (XTERM extension)
         *
         * Defaults:
         *   args[0]: 0
         *
         * This function is not affected by the scrolling margins.
         *
         * If ERM is set, erases only non-protected characters; if
         * ERM is reset, erases all characters.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.39
         *             VT525
         *             Terminal-wg/bidi
         */

        erase_in_display(seq);
}

void
Terminal::EF(vte::parser::Sequence const& seq)
{
        /*
         * EF - erase in field
         * Erases characters in the active field.
         *
         * Arguments:
         *   args[0]: mode
         *    0 = Erase the active position and all positions to the end
         *        of the field (inclusive).
         *    1 = Erase from the beginning of the field to
         *        the active position (inclusive).
         *    2 = Erase all of the qualified area.
         *
         * Defaults:
         *   args[0]: 0
         *
         * If ERM is set, erases only non-protected characters; if
         * ERM is reset, erases all characters.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.40
         *             Terminal-wg/bidi
         */
}

void
Terminal::EL(vte::parser::Sequence const& seq)
{
        /*
         * EL - erase-in-line
         * Erases characters.
         *
         * Arguments:
         *   args[0]: mode
         *     0 = erase from the cursor position to the end of the line
         *     1 = erase from the beginning of the line to the cursor
         *         position (inclusive)
         *     2 = erase line (FIXME: does this clear line attributes?)
         *
         * Defaults:
         *   args[0]: 0
         *
         * This function does not respect the scrolling margins.
         *
         * If ERM is set, erases only non-protected characters; if
         * ERM is reset, erases all characters.
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.41
         *             VT525
         *             Terminal-wg/bidi
         */

        erase_in_line(seq);
}

void
Terminal::EM(vte::parser::Sequence const& seq)
{
        /*
         * EM - end of medium
         *
         * References: ECMA-48 § 8.3.42
         */
}

void
Terminal::EMI(vte::parser::Sequence const& seq)
{
        /*
         * DMI - enable manual input
         *
         * References: ECMA-48 § 8.3.43
         *
         * Probably not worth implementing.
         */
}

void
Terminal::ENQ(vte::parser::Sequence const& seq)
{
        /*
         * ENQ - enquiry
         * Transmit the answerback-string. If none is set, do nothing.
         *
         * References: ECMA-48 § 8.3.44
         *             ECMA-16 § 3.1.5
         */

        /* No-op for security reasons */
}

void
Terminal::EOT(vte::parser::Sequence const& seq)
{
        /*
         * EOT - end of transmission
         *
         * References: ECMA-48 § 8.3.45
         *             ECMA-16 § 3.1.4
         *
         * Not worth implementing.
         */
}

void
Terminal::EPA(vte::parser::Sequence const& seq)
{
        /*
         * EPA - end of guarded area
         * Marks the end of an area of positions (presentation)
         * that are protected; the beginning of the area was
         * marked by SPA.
         *
         * The contents of the area will be protected against
         * alteration, transfer (depending on the GATM setting),
         * and erasure (depending on the ERM setting).
         *
         * References: ECMA-48 § 8.3.46
         */
}

void
Terminal::ESA(vte::parser::Sequence const& seq)
{
        /*
         * ESA - end of selected area
         * Marks the end of an area of positions (presentation)
         * that are selected for transfer; the beginning of the area
         * was marked by SSA.
         *
         * References: ECMA-48 § 8.3.47
         */
}

void
Terminal::ETB(vte::parser::Sequence const& seq)
{
        /*
         * ETB - end of transmission block
         *
         * References: ECMA-48 § 8.3.49
         *             ECMA-16 § 3.1.10
         *
         * Not worth implementing.
         */
}

void
Terminal::ETX(vte::parser::Sequence const& seq)
{
        /*
         * ETX - end of text
         *
         * References: ECMA-48 § 8.3.49
         *             ECMA-16 § 3.1.3
         *
         * Not worth implementing.
         */
}

void
Terminal::FF(vte::parser::Sequence const& seq)
{
        /*
         * FF - form-feed
         * This causes the cursor to jump to the next line (presentation).
         *
         * References: ECMA-48 § 8.3.51
         */

        LF(seq);
}

void
Terminal::FNK(vte::parser::Sequence const& seq)
{
        /*
         * FNK - function key
         *
         * Arguments:
         *  args[0]: function key that was operated
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.52
         *
         * Probably not worth implementing.
         */
}

void
Terminal::FNT(vte::parser::Sequence const& seq)
{
        /*
         * FNT - font selection
         * Select the font to be used by subsequent SGR 10…19.
         *
         * Arguments:
         *  args[0]: the font 0…9
         *  args[1]: font identifier
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * References: ECMA-48 § 8.3.53
         *
         * Probably not worth implementing.
         */
}

void
Terminal::GCC(vte::parser::Sequence const& seq)
{
        /*
         * GCC - graphic character combination
         * Two or more graphic characters that follow should be
         * imaged as one symbol.
         *
         * Arguments:
         *  args[0]: mode
         *    0 = Combine the following two graphic characters
         *    1 = Start of string of characters to be combined
         *    2 = End of string of characters to be combined
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.54
         *             ECMA-43 Annex C
         */
}

void
Terminal::GSM(vte::parser::Sequence const& seq)
{
        /*
         * GSM - graphic size modification
         *
         * Arguments:
         *  args[0]: height as percentage of height set by GSS
         *  args[1]: width as percentage of width set by GSS
         *
         * Defaults:
         *   args[0]: 100
         *   args[0]: 100
         *
         * References: ECMA-48 § 8.3.55
         *
         * Not applicable to VTE.
         */
}

void
Terminal::GSS(vte::parser::Sequence const& seq)
{
        /*
         * GSM - graphic size selection
         *
         * Arguments:
         *  args[0]: size in the unit set by SSU
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.56
         *
         * Not applicable to VTE.
         */
}

void
Terminal::GnDm(vte::parser::Sequence const& seq)
{
        /*
         * GnDm - Gn-designate 9m-charset
         *
         * Designate character sets to G-sets.
         *
         * References: ECMA-35 § 14.3
         *             ISO 2375 IR
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can mostly ignore this. */

        VteCharacterReplacement replacement;
        switch (seq.charset()) {
        case VTE_CHARSET_DEC_SPECIAL_GRAPHIC:
                /* Some characters replaced by line drawing characters.
                 * This is still used by ncurses :-(
                 */
                replacement = VTE_CHARACTER_REPLACEMENT_LINE_DRAWING;
                break;

        default:
                replacement = VTE_CHARACTER_REPLACEMENT_NONE;
                break;
        }

        unsigned int slot = seq.slot();
        if (slot >= G_N_ELEMENTS(m_character_replacements))
                return;

        m_character_replacements[slot] = replacement;
}

void
Terminal::GnDMm(vte::parser::Sequence const& seq)
{
        /*
         * GnDm - Gn-designate multibyte 9m-charset
         *
         * Designate multibyte character sets to G-sets.
         *
         * References: ECMA-35 § 14.3
         *             ISO 2375 IR
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can ignore this */
}

void
Terminal::HPA(vte::parser::Sequence const& seq)
{
        /*
         * HPA - horizontal position absolute
         * Move the active position (data) to the position specified by @args[0]
         * in the active line.
         *
         * Arguments:
         *   args[0]: position (data)
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.57
         *             VT525
         */

#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, num - 1, screen->state.cursor_y);
#endif

        auto value = seq.collect1(0, 1, 1, m_column_count);
        set_cursor_column1(value);
}

void
Terminal::HPB(vte::parser::Sequence const& seq)
{
        /*
         * HPB - horizontal position backward
         * Move the active position (data) to the backward by @args[0] positions
         * in the active line.
         *
         * Arguments:
         *   args[0]: number of positions to move
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.58
         */
}

void
Terminal::HPR(vte::parser::Sequence const& seq)
{
        /*
         * HPR - horizontal-position-relative
         * Move the active position (data) to the foward by @args[0] positions
         * in the active line.
         *
         * Arguments:
         *   args[0]: number of positions to move
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.59
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_right(screen, num);
#endif
}

void
Terminal::HT(vte::parser::Sequence const& seq)
{
        /*
         * HT - character tabulation
         * Move the active position (presentation) to the next tab stop.
         * If there are no more tab stops, the cursor moves to the right
         * margin. Does not cause text to auto wrap.
         *
         * (If that next tabstop was set by TAC, TALE, TATE or TCC,
         * the properties of that tabstop will determine how subsequently
         * inserted text is positioned.)
         *
         * References: ECMA-48 § 8.3.60
         *             VT525
         */
#if 0
        screen_cursor_clear_wrap(screen);
#endif

        move_cursor_tab_forward();
}

void
Terminal::HTJ(vte::parser::Sequence const& seq)
{
        /*
         * HTJ - character tabulation with justification
         *
         * References: ECMA-48 § 8.3.61
         *             VT525
         */
#if 0
        screen_cursor_clear_wrap(screen);
#endif

        move_cursor_tab_forward();
}

void
Terminal::HTS(vte::parser::Sequence const& seq)
{
        /*
         * HTS - horizontal-tab-set
         * Set a tabstop at the active position (presentation).
         *
         * Affected by TSM mode.
         *
         * References: ECMA-48 § 8.3.62
         *             VT525
         */

        m_tabstops.set(get_xterm_cursor_column());
}

void
Terminal::HVP(vte::parser::Sequence const& seq)
{
        /*
         * HVP - horizontal-and-vertical-position
         * Sets the active position (data)
         *
         * Arguments:
         *   args[0]: the line
         *   args[1]: the column
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *
         * If DECOM is set, the position is relative to the top/bottom
         * margins, and may not be outside it.
         *
         * References: ECMA-48 § 8.3.63
         *             VT525
         */

        CUP(seq);
}

void
Terminal::ICH(vte::parser::Sequence const& seq)
{
        /*
         * ICH - insert-character
         * Inserts SPACE (2/0) character(s) at the cursor position.
         *
         * Arguments:
         *   args[0]: the number of characters to insert
         *
         * Defaults:
         *   args[0]: 1
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.

         * Also affected by HEM mode, and the SLH, and SEE functions.
         *
         * References: ECMA-48 §8.3.64
         *             VT525
         *             Terminal-wg/bidi
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_insert_cells(screen->page,
                                 screen->state.cursor_x,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        /* If the cursor (xterm-like interpretation when about to wrap) is horizontally outside
         * the DECSLRM margins then do nothing. */
        if (cursor_col < m_scrolling_region.left() || cursor_col > m_scrolling_region.right()) {
                return;
        }

        maybe_retreat_cursor();

        auto const count = seq.collect1(0, 1);
        /* Scroll right in a custom region: only the cursor's row, from the cursor to the DECSLRM right margin. */
        auto scrolling_region{m_scrolling_region};
        scrolling_region.set_vertical(cursor_row, cursor_row);
        scrolling_region.set_horizontal(cursor_col, scrolling_region.right());
        scroll_text_right(scrolling_region, count, true /* fill */);
}

void
Terminal::IDCS(vte::parser::Sequence const& seq)
{
        /*
         * IDCS - identify device control string
         *
         * Arguments:
         *   args[0]: mode
         *     1 = reserved for use with SRTM mode
         *     2 = reservewd for DRCS according to ECMA-35

         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.65
         */
}

void
Terminal::IGS(vte::parser::Sequence const& seq)
{
        /*
         * IGS - identify graphic subrepertoire
         * Specifies a repertoire of graphic characters to be used
         * in the following text.
         *
         * Arguments:
         *   args[0]: identifier from ISO 7350 registry

         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.66
         *             ISO/IEC 7350
         *             ISO/IEC 10367
         *
         * Not worth implementing.
         */
}

void
Terminal::IL(vte::parser::Sequence const& seq)
{
        /*
         * IL - insert-line
         * Insert (a) blank line(s) at the active position.
         *
         * Arguments:
         *   args[0]: the number of lines
         *
         * Defaults:
         *   args[0]: 1
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * Also affected by the TSM and VEM modes,
         * and the SLH and SEE functions.
         *
         * References: ECMA-48 § 8.3.67
         *             DEC STD 070 page 5-146
         *             Terminal-wg/bidi
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_insert_lines(screen->page,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        auto const cursor_row = get_xterm_cursor_row();
        auto const cursor_col = get_xterm_cursor_column();

        /* If the cursor (xterm-like interpretation when about to wrap) is outside
         * the DECSTBM / DECSLRM scrolling region then do nothing. */
        if (!m_scrolling_region.contains_row_col(cursor_row, cursor_col)) {
                return;
        }

        carriage_return();

        auto const count = seq.collect1(0, 1);
        /* Scroll down in a custom region: the top is at the cursor, the rest is according to DECSTBM / DECSLRM. */
        auto scrolling_region{m_scrolling_region};
        scrolling_region.set_vertical(cursor_row, scrolling_region.bottom());
        scroll_text_down(scrolling_region, count, true /* fill */);
}

void
Terminal::IND(vte::parser::Sequence const& seq)
{
        /*
         * IND - index - DEPRECATED
         *
         * References: ECMA-48 § F.8.2
         */

        LF(seq);
}

void
Terminal::INT(vte::parser::Sequence const& seq)
{
        /*
         * INT - interrupt
         *
         * References: ECMA-48 § 8.3.68
         */
}

void
Terminal::IRR(vte::parser::Sequence const& seq)
{
        /*
         * IRR - identify-revised-registration
         *
         * Note that the IRR comes _before_ the GnDm/GnDMm/CnD, see e.g.
         * IR#124 whose C1 designation sequence is ESC 2/6 4/0 ESC 2/2 4/2,
         * i.e. IRR '@', C1D 'B'.
         *
         * References: ECMA-35 § 14.5
         *
         * Probably not worth implementing.
         */

        /* Since we mostly don't implement ECMA-35 anymore, we can ignore this */
}

void
Terminal::IS1(vte::parser::Sequence const& seq)
{
        /*
         * IS1 - information separator 1 / unit separator (US)
         *
         * References: ECMA-48 § 8.3.69, § 8.2.10
         */
}

void
Terminal::IS2(vte::parser::Sequence const& seq)
{
        /*
         * IS2 - information separator 2 / record separator (RS)
         *
         * References: ECMA-48 § 8.3.70, § 8.2.10
         */
}

void
Terminal::IS3(vte::parser::Sequence const& seq)
{
        /*
         * IS3 - information separator 3 / group separator (GS)
         *
         * References: ECMA-48 § 8.3.71, § 8.2.10
         */
}

void
Terminal::IS4(vte::parser::Sequence const& seq)
{
        /*
         * IS4 - information separator 4 / file separator (FS)
         *
         * References: ECMA-48 § 8.3.72, § 8.2.10
         */
}

void
Terminal::JFY(vte::parser::Sequence const& seq)
{
        /*
         * JFY - justify
         *
         * References: ECMA-48 § 8.3.73
         *
         * Probably not worth implementing.
         */
}

void
Terminal::LF(vte::parser::Sequence const& seq)
{
        /*
         * LF - line-feed
         * XXXX
         *
         * References: ECMA-48 § 8.3.74
         */

#if 0
        screen_cursor_down(screen, 1, true);
        if (screen->flags & VTE_FLAG_NEWLINE_MODE)
                screen_cursor_left(screen, screen->state.cursor_x);
#endif

        line_feed();
}

void
Terminal::LS0(vte::parser::Sequence const& seq)
{
        /*
         * LS0 -locking shift 0 (8 bit)
         * SI - shift-in (7 bit)
         *
         * Map G0 into GL.
         *
         * References: ECMA-35 § 9.3.1
         *             ECMA-48 § 8.3.75, 8.3.119
         */
#if 0
        screen->state.gl = &screen->g0;
#endif

        set_character_replacement(0);
}

void
Terminal::LS1(vte::parser::Sequence const& seq)
{
        /*
         * LS1 -locking shift 1 (8 bit)
         * SO - shift-out (7 bit)
         *
         * Map G1 into GL.
         *
         * References: ECMA-35 § 9.3.1
         *             ECMA-48 § 8.3.76, 8.3.126
         */
#if 0
        screen->state.gl = &screen->g1;
#endif

        set_character_replacement(1);
}

void
Terminal::LS1R(vte::parser::Sequence const& seq)
{
        /*
         * LS1R - locking-shift-1-right
         * Map G1 into GR.
         *
         * References: ECMA-35 § 9.3.2
         *             ECMA-48 § 8.3.77
         */
#if 0
        screen->state.gr = &screen->g1;
#endif
}

void
Terminal::LS2(vte::parser::Sequence const& seq)
{
        /*
         * LS2 - locking-shift-2
         * Map G2 into GL.
         *
         * References: ECMA-35 § 9.3.1
         *             ECMA-48 § 8.3.78
         */
#if 0
        screen->state.gl = &screen->g2;
#endif
}

void
Terminal::LS2R(vte::parser::Sequence const& seq)
{
        /*
         * LS2R - locking-shift-2-right
         * Map G2 into GR.
         *
         * References: ECMA-35 § 9.3.2
         *             ECMA-48 § 8.3.79
         */
#if 0
        screen->state.gr = &screen->g2;
#endif
}

void
Terminal::LS3(vte::parser::Sequence const& seq)
{
        /*
         * LS3 - locking-shift-3
         * Map G3 into GL.
         *
         * References: ECMA-35 § 9.3.1
         *             ECMA-48 § 8.3.80
         */

#if 0
        screen->state.gl = &screen->g3;
#endif
}

void
Terminal::LS3R(vte::parser::Sequence const& seq)
{
        /*
         * LS3R - locking-shift-3-right
         * Map G3 into GR.
         *
         * References: ECMA-35 § 9.3.2
         *             ECMA-48 § 8.3.81
         */
#if 0
        screen->state.gr = &screen->g3;
#endif
}

void
Terminal::MC_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * MC_ECMA - media-copy-ecma
         *
         * References: ECMA-48 § 8.3.82
         *             VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::MC_DEC(vte::parser::Sequence const& seq)
{
        /*
         * MC_DEC - media-copy-dec
         *
         * References: VT525
         *
         * Probably not worth implementing.
         */
}

void
Terminal::MW(vte::parser::Sequence const& seq)
{
        /*
         * MW - message waiting
         *
         * References: ECMA-48 § 8.3.83
         *
         * Not worth implementing.
         */
}

void
Terminal::NAK(vte::parser::Sequence const& seq)
{
        /*
         * NAK - negative acknowledge
         *
         * References: ECMA-48 § 8.3.84
         *             ECMA-16 § 3.1.8
         *
         * Not worth implementing.
         */
}

void
Terminal::NBH(vte::parser::Sequence const& seq)
{
        /*
         * BPH - no break permitted here
         *
         * References: ECMA-48 § 8.3.85
         *
         * Not worth implementing.
         */
}

void
Terminal::NEL(vte::parser::Sequence const& seq)
{
        /*
         * NEL - next-line
         * Moves the cursor to the first column in the next line.
         * If the cursor is on the bottom margin, this scrolls up.
         *
         * References: ECMA-48 § 8.3.86
         */
#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, 1, true);
        screen_cursor_set(screen, 0, screen->state.cursor_y);
#endif

        /* If the cursor is on the bottom margin but to the right of the right margin then
         * Xterm doesn't scroll. esctest also checks for this behavior. In order to achieve
         * this, move the cursor down (with scrolling) first, and then return the carriage.
         * DEC STD 070 p5-64 disagrees, it says we should return the carriage first.
         * See https://gitlab.gnome.org/GNOME/vte/-/issues/2526#note_1910803 */
        cursor_down_with_scrolling(true);
        carriage_return();
}

void
Terminal::NP(vte::parser::Sequence const& seq)
{
        /*
         * NP - next-page
         * Move cursor to home on the next page (presentation).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: number of pages to move forward
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.87
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::NUL(vte::parser::Sequence const& seq)
{
        /*
         * NUL - nothing
         *
         * References: ECMA-48 § 8.3.88
         */
}

void
Terminal::OSC(vte::parser::Sequence const& seq)
{
        /*
         * OSC - operating system command
         *
         * References: ECMA-48 § 8.3.89
         *             XTERM
         */

        /* Our OSC have the format
         *   OSC number ; rest of string ST
         * where the rest of the string may or may not contain more semicolons.
         *
         * First, extract the number.
         */

        auto str = seq.string_utf8();
        vte::parser::StringTokeniser tokeniser{str, ';'};
        auto it = tokeniser.cbegin();
        int osc;
        if (!it.number(osc))
                return;

        auto const cend = tokeniser.cend();
        ++it; /* could now be cend */

        switch (osc) {
        case VTE_OSC_VTECWF:
                set_termprop_uri(seq, it, cend,
                                 VTE_PROPERTY_ID_CURRENT_FILE_URI,
                                 PendingChanges::CWF);
                break;

        case VTE_OSC_VTECWD:
                set_termprop_uri(seq, it, cend,
                                 VTE_PROPERTY_ID_CURRENT_DIRECTORY_URI,
                                 PendingChanges::CWD);
                break;

        case VTE_OSC_VTEHYPER:
                set_current_hyperlink(seq, it, cend);
                break;

        case VTE_OSC_ITERM2_SHELL_INTEGRATION:
                set_current_shell_integration_mode(seq, it, cend);
                break;

        case -1: /* default */
        case VTE_OSC_XTERM_SET_WINDOW_AND_ICON_TITLE:
        case VTE_OSC_XTERM_SET_WINDOW_TITLE: {
                /* Only sets window title; icon title is not supported */
                auto const info = get_termprop_info(VTE_PROPERTY_ID_XTERM_TITLE);
                assert(info);

                auto set = false;
                if (it != cend &&
                    it.size_remaining() <= vte::terminal::TermpropInfo::k_max_string_len) {
                        if (auto const old_value = get_termprop(*info);
                            !old_value ||
                            !std::holds_alternative<std::string>(*old_value) ||
                            std::get<std::string>(*old_value) != it.string_view_remaining()) {
                                set = true;
                                m_termprops_dirty.at(info->id()) = true;
                                m_termprop_values.at(info->id()) = it.string_remaining();
                        }
                } else {
                        set = true;
                        reset_termprop(*info);
                }

                if (set) {
                        m_pending_changes |= vte::to_integral(PendingChanges::TERMPROPS) |
                                vte::to_integral(PendingChanges::TITLE);
                }
                break;
        }

        case VTE_OSC_XTERM_SET_COLOR:
                set_color(seq, it, cend, OSCValuedColorSequenceKind::XTermColor, osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_SPECIAL:
                set_color(seq, it, cend, OSCValuedColorSequenceKind::XTermSpecialColor, osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_TEXT_FG:
                set_special_color(seq, it, cend, ColorPaletteIndex::default_fg(), osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_TEXT_BG:
                set_special_color(seq, it, cend, ColorPaletteIndex::default_bg(), osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_CURSOR_BG:
                set_special_color(seq, it, cend, ColorPaletteIndex::cursor_bg(), osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_HIGHLIGHT_BG:
                set_special_color(seq, it, cend, ColorPaletteIndex::highlight_bg(), osc);
                break;

        case VTE_OSC_XTERM_SET_COLOR_HIGHLIGHT_FG:
                set_special_color(seq, it, cend, ColorPaletteIndex::highlight_fg(), osc);
                break;

        case VTE_OSC_XTERM_RESET_COLOR:
                reset_color(seq, it, cend, OSCValuedColorSequenceKind::XTermColor);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_SPECIAL:
                reset_color(seq, it, cend, OSCValuedColorSequenceKind::XTermSpecialColor);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_TEXT_FG:
                reset_color(ColorPaletteIndex::default_fg(), ColorSource::Escape);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_TEXT_BG:
                reset_color(ColorPaletteIndex::default_bg(), ColorSource::Escape);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_CURSOR_BG:
                reset_color(ColorPaletteIndex::cursor_bg(), ColorSource::Escape);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_HIGHLIGHT_BG:
                reset_color(ColorPaletteIndex::highlight_bg(), ColorSource::Escape);
                break;

        case VTE_OSC_XTERM_RESET_COLOR_HIGHLIGHT_FG:
                reset_color(ColorPaletteIndex::highlight_fg(), ColorSource::Escape);
                break;

        case VTE_OSC_VTE_TERMPROP:
                vte_termprop(seq, it, cend);
                break;

        case VTE_OSC_URXVT_EXTENSION:
                urxvt_extension(seq, it, cend);
                break;

        case VTE_OSC_XTERM_SET_ICON_TITLE:
        case VTE_OSC_XTERM_SET_XPROPERTY:
        case VTE_OSC_XTERM_SET_COLOR_MOUSE_CURSOR_FG:
        case VTE_OSC_XTERM_SET_COLOR_MOUSE_CURSOR_BG:
        case VTE_OSC_XTERM_SET_COLOR_TEK_FG:
        case VTE_OSC_XTERM_SET_COLOR_TEK_BG:
        case VTE_OSC_XTERM_SET_COLOR_TEK_CURSOR:
        case VTE_OSC_XTERM_SET_CURSOR_NAME:
        case VTE_OSC_XTERM_LOGFILE:
        case VTE_OSC_XTERM_SET_FONT:
        case VTE_OSC_XTERM_SET_XSELECTION:
        case VTE_OSC_XTERM_SET_COLOR_MODE:
        case VTE_OSC_XTERM_RESET_COLOR_MOUSE_CURSOR_FG:
        case VTE_OSC_XTERM_RESET_COLOR_MOUSE_CURSOR_BG:
        case VTE_OSC_XTERM_RESET_COLOR_TEK_FG:
        case VTE_OSC_XTERM_RESET_COLOR_TEK_BG:
        case VTE_OSC_XTERM_RESET_COLOR_TEK_CURSOR:
        case VTE_OSC_EMACS_51:
        case VTE_OSC_ITERM2_1337:
        case VTE_OSC_ITERM2_GROWL:
        case VTE_OSC_KONSOLE_30:
        case VTE_OSC_KONSOLE_31:
        case VTE_OSC_RLOGIN_SET_KANJI_MODE:
        case VTE_OSC_RLOGIN_SPEECH:
        case VTE_OSC_RXVT_SET_BACKGROUND_PIXMAP:
        case VTE_OSC_RXVT_SET_COLOR_FG:
        case VTE_OSC_RXVT_SET_COLOR_BG:
        case VTE_OSC_RXVT_DUMP_SCREEN:
        case VTE_OSC_URXVT_SET_LOCALE:
        case VTE_OSC_URXVT_VERSION:
        case VTE_OSC_URXVT_SET_COLOR_TEXT_ITALIC:
        case VTE_OSC_URXVT_SET_COLOR_TEXT_BOLD:
        case VTE_OSC_URXVT_SET_COLOR_UNDERLINE:
        case VTE_OSC_URXVT_SET_COLOR_BORDER:
        case VTE_OSC_URXVT_SET_FONT:
        case VTE_OSC_URXVT_SET_FONT_BOLD:
        case VTE_OSC_URXVT_SET_FONT_ITALIC:
        case VTE_OSC_URXVT_SET_FONT_BOLD_ITALIC:
        case VTE_OSC_URXVT_VIEW_UP:
        case VTE_OSC_URXVT_VIEW_DOWN:
        case VTE_OSC_YF_RQGWR:
        default:
                break;
        }
}

void
Terminal::PEC(vte::parser::Sequence const& seq)
{
        /*
         * PEC - presentation expand or contract
         *
         * References: ECMA-48 § 8.3.90
         *
         * Not applicable in VTE.
         */
}

void
Terminal::PFS(vte::parser::Sequence const& seq)
{
        /*
         * PFS - page format selection
         *
         * References: ECMA-48 § 8.3.91
         *
         * Not applicable in VTE.
         */
}

void
Terminal::PLD(vte::parser::Sequence const& seq)
{
        /*
         * PLD - partial line forward
         *
         * References: ECMA-48 § 8.3.92
         *
         * Could use this to implement subscript text.
         */
}

void
Terminal::PLU(vte::parser::Sequence const& seq)
{
        /*
         * PLU - partial line backward
         *
         * References: ECMA-48 § 8.3.93
         *
         * Could use this to implement superscript text.
         */
}

void
Terminal::PP(vte::parser::Sequence const& seq)
{
        /*
         * PP - preceding page
         * Move cursor to home on the previous page (presentation).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: number of pages to move backward
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.95
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PPA(vte::parser::Sequence const& seq)
{
        /*
         * PPA - page position absolute
         * Move the cursor to the current position on the specified page
         * (data).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: absolute page number
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.96
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PPB(vte::parser::Sequence const& seq)
{
        /*
         * PPB - page position backward
         * Move the cursor to the current position on a preceding page (data).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: number of pages to move backward
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.97
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PPR(vte::parser::Sequence const& seq)
{
        /*
         * PPR - page position foward
         * Move the cursor to the current position on a following page (data).
         * (Ignored if there is only one page.)
         *
         * Arguments:
         *   args[0]: number of pages to move forward
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.98
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PTX(vte::parser::Sequence const& seq)
{
        /*
         * PTX - parallel texts
         *
         * Arguments:
         *   args[0]: mode
         *     0 = End of parallel texts
         *     1 = Start of a string of principal parallel text
         *     2 = Start of a string of supplementary parallel text
         *     3 = Start of a string of supplementary japanese
         *         phonetic annotations
         *     4 = Start of a string of supplementary chinese
         *         phonetic annotations
         *     5 = Start of a string of supplementary phonetic
         *        annotations
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.99
         *             VT525
         *
         * Since VTE only has one page, this is ignored.
         */
}

void
Terminal::PU1(vte::parser::Sequence const& seq)
{
        /*
         * PU1 - private use 1
         *
         * References: ECMA-48 § 8.3.100
         *
         * Not worth implementing.
         */
}

void
Terminal::PU2(vte::parser::Sequence const& seq)
{
        /*
         * PU1 - private use 2
         *
         * References: ECMA-48 § 8.3.101
         *
         * Not worth implementing.
         */
}

void
Terminal::QUAD(vte::parser::Sequence const& seq)
{
        /*
         * QUAD - quad
         *
         * References: ECMA-48 § 8.3.102
         *
         * Probably not worth implementing.
         */
}

void
Terminal::REP(vte::parser::Sequence const& seq)
{
        /*
         * REP - repeat
         * Repeat the preceding graphics-character the given number of times.
         * @args[0] specifies how often it shall be repeated. 0 is treated as 1.
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.103
         */

        if (m_last_graphic_character == 0)
                return;

        auto const count = seq.collect1(0, 1, 1, int(m_column_count - m_screen->cursor.col));

        // FIXMEchpe insert in one run so we only clean up fragments once
        for (auto i = 0; i < count; i++)
                insert_char(m_last_graphic_character, true);
}

void
Terminal::RI(vte::parser::Sequence const& seq)
{
        /*
         * RI - reverse-index
         * Moves the cursor up one line in the same column. If the cursor is at
         * the top margin, the page scrolls down.
         *
         * References: ECMA-48 § 8.3.104
         */
#if 0
        screen_cursor_up(screen, 1, true);
#endif

        maybe_retreat_cursor();
        cursor_up_with_scrolling(true);
}

void
Terminal::RIS(vte::parser::Sequence const& seq)
{
        /*
         * RIS - reset-to-initial-state
         * Reset to initial state.
         * [list of things reset]
         *
         * When not using private colour registers, this
         * must also clear (assign to black) all SIXEL
         * colour registers. (DEC PPLV2 § 5.8)
         *
         * References: ECMA-48 § 8.3.105
         */

	reset(true, true);
}

void
Terminal::RLOGIN_MML(vte::parser::Sequence const& seq)
{
        /*
         * RLOGIN_MML - RLogin music markup language
         * Music macro language and Midi file support.
         *
         * The music macro language appears to be (a variant of,
         * or based on) the Microsoft and/or Tandy BASIC MML, see
         * [http://www.vgmpf.com/Wiki/index.php?title=Microsoft_BASIC_MML]
         * and
         * [http://www.vgmpf.com/Wiki/index.php?title=Tandy_BASIC_MML].
         * for more information on them, and the RLogin source code; as
         * well as [http://nanno.dip.jp/softlib/man/rlogin/ctrlcode.html#DCS]
         * for this escape sequence's parameters.
         *
         * Probably not worth implementing.
         *
         * References: RLogin
         */
}

void
Terminal::RM_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * RM_ECMA - reset-mode-ecma
         *
         * Defaults: none
         *
         * References: ECMA-48 § 8.3.106
         */

        set_mode_ecma(seq, false);
}

void
Terminal::RM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * RM_DEC - reset-mode-dec
         * This is the same as RM_ECMA but for DEC modes.
         *
         * Defaults: none
         *
         * References: VT525
         */

        set_mode_private(seq, false);
}


void
Terminal::RM_HP(vte::parser::Sequence const& seq)
{
        /*
         * RM_HP - set mode hp
         * This is the same as RM_ECMA but for HP private modes.
         *
         * See SM_HP for information about known modes.
         *
         * Defaults: none
         *
         * References: HP 2397A
         */

        /* Not worth implementing */
}
void
Terminal::SCORC(vte::parser::Sequence const& seq)
{
        /*
         * SCORC - SCO restore cursor
         * Works like DECRC, except in that it does not restore the page.
         * While this is an obsolete sequence from an obsolete terminal,
         * and not used in terminfo, there still are some programmes
         * that use it and break when it's not implemented; see issue#48.
         *
         * References: VT525
         */

        restore_cursor();
}

void
Terminal::SCOSC(vte::parser::Sequence const& seq)
{
        /*
         * SCORC - SCO save cursor
         * Works like DECSC, except in that it does not save the page.
         * While this is an obsolete sequence from an obsolete terminal,
         * and not used in terminfo, there still are some programmes
         * that use it and break when it's not implemented; see issue#48.
         *
         * References: VT525
         */

        save_cursor();
}

void
Terminal::SACS(vte::parser::Sequence const& seq)
{
        /*
         * SACS - set additional character separation
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.107
         *
         * Not applicable in VTE.
         */
}

void
Terminal::SAPV(vte::parser::Sequence const& seq)
{
        /*
         * SAPV - select alternative presentation variants
         * Set variants for the presentation of following text.
         *
         * Arguments:
         *   args[0]: type
         *     0 = default presentation; cancels the previous SAPV
         *     ...
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.108
         */
}

void
Terminal::SCO(vte::parser::Sequence const& seq)
{
        /*
         * SCO - select character orientation
         * Set the rotation for the presentation of following text.
         * (positive orientation).
         *
         * Arguments:
         *   args[0]: orientation 0…7 specifying a multiple of 45°
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.110
         */
}

void
Terminal::SCP(vte::parser::Sequence const& seq)
{
        /*
         * SCP - select character path
         * Set the character path relative to the line orientation
         * (presentation).
         *
         * Arguments:
         *   args[0]: path
         *     0 in Terminal-wg/bidi and VTE = terminal's default
         *     1 = LTR or TTB (for horizontal/vertical line orientation)
         *     2 = RTL or BTT (for horizontal/vertical line orientation)
         *   args[1]: effect
         *     0 in ECMA = implementation-defined
         *     0 in Terminal-wg/bidi and VTE = see Terminal-wg/bidi
         *     1 = ...
         *     2 = ...
         *
         * Defaults:
         *   args[0] in ECMA: no default
         *   args[1] in ECMA: no default
         *   args[0] in Terminal-wg/bidi: 0
         *   args[1] in Terminal-wg/bidi: 0
         *
         * References: ECMA-48 § 8.3.111
         *             Terminal-wg/bidi
         */

        auto const param = seq.collect1(0);
        switch (param) {
        case -1:
        case 0:
                /* FIXME switch to the emulator's default, once we have that concept */
                m_bidi_rtl = FALSE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: default direction restored\n");
                break;
        case 1:
                m_bidi_rtl = FALSE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: switch to LTR\n");
                break;
        case 2:
                m_bidi_rtl = TRUE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: switch to RTL\n");
                break;
        default:
                return;
        }

        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_RTL);
}

void
Terminal::SCS(vte::parser::Sequence const& seq)
{
        /*
         * SCS - set character spacing
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.112
         */
}

void
Terminal::SD(vte::parser::Sequence const& seq)
{
        /*
         * SD - scroll down / pan up
         * Scrolls down a number of lines (presentation).
         *
         * Arguments:
         *   args[0]: number of lines to scroll
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.113
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_scroll_down(screen->page,
                                num,
                                &screen->state.attr,
                                screen->age,
                                NULL);
#endif

        /* Scroll the text down N lines in the scrolling region, but don't move the cursor. */
        auto value = std::max(seq.collect1(0, 1), int(1));
        scroll_text_down(m_scrolling_region, value, true /* fill */);
}

void
Terminal::SD_OR_XTERM_IHMT(vte::parser::Sequence const& seq)
{
        /*
         * There's a conflict between SD and XTERM IHMT that we
         * have to resolve by checking the parameter count.
         * XTERM_IHMT needs exactly 5 arguments, SD takes 0 or 1.
         */
        if (seq.size_final() <= 1)
                SD(seq);
        #ifdef PARSER_INCLUDE_NOP
        else
                XTERM_IHMT(seq);
        #endif
}

void
Terminal::SDS(vte::parser::Sequence const& seq)
{
        /*
         * SDS - start directed string
         *
         * Arguments:
         *   args[0]: direction
         *     0 = End of directed string
         *     1 = Start of LTR string
         *     2 = Start of RTL string
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.114
         */
}

void
Terminal::SEE(vte::parser::Sequence const& seq)
{
        /*
         * SEE - select editing extent
         *
         * Arguments:
         *   args[0]: extent
         *     0 = ...
         *     1 = ...
         *     2 = ...
         *     3 = ...
         *     4 = ...
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.115
         */
}

void
Terminal::SEF(vte::parser::Sequence const& seq)
{
        /*
         * SEF - sheet eject and feed
         *
         * Arguments:
         *   args[0]:
         *   args[1]:
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * References: ECMA-48 § 8.3.116
         *
         * Probably not worth implementing.
         */
}

void
Terminal::SGR(vte::parser::Sequence const& seq)
{
        /*
         * SGR - select-graphics-rendition
         * Selects the character attributes to use for newly inserted
         * characters.
         *
         * Arguments:
         *   args[0:]: the attributes
         *     0 = reset all attributes
         *
         * Defaults:
         *   args[0]: 0 (reset all attributes)
         *
         * References: ECMA-48 § 8.3.117
         *             VT525
         */

        vte::parser::collect_sgr(seq, 0, m_defaults.attr);

        // ... and save the new colors
        m_color_defaults.attr.copy_colors(m_defaults.attr);
}

void
Terminal::SHS(vte::parser::Sequence const& seq)
{
        /*
         * SHS - select character spacing
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.118
         *
         * Not applicable in VTE.
         */
}

void
Terminal::SIMD(vte::parser::Sequence const& seq)
{
        /*
         * SIMD - select implicit movement direction
         *
         * Arguments:
         *   args[0]: direction
         *     0 = character progression
         *     1 = opposite of character progression
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.120
         */
}

void
Terminal::SL(vte::parser::Sequence const& seq)
{
        /*
         * SL - scroll left
         *
         * Arguments:
         *   args[0]: number of character positions (presentation)
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.121
         */

        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        /* If the cursor (xterm-like interpretation when about to wrap) is outside
         * the DECSTBM / DECSLRM scrolling region then do nothing. */
        if (!m_scrolling_region.contains_row_col(cursor_row, cursor_col)) {
                return;
        }

        /* As per xterm, do not clear the "about to wrap" state, so no maybe_retreat_cursor() here. */

        /* Scroll the text to the left by N lines in the scrolling region, but don't move the cursor. */
        auto value = std::max(seq.collect1(0, 1), int(1));
        scroll_text_left(m_scrolling_region, value, true /* fill */);
}

void
Terminal::SLH(vte::parser::Sequence const& seq)
{
        /*
         * SLH - set line home
         *
         * Arguments:
         *   args[0]: position in the active line
         *
         * Defaults:
         *   args[0]: no default
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.122
         *             Terminal-wg/bidi
         */
}

void
Terminal::SLL(vte::parser::Sequence const& seq)
{
        /*
         * SLL - set line limit
         *
         * Arguments:
         *   args[0]: position in the active line
         *
         * Defaults:
         *   args[0]: no default
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.123
         *             Terminal-wg/bidi
         */
}

void
Terminal::SLS(vte::parser::Sequence const& seq)
{
        /*
         * SLS - set line spacing
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.124
         *
         * Not applicable in VTE.
         */
}

void
Terminal::SM_ECMA(vte::parser::Sequence const& seq)
{
        /*
         * SM_ECMA - set-mode-ecma
         *
         * Defaults: none
         *
         * References: ECMA-48 § 8.3.125
         */

        set_mode_ecma(seq, true);
}

void
Terminal::SM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * SM_DEC - set-mode-dec
         * This is the same as SM_ECMA but for DEC modes.
         *
         * Defaults: none
         *
         * References: VT525
         */

        set_mode_private(seq, true);
}

void
Terminal::SM_HP(vte::parser::Sequence const& seq)
{
        /*
         * SM_HP - set mode hp
         * This is the same as SM_ECMA but for HP private modes.
         *
         * Known modes:
         *   1: multipage mode
         *      If reset, the terminal only has one page of 24 lines of display memory
         *      Default: reset
         *   2: memory lock
         *      Default: reset
         *
         * Defaults: none
         *
         * References: HP 2397A
         */

        /* Not worth implementing */
}

void
Terminal::SOH(vte::parser::Sequence const& seq)
{
        /*
         * SOH - start of heading
         *
         * References: ECMA-48 § 8.3.127
         *             ECMA-16 § 3.1.1
         */
}

void
Terminal::SPA(vte::parser::Sequence const& seq)
{
        /*
         * SPA - start of protected area
         * Marks the start of an area of positions (presentation)
         * that are protected; the end of the area will be
         * marked by EPA.
         *
         * The contents of the area will be protected against
         * alteration, transfer (depending on the GATM setting),
         * and erasure (depending on the ERM setting).
         *
         * References: ECMA-48 § 8.3.129
         */
}

void
Terminal::SPD(vte::parser::Sequence const& seq)
{
        /*
         * SPD - select presentation directions
         *
         * Arguments:
         *   args[0]: line orientation, progression, character path
         *     0 = horizontal, TTB, LTR
         *     1 = vertical,   RTL, TTB
         *     2 = vertical,   LTR, TTB
         *     3 = horizontal, TTB, RTL
         *     4 = vertical,   LTR, BTT
         *     5 = horizontal, BTT, RTL
         *     6 = horizontal, BTT, LTR
         *     7 = vertical,   RTL, BTT
         *
         *   args[1]: effect
         *     0 = implementation-defined
         *     1 = ...
         *     2 = ...
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * References: ECMA-48 § 8.3.130
         *             Terminal-wg/bidi
         */

        auto const param = seq.collect1(0);
        switch (param) {
        case -1:
        case 0:
                m_bidi_rtl = FALSE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: switch to LTR\n");
                break;
        case 3:
                m_bidi_rtl = TRUE;
                _vte_debug_print(VTE_DEBUG_BIDI, "BiDi: switch to RTL\n");
                break;
        default:
                return;
        }

        maybe_apply_bidi_attributes(VTE_BIDI_FLAG_RTL);

        /* FIXME maybe apply to all the onscreen lines? */
}

void
Terminal::SPH(vte::parser::Sequence const& seq)
{
        /*
         * SPH - set page home
         *
         * Arguments:
         *   args[0]: position in the active page
         *
         * Defaults:
         *   args[0]: no default
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.131
         *             Terminal-wg/bidi
         */
}

void
Terminal::SPI(vte::parser::Sequence const& seq)
{
        /*
         * SPI - spacing increment
         * Set line and character spacing for following text.
         *
         * Arguments:
         *   args[0]: line spacing (in the unit set by SSU)
         *   args[0]: character spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *   args[1]: no default
         *
         * References: ECMA-48 § 8.3.132
         */
}

void
Terminal::SPL(vte::parser::Sequence const& seq)
{
        /*
         * SPL - set page limit
         *
         * Arguments:
         *   args[0]: line position in the active page
         *
         * Defaults:
         *   args[0]: no default
         *
         * Depending on DCSM, this function works on the presentation
         * or data position. Terminal-wg/bidi forces DCSM to DATA.
         *
         * References: ECMA-48 § 8.3.133
         *             Terminal-wg/bidi
         */
}

void
Terminal::SPQR(vte::parser::Sequence const& seq)
{
        /*
         * SPQR - select print quality and rapidity
         *
         * Arguments:
         *   args[0]:
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.134
         */
}

void
Terminal::SR(vte::parser::Sequence const& seq)
{
        /*
         * SL - scroll right
         *
         * Arguments:
         *   args[0]: number of character positions (presentation)
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.135
         */

        auto cursor_row = get_xterm_cursor_row();
        auto cursor_col = get_xterm_cursor_column();

        /* If the cursor (xterm-like interpretation when about to wrap) is outside
         * the DECSTBM / DECSLRM scrolling region then do nothing. */
        if (!m_scrolling_region.contains_row_col(cursor_row, cursor_col)) {
                return;
        }

        /* As per xterm, do not clear the "about to wrap" state, so no maybe_retreat_cursor() here. */

        /* Scroll the text to the right by N lines in the scrolling region, but don't move the cursor. */
        auto value = std::max(seq.collect1(0, 1), int(1));
        scroll_text_right(m_scrolling_region, value, true /* fill */);
}

void
Terminal::SRCS(vte::parser::Sequence const& seq)
{
        /*
         * SRCS - set reduced character separation
         *
         * Arguments:
         *   args[0]: spacing (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.136
         *
         * Not applicable in VTE.
         */
}

void
Terminal::SRS(vte::parser::Sequence const& seq)
{
        /*
         * SRS - start reversed string
         *
         * Arguments:
         *   args[0]: direction
         *     0 = End of reversed string
         *     1 = Start of reversed string
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.137
         */
}

void
Terminal::SSA(vte::parser::Sequence const& seq)
{
        /*
         * SSA - start of selected area
         * Marks the start of an area of positions (presentation)
         * that are selected for transfer; the end of the area will
         * be marked by ESA.
         *
         * What will actually be transmitted depends on the setting
         * of the GATM mode, and areas set by the DAQ and SPA/EPA
         * functions.
         *
         * References: ECMA-48 § 8.3.138
         */
}

void
Terminal::SSU(vte::parser::Sequence const& seq)
{
        /*
         * SSU - set size unit
         *
         * Arguments:
         *   args[0]: unit
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.139
         */
}

void
Terminal::SSW(vte::parser::Sequence const& seq)
{
        /*
         * SSW - set space width
         *
         * Arguments:
         *   args[0]: width (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.140
         */
}

void
Terminal::SS2(vte::parser::Sequence const& seq)
{
        /*
         * SS2 - single-shift-2
         * Temporarily map G2 into GL for the next graphics character.
         *
         * References: ECMA-35 § 8.4, 9.4
         *             ECMA-48 § 8.3.141
         *             VT525
         */
#if 0
        screen->state.glt = &screen->g2;
#endif
}

void
Terminal::SS3(vte::parser::Sequence const& seq)
{
        /*
         * SS3 - single-shift-3
         * Temporarily map G3 into GL for the next graphics character
         *
         * References: ECMA-35 § 8.4, 9.4
         *             ECMA-48 § 8.3.142
         *             VT525
         */
#if 0
        screen->state.glt = &screen->g3;
#endif
}

void
Terminal::ST(vte::parser::Sequence const& seq)
{
        /*
         * ST - string-terminator
         * The string-terminator is usually part of control-sequences and
         * handled by the parser. In all other situations it is silently
         * ignored.
         *
         * References: ECMA-48 § 8.3.143
         */
}

void
Terminal::STAB(vte::parser::Sequence const& seq)
{
        /*
         * STAB - selective tabulation
         *
         * Arguments:
         *   args[0]:
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.144
         *             ITU-T Rec. T.416 (Open Document Architecture)
         */
}

void
Terminal::STS(vte::parser::Sequence const& seq)
{
        /*
         * STS - set transmit state
         *
         * References: ECMA-48 § 8.3.145
         *
         * Not worth implementing.
         */
}

void
Terminal::STX(vte::parser::Sequence const& seq)
{
        /*
         * STX - start of text
         *
         * References: ECMA-48 § 8.3.146
         *             ECMA-16 § 3.1.2
         *
         * Not worth implementing.
         */
}

void
Terminal::SU(vte::parser::Sequence const& seq)
{
        /*
         * SU - scroll-up / pan down
         * Scrolls up a number of lines (presentation).
         *
         * Arguments:
         *   args[0]: number of lines to scroll
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: EMCA-48 § 8.3.147
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_scroll_up(screen->page,
                              num,
                              &screen->state.attr,
                              screen->age,
                              screen->history);
#endif

        /* Scroll the text up N lines in the scrolling region, but don't move the cursor. */
        auto value = std::max(seq.collect1(0, 1), int(1));
        scroll_text_up(m_scrolling_region, value, true /* fill */);
}

void
Terminal::SUB(vte::parser::Sequence const& seq)
{
        /*
         * SUB - substitute
         * Cancel the current control-sequence and print a replacement
         * character. Our parser already handles the state changes, so
         * all we have to do is print the character.
         *
         * Use U+2426 SYMBOL FOR SUBSTITUTE FORM TWO as the character
         * to insert, since it was specifically made for this use case
         * (see https://www.unicode.org/L2/L1998/98353.pdf).
         * (Previous vte versions used U+FFFD REPLACEMENT CHARACTER.)
         * See https://gitlab.gnome.org/GNOME/vte/-/issues/2843 .
         *
         * References: ECMA-48 § 8.3.148
         *             DEC STD 070 p5-132
         */

        insert_char(0x2426u, true);
}

void
Terminal::SVS(vte::parser::Sequence const& seq)
{
        /*
         * SVS - select line spacing
         *
         * Arguments:
         *   args[0]: spacing
         *     0 = ...
         *     ...
         *     9 = ...
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.149
         */
}

void
Terminal::SYN(vte::parser::Sequence const& seq)
{
        /*
         * SYN - synchronous idle
         *
         * References: ECMA-48 § 8.3.150
         *             ECMA-16 § 3.1.9
         *
         * Not worth implementing.
         */
}

void
Terminal::TAC(vte::parser::Sequence const& seq)
{
        /*
         * TAC - tabulation aligned centre
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.151
         */
}

void
Terminal::TALE(vte::parser::Sequence const& seq)
{
        /*
         * TALE - tabulation aligned leading edge
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.152
         */
}

void
Terminal::TATE(vte::parser::Sequence const& seq)
{
        /*
         * TATE - tabulation aligned trailing edge
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.153
         */
}

void
Terminal::TBC(vte::parser::Sequence const& seq)
{
        /*
         * TBC - tab-clear
         * Clears tab stops.
         *
         * Arguments:
         *   args[0]: mode
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: ECMA-48 § 8.3.154
         */

        auto const param = seq.collect1(0);
        switch (param) {
        case -1:
        case 0:
                /* Clear character tabstop at the current presentation position */
                m_tabstops.unset(get_xterm_cursor_column());
                break;
        case 1:
                /* Clear line tabstop at the current line */
                break;
        case 2:
                /* Clear all character tabstops in the current line */
                /* NOTE: vttest issues this but claims it's a 'no-op' */
                m_tabstops.clear();
                break;
        case 3:
                /* Clear all character tabstops */
                m_tabstops.clear();
                break;
        case 4:
                /* Clear all line tabstops */
                break;
        case 5:
                /* Clear all (character and line) tabstops */
                m_tabstops.clear();
                break;
        default:
                break;
	}
}

void
Terminal::TCC(vte::parser::Sequence const& seq)
{
        /*
         * TCC - tabulation centred on character
         *
         * Defaults:
         *   args[0]: no default
         *   args[1]: 32 (SPACE)
         *
         * References: ECMA-48 § 8.3.155
         */
}

void
Terminal::TSR(vte::parser::Sequence const& seq)
{
        /*
         * TSR - tabulation stop remove
         * This clears a tab stop at position @arg[0] in the active line (presentation),
         * and on any lines below it.
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.156
         */

        auto const pos = seq.collect1(0);
        if (pos < 1 || pos > m_column_count)
                return;

        m_tabstops.unset(pos - 1);
}

void
Terminal::TSS(vte::parser::Sequence const& seq)
{
        /*
         * TSS - thin space specification
         *
         * Arguments:
         *   args[0]: width (in the unit set by SSU)
         *
         * Defaults:
         *   args[0]: no default
         *
         * References: ECMA-48 § 8.3.157
         *
         * Not applicable in VTE.
         */
}

void
Terminal::VPA(vte::parser::Sequence const& seq)
{
        /*
         * VPA - vertical line position absolute
         * Moves the cursor to the specified line on the current column (data).
         *
         * Arguments:
         *   args[0]: line number
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.158
         *             VT525
         */
#if 0
        unsigned int pos = 1;

        if (seq->args[0] > 0)
                pos = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set_rel(screen, screen->state.cursor_x, pos - 1);
#endif

        // FIXMEchpe shouldn't we maybe_retreat_cursor AFTER setting the new cursor row?
        maybe_retreat_cursor();

        auto value = seq.collect1(0, 1, 1, m_row_count);
        set_cursor_row1(value);
}

void
Terminal::VPB(vte::parser::Sequence const& seq)
{
        /*
         * VPB - line position backward
         * Moves the cursor up the specified number of lines on
         * the current column (data).
         *
         * Arguments:
         *   args[0]: line number
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.159
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif
}

void
Terminal::VPR(vte::parser::Sequence const& seq)
{
        /*
         * VPR - vertical line position relative
         * Moves the cursor down the specified number of lines
         * on the current column (data).
         *
         * Arguments:
         *   args[0]: line number
         *
         * Defaults:
         *   args[0]: 1
         *
         * References: ECMA-48 § 8.3.160
         *             VT525
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif
}

void
Terminal::VT(vte::parser::Sequence const& seq)
{
        /*
         * VT - vertical-tab
         * This causes a vertical jump by one line. Terminals treat it exactly
         * the same as LF.
         *
         * References: ECMA-48 § 8.3.161
         */

        LF(seq);
}

void
Terminal::VTS(vte::parser::Sequence const& seq)
{
        /*
         * VTS - line tabulation set
         * Sets a tabstop in the active line (presentation).
         *
         * References: ECMA-48 § 8.3.162
         *
         * Not worth implementing.
         */
}

void
Terminal::WYCAA(vte::parser::Sequence const& seq)
{
        /*
         * WYCAA - redefine character display attribute association
         *
         * Arguments:
         *   args[0]: mode
         *
         * Defaults:
         *   args[0]: no defaults
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */

        switch (seq.collect1(0)) {
        case -1:
                break;

        case 0 ... 47:
                /* WYCAA - redefine character attribute association
                 *
                 * Arguments:
                 *   args[0]: character attribute association to be set (0…47)
                 *   args[1]: palette color index for foreground color (0…64)
                 *   args[2]: palette color index for background color (0…64)
                 *   args[3]: new definition for the attribute association @args[0]
                 *
                 * Defaults:
                 *   args[0]: ?
                 *   args[1]: ?
                 *   args[2]: ?
                 *   args[3]: ?
                 */
                break;

        case 48:
                /* WYCOLOR - select foreground color palette
                 *
                 * Arguments:
                 *   args[1]: color palette number 0…7
                 *
                 * Defaults:
                 *   args[1]: ?
                 */
                break;

        case 49:
        case 51 ... 52:
                /* WYCOLOR - select background (49)/screen border(51)/cursor(52) color
                 * Selects the background (and screen border) color.
                 *
                 * Arguments:
                 *   args[1]: palette color index 0…64
                 *
                 * Defaults:
                 *   args[1]: ?
                 */
                break;

        case 50:
                /* WYCOLOR - restore fore- and background colors to set-up default */
                break;

        case 53:
                /* WYSOVR - select overstrike position
                 *
                 * Arguments:
                 *   args[1]: scanline number in the charcell (0=top, …bottom) to
                 *            put the overstrike
                 *
                 * Defaults:
                 *   args[1]:
                 */
                break;

        case 54 ... 57:
                /* WYCOLOR - select attributes and colors
                 * for user status line (54), system status line(55),
                 * replacement character(56), noneraseable character(57).
                 *
                 * Arguments:
                 *   args[1]:
                 *   args[2]:
                 *
                 * Defaults:
                 *   args[1]:
                 *   args[2]:
                 */

        case 58:
                /* WYDTSET - set date and time */
                break;

        case 59:
                /* WYDFPG - define page for session
                 *
                 * Arguments:
                 *   args[1]:
                 *   args[2]:
                 *   args[3]:
                 *   args[4]:
                 *
                 * Defaults:
                 *   args[1]:
                 *   args[2]:
                 *   args[3]:
                 *   args[4]:
                 */
                break;

        case 60:
                /* WYIND - restore default color index values */
                break;

        case 61 ... 62:
        case 64 ... 65:
                /* WYIND - set current fore/background color
                 * Sets the current fore- (61, 64) or background (62, 65)
                 * color for eraseable (61, 62) or noneraseable (64, 65)
                 * characters.
                 *
                 * Also turns on color index mode.
                 *
                 * Arguments:
                 *   args[1]: color index
                 *
                 * Defaults:
                 *   args[1]: ?
                 */
                break;

        case 63:
                /* WYIND - turn color index mode on/off
                 *
                 * Arguments:
                 *   args[1]: setting (0 = off, 1 = on)
                 *
                 * Defaults:
                 *   args[1]: ?
                 */
                break;

        case 66:
                /* WYIND - redefine color index
                 *
                 * Arguments:
                 *   args[1]: index
                 *   args[2]: value
                 *
                 * Defaults:
                 *   args[1]: ?
                 *   args[2]: ?
                 */
                break;
        }
}

void
Terminal::WYCDIR(vte::parser::Sequence const& seq)
{
        /*
         * WYCDIR - set current character color and attributes
         * Sets the foreground and background colours used for SGR attributes.
         *
         * Arguments:
         *   args[0]: foreground colour (0…64)
         *   args[1]: background colour (0…64)
         *   args[2]: SGR attribute (0…15)
         *
         * Defaults:
         *   args[0]: default foreground colour
         *   args[1]: default background colour
         *   args[2]: default attribute (0)
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::WYDHL_BH(vte::parser::Sequence const& seq)
{
        /*
         * WYDHL_BH - single width double height line: bottom half
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::WYDHL_TH(vte::parser::Sequence const& seq)
{
        /*
         * WYDHL_TH - single width double height line: top half
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::WYSCRATE(vte::parser::Sequence const& seq)
{
        /*
         * WYSCRATE - set smooth scroll rate
         * Selects scrolling rate if DECSCLM is set.
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::WYLSFNT(vte::parser::Sequence const& seq)
{
        /*
         * WYLSFNT - load soft font
         *
         * Probably not worth implementing.
         *
         * References: WY370
         */
}

void
Terminal::XDGSYNC(vte::parser::Sequence const& seq)
{
        /*
         * XDGSYNC - synchronous update
         * Content received between BSU and ESU will be committed
         * atomically on ESU. This is to avoid half-drawn screen
         * content.
         * The terminal may ignore this, or apply a timeout, or
         * terminate the synchronous update prematurely for any
         * reason.
         *
         * Arguments:
         *   args[0]:
         *     1: start (begin synchronous update, BSU)
         *     2: end   (end synchronous update, ESU)
         *
         * Defaults:
         *   args[0]: no defaults
         *
         * References: https://gitlab.com/gnachman/iterm2/wikis/synchronized-updates-spec
         */

        /* TODO: implement this! https://gitlab.gnome.org/GNOME/vte/issues/15 */
}

void
Terminal::XTERM_CHECKSUM_MODE(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_CHECKSUM_MODE - xterm DECRQCRA checksum mode
         * Sets how DECRQCRA calculates the area checksum.
         *
         * Arguments:
         *   args[0]: flag value composed of the following flags:
         *     1: no negation
         *     2: don't report attributes
         *     4: checksum trailing blanks
         *     8: don't checksum empty cells
         *     16: no 8-bit masking or ignoring combining characters
         *     32: no 7-bit masking
         *
         * Defaults:
         *   args[0]: 0, matching the output from VTxxx terminals
         *
         * References: XTERM 335
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_GETXRES(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_GETXRES - xterm get X resource
         *
         * References: XTERM 350
         *
         * Won't implement.
         */
}

void
Terminal::XTERM_IHMT(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_IHMT - xterm-initiate-highlight-mouse-tracking
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_MLHP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_MLHP - xterm-memory-lock-hp-bugfix
         *
         * This seems bogus; SM_HP 2 is the way to set the memory lock on
         * HP terminal.
         *
         * References: XTERM
         *
         * Not worth implementing.
         */
}

void
Terminal::XTERM_MUHP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_MUHP - xterm-memory-unlock-hp-bugfix
         *
         * This seems bogus; RM_HP 2 is the way to unset the memory lock on
         * HP terminal.
         *
         * References: XTERM
         *
         * Not worth implementing.
         */
}

void
Terminal::XTERM_MODKEYS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_MODKEYS - xterm set key modifier options
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_POPCOLORS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_POPCOLORS: pop color palette stack
         * Restore color palette attributes previously pushed to the stack
         * with XTERM_PUSHCOLORS. If there is nothing on the
         * stack, does nothing.
         *
         * Arguments: none
         *
         * References: XTERM 357
         *
         * See issue vte#23.
         */
}

void
Terminal::XTERM_POPSGR(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_POPSGR: pop SGR stack
         * Restore SGR attributes previously pushed to the stack
         * with XTERM_PUSHSGR. If there is nothing on the
         * stack, does nothing.
         *
         * Arguments: none
         *
         * References: XTERM 334
         *
         * Note: The {PUSH,POP,REPORT}SGR protocol is poorly thought-out, and has
         * no real use case. See the discussion at issue vte#23.
         * Probably won't implement.
         */
}

void
Terminal::XTERM_PUSHCOLORS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_PUSHSGR: push color palette stack
         * Push current color palette to the stack.
         * If the stack is full, drops the bottommost item before
         * pushing on the stack.
         *
         * If there are any arguments, they are interpreted as in SGR
         * to denote which attributes to save; if there are no arguments,
         * all attributes are saved.
         *
         * Arguments:
         *   args[0:]: the attributes
         *     0 = save all attributes
         *
         * Defaults:
         *   args[0]: 0 (save all attributes)
         *
         * References: XTERM 357
         *
         * See issue vte#23.
         */
}

void
Terminal::XTERM_PUSHSGR(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_PUSHSGR: push SGR stack
         * Push current SGR attributes to the stack.
         * If the stack is full, drops the bottommost item before
         * pushing on the stack.
         *
         * If there are any arguments, they are interpreted as in SGR
         * to denote which attributes to save; if there are no arguments,
         * all attributes are saved.
         *
         * Arguments:
         *   args[0:]: the attributes
         *     0 = save all attributes
         *
         * Defaults:
         *   args[0]: 0 (save all attributes)
         *
         * References: XTERM 334
         *
         * Note: The {PUSH,POP,REPORT}SGR protocol is poorly thought-out, and has
         * no real use case. See the discussion at issue vte#23.
         * Probably won't implement.
         */
}

void
Terminal::XTERM_REPORTCOLORS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_REPORTCOLORS: report color palette on stack
         *
         * References: XTERM 357
         *
         * See issue vte#23.
         */
}

void
Terminal::XTERM_REPORTSGR(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_REPORTSGR: report SGR attributes in rectangular area
         * Report common character attributes in the specified rectangle.
         *
         * Arguments;
         *   args[0..3]: top, left, bottom, right of the rectangle (1-based)
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *   args[2]: height of current page
         *   args[3]: width of current page
         *
         * Reply: SGR
         *
         * If the top > bottom or left > right, the command is ignored.
         *
         * These coordinates are interpreted according to origin mode (DECOM),
         * but unaffected by the page margins (DECSLRM?).
         *
         * References: XTERM 334
         *
         * Note: The {PUSH,POP,REPORT}SGR protocol is poorly thought-out, and has
         * no real use case except for REPORTSGR which is used for esctest.
         * See the discussion at issue vte#23.
         */

#if VTE_DEBUG
        // Send a dummy reply unless in test mode (reuse DECRQCRA test flag)
        if ((g_test_flags & VTE_TEST_FLAG_DECRQCRA) == 0)
                return reply(seq, VTE_REPLY_SGR, {});

        auto idx = 0u;
        auto const rect = collect_rect(seq, idx);
        if (!rect)
                return; // ignore

        // This function is only exposed to esctest which will query
        // the attributes one cell at a time; don't bother trying to
        // gather the common attributes in a larger rect.
        if (rect.width() > 1 || rect.height() > 1)
                return reply(seq, VTE_REPLY_SGR, {});

        auto attr = VteCellAttr{};
        if (auto rowdata =
            m_screen->row_data->index_writable(m_screen->insert_delta + rect.top())) {
                if (auto const cell = _vte_row_data_get(rowdata, rect.left())) {
                        attr = cell->attr;
                }
        }

        auto builder = vte::parser::ReplyBuilder(VTE_REPLY_SGR, {});
        append_attr_sgr_params(attr, builder);
        return send(seq, builder);
#endif // VTE_DEBUG
}

void
Terminal::XTERM_RPM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RPM - xterm restore DEC private mode
         *
         * Defaults: none
         *
         * References: XTERM
         */

        save_mode_private(seq, false);
}

void
Terminal::XTERM_RQTCAP(vte::parser::Sequence const& seq)
try
{
        /*
         * XTERM_RQTCAP - xterm request termcap/terminfo
         *
         * Gets the terminfo/termcap string. The constrol string
         * consists of semicolon (';') separated parameters, which
         * are hex-encoded terminfo/termcap capability names.
         *
         * The response is a XTERM_TCAPR report, which consists
         * of semicolon (';') separated parameters, each of which
         * is the hex-encoded capability name, followed by an equal
         * sign ('='), followed by the hex-encoded capability.
         *
         * In xterm, an unknown capability in the control string
         * terminates processing of the control string; in vte
         * we continue past an unknown capability to process the
         * remaining capability requests.
         *
         * For now, only reply to "TN" and "name" queries, because
         * systemd requires them. See
         * https://gitlab.gnome.org/GNOME/vte/-/issues/2884 and the
         * linked systemd issue for more information.
         *
         * References: XTERM
         */

        auto str = seq.string_utf8();
        auto tokeniser = vte::parser::StringTokeniser{str, ';'};
        auto it = tokeniser.cbegin();
        auto const cend = tokeniser.cend();

        auto replystr = std::string{};
        while (it != cend) {
                auto cap = *it;
                for (auto& c : cap)
                        c = g_ascii_toupper(c);

                if (cap == "6E616D65" /* terminfo "name" */ ||
                    cap == "544E" /* termcap "TN" */) {
                        if (replystr.size())
                                replystr.push_back(';');

                        // Reply with "[cap]=xterm-256color"
                        replystr.append(cap);
                        replystr.append("=787465726D2D323536636F6C6F72");
                } else {
                        // other capability
                }

                ++it;
        }

        reply(seq, VTE_REPLY_XTERM_TCAPR, {replystr.size() ? 1 : 0}, "%s", replystr.c_str());
}
catch (...)
{
}

void
Terminal::XTERM_RRV(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RRV - xterm-reset-resource-value
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_RTM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RTM - xterm-reset-title-mode
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_SHIFTESCAPE(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SHIFTESCAPE - xterm set/reset shift escape
         * Selects whether the SHIFT key acts as a modifier in the mouse
         * protocol, or overrides the mouse protocol.
         *
         * Arguments:
         *   args[0]:
         *     0: overrides mouse protocol
         *     1: conditionally acts as modifier
         *     2: always acts as modifier
         *     3: never acts as modifier
         *
         * Defaults:
         *   args[0]: 0
         *
         * Note that args[0] values 2 and 3 are not actually executed
         * from an escape sequence, they correspond to the value of the
         * xterm resource controlling this setting.
         *
         * References: XTERM 362
         */
        /* Not worth implementing this */
}


void
Terminal::XTERM_SMGRAPHICS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SMGRAPHICS - xterm set or request graphics attributes
         * Set or request graphics attributes for SIXEL and REGIS.
         *
         * Reply: XTERM_SMGRAPHICS_REPORT
         *
         * Arguments:
         *   args[0]: select function
         *     0: number of colour registers
         *     1: SIXEL geometry
         *     2: REGIS geometry
         *   args[1]: select subfunction
         *     1: read attribute
         *     2: reset attribute
         *     3: set attribute
         *     4: read maximum value of attribute
         *   args[2:]: values, used only for subfuncion 3
         *
         * Defaults:
         *   args[0]: no default
         *   args[1]: no default
         *   args[2:]: no default
         *
         * The reply is XTERM_SMGRAPHICS_REPORT, with arguments:
         *   args[0]: function
         *   args[1]: status
         *     0: success
         *     1: error in function parameter
         *     2: error in subfunction parameter
         *     3: failure
         *
         * References: XTERM
         */

        auto const attr = seq.collect1(0);
        auto status = 1, rv0 = -2, rv1 = -2;

        reply(seq, VTE_REPLY_XTERM_SMGRAPHICS_REPORT, {attr, status, rv0, rv1});
}

void
Terminal::XTERM_SPM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SPM - xterm save DEC private mode
         *
         * Defaults: none
         *
         * References: XTERM
         */

        save_mode_private(seq, true);
}

void
Terminal::XTERM_PTRMODE(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_PTRMODE - xterm set pointer mode
         *
         * Defaults: none
         *
         * References: XTERM
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_STM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_STM - xterm-set-title-mode
         *
         * Probably not worth implementing.
         */
}

void
Terminal::XTERM_STCAP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_STCAP - xterm set termcap/terminfo
         *
         * Won't implement.
         */
}

void
Terminal::XTERM_VERSION(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_VERSION - xterm request version report
         *
         * Returns the xterm name and version as XTERM_DSR.
         *
         * Arguments:
         *   args[0]: select function
         *     0: report xterm name and version
         *
         * Defaults:
         *   args[0]: 0 (as per xterm code, no default as per xterm docs)
         *
         * References: XTERM
         */

        if (seq.collect1(0, 0) != 0)
                return;

        reply(seq, VTE_REPLY_XTERM_DSR, {}, "VTE(%d)", firmware_version());
}

void
Terminal::XTERM_WM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_WM - xterm-window-management
         *
         * Window manipulation control sequences.  Most of these are considered
         * bad ideas, but they're implemented as signals which the application
         * is free to ignore, so they're harmless.  Handle at most one action,
         * see bug 741402.
         *
         * No parameter default values.
         *
         * References: XTERM
         *             VT525
         */

        #if 0
	char buf[128];
        #endif

        int param = seq.collect1(0);
        switch (param) {
        case -1:
        case 0:
                break;

        case VTE_XTERM_WM_RESTORE_WINDOW:
                m_xterm_wm_iconified = false;
                break;

        case VTE_XTERM_WM_MINIMIZE_WINDOW:
                m_xterm_wm_iconified = true;
                break;

        case VTE_XTERM_WM_SET_WINDOW_POSITION:
                /* No-op */
                break;

        case VTE_XTERM_WM_SET_WINDOW_SIZE_PIXELS: {
                int width, height;
                seq.collect(1, {&height, &width});

                if (width != -1 && height != -1) {
                        _vte_debug_print(VTE_DEBUG_EMULATION,
                                         "Resizing window to %dx%d pixels, grid size %dx%d.\n",
                                         width, height,
                                         width / int(m_cell_height), height / int(m_cell_width));
                        emit_resize_window(width / int(m_cell_height), height / int(m_cell_width));
                }
                break;
        }

        case VTE_XTERM_WM_RAISE_WINDOW:
                break;

        case VTE_XTERM_WM_LOWER_WINDOW:
                break;

        case VTE_XTERM_WM_REFRESH_WINDOW:
                break;

        case VTE_XTERM_WM_SET_WINDOW_SIZE_CELLS: {
                int width, height;
                seq.collect(1, {&height, &width});

                if (width != -1 && height != -1) {
                        emit_resize_window(width, height);
                }
                break;
        }

        case VTE_XTERM_WM_MAXIMIZE_WINDOW:
                switch (seq.collect1(1)) {
                case -1: /* default */
                case 0:
                        /* Restore */
                        break;
                case 1:
                        /* Maximise */
                        break;
                case 2:
                        /* Maximise Vertically */
                        break;
                case 3:
                        /* Maximise Horizontally */
                        break;
                default:
                        break;
                }
                break;

        case VTE_XTERM_WM_FULLSCREEN_WINDOW:
                break;

        case VTE_XTERM_WM_GET_WINDOW_STATE:
                reply(seq, VTE_REPLY_XTERM_WM, {m_xterm_wm_iconified ? 2 : 1});
                break;

        case VTE_XTERM_WM_GET_WINDOW_POSITION:
                /* Reply with fixed origin. */
                reply(seq, VTE_REPLY_XTERM_WM, {3, 0, 0});
                break;

        case VTE_XTERM_WM_GET_WINDOW_SIZE_PIXELS: {
                auto const height = int(m_row_count * m_cell_height_unscaled);
                auto const width = int(m_column_count * m_cell_width_unscaled);
                reply(seq, VTE_REPLY_XTERM_WM, {4, height, width});
                break;
        }

        case VTE_XTERM_WM_GET_WINDOW_SIZE_CELLS:
                reply(seq, VTE_REPLY_XTERM_WM,
                      {8, (int)m_row_count, (int)m_column_count});
                break;

        case VTE_XTERM_WM_GET_SCREEN_SIZE_CELLS: {
                /* FIMXE: this should really report the monitor's workarea,
                 * or even just a fixed value.
                 */
#if VTE_GTK == 3
                auto gdkscreen = gtk_widget_get_screen(m_widget);
                int height = gdk_screen_get_height(gdkscreen);
                int width = gdk_screen_get_width(gdkscreen);
                _vte_debug_print(VTE_DEBUG_EMULATION,
                                 "Reporting screen size as %dx%d cells.\n",
                                 height / int(m_cell_height), width / int(m_cell_width));
#elif VTE_GTK == 4
                auto height = int(m_row_count * m_cell_height);
                auto width = int(m_column_count * m_cell_width);
#endif

                reply(seq, VTE_REPLY_XTERM_WM,
                      {9, height / int(m_cell_height), width / int(m_cell_width)});
                break;
        }

        case VTE_XTERM_WM_GET_ICON_TITLE:
                /* Report a static icon title, since the real
                 * icon title should NEVER be reported, as it
                 * creates a security vulnerability.  See
                 * http://marc.info/?l=bugtraq&m=104612710031920&w=2
                 * and CVE-2003-0070.
                 */
                _vte_debug_print(VTE_DEBUG_EMULATION,
                                 "Reporting empty icon title.\n");

                send(seq, vte::parser::u8SequenceBuilder{VTE_SEQ_OSC, "L"s});
                break;

        case VTE_XTERM_WM_GET_WINDOW_TITLE:
                /* Report a static window title, since the real
                 * window title should NEVER be reported, as it
                 * creates a security vulnerability.  See
                 * http://marc.info/?l=bugtraq&m=104612710031920&w=2
                 * and CVE-2003-0070.
                 */
                _vte_debug_print(VTE_DEBUG_EMULATION,
                                 "Reporting empty window title.\n");

                send(seq, vte::parser::u8SequenceBuilder{VTE_SEQ_OSC, "l"s});
                break;

        case VTE_XTERM_WM_TITLE_STACK_PUSH:
                switch (seq.collect1(1)) {
                case -1:
                case VTE_OSC_XTERM_SET_WINDOW_AND_ICON_TITLE:
                case VTE_OSC_XTERM_SET_WINDOW_TITLE: {
                        if (m_window_title_stack.size() >= VTE_WINDOW_TITLE_STACK_MAX_DEPTH) {
                                /* Drop the bottommost item */
                                m_window_title_stack.erase(m_window_title_stack.cbegin());
                        }

                        auto const info = get_termprop_info(VTE_PROPERTY_ID_XTERM_TITLE);
                        auto const value = get_termprop(*info);
                        if (value &&
                            std::holds_alternative<std::string>(*value))
                                m_window_title_stack.emplace(m_window_title_stack.cend(),
                                                             std::get<std::string>(*value));
                        else
                                m_window_title_stack.emplace(m_window_title_stack.cend(),
                                                             std::string{});

                        vte_assert_cmpuint(m_window_title_stack.size(), <=, VTE_WINDOW_TITLE_STACK_MAX_DEPTH);
                        break;
                }

                case VTE_OSC_XTERM_SET_ICON_TITLE:
                default:
                        break;
                }
                break;

        case VTE_XTERM_WM_TITLE_STACK_POP:
                switch (seq.collect1(1)) {
                case -1:
                case VTE_OSC_XTERM_SET_WINDOW_AND_ICON_TITLE:
                case VTE_OSC_XTERM_SET_WINDOW_TITLE: {
                        if (m_window_title_stack.empty())
                                break;

                        auto const info = get_termprop_info(VTE_PROPERTY_ID_XTERM_TITLE);
                        m_termprops_dirty.at(info->id()) = true;
                        m_termprop_values.at(info->id()) = std::move(m_window_title_stack.back());
                        m_window_title_stack.pop_back();

                        m_pending_changes |= vte::to_integral(PendingChanges::TERMPROPS) |
                                vte::to_integral(PendingChanges::TITLE);
                        break;
                }

                case VTE_OSC_XTERM_SET_ICON_TITLE:
                default:
                        break;
                }
                break;

        default:
                /* DECSLPP, handled elsewhere */
                break;
        }
}

} // namespace terminal
} // namespace vte

/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
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

#pragma once

/* BEGIN sanity checks */

#ifndef __EXCEPTIONS
#error You MUST NOT use -fno-exceptions to build vte! Fix your build; and DO NOT file a bug upstream!
#endif

#ifndef __GXX_RTTI
#error You MUST NOT use -fno-rtti to build vte! Fix your build system; and DO NOT file a bug upstream!
#endif

/* END sanity checks */

#include <glib.h>
#include "glib-glue.hh"
#include "pango-glue.hh"

#include "debug.h"
#include "clipboard-gtk.hh"
#if VTE_GTK == 3
# include "drawing-cairo.hh"
#elif VTE_GTK == 4
# include "drawing-gsk.hh"
#endif
#include "vtedefines.hh"
#include "vtetypes.hh"
#include "reaper.hh"
#include "ring.hh"
#include "ringview.hh"
#include "buffer.h"
#include "parser.hh"
#include "parser-glue.hh"
#include "modes.hh"
#include "tabstops.hh"
#include "termprops.hh"
#include "refptr.hh"
#include "fwd.hh"
#include "color-palette.hh"
#include "osc-colors.hh"
#include "rect.hh"

#include "pcre2-glue.hh"
#include "vteregexinternal.hh"

#include "chunk.hh"
#include "pty.hh"
#include "utf8.hh"

#include <list>
#include <queue>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#define GDK_ARRAY_NAME vte_char_attr_list
#define GDK_ARRAY_TYPE_NAME VteCharAttrList
#define GDK_ARRAY_ELEMENT_TYPE VteCharAttributes
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

#if WITH_A11Y
#if VTE_GTK == 3
#include "vteaccess.h"
#else
#endif
#endif

#if WITH_ICU
#include "icu-converter.hh"
#endif

enum {
        VTE_BIDI_FLAG_IMPLICIT   = 1 << 0,
        VTE_BIDI_FLAG_RTL        = 1 << 1,
        VTE_BIDI_FLAG_AUTO       = 1 << 2,
        VTE_BIDI_FLAG_BOX_MIRROR = 1 << 3,
        VTE_BIDI_FLAG_ALL        = (1 << 4) - 1,
};

namespace vte {

namespace platform {

/*
 * Cursor:
 *
 * Holds a platform cursor. This is either a named cursor (string),
 * a reference to a GdkCursor*, or a cursor type.
 */
#if VTE_GTK == 3
using Cursor = std::variant<std::string,
                            vte::glib::RefPtr<GdkCursor>,
                            GdkCursorType>;
#elif VTE_GTK == 4
using Cursor = std::variant<std::string,
                            vte::glib::RefPtr<GdkCursor>>;
#endif

} // namespace platform
} // namespace vte

typedef enum _VteCharacterReplacement {
        VTE_CHARACTER_REPLACEMENT_NONE,
        VTE_CHARACTER_REPLACEMENT_LINE_DRAWING
} VteCharacterReplacement;

typedef struct _VtePaletteColor {
	struct {
		vte::color::rgb color;
		gboolean is_set;
	} sources[2];
} VtePaletteColor;

struct VteScreen {
public:
        VteScreen(gulong max_rows,
                  bool has_streams) :
                m_ring{max_rows, has_streams},
                row_data(&m_ring),
                cursor{0,0}
        {
        }

        vte::base::Ring m_ring; /* buffer contents */
        vte::base::Ring *row_data;
        VteVisualPosition cursor;  /* absolute value, from the beginning of the terminal history */
        /* Whether the last relevant input was an explicit cursor movement or a graphic character.
         * Needed to decide if the next character will wrap at the right margin, if that differs from
         * the right edge of the terminal. See https://gitlab.gnome.org/GNOME/vte/-/issues/2677. */
        bool cursor_advanced_by_graphic_character{false};
        double scroll_delta{0.0}; /* scroll offset */
        long insert_delta{0}; /* insertion offset */

        /* Stuff saved along with the cursor */
        struct {
                VteVisualPosition cursor;  /* onscreen coordinate, that is, relative to insert_delta */
                bool cursor_advanced_by_graphic_character;
                bool reverse_mode;
                bool origin_mode;
                VteCell defaults;
                VteCell color_defaults;
                VteCharacterReplacement character_replacements[2];
                VteCharacterReplacement *character_replacement;
        } saved;
};

namespace vte {

        // Inclusive rect of integer
        using grid_rect = vte::rect_inclusive<int>;
        using grid_point = vte::point<int>;

/* Tracks the DECSTBM / DECSLRM scrolling region, a.k.a. margins.
 * For effective operation, it stores in a single boolean if at its default state. */
struct scrolling_region {
private:
        int m_width{1};
        int m_height{1};
        /* The following are 0-based, inclusive */
        int m_top{0};
        int m_bottom{0};
        int m_left{0};
        int m_right{0};
        bool m_is_restricted{false};

        constexpr void update_is_restricted() noexcept
        {
                m_is_restricted = (m_top != 0) || (m_bottom != m_height - 1) ||
                                  (m_left != 0) || (m_right != m_width - 1);
        }

public:
        constexpr scrolling_region() noexcept = default;

        inline constexpr auto top() const noexcept { return m_top; }
        inline constexpr auto bottom() const noexcept { return m_bottom; }
        inline constexpr auto left() const noexcept { return m_left; }
        inline constexpr auto right() const noexcept { return m_right; }
        inline constexpr auto is_restricted() const noexcept { return m_is_restricted; }
        inline constexpr bool contains_row_col(int row, int col) const noexcept {
                return row >= m_top && row <= m_bottom && col >= m_left && col <= m_right;
        }

        void set_vertical(int t, int b) noexcept { m_top = t; m_bottom = b; update_is_restricted(); }
        void reset_vertical() noexcept { set_vertical(0, m_height - 1); }
        void set_horizontal(int l, int r) noexcept { m_left = l; m_right = r; update_is_restricted(); }
        void reset_horizontal() noexcept { set_horizontal(0, m_width - 1); }
        void reset() noexcept { reset_vertical(); reset_horizontal(); }
        void reset_with_size(int w, int h) noexcept { m_width = w; m_height = h; reset(); }

        // FIXME inherit from grid_rect instead
        constexpr grid_rect as_rect() const noexcept
        {
                return grid_rect{left(), top(), right(), bottom()};
        }

        constexpr grid_point origin() const noexcept
        {
                return grid_point{left(), top()};
        }

}; // class scrolling_region

namespace platform {
class Widget;
enum class ClipboardType;
}

namespace terminal {

class Terminal {
        friend class vte::platform::Widget;

private:
        class ProcessingContext;

        /* These correspond to the parameters for DECSCUSR (Set cursor style). */
        enum class CursorStyle {
                /* We treat 0 and 1 differently, assuming that the VT510 does so too.
                 *
                 * See, according to the "VT510 Video Terminal Programmer Information",
                 * from vt100.net, paragraph "2.5.7 Cursor Display", there was a menu
                 * item in the "Terminal Set-Up" to set the cursor's style. It looks
                 * like that defaulted to blinking block. So it makes sense for 0 to
                 * mean "set cursor style to default (set by Set-Up)" and 1 to mean
                 * "set cursor style to blinking block", since that default need not be
                 * blinking block. Access to a VT510 is needed to test this theory,
                 * but it seems plausible. And, anyhow, we can even decide we know
                 * better than the VT510 designers!
                 */
                eTERMINAL_DEFAULT = 0,
                eBLINK_BLOCK      = 1,
                eSTEADY_BLOCK     = 2,
                eBLINK_UNDERLINE  = 3,
                eSTEADY_UNDERLINE = 4,
                /* *_IBEAM are xterm extensions */
                eBLINK_IBEAM      = 5,
                eSTEADY_IBEAM     = 6,
        };

        /* The order is important */
        enum class MouseTrackingMode {
	        eNONE,
                eSEND_XY_ON_CLICK,
                eSEND_XY_ON_BUTTON,
                eHILITE_TRACKING,
                eCELL_MOTION_TRACKING,
                eALL_MOTION_TRACKING,
        };

        enum class SelectionType {
               eCHAR,
               eWORD,
               eLINE,
        };

        enum class Alignment : uint8_t {
                START  = 0u,
                CENTRE = 1u,
                END    = 2u,
        };

protected:

        /* NOTE: This needs to be kept in sync with the public VteCursorBlinkMode enum */
        enum class CursorBlinkMode {
                eSYSTEM,
                eON,
                eOFF
        };

        /* NOTE: This needs to be kept in sync with the public VteCursorShape enum */
        enum class CursorShape {
                eBLOCK,
                eIBEAM,
                eUNDERLINE
        };

        /* NOTE: This needs to be kept in sync with the public VteEraseMode enum */
        enum EraseMode {
                eAUTO,
                eASCII_BACKSPACE,
                eASCII_DELETE,
                eDELETE_SEQUENCE,
                eTTY,
        };

        /* NOTE: This needs to be kept in sync with the public VteTextBlinkMode enum */
        enum class TextBlinkMode {
                eNEVER     = 0,
                eFOCUSED   = 1,
                eUNFOCUSED = 2,
                eALWAYS    = 3
        };

public:
        Terminal(vte::platform::Widget* w,
                 VteTerminal *t);
        ~Terminal();

public:
        vte::platform::Widget* m_real_widget{nullptr};
        inline constexpr auto widget() const noexcept { return m_real_widget; }

        VteTerminal *m_terminal{nullptr};
        inline constexpr auto vte_terminal() const noexcept { return m_terminal; }

        GtkWidget *m_widget{nullptr};
        inline constexpr auto gtk_widget() const noexcept { return m_widget; }

        void unset_widget() noexcept;

#if WITH_A11Y && VTE_GTK == 3
        /* Accessible */
        vte::glib::RefPtr<VteTerminalAccessible> m_accessible{};
#endif

        /* Metric and sizing data: dimensions of the window */
        vte::grid::row_t m_row_count{VTE_ROWS};
        vte::grid::column_t m_column_count{VTE_COLUMNS};

        inline constexpr auto row_count() const noexcept -> long { return m_row_count; }
        inline constexpr auto column_count() const noexcept -> long { return m_column_count; }

        vte::terminal::Tabstops m_tabstops{};

        vte::parser::Parser m_parser; /* control sequence state machine */

        vte::terminal::modes::ECMA m_modes_ecma{};
        vte::terminal::modes::Private m_modes_private{};
        bool m_decsace_is_rectangle{false};

	/* PTY handling data. */
        vte::base::RefPtr<vte::base::Pty> m_pty{};
        inline constexpr auto& pty() const noexcept { return m_pty; }

        void unset_pty(bool notify_widget = true);
        bool set_pty(vte::base::Pty* pty);

        guint m_pty_input_source{0};
        guint m_pty_output_source{0};
        bool m_pty_input_active{false};
        pid_t m_pty_pid{-1};           /* pid of child process */
        int m_child_exit_status{-1};   /* pid's exit status, or -1 */
        bool m_eos_pending{false};
        VteReaper *m_reaper;

	/* Queue of chunks of data read from the PTY.
         * Chunks are inserted at the back, and processed from the front.
         */
        std::queue<vte::base::Chunk::unique_type, std::list<vte::base::Chunk::unique_type>> m_incoming_queue;

        vte::base::UTF8Decoder m_utf8_decoder;

        enum class DataSyntax {
                /* The primary data syntax is always one of the following: */
                ECMA48_UTF8,
                #if WITH_ICU
                ECMA48_PCTERM,
                #endif
                /* ECMA48_ECMA35, not supported */

                /* The following can never be primary data syntax: */
        };

        DataSyntax m_primary_data_syntax{DataSyntax::ECMA48_UTF8};
        DataSyntax m_current_data_syntax{DataSyntax::ECMA48_UTF8};

        auto primary_data_syntax() const noexcept { return m_primary_data_syntax; }
        auto current_data_syntax() const noexcept { return m_current_data_syntax; }

        void push_data_syntax(DataSyntax syntax) noexcept
        {
                _vte_debug_print(VTE_DEBUG_IO, "Pushing data syntax %d -> %d\n",
                                 int(m_current_data_syntax), int(syntax));
                m_current_data_syntax = syntax;
        }

        void pop_data_syntax() noexcept
        {
                _vte_debug_print(VTE_DEBUG_IO, "Popping data syntax %d -> %d\n",
                                 int(m_current_data_syntax), int(m_primary_data_syntax));
                m_current_data_syntax = m_primary_data_syntax;
        }

        void reset_data_syntax();

        int m_utf8_ambiguous_width{VTE_DEFAULT_UTF8_AMBIGUOUS_WIDTH};
        gunichar m_last_graphic_character{0}; /* for REP */
        /* Array of dirty rectangles in view coordinates; need to
         * add allocation origin and padding when passing to gtk.
         */
#if VTE_GTK == 3
        GArray *m_update_rects;
#endif
        bool m_invalidated_all{false};       /* pending refresh of entire terminal */
        bool m_is_processing{false};
        // FIXMEchpe should these two be g[s]size ?
        size_t m_input_bytes;
        long m_max_input_bytes{VTE_MAX_INPUT_READ};

	/* Output data queue. */
        VteByteArray *m_outgoing; /* pending input characters */

#if WITH_ICU
        /* Legacy charset support */
        // The main converter for the PTY stream
        std::unique_ptr<vte::base::ICUConverter> m_converter;
        // Extra converter for use in one-off conversion e.g. for
        // DECFRA, instantiated on-demand
        std::unique_ptr<vte::base::ICUDecoder> m_oneoff_decoder;
#endif /* WITH_ICU */

        char const* encoding() const noexcept
        {
                switch (primary_data_syntax()) {
                case DataSyntax::ECMA48_UTF8:   return "UTF-8";
                #if WITH_ICU
                case DataSyntax::ECMA48_PCTERM: return m_converter->charset().c_str();
                #endif
                default: g_assert_not_reached(); return nullptr;
                }
        }

	/* Screen data.  We support the normal screen, and an alternate
	 * screen, which seems to be a DEC-specific feature. */
        VteScreen m_normal_screen;
        VteScreen m_alternate_screen;
        VteScreen *m_screen; /* points to either m_normal_screen or m_alternate_screen */

        VteCell m_defaults;        /* Default characteristics for insertion of new characters:
                                      colors (fore, back, deco) and other attributes (bold, italic,
                                      explicit hyperlink etc.). */
        VteCell m_color_defaults;  /* Default characteristics for erasing characters:
                                      colors (fore, back, deco) but no other attributes,
                                      and the U+0000 character that denotes erased cells. */

        /* charsets in the G0 and G1 slots */
        VteCharacterReplacement m_character_replacements[2] = { VTE_CHARACTER_REPLACEMENT_NONE,
                                                                VTE_CHARACTER_REPLACEMENT_NONE };
        /* pointer to the active one */
        VteCharacterReplacement *m_character_replacement{&m_character_replacements[0]};

        /* Word chars */
        std::vector<char32_t> m_word_char_exceptions;

	/* Selection information. */
        gboolean m_selecting;
        gboolean m_will_select_after_threshold;
        gboolean m_selecting_had_delta;
        bool m_selection_block_mode{false};  // FIXMEegmont move it into a 4th value in SelectionType?
        SelectionType m_selection_type{SelectionType::eCHAR};
        vte::grid::halfcoords m_selection_origin, m_selection_last;  /* BiDi: logical in normal modes, visual in m_selection_block_mode */
        vte::grid::span m_selection_resolved;

	/* Clipboard data information. */
        bool m_selection_owned[2]{false, false};
        bool m_changing_selection{false};
        vte::platform::ClipboardFormat m_selection_format[2];
        GString *m_selection[2];  // FIXMEegmont rename this so that m_selection_resolved can become m_selection?

	/* Miscellaneous options. */
        EraseMode m_backspace_binding{EraseMode::eAUTO};
        EraseMode m_delete_binding{EraseMode::eAUTO};
        bool m_audible_bell{true};
        bool m_allow_bold{true};
        bool m_bold_is_bright{false};
        bool m_rewrap_on_resize{true};
        gboolean m_text_modified_flag;
        gboolean m_text_inserted_flag;
        gboolean m_text_deleted_flag;

	/* Scrolling options. */
        bool m_fallback_scrolling{true};
        bool m_scroll_on_insert{false};
        bool m_scroll_on_output{false};
        bool m_scroll_on_keystroke{true};
        vte::grid::row_t m_scrollback_lines{0};

        inline auto scroll_limit_lower() const noexcept
        {
                return m_screen->row_data->delta();
        }

        inline constexpr auto scroll_limit_upper() const noexcept
        {
                return m_screen->insert_delta + m_row_count;
        }

        inline constexpr auto scroll_position() const noexcept
        {
                return m_screen->scroll_delta;
        }

        /* Restricted scrolling */
        scrolling_region m_scrolling_region;     /* the region we scroll in */
        inline void reset_scrolling_region() { m_scrolling_region.reset_with_size(m_column_count, m_row_count); }

	/* Cursor shape, as set via API */
        CursorShape m_cursor_shape{CursorShape::eBLOCK};
        double m_cursor_aspect_ratio{0.04};

	/* Cursor blinking */
        bool cursor_blink_timer_callback();
        vte::glib::Timer m_cursor_blink_timer{std::bind(&Terminal::cursor_blink_timer_callback,
                                                        this),
                                              "cursor-blink-timer"};
        CursorBlinkMode m_cursor_blink_mode{CursorBlinkMode::eSYSTEM};
        bool m_cursor_blink_state{true};
        bool m_cursor_blinks{false};        /* whether the cursor is actually blinking */
        bool m_cursor_blinks_system{true};  /* gtk-cursor-blink */
        int m_cursor_blink_cycle_ms{1000};  /* gtk-cursor-blink-time / 2 */
        int m_cursor_blink_timeout_ms{500}; /* gtk-cursor-blink-timeout */
        int64_t m_cursor_blink_time_ms;     /* how long the cursor has been blinking yet */
        bool m_has_focus{false};            /* is the widget focused */

        /* Contents blinking */
        bool text_blink_timer_callback();
        vte::glib::Timer m_text_blink_timer{std::bind(&Terminal::text_blink_timer_callback,
                                                      this),
                                            "text-blink-timer"};
        bool m_text_blink_state{false};  /* whether blinking text should be visible at this very moment */
        bool m_text_to_blink{false};     /* drawing signals here if it encounters any cell with blink attribute */
        TextBlinkMode m_text_blink_mode{TextBlinkMode::eALWAYS};
        int m_text_blink_cycle_ms;  /* gtk-cursor-blink-time / 2 */

        /* DECSCUSR cursor style (shape and blinking possibly overridden
         * via escape sequence) */

        CursorStyle m_cursor_style{CursorStyle::eTERMINAL_DEFAULT};

	/* Input device options. */
        bool m_input_enabled{true};
        time_t m_last_keypress_time;

        MouseTrackingMode m_mouse_tracking_mode{MouseTrackingMode::eNONE};
        guint m_mouse_pressed_buttons;      /* bits 0..14 resp. for buttons 1..15 */
        guint m_mouse_handled_buttons;      /* similar bitmap for buttons we handled ourselves */
        /* The last known position the mouse pointer from an event. We don't store
         * this in grid coordinates because we want also to check if they were outside
         * the viewable area, and also want to catch in-cell movements if they make the pointer visible.
         */
        vte::view::coords m_mouse_last_position{-1, -1};
        double m_mouse_smooth_scroll_x_delta{0.0};
        double m_mouse_smooth_scroll_y_delta{0.0};
        bool mouse_autoscroll_timer_callback();
        vte::glib::Timer m_mouse_autoscroll_timer{std::bind(&Terminal::mouse_autoscroll_timer_callback,
                                                            this),
                                                  "mouse-autoscroll-timer"};

	/* State variables for handling match checks. */
        int m_match_regex_next_tag{0};
        auto regex_match_next_tag() noexcept { return m_match_regex_next_tag++; }

        class MatchRegex {
        public:
                MatchRegex() = default;
                MatchRegex(MatchRegex&&) = default;
                MatchRegex& operator= (MatchRegex&&) = default;

                MatchRegex(MatchRegex const&) = delete;
                MatchRegex& operator= (MatchRegex const&) = delete;

                MatchRegex(vte::base::RefPtr<vte::base::Regex>&& regex,
                           uint32_t match_flags,
                           vte::platform::Cursor&& cursor,
                           int tag = -1)
                        : m_regex{std::move(regex)},
                          m_match_flags{match_flags},
                          m_cursor{std::move(cursor)},
                          m_tag{tag}
                {
                }

                auto regex() const noexcept { return m_regex.get(); }
                auto match_flags() const noexcept { return m_match_flags; }
                auto const& cursor() const noexcept { return m_cursor; }
                auto tag() const noexcept { return m_tag; }

                void set_cursor(vte::platform::Cursor&& cursor) { m_cursor = std::move(cursor); }

        private:
                vte::base::RefPtr<vte::base::Regex> m_regex{};
                uint32_t m_match_flags{0};
                vte::platform::Cursor m_cursor{VTE_DEFAULT_CURSOR};
                int m_tag{-1};
        };

        MatchRegex const* m_match_current{nullptr};
        bool regex_match_has_current() const noexcept { return m_match_current != nullptr; }
        auto const* regex_match_current() const noexcept { return m_match_current; }

        std::vector<MatchRegex> m_match_regexes{};

        // m_match_current points into m_match_regex, so every write access to
        // m_match_regex must go through this function that clears m_current_match
        auto& match_regexes_writable() noexcept
        {
                match_hilite_clear();
                return m_match_regexes;
        }

        auto regex_match_get_iter(int tag) noexcept
        {
                return std::find_if(std::begin(m_match_regexes), std::end(m_match_regexes),
                                    [tag](MatchRegex const& rem) { return rem.tag() == tag; });
        }

        MatchRegex* regex_match_get(int tag) noexcept
        {
                auto i = regex_match_get_iter(tag);
                if (i == std::end(m_match_regexes))
                        return nullptr;

                return std::addressof(*i);
        }

        template<class... Args>
        auto& regex_match_add(Args&&... args)
        {
                return match_regexes_writable().emplace_back(std::forward<Args>(args)...);
        }

        GString* m_match_contents;
        VteCharAttrList m_match_attributes;
        char* m_match;
        /* If m_match non-null, then m_match_span contains the region of the match.
         * If m_match is null, and m_match_span is not .empty(), then it contains
         * the minimal region around the last checked coordinates that don't contain
         * a match for any of the dingu regexes.
         */
        vte::grid::span m_match_span;

	/* Search data. */
        vte::base::RefPtr<vte::base::Regex> m_search_regex{};
        uint32_t m_search_regex_match_flags{0};
        gboolean m_search_wrap_around;
        VteCharAttrList m_search_attrs; /* Cache attrs */

	/* Data used when rendering the text which does not require server
	 * resources and which can be kept after unrealizing. */
        vte::Freeable<cairo_font_options_t> m_font_options{};
        vte::Freeable<PangoFontDescription> m_api_font_desc{};
        vte::Freeable<PangoFontDescription> m_unscaled_font_desc{};
        vte::Freeable<PangoFontDescription> m_fontdesc{};
        double m_font_scale{1.};

        auto unscaled_font_description() const noexcept { return m_unscaled_font_desc.get(); }

        /* First, the dimensions of ASCII characters are measured. The result
         * could probably be called char_{width,height} or font_{width,height}
         * but these aren't stored directly here, not to accidentally be confused
         * with m_cell_{width_height}. The values are stored in FontInfo.
         *
         * Then in case of nondefault m_cell_{width,height}_scale an additional
         * m_char_padding is added, resulting in m_cell_{width,height} which are
         * hence potentially larger than the characters. This is to implement
         * line spacing and letter spacing, primarly for accessibility (bug 781479).
         *
         * Char width/height, if really needed, can be computed by subtracting
         * the char padding from the cell dimensions. Char height can also be
         * reconstructed from m_char_{ascent,descent}, one of which is redundant,
         * stored for convenience only.
         */
        long m_char_ascent{0};
        long m_char_descent{0};
        double m_cell_width_scale{1.};
        double m_cell_height_scale{1.};
        GtkBorder m_char_padding{0, 0, 0, 0};
        long m_cell_width{0};
        long m_cell_height{0};
        long m_cell_width_unscaled{0};
        long m_cell_height_unscaled{0};

        /* We allow the cell's text to draw a bit outside the cell at the top
         * and bottom. The following two functions return how much is the
         * maximally allowed overdraw (in px).
         */
        inline constexpr auto cell_overflow_top() const noexcept
        {
                /* Allow overdrawing up into the underline of the cell on top */
                return int(m_cell_height - m_underline_position);
        }

        inline constexpr auto cell_overflow_bottom() const noexcept
        {
                /* Allow overdrawing up into the overline of the cell on bottom */
                return int(m_overline_position + m_overline_thickness);
        }

	/* Data used when rendering */
#if VTE_GTK == 3
        vte::view::DrawingCairo m_draw{};
#elif VTE_GTK == 4
        vte::view::DrawingGsk m_draw{};
#endif
        bool m_clear_background{true};

        VtePaletteColor m_palette[VTE_PALETTE_SIZE];

	/* Mouse cursors. */
        gboolean m_mouse_cursor_over_widget; /* as per enter and leave events */
        gboolean m_mouse_autohide;           /* the API setting */
        gboolean m_mouse_cursor_autohidden;  /* whether the autohiding logic wants to hide it; even if autohiding is disabled via API */

	/* Input method support. */
        bool m_im_preedit_active;
        std::string m_im_preedit;
        vte::Freeable<PangoAttrList> m_im_preedit_attrs{};
        int m_im_preedit_cursor;

        /* Adjustment updates pending. */
        bool m_adjustment_changed_pending;
        bool m_adjustment_value_changed_pending;
        gboolean m_cursor_moved_pending;
        gboolean m_contents_changed_pending;

        std::vector<std::string> m_window_title_stack{};

        enum class PendingChanges {
                TERMPROPS = 1u << 0,

                // deprecated but still emitted for now
                TITLE = 1u << 1,
                CWD   = 1u << 2,
                CWF   = 1u << 3,
        };
        unsigned m_pending_changes{0};

	/* Background */
        double m_background_alpha{1.};

        /* Bell */
        int64_t m_bell_timestamp;
        bool m_bell_pending{false};

	/* Key modifiers. */
        guint m_modifiers;

	/* Font stuff. */
        bool m_has_fonts{false};
        bool m_fontdirty{true};
        long m_line_thickness{VTE_LINE_WIDTH};
        long m_underline_position{0};
        long m_underline_thickness{VTE_LINE_WIDTH};
        long m_double_underline_position{0};
        long m_double_underline_thickness{VTE_LINE_WIDTH};
        long m_strikethrough_position{0};
        long m_strikethrough_thickness{VTE_LINE_WIDTH};
        long m_overline_position{0};
        long m_overline_thickness{VTE_LINE_WIDTH};
        long m_regex_underline_position{0};
        long m_regex_underline_thickness{VTE_LINE_WIDTH};
        double m_undercurl_position{0.};
        double m_undercurl_thickness{VTE_LINE_WIDTH};

        /* Style stuff */
        /* On gtk3, the style border (comprising padding, margins and border)
         * is part of the widget's allocation; on gtk4, it's outside of it.
         */
        GtkBorder m_style_border{
#if VTE_GTK == 3
                1, 1, 1, 1
#elif VTE_GTK == 4
                0, 0, 0, 0
#endif
        };
        /* The total padding. On gtk3, this comprises the style border as above,
         * plus the inner border due to [xy]align and [xy]fill properties; on gtk4,
         * it comprises only the latter.
         */
        GtkBorder m_border{m_style_border};

        /* Hyperlinks */
        bool m_allow_hyperlink{false};
        vte::base::Ring::hyperlink_idx_t m_hyperlink_hover_idx;
        const char *m_hyperlink_hover_uri; /* data is owned by the ring */
        long m_hyperlink_auto_id{0};

        /* Accessibility support */
#if VTE_GTK == 3
        bool m_enable_a11y{true};
#elif VTE_GTK == 4
        bool m_enable_a11y{false};
#endif

        /* RingView and friends */
        vte::base::RingView m_ringview;
        bool m_enable_bidi{true};
        bool m_enable_shaping{true};

        /* FrameClock driven updates */
        gpointer m_scheduler;

        /* BiDi parameters outside of ECMA and DEC private modes */
        guint m_bidi_rtl : 1;

        // Termprops
        std::vector<TermpropValue> m_termprop_values{};
        std::vector<bool> m_termprops_dirty{}; // FIMXE: make this a dynamic_bitset

        auto get_termprop_info(std::string_view const& name) const
        {
                return vte::terminal::get_termprop_info(name);
        }

        auto get_termprop_info(int id) const
        {
                return vte::terminal::get_termprop_info(id);
        }

        auto get_termprop(TermpropInfo const& info) const
        {
                return std::addressof(m_termprop_values.at(info.id()));
        }

        auto get_termprop(TermpropInfo const& info)
        {
                return std::addressof(m_termprop_values.at(info.id()));
        }

        void reset_termprop(TermpropInfo const& info)
        {
                auto const is_valueless = info.type() == vte::terminal::TermpropType::VALUELESS;
                auto value = get_termprop(info);
                if (value &&
                    !std::holds_alternative<std::monostate>(*value)) {
                        *value = {};
                        m_termprops_dirty.at(info.id()) = !is_valueless;
                } else if (is_valueless) {
                        m_termprops_dirty.at(info.id()) = false;
                }
        }

        void reset_termprops()
        {
                for (auto const& info: vte::terminal::s_registered_termprops) {
                        reset_termprop(info);
                }

                m_pending_changes |= vte::to_integral(PendingChanges::TERMPROPS);
        }

        bool m_enable_legacy_osc777{false};

        bool set_enable_legacy_osc777(bool enable) noexcept
        {
                if (enable == m_enable_legacy_osc777)
                        return false;

                m_enable_legacy_osc777 = enable;
                return true;
        }

        constexpr auto enable_legacy_osc777() const noexcept
        {
                return m_enable_legacy_osc777;
        }

public:

        inline VteRowData const* find_row_data(vte::grid::row_t row) const;
        inline VteRowData* find_row_data_writable(vte::grid::row_t row) const;
        inline VteCell const* find_charcell(vte::grid::column_t col,
                                            vte::grid::row_t row) const;
        inline vte::grid::column_t find_start_column(vte::grid::column_t col,
                                                     vte::grid::row_t row) const;
        inline vte::grid::column_t find_end_column(vte::grid::column_t col,
                                                   vte::grid::row_t row) const;

        inline vte::view::coord_t scroll_delta_pixel() const;
        inline vte::grid::row_t pixel_to_row(vte::view::coord_t y) const;
        inline vte::view::coord_t row_to_pixel(vte::grid::row_t row) const;
        inline vte::grid::row_t first_displayed_row() const;
        inline vte::grid::row_t last_displayed_row() const;
        inline bool cursor_is_onscreen() const noexcept;

        VteRowData *ensure_cursor();
        void update_insert_delta();

        // FIXMEchpe replace this with a method on VteRing
        inline VteRowData* ring_insert(vte::grid::row_t position, bool fill) {
                VteRowData *row;
                auto ring = m_screen->row_data;
                bool const not_default_bg = (m_color_defaults.attr.back() != VTE_DEFAULT_BG);

                while G_UNLIKELY (long(ring->next()) < position) {
                        row = ring->append(get_bidi_flags());
                        if (fill && not_default_bg)
                                _vte_row_data_fill (row, &m_color_defaults, m_column_count);
                }
                row = ring->insert(position, get_bidi_flags());
                if (fill && not_default_bg)
                        _vte_row_data_fill (row, &m_color_defaults, m_column_count);
                return row;
        }

        inline VteRowData* ring_append(bool fill) {
                return ring_insert(m_screen->row_data->next(), fill);
        }

        // FIXMEchpe replace this with a method on Ring
        inline void ring_remove(vte::grid::row_t position) {
                m_screen->row_data->remove(position);
        }

        // FIXMEchpe replace this with a method on Ring
        inline VteRowData* insert_rows (guint cnt) {
                VteRowData* row;
                do {
                        row = ring_append(false);
                } while(--cnt);
                return row;
        }

        // Make sure we have enough rows and columns to hold data at the current
        // cursor position.
        inline VteRowData *ensure_row() {
                VteRowData *row;

                // Figure out how many rows we need to add.
                auto const delta = m_screen->cursor.row - long(m_screen->row_data->next()) + 1;
                if G_UNLIKELY (delta > 0) {
                        row = insert_rows(delta);
                        adjust_adjustments();
                } else {
                        // Find the row the cursor is in.
                        row = m_screen->row_data->index_writable(m_screen->cursor.row);
                }
                g_assert(row != NULL);

                return row;
        }


        void set_hard_wrapped(vte::grid::row_t row);
        void set_soft_wrapped(vte::grid::row_t row);

        inline void cleanup_fragments(long start,
                                      long end) {
                ensure_row();
                cleanup_fragments(m_screen->cursor.row, start, end);
        }
        void cleanup_fragments(VteRowData* row,
                               long rownum,
                               long start,
                               long end);
        void cleanup_fragments(long rownum,
                               long start,
                               long end) {
                auto const row = m_screen->row_data->index_writable(rownum);
                if (!row)
                        return;

                cleanup_fragments(row, rownum, start, end);
        }

        void scroll_text_up(scrolling_region const& scrolling_region,
                            vte::grid::row_t amount, bool fill);
        void scroll_text_down(scrolling_region const& scrolling_region,
                              vte::grid::row_t amount, bool fill);
        void scroll_text_left(scrolling_region const& scrolling_region,
                              vte::grid::row_t amount, bool fill);
        void scroll_text_right(scrolling_region const& scrolling_region,
                               vte::grid::row_t amount, bool fill);
        void cursor_down_with_scrolling(bool fill);
        void cursor_up_with_scrolling(bool fill);
        void cursor_right_with_scrolling(bool fill);
        void cursor_left_with_scrolling(bool fill);

        void drop_scrollback();

        void restore_cursor(VteScreen *screen__);
        void save_cursor(VteScreen *screen__);

        /* [[gnu::always_inline]] */ /* C++23 constexpr */ gunichar character_replacement(gunichar c) noexcept;
        int character_width(gunichar c) noexcept;

        void insert_char(gunichar c,
                         bool invalidate_now);
        void insert_single_width_chars(gunichar const *p,
                                       int len);

        void invalidate_row(vte::grid::row_t row);
        void invalidate_rows(vte::grid::row_t row_start,
                             vte::grid::row_t row_end /* inclusive */);
        void invalidate_row_and_context(vte::grid::row_t row);
        void invalidate_rows_and_context(vte::grid::row_t row_start,
                                         vte::grid::row_t row_end /* inclusive */);
        void invalidate(vte::grid::span const& s);
        void invalidate_symmetrical_difference(vte::grid::span const& a, vte::grid::span const& b, bool block);
        void invalidate_match_span();
        void invalidate_all();

        guint8 get_bidi_flags() const noexcept;
        void apply_bidi_attributes(vte::grid::row_t start, guint8 bidi_flags, guint8 bidi_flags_mask);
        void maybe_apply_bidi_attributes(guint8 bidi_flags_mask);

        void reset_update_rects();
        bool invalidate_dirty_rects_and_process_updates();
        void time_process_incoming();
        void process_incoming();
        void process_incoming_utf8(ProcessingContext& context,
                                   vte::base::Chunk& chunk);
        #if WITH_ICU
        void process_incoming_pcterm(ProcessingContext& context,
                                     vte::base::Chunk& chunk);
        #endif
        bool process();
        inline bool is_processing() const { return m_is_processing; };
        void start_processing();

        gssize get_preedit_width(bool left_only);
        gssize get_preedit_length(bool left_only);

        void invalidate_cursor_once(bool periodic = false);
        void check_cursor_blink();
        void add_cursor_timeout();
        void remove_cursor_timeout();
        void update_cursor_blinks();
        CursorBlinkMode decscusr_cursor_blink() const noexcept;
        CursorShape decscusr_cursor_shape() const noexcept;

        /* The allocation of the widget */
        cairo_rectangle_int_t m_allocated_rect;

        constexpr auto const* allocated_rect() const noexcept { return &m_allocated_rect; }

        /* The usable view area. This is the allocation, minus the padding, but
         * including additional right/bottom area if the allocation is not grid aligned.
         */
        vte::view::extents m_view_usable_extents;

        void set_allocated_rect(cairo_rectangle_int_t const& r) { m_allocated_rect = r; update_view_extents(); }
        void update_view_extents() {
                m_view_usable_extents =
                        vte::view::extents(m_allocated_rect.width - m_border.left - m_border.right,
                                           m_allocated_rect.height - m_border.top - m_border.bottom);
        }

        bool widget_realized() const noexcept;
        inline cairo_rectangle_int_t const& get_allocated_rect() const { return m_allocated_rect; }
        inline vte::view::coord_t get_allocated_width() const { return m_allocated_rect.width; }
        inline vte::view::coord_t get_allocated_height() const { return m_allocated_rect.height; }

        vte::view::coords view_coords_from_event(vte::platform::MouseEvent const& event) const;
        vte::grid::coords grid_coords_from_event(vte::platform::MouseEvent const& event) const;

        vte::view::coords view_coords_from_grid_coords(vte::grid::coords const& rowcol) const;
        vte::grid::coords grid_coords_from_view_coords(vte::view::coords const& pos) const;

        vte::grid::halfcoords selection_grid_halfcoords_from_view_coords(vte::view::coords const& pos) const;
        bool view_coords_visible(vte::view::coords const& pos) const;
        bool grid_coords_visible(vte::grid::coords const& rowcol) const;

        inline bool grid_coords_in_scrollback(vte::grid::coords const& rowcol) const { return rowcol.row() < m_screen->insert_delta; }

        vte::grid::row_t confine_grid_row(vte::grid::row_t const& row) const;
        vte::grid::coords confine_grid_coords(vte::grid::coords const& rowcol) const;
        vte::grid::coords confined_grid_coords_from_event(vte::platform::MouseEvent const&) const;
        vte::grid::coords confined_grid_coords_from_view_coords(vte::view::coords const& pos) const;

        void confine_coordinates(long *xp,
                                 long *yp);

        bool set_style_border(GtkBorder const& border) noexcept;
        void set_cursor_aspect(float aspect);

        void widget_copy(vte::platform::ClipboardType selection,
                         vte::platform::ClipboardFormat format);

        void widget_paste(std::string_view const& text);

        std::optional<std::string_view> widget_clipboard_data_get(vte::platform::Clipboard const& clipboard,
                                                                  vte::platform::ClipboardFormat format);
        void widget_clipboard_data_clear(vte::platform::Clipboard const& clipboard);

        void widget_realize();
        void widget_unrealize();
        void widget_unmap();
        void widget_style_updated();
        void widget_focus_in();
        void widget_focus_out();
        bool widget_key_press(vte::platform::KeyEvent const& event);
        bool widget_key_release(vte::platform::KeyEvent const& event);
        bool widget_mouse_motion(vte::platform::MouseEvent const& event);
        bool widget_mouse_press(vte::platform::MouseEvent const& event);
        bool widget_mouse_release(vte::platform::MouseEvent const& event);
        void widget_mouse_enter(vte::platform::MouseEvent const& event);
        void widget_mouse_leave(vte::platform::MouseEvent const& event);
        bool widget_mouse_scroll(vte::platform::ScrollEvent const& event);
#if VTE_GTK == 4
        bool widget_key_modifiers(unsigned modifiers);
#endif /* VTE_GTK == 4 */
#if VTE_GTK == 3
        void widget_draw(cairo_t *cr) noexcept;
#elif VTE_GTK == 4
        void widget_snapshot(GtkSnapshot* snapshot_object) noexcept;
#endif /* VTE_GTK == 3 */
        void widget_measure_width(int *minimum_width,
                                  int *natural_width) noexcept;
        void widget_measure_height(int *minimum_height,
                                   int *natural_height) noexcept;

#if VTE_GTK == 3
        void widget_size_allocate(int x,
                                  int y,
                                  int width,
                                  int height,
                                  int baseline,
                                  Alignment xalign,
                                  Alignment yalign,
                                  bool xfill,
                                  bool yfill) noexcept;
#elif VTE_GTK == 4
        void widget_size_allocate(int width,
                                  int height,
                                  int baseline,
                                  Alignment xalign,
                                  Alignment yalign,
                                  bool xfill,
                                  bool yfill) noexcept;
#endif /* VTE_GTK */

        void set_blink_settings(bool blink,
                                int blink_time_ms,
                                int blink_timeout_ms) noexcept;

        void draw(cairo_region_t const* region) noexcept;
        vte::view::Rectangle cursor_rect();
        void paint_cursor();
        void paint_im_preedit_string();
        void draw_cells(vte::view::DrawingContext::TextRequest* items,
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
                        int row_height);
        void fudge_pango_colors(GSList *attributes,
                                VteCell *cells,
                                gsize n);
        void apply_pango_attr(PangoAttribute *attr,
                              VteCell *cells,
                              gsize n_cells);
        void translate_pango_cells(PangoAttrList *attrs,
                                   VteCell *cells,
                                   gsize n_cells);
        void draw_cells_with_attributes(vte::view::DrawingContext::TextRequest* items,
                                        gssize n,
                                        PangoAttrList *attrs,
                                        bool draw_default_bg,
                                        int column_width,
                                        int height);
        void draw_rows(VteScreen *screen,
                       cairo_region_t const* region,
                       vte::grid::row_t start_row,
                       long row_count,
                       gint start_y,
                       gint column_width,
                       gint row_height);

        void start_autoscroll();
        void stop_autoscroll() noexcept { m_mouse_autoscroll_timer.abort(); }

        void connect_pty_read();
        void disconnect_pty_read();

        void connect_pty_write();
        void disconnect_pty_write();

        void pty_termios_changed();
        void pty_scroll_lock_changed(bool locked);

        void pty_channel_eof();
        bool pty_io_read(int const fd,
                         GIOCondition const condition,
                         int amount = -1);
        bool pty_io_write(int const fd,
                          GIOCondition const condition);

        void send_child(std::string_view const& data);

        void watch_child (pid_t child_pid);
        bool terminate_child () noexcept;
        void child_watch_done(pid_t pid,
                              int status);
        void emit_child_exited();

        void im_commit(std::string_view const& str);
        void im_preedit_set_active(bool active) noexcept;
        void im_preedit_reset() noexcept;
        void im_preedit_changed(std::string_view const& str,
                                int cursorpos,
                                vte::Freeable<PangoAttrList> attrs) noexcept;
        bool im_retrieve_surrounding();
        bool im_delete_surrounding(int offset,
                                   int n_chars);
        void im_reset();
        void im_update_cursor();

        void reset(bool clear_tabstops,
                   bool clear_history,
                   bool from_api = false);
        void reset_decoder();

        void feed(std::string_view const& data,
                  bool start_processsing_ = true);
        void feed_child(char const* data,
                        size_t length) { assert(data); feed_child({data, length}); }
        void feed_child(std::string_view const& str);
        void feed_child_binary(std::string_view const& data);

        bool is_word_char(gunichar c) const;
        bool is_same_class(vte::grid::column_t acol,
                           vte::grid::row_t arow,
                           vte::grid::column_t bcol,
                           vte::grid::row_t brow) const;

        void get_text(vte::grid::row_t start_row,
                      vte::grid::column_t start_col,
                      vte::grid::row_t end_row,
                      vte::grid::column_t end_col,
                      bool block,
                      bool preserve_empty,
                      GString* string,
                      VteCharAttrList* attributes = nullptr);

        void get_text_displayed(GString* string,
                                VteCharAttrList* attributes = nullptr);

        void get_text_displayed_a11y(GString* string,
                                     VteCharAttrList* attributes = nullptr);

        void get_selected_text(GString *string,
                               VteCharAttrList* attributes = nullptr);

        template<unsigned int redbits, unsigned int greenbits, unsigned int bluebits>
        inline void rgb_from_index(guint index,
                                   vte::color::rgb& color) const;
        inline void determine_colors(VteCellAttr const* attr,
                                     bool selected,
                                     bool cursor,
                                     guint *pfore,
                                     guint *pback,
                                     guint *pdeco) const;
        inline void determine_colors(VteCell const* cell,
                                     bool selected,
                                     guint *pfore,
                                     guint *pback,
                                     guint *pdeco) const;
        inline void determine_cursor_colors(VteCell const* cell,
                                            bool selected,
                                            guint *pfore,
                                            guint *pback,
                                            guint *pdeco) const;

        void resolve_normal_colors(VteCell const* cell,
                                   unsigned* pfore,
                                   unsigned* pback,
                                   vte::color::rgb& fg,
                                   vte::color::rgb& bg);

        char *cellattr_to_html(VteCellAttr const* attr,
                               char const* text) const;
        VteCellAttr const* char_to_cell_attr(VteCharAttributes const* attr) const;

        GString* attributes_to_html(GString* text_string,
                                    VteCharAttrList* attrs);

        void start_selection(vte::view::coords const& pos,
                             SelectionType type);
        bool maybe_end_selection();

        void select_all();
        void deselect_all();

        vte::grid::coords resolve_selection_endpoint(vte::grid::halfcoords const& rowcolhalf, bool after) const;
        void resolve_selection();
        void selection_maybe_swap_endpoints(vte::view::coords const& pos);
        void modify_selection(vte::view::coords const& pos);
        bool _cell_is_selected_log(vte::grid::column_t lcol,
                                   vte::grid::row_t) const;
        bool cell_is_selected_vis(vte::grid::column_t vcol,
                                  vte::grid::row_t) const;

        inline bool cell_is_selected_log(vte::grid::column_t lcol,
                                         vte::grid::row_t row) const {
                // Callers need to update the ringview. However, don't assert, just
                // return out-of-view coords. FIXME: may want to throw instead
                if (!m_ringview.is_updated())
                        [[unlikely]] return false;

                // In normal modes, resolve_selection() made sure to generate
                // such boundaries for m_selection_resolved.
                if (!m_selection_block_mode)
                        [[likely]] return m_selection_resolved.contains ({row, lcol});

                return _cell_is_selected_log(lcol, row);
        }

        void ensure_font();
        void update_font();
        void apply_font_metrics(int cell_width_unscaled,
                                int cell_height_unscaled,
                                int cell_width,
                                int cell_height,
                                int char_ascent,
                                int char_descent,
                                GtkBorder char_spacing);

        void refresh_size();
        void screen_set_size(VteScreen *screen_,
                             long old_columns,
                             long old_rows,
                             bool do_rewrap);

        unsigned translate_ctrlkey(vte::platform::KeyEvent const& event) const noexcept;

        void apply_mouse_cursor();
        void set_pointer_autohidden(bool autohidden);

        void beep();

        void set_scroll_value(double value);
        void emit_adjustment_changed();
        void emit_commit(std::string_view const& str);
        void emit_eof();
        void emit_selection_changed();
        void queue_adjustment_changed();
        void queue_adjustment_value_changed(double v);
        void queue_adjustment_value_changed_clamped(double v);
        void adjust_adjustments();
        void adjust_adjustments_full();

        void scroll_lines(long lines);
        void scroll_pages(long pages) { scroll_lines(pages * m_row_count); }
        void scroll_to_top();
        void scroll_to_bottom();
        void scroll_to_previous_prompt();
        void scroll_to_next_prompt();

        void queue_cursor_moved();
        void queue_contents_changed();
        void queue_child_exited();
        void queue_eof();

#if WITH_A11Y && VTE_GTK == 3

        void set_accessible(VteTerminalAccessible* accessible) noexcept
        {
                /* Note: The accessible only keeps a weak ref on the widget,
                 * while GtkWidget holds a ref to the accessible already;
                 * so this does not lead to a ref cycle.
                 */
                m_accessible = vte::glib::make_ref(accessible);
        }

        void emit_text_deleted() noexcept
        {
                if (m_accessible)
                        _vte_terminal_accessible_text_deleted(m_accessible.get());
        }

        void emit_text_inserted()
        {
                if (m_accessible)
                        _vte_terminal_accessible_text_inserted(m_accessible.get());
        }

        void emit_text_modified()

        {
                if (m_accessible)
                        _vte_terminal_accessible_text_modified(m_accessible.get());
        }

        void emit_text_scrolled(long delta)
        {
                if (m_accessible)
                        _vte_terminal_accessible_text_scrolled(m_accessible.get(), delta);
        }

#else

        inline constexpr void emit_text_deleted() const noexcept { }
        inline constexpr void emit_text_inserted() const noexcept { }
        inline constexpr void emit_text_modified() const noexcept { }
        inline constexpr void emit_text_scrolled(long delta) const noexcept { }

#endif /* WITH_A11Y && VTE_GTK == 3*/

        bool m_no_legacy_signals{false};

        void set_no_legacy_signals() noexcept
        {
                m_no_legacy_signals = true;
        }

        void emit_pending_signals();
        void emit_increase_font_size();
        void emit_decrease_font_size();
        void emit_bell();

        bool m_xterm_wm_iconified{false};

        void emit_resize_window(guint columns,
                                guint rows);
        void emit_copy_clipboard();
        void emit_paste_clipboard();
        void emit_hyperlink_hover_uri_changed(const GdkRectangle *bbox);

        void hyperlink_invalidate_and_get_bbox(vte::base::Ring::hyperlink_idx_t idx, GdkRectangle *bbox);
        void hyperlink_hilite_update();

        void match_contents_clear();
        void match_contents_refresh();
        void match_hilite_clear();
        void match_hilite_update();

        bool rowcol_from_event(vte::platform::MouseEvent const& event,
                               long *column,
                               long *row);
#if VTE_GTK == 4
        bool rowcol_at(double x,
                       double y,
                       long* column,
                       long* row);
#endif

        char *hyperlink_check(vte::platform::MouseEvent const& event);
        char *hyperlink_check(vte::grid::column_t column,
                              vte::grid::row_t row);

        bool regex_match_check_extra(vte::platform::MouseEvent const& event,
                                     vte::base::Regex const** regexes,
                                     size_t n_regexes,
                                     uint32_t match_flags,
                                     char** matches);
        bool regex_match_check_extra(vte::grid::column_t column,
                                     vte::grid::row_t row,
                                     vte::base::Regex const** regexes,
                                     size_t n_regexes,
                                     uint32_t match_flags,
                                     char** matches);

        char *regex_match_check(vte::grid::column_t column,
                                vte::grid::row_t row,
                                int *tag);
        char *regex_match_check(vte::platform::MouseEvent const& event,
                                int *tag);

#if VTE_GTK == 4
        char *hyperlink_check_at(double x,
                                 double y);
        bool regex_match_check_extra_at(double x,
                                        double y,
                                        vte::base::Regex const** regexes,
                                        size_t n_regexes,
                                        uint32_t match_flags,
                                        char** matches);
        char *regex_match_check_at(double x,
                                   double y,
                                   int *tag);
#endif /* VTE_GTK == 4 */

        void regex_match_remove(int tag) noexcept;
        void regex_match_remove_all() noexcept;
        void regex_match_set_cursor(int tag,
                                    GdkCursor *gdk_cursor);
        #if VTE_GTK == 3
        void regex_match_set_cursor(int tag,
                                    GdkCursorType cursor_type);
        #endif
        void regex_match_set_cursor(int tag,
                                    char const* cursor_name);
        bool match_rowcol_to_offset(vte::grid::column_t column,
                                    vte::grid::row_t row,
                                    gsize *offset_ptr,
                                    gsize *sattr_ptr,
                                    gsize *eattr_ptr);

        vte::Freeable<pcre2_match_context_8> create_match_context();
        bool match_check_pcre(pcre2_match_data_8 *match_data,
                              pcre2_match_context_8 *match_context,
                              vte::base::Regex const* regex,
                              uint32_t match_flags,
                              gsize sattr,
                              gsize eattr,
                              gsize offset,
                              char **result,
                              gsize *start,
                              gsize *end,
                              gsize *sblank_ptr,
                              gsize *eblank_ptr);
        char *match_check_internal_pcre(vte::grid::column_t column,
                                        vte::grid::row_t row,
                                        MatchRegex const** match,
                                        gsize *start,
                                        gsize *end);

        char *match_check_internal(vte::grid::column_t column,
                                   vte::grid::row_t row,
                                   MatchRegex const** match,
                                   gsize *start,
                                   gsize *end);

        bool feed_mouse_event(vte::grid::coords const& unconfined_rowcol,
                              int button,
                              bool is_drag,
                              bool is_release);
        bool maybe_send_mouse_button(vte::grid::coords const& rowcol,
                                     vte::platform::MouseEvent const& event);
        bool maybe_send_mouse_drag(vte::grid::coords const& rowcol,
                                   vte::platform::MouseEvent const& event);

        void feed_focus_event(bool in);
        void feed_focus_event_initial();
        void maybe_feed_focus_event(bool in);

        bool search_set_regex(vte::base::RefPtr<vte::base::Regex>&& regex,
                              uint32_t flags);
        auto search_regex() const noexcept { return m_search_regex.get(); }

        bool search_rows(pcre2_match_context_8 *match_context,
                         pcre2_match_data_8 *match_data,
                         vte::grid::row_t start_row,
                         vte::grid::row_t end_row,
                         bool backward);
        bool search_rows_iter(pcre2_match_context_8 *match_context,
                              pcre2_match_data_8 *match_data,
                              vte::grid::row_t start_row,
                              vte::grid::row_t end_row,
                              bool backward);
        bool search_find(bool backward);
        bool search_set_wrap_around(bool wrap);

        void set_size(long columns,
                      long rows,
                      bool allocating);

        std::optional<std::vector<char32_t>> process_word_char_exceptions(std::string_view str) const noexcept;

        long get_cell_height() { ensure_font(); return m_cell_height; }
        long get_cell_width()  { ensure_font(); return m_cell_width;  }

        vte::color::rgb const* get_color(int entry) const;
        vte::color::rgb const* get_color(color_palette::ColorPaletteIndex entry) const noexcept;
        auto get_color_opt(color_palette::ColorPaletteIndex entry) const noexcept -> std::optional<vte::color::rgb>;
        void set_color(int entry,
                       color_palette::ColorSource source,
                       vte::color::rgb const& proposed);
        void set_color(color_palette::ColorPaletteIndex entry,
                       color_palette::ColorSource source,
                       vte::color::rgb const& proposed);
        void reset_color(int entry,
                         color_palette::ColorSource source);
        void reset_color(color_palette::ColorPaletteIndex entry,
                         color_palette::ColorSource source);

        bool set_audible_bell(bool setting);
        bool set_text_blink_mode(TextBlinkMode setting);
        auto text_blink_mode() const noexcept { return m_text_blink_mode; }
        bool set_allow_bold(bool setting);
        bool set_allow_hyperlink(bool setting);
        bool set_backspace_binding(EraseMode binding);
        auto backspace_binding() const noexcept { return m_backspace_binding; }
        bool set_background_alpha(double alpha);
        bool set_bold_is_bright(bool setting);
        bool set_cell_height_scale(double scale);
        bool set_cell_width_scale(double scale);
        bool set_cjk_ambiguous_width(int width);
        void set_color_background(vte::color::rgb const &color);
        void set_color_bold(vte::color::rgb const& color);
        void reset_color_bold();
        void set_color_cursor_background(vte::color::rgb const& color);
        void reset_color_cursor_background();
        void set_color_cursor_foreground(vte::color::rgb const& color);
        void reset_color_cursor_foreground();
        void set_color_foreground(vte::color::rgb const& color);
        void set_color_highlight_background(vte::color::rgb const& color);
        void reset_color_highlight_background();
        void set_color_highlight_foreground(vte::color::rgb const& color);
        void reset_color_highlight_foreground();
        void set_colors(vte::color::rgb const *foreground,
                        vte::color::rgb const *background,
                        vte::color::rgb const *palette,
                        gsize palette_size);
        void set_colors_default();
        bool set_cursor_blink_mode(CursorBlinkMode mode);
        auto cursor_blink_mode() const noexcept { return m_cursor_blink_mode; }
        bool set_cursor_shape(CursorShape shape);
        auto cursor_shape() const noexcept { return m_cursor_shape; }
        bool set_cursor_style(CursorStyle style);
        bool set_delete_binding(EraseMode binding);
        auto delete_binding() const noexcept { return m_delete_binding; }
        bool set_enable_a11y(bool setting);
        bool set_enable_bidi(bool setting);
        bool set_enable_shaping(bool setting);
        bool set_encoding(char const* codeset,
                          GError** error);
        bool set_font_desc(vte::Freeable<PangoFontDescription> desc);
        bool update_font_desc();
        bool set_font_options(vte::Freeable<cairo_font_options_t> font_options);
        cairo_font_options_t const* get_font_options() const noexcept { return m_font_options.get(); }
        bool set_font_scale(double scale);
        bool set_input_enabled(bool enabled);
        bool set_mouse_autohide(bool autohide);
        bool set_rewrap_on_resize(bool rewrap);
        bool set_scrollback_lines(long lines);
        bool set_fallback_scrolling(bool set);
        auto fallback_scrolling() const noexcept { return m_fallback_scrolling; }
        bool set_scroll_on_insert(bool scroll);
        bool set_scroll_on_keystroke(bool scroll);
        bool set_scroll_on_output(bool scroll);
        bool set_images_enabled(bool enabled);
        bool set_word_char_exceptions(std::optional<std::string_view> stropt);
        void set_clear_background(bool setting);

        bool write_contents_sync (GOutputStream *stream,
                                  VteWriteFlags flags,
                                  GCancellable *cancellable,
                                  GError **error);

        inline void maybe_retreat_cursor();
        inline void home_cursor();
        inline void clear_screen();
        inline void clear_current_line();
        inline void clear_above_current();
        inline void scroll_text(vte::grid::row_t scroll_amount);
        inline void switch_screen(VteScreen *new_screen);
        inline void switch_normal_screen();
        inline void switch_alternate_screen();
        inline void save_cursor();
        inline void restore_cursor();

        inline void set_mode_ecma(vte::parser::Sequence const& seq,
                                  bool set) noexcept;
        inline void set_mode_private(vte::parser::Sequence const& seq,
                                     bool set) noexcept;
        inline void set_mode_private(int mode,
                                     bool set) noexcept;
        inline void save_mode_private(vte::parser::Sequence const& seq,
                                      bool save) noexcept;
        void update_mouse_protocol() noexcept;

        inline void set_character_replacements(unsigned slot,
                                               VteCharacterReplacement replacement);
        inline void set_character_replacement(unsigned slot);
        inline void clear_to_bol();
        inline void clear_below_current();
        inline void clear_to_eol();
        inline void set_cursor_column(vte::grid::column_t col);
        inline void set_cursor_column1(vte::grid::column_t col); /* 1-based */
        /* Return the xterm-like cursor column, 0-based, decremented by 1 if about to wrap.
         * See maybe_retreat_cursor() for further details. */
        inline int get_xterm_cursor_column() const noexcept {
                if (m_screen->cursor.col >= m_column_count) [[unlikely]] {
                        return m_column_count - 1;
                } else if (m_screen->cursor.col == m_scrolling_region.right() + 1 &&
                           m_screen->cursor_advanced_by_graphic_character) [[unlikely]] {
                        return m_screen->cursor.col - 1;
                } else {
                        return m_screen->cursor.col;
                }
        }
        inline void set_cursor_row(vte::grid::row_t row /* relative to scrolling region */);
        inline void set_cursor_row1(vte::grid::row_t row /* relative to scrolling region */); /* 1-based */
        inline int get_xterm_cursor_row() const noexcept { return m_screen->cursor.row - m_screen->insert_delta; }
        inline void set_cursor_coords(vte::grid::row_t row /* relative to scrolling region */,
                                      vte::grid::column_t column);
        inline void set_cursor_coords1(vte::grid::row_t row /* relative to scrolling region */,
                                       vte::grid::column_t column); /* 1-based */
        inline void erase_characters(long count,
                                     bool use_basic = false);

        inline void move_cursor_up(vte::grid::row_t rows);
        inline void move_cursor_down(vte::grid::row_t rows);
        inline void move_cursor_backward(vte::grid::column_t columns);
        inline void move_cursor_forward(vte::grid::column_t columns);
        inline void move_cursor_tab_backward(int count = 1);
        inline void move_cursor_tab_forward(int count = 1);

        inline void carriage_return();
        inline void line_feed();

        inline void erase_in_display(vte::parser::Sequence const& seq);
        inline void erase_in_line(vte::parser::Sequence const& seq);

        unsigned int checksum_area(grid_rect rect);

        void select_text(vte::grid::column_t start_col,
                         vte::grid::row_t start_row,
                         vte::grid::column_t end_col,
                         vte::grid::row_t end_row);
        void select_empty(vte::grid::column_t col,
                          vte::grid::row_t row);

        void send(vte::parser::u8SequenceBuilder const& builder,
                  bool c1 = true,
                  vte::parser::u8SequenceBuilder::Introducer introducer = vte::parser::u8SequenceBuilder::Introducer::DEFAULT,
                  vte::parser::u8SequenceBuilder::ST st = vte::parser::u8SequenceBuilder::ST::DEFAULT) noexcept;
        void send(vte::parser::Sequence const& seq,
                  vte::parser::u8SequenceBuilder const& builder) noexcept;
        void send(unsigned int type,
                  std::initializer_list<int> params) noexcept;
        void reply(vte::parser::Sequence const& seq,
                   unsigned int type,
                   std::initializer_list<int> params) noexcept;
        void reply(vte::parser::Sequence const& seq,
                   unsigned int type,
                   std::initializer_list<int> params,
                   vte::parser::ReplyBuilder const& builder) noexcept;
        #if 0
        void reply(vte::parser::Sequence const& seq,
                   unsigned int type,
                   std::initializer_list<int> params,
                   std::string const& str) noexcept;
        #endif
        void reply(vte::parser::Sequence const& seq,
                   unsigned int type,
                   std::initializer_list<int> params,
                   char const* format,
                   ...) noexcept G_GNUC_PRINTF(5, 6);

        /* OSC handler helpers */
        void set_color_index(vte::parser::Sequence const& seq,
                             vte::parser::StringTokeniser::const_iterator& token,
                             vte::parser::StringTokeniser::const_iterator const& endtoken,
                             std::optional<int> number,
                             osc_colors::OSCColorIndex index,
                             int osc) noexcept;
        auto resolve_reported_color(osc_colors::OSCColorIndex index) const noexcept -> std::optional<vte::color::rgb>;
        void parse_termprop(vte::parser::Sequence const& seq,
                            std::string_view const& str,
                            bool& set,
                            bool& query) noexcept;
        #if VTE_DEBUG
        void reply_termprop_query(vte::parser::Sequence const& seq,
                                  vte::terminal::TermpropInfo const* info);
        #endif

        /* OSC handlers */
        void set_color(vte::parser::Sequence const& seq,
                       vte::parser::StringTokeniser::const_iterator& token,
                       vte::parser::StringTokeniser::const_iterator const& endtoken,
                       osc_colors::OSCValuedColorSequenceKind osc_kind,
                       int osc) noexcept;
        void set_special_color(vte::parser::Sequence const& seq,
                               vte::parser::StringTokeniser::const_iterator& token,
                               vte::parser::StringTokeniser::const_iterator const& endtoken,
                               color_palette::ColorPaletteIndex index,
                               int osc) noexcept;
        void reset_color(vte::parser::Sequence const& seq,
                         vte::parser::StringTokeniser::const_iterator& token,
                         vte::parser::StringTokeniser::const_iterator const& endtoken,
                         osc_colors::OSCValuedColorSequenceKind osc_kind) noexcept;
        void set_termprop_uri(vte::parser::Sequence const& seq,
                              vte::parser::StringTokeniser::const_iterator& token,
                              vte::parser::StringTokeniser::const_iterator const& endtoken,
                              int termprop_id,
                              PendingChanges legacy_pending_change) noexcept;
        void set_current_hyperlink(vte::parser::Sequence const& seq,
                                   vte::parser::StringTokeniser::const_iterator& token,
                                   vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept;
        void set_current_shell_integration_mode(vte::parser::Sequence const& seq,
                                                vte::parser::StringTokeniser::const_iterator& token,
                                                vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept;
        void vte_termprop(vte::parser::Sequence const& seq,
                          vte::parser::StringTokeniser::const_iterator& token,
                          vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept;

        void urxvt_extension(vte::parser::Sequence const& seq,
                             vte::parser::StringTokeniser::const_iterator& token,
                             vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept;

        // helpers

        grid_rect collect_rect(vte::parser::Sequence const&,
                               unsigned&) noexcept;

        void copy_rect(grid_rect srect,
                       grid_point dest) noexcept;

        void fill_rect(grid_rect rect,
                       char32_t c,
                       VteCellAttr attr) noexcept;

        template<class P>
        void rewrite_rect(grid_rect rect,
                          bool as_rectangle,
                          bool only_attrs,
                          P&& pen) noexcept;

        // ringview
        void ringview_update();

        /* Sequence handlers */
        // Note: inlining the handlers seems to worsen the performance, so we don't do that
#define _VTE_CMD_HANDLER(cmd) \
	/* inline */ void cmd (vte::parser::Sequence const& seq);
#define _VTE_CMD_HANDLER_NOP(cmd) \
	/* inline */ void cmd (vte::parser::Sequence const& seq);
#define _VTE_CMD_HANDLER_R(cmd) \
	/* inline */ bool cmd (vte::parser::Sequence const& seq);
#include "parser-cmd-handlers.hh"
#undef _VTE_CMD_HANDLER
#undef _VTE_CMD_HANDLER_NOP
#undef _VTE_CMD_HANDLER_R
};

} // namespace terminal
} // namespace vte

extern GTimer *process_timer;

vte::terminal::Terminal* _vte_terminal_get_impl(VteTerminal *terminal);

static inline bool
_vte_double_equal(double a,
                  double b)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        return a == b;
#pragma GCC diagnostic pop
}

#define VTE_TEST_FLAG_DECRQCRA (G_GUINT64_CONSTANT(1) << 0)
#define VTE_TEST_FLAG_TERMPROP (G_GUINT64_CONSTANT(1) << 1)

extern uint64_t g_test_flags;

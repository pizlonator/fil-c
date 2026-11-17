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

#pragma once

#include <glib.h>

#include "ring.hh"
#include "ringview.hh"
#include "vterowdata.hh"
#include "vtetypes.hh"
#include "vteunistr.h"

#if WITH_FRIBIDI
#include <fribidi.h>
#include "bidiarrays.hh"

// FIXME: make gdkarrayimpl a template!

#define GDK_ARRAY_NAME vte_bidi_char_types
#define GDK_ARRAY_TYPE_NAME VteBidiCharTypes
#define GDK_ARRAY_ELEMENT_TYPE FriBidiCharType
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

#define GDK_ARRAY_NAME vte_bidi_bracket_types
#define GDK_ARRAY_TYPE_NAME VteBidiBracketTypes
#define GDK_ARRAY_ELEMENT_TYPE FriBidiBracketType
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

#define GDK_ARRAY_NAME vte_bidi_joining_types
#define GDK_ARRAY_TYPE_NAME VteBidiJoiningTypes
#define GDK_ARRAY_ELEMENT_TYPE FriBidiJoiningType
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

#define GDK_ARRAY_NAME vte_bidi_levels
#define GDK_ARRAY_TYPE_NAME VteBidiLevels
#define GDK_ARRAY_ELEMENT_TYPE FriBidiLevel
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

#endif // WITH_FRIBIDI

namespace vte {

namespace base {

class RingView;

/* BidiRow contains the BiDi transformation of a single row. */
class BidiRow {
        friend class BidiRunner;

public:
        BidiRow() { }
        ~BidiRow();

        // prevent accidents
        BidiRow(BidiRow& o) = delete;
        BidiRow(BidiRow const& o) = delete;
        BidiRow(BidiRow&& o) = delete;
        BidiRow& operator= (BidiRow& o) = delete;
        BidiRow& operator= (BidiRow const& o) = delete;
        BidiRow& operator= (BidiRow&& o) = delete;

        // Converts from logical to visual column. Offscreen columns are mirrored
        // for RTL lines, e.g. (assuming 80 columns) -1 <=> 80, -2 <=> 81 etc.
        inline vte::grid::column_t log2vis(vte::grid::column_t col) const {
                if (col >= 0 && col < m_width) {
                        return m_log2vis[col];
                } else {
                        return m_base_rtl ? m_width - 1 - col : col;
                }
        }

        // Converts from visual to logical column. Offscreen columns are mirrored
        // for RTL lines, e.g. (assuming 80 columns) -1 <=> 80, -2 <=> 81 etc.
        inline vte::grid::column_t vis2log(vte::grid::column_t col) const {
                if (col >= 0 && col < m_width) {
                        return m_vis2log[col];
                } else {
                        return m_base_rtl ? m_width - 1 - col : col;
                }
        }

        bool log_is_rtl(vte::grid::column_t col) const;
        bool vis_is_rtl(vte::grid::column_t col) const;
        vteunistr vis_get_shaped_char(vte::grid::column_t col, vteunistr s) const;

        /* Whether the line's base direction is RTL. */
        inline constexpr bool base_is_rtl() const noexcept { return m_base_rtl; }

        /* Whether the implicit paragraph contains a foreign directionality character.
         * This is used in the cursor, showing the character's directionality. */
        inline constexpr bool has_foreign() const noexcept { return m_has_foreign; }

private:
        void set_width(vte::grid::column_t width);

        /* The value of m_width == 0 is a valid representation of the trivial LTR mapping. */
        uint16_t m_width{0};
        uint16_t m_width_alloc{0};

        /* These will be initialized / allocated on demand, when some shuffling or shaping is needed. */
        uint16_t *m_log2vis{nullptr};
        uint16_t *m_vis2log{nullptr};
        uint8_t  *m_vis_rtl{nullptr};  /* FIXME use a bitset */
        gunichar *m_vis_shaped_base_char{nullptr};  /* without combining accents */

        bool m_base_rtl{false};
        bool m_has_foreign{false};
};


/* BidiRunner is not a "real" class, rather the collection of methods that run the BiDi algorithm. */
class BidiRunner {
public:
        BidiRunner(RingView *ringview) : m_ringview{ringview} {
#if WITH_FRIBIDI
                vte_bidi_chars_init(&m_fribidi_chars_array);
                vte_bidi_indexes_init(&m_fribidi_map_array);
                vte_bidi_indexes_init(&m_fribidi_to_term_array);
#endif
        }
        ~BidiRunner() {
#if WITH_FRIBIDI
                vte_bidi_chars_clear(&m_fribidi_chars_array);
                vte_bidi_indexes_clear(&m_fribidi_map_array);
                vte_bidi_indexes_clear(&m_fribidi_to_term_array);
#endif
        }

        // prevent accidents
        BidiRunner(BidiRunner& o) = delete;
        BidiRunner(BidiRunner const& o) = delete;
        BidiRunner(BidiRunner&& o) = delete;
        BidiRunner& operator= (BidiRunner& o) = delete;
        BidiRunner& operator= (BidiRunner const& o) = delete;
        BidiRunner& operator= (BidiRunner&& o) = delete;

        void paragraph(vte::grid::row_t start, vte::grid::row_t end,
                       bool do_bidi, bool do_shaping);

private:
        RingView *m_ringview;

        void explicit_line(vte::grid::row_t row, bool rtl, bool do_shaping);
        void explicit_paragraph(vte::grid::row_t start, vte::grid::row_t end, bool rtl, bool do_shaping);

#if WITH_FRIBIDI

        class Workspace {
        private:
                std::size_t m_capacity{0};

                VteBidiCharTypes m_char_types_array;
                VteBidiBracketTypes m_bracket_types_array;
                VteBidiJoiningTypes m_joining_types_array;
                VteBidiLevels m_levels_array;

        public:

                Workspace()
                {
                        vte_bidi_char_types_init(&m_char_types_array);
                        vte_bidi_bracket_types_init(&m_bracket_types_array);
                        vte_bidi_joining_types_init(&m_joining_types_array);
                        vte_bidi_levels_init(&m_levels_array);
                }

                ~Workspace()
                {
                        vte_bidi_char_types_clear(&m_char_types_array);
                        vte_bidi_bracket_types_clear(&m_bracket_types_array);
                        vte_bidi_joining_types_clear(&m_joining_types_array);
                        vte_bidi_levels_clear(&m_levels_array);
                }

                Workspace(Workspace const&) = delete;
                Workspace(Workspace&&) = delete;

                Workspace& operator=(Workspace const&) = delete;
                Workspace& operator=(Workspace&&) = delete;

                inline auto char_types_data() const noexcept { return vte_bidi_char_types_get_data(&m_char_types_array); }
                inline auto bracket_types_data() const noexcept { return vte_bidi_bracket_types_get_data(&m_bracket_types_array); }
                inline auto joining_types_data() const noexcept { return vte_bidi_joining_types_get_data(&m_joining_types_array); }
                inline auto levels_data() const noexcept { return vte_bidi_levels_get_data(&m_levels_array); }

                void reserve(std::size_t capacity)
                {
                        if (capacity <= m_capacity)
                                return;

                        vte_bidi_char_types_reserve(&m_char_types_array, capacity);
                        vte_bidi_bracket_types_reserve(&m_bracket_types_array, capacity);
                        vte_bidi_joining_types_reserve(&m_joining_types_array, capacity);
                        vte_bidi_levels_reserve(&m_levels_array, capacity);

                        m_capacity = capacity;
                }

                void set_size(std::size_t size)
                {
                        vte_bidi_char_types_set_size(&m_char_types_array, size);
                        vte_bidi_bracket_types_set_size(&m_bracket_types_array, size);
                        vte_bidi_joining_types_set_size(&m_joining_types_array, size);
                        vte_bidi_levels_set_size(&m_levels_array, size);
                }

        }; // class Workspace

        VteBidiChars m_fribidi_chars_array;
        VteBidiIndexes m_fribidi_map_array;
        VteBidiIndexes m_fribidi_to_term_array;

        void explicit_line_shape(vte::grid::row_t row);

        bool implicit_paragraph(vte::grid::row_t start, vte::grid::row_t end, bool do_shaping);
#endif
};

}; /* namespace base */

}; /* namespace vte */


gboolean vte_bidi_get_mirror_char (vteunistr unistr, gboolean mirror_box_drawing, vteunistr *unistr_mirrored);

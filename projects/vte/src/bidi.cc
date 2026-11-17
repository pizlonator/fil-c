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

/*
 * A BidiRow object stores the BiDi mapping between logical and visual positions
 * for one visual line of text. (Characters are always shuffled within a line,
 * never across lines.)
 *
 * It also stores additional per-character properties: the character's direction
 * (needed for mirroring and mouse selecting) and Arabic shaping (as currently
 * done using presentation form characters, although HarfBuzz would probably be
 * a better approach).
 *
 * There are per-line properties as well, which are actually per-paragraph
 * properties stored for each line: the overall potentially autodetected
 * direction (needed for keyboard arrow swapping), and whether the paragraph
 * contains any foreign direction character (used for the cursor shape).
 *
 * Note that the trivial LTR mapping, with no RTL or shaped characters at all,
 * might be denoted by setting the BidiRow's width to 0.
 *
 * BidiRunner is a collection of methods that run the BiDi algorithm on one
 * paragraph of RingView, and stores the result in BidiRow objects.
 *
 * BiDi is implemented according to Terminal-wg/bidi v0.2:
 * https://terminal-wg.pages.freedesktop.org/bidi/
 */

#include "config.h"

#include "bidi.hh"
#include "debug.h"
#include "vtedefines.hh"
#include "vteinternal.hh"

#if WITH_FRIBIDI
static_assert (sizeof (FriBidiChar) == sizeof (gunichar), "Unexpected FriBidiChar size");
static_assert (sizeof (FriBidiStrIndex) == sizeof (int), "Unexpected FriBidiStrIndex size");
#endif

/* Don't do Arabic ligatures as per bug 142. */
#define VTE_ARABIC_SHAPING_FLAGS (FRIBIDI_FLAGS_ARABIC & ~FRIBIDI_FLAG_SHAPE_ARAB_LIGA)

using namespace vte::base;

BidiRow::~BidiRow()
{
        g_free (m_log2vis);
        g_free (m_vis2log);
        g_free (m_vis_rtl);
        g_free (m_vis_shaped_base_char);
}

void
BidiRow::set_width(vte::grid::column_t width)
{
        vte_assert_cmpint(width, >=, 0);
        if (G_UNLIKELY (width > G_MAXUSHORT)) {
                width = G_MAXUSHORT;
        }

        if (G_UNLIKELY (width > m_width_alloc)) {
                uint32_t alloc = m_width_alloc;  /* use a wider data type to avoid overflow */
                if (alloc == 0) {
                        alloc = MAX(width, 80);
                }
                while (width > alloc) {
                        /* Don't realloc too aggressively. */
                        alloc = alloc * 5 / 4;
                }
                if (alloc > G_MAXUSHORT) {
                        alloc = G_MAXUSHORT;
                }
                m_width_alloc = alloc;

                m_log2vis = (uint16_t *) g_realloc (m_log2vis, sizeof (uint16_t) * m_width_alloc);
                m_vis2log = (uint16_t *) g_realloc (m_vis2log, sizeof (uint16_t) * m_width_alloc);
                m_vis_rtl = (uint8_t *)  g_realloc (m_vis_rtl, sizeof (uint8_t)  * m_width_alloc);
                m_vis_shaped_base_char = (gunichar *) g_realloc (m_vis_shaped_base_char, sizeof (gunichar) * m_width_alloc);
        }

        m_width = width;
}

/* Whether the cell at the given visual position has RTL directionality.
 * For offscreen columns the line's base direction is returned. */
bool
BidiRow::vis_is_rtl(vte::grid::column_t col) const
{
        if (col >= 0 && col < m_width) {
                return m_vis_rtl[col];
        } else {
                return m_base_rtl;
        }
}

/* Whether the cell at the given logical position has RTL directionality.
 * For offscreen columns the line's base direction is returned. */
bool
BidiRow::log_is_rtl(vte::grid::column_t col) const
{
        if (col >= 0 && col < m_width) {
                col = m_log2vis[col];
                return m_vis_rtl[col];
        } else {
                return m_base_rtl;
        }
}

/* Get the shaped character (including combining accents, i.e. vteunistr) for the
 * given visual position.
 *
 * The unshaped character (including combining accents, i.e. vteunistr) needs to be
 * passed to this method.
 *
 * m_vis_shaped_base_char stores the shaped base character without combining accents.
 * Apply the combining accents here. There's no design rationale behind this, it's
 * just much simpler to do it here than during the BiDi algorithm.
 *
 * In some cases a fully LTR line is denoted by m_width being 0. In other cases a
 * character that didn't need shaping is stored as the value 0. In order to provide a
 * consistent and straightforward behavior (where the caller doesn't need to special
 * case the return value of 0) we need to ask for the unshaped character anyway.
 *
 * FIXMEegmont This should have a wrapper method in RingView. That could always return
 * the actual (potentially shaped) character without asking for the unshaped one.
 */
vteunistr
BidiRow::vis_get_shaped_char(vte::grid::column_t col, vteunistr s) const
{
        vte_assert_cmpint (col, >=, 0);

        if (col >= m_width || m_vis_shaped_base_char[col] == 0)
                return s;

        return _vte_unistr_replace_base(s, m_vis_shaped_base_char[col]);
}


#if WITH_FRIBIDI
static inline bool
is_arabic(gunichar c)
{
        return FRIBIDI_IS_ARABIC (fribidi_get_bidi_type (c));
}

/* Perform Arabic shaping on an explicit line (which could be explicit LTR or explicit RTL),
 * using presentation form characters.
 *
 * Don't do shaping across lines. (I'm unsure about this design decision.
 * Shaping across soft linebreaks would require an even much more complex code.)
 *
 * The FriBiDi API doesn't have a method for shaping a visual string, so we need to extract
 * Arabic words ourselves, by walking in the visual order from right to left. It's painful.
 *
 * This whole shaping business with presentation form characters should be replaced by HarfBuzz.
 */
void
BidiRunner::explicit_line_shape(vte::grid::row_t row)
{
        const VteRowData *row_data = m_ringview->get_row(row);

        BidiRow *bidirow = m_ringview->get_bidirow_writable(row);

        auto width = m_ringview->get_width();

        FriBidiParType pbase_dir = FRIBIDI_PAR_RTL;
        FriBidiLevel level;
        FriBidiChar *fribidi_chars;

        int count;

        const VteCell *cell;
        gunichar c;
        gunichar base;
        int i, j;  /* visual columns */

        VteBidiChars *fribidi_chars_array = &m_fribidi_chars_array;
        vte_bidi_chars_set_size(fribidi_chars_array, 0);
        vte_bidi_chars_reserve(fribidi_chars_array, width);

        static thread_local auto workspace = Workspace{};

        /* Walk in visual order from right to left. */
        i = width - 1;
        while (i >= 0) {
                cell = _vte_row_data_get(row_data, bidirow->vis2log(i));
                c = cell ? cell->c : 0;
                base = _vte_unistr_get_base(c);
                if (!is_arabic(base)) {
                        i--;
                        continue;
                }

                /* Found an Arabic character. Keep walking to the left, extracting the word. */
                vte_bidi_chars_set_size(fribidi_chars_array, 0);
                j = i;
                do {
                        G_GNUC_UNUSED auto prev_len = vte_bidi_chars_get_size (fribidi_chars_array);
                        _vte_unistr_append_to_gunichars (cell->c, fribidi_chars_array);
                        vte_assert_cmpint (vte_bidi_chars_get_size (fribidi_chars_array), >, prev_len);

                        j--;
                        if (j >= 0) {
                                cell = _vte_row_data_get(row_data, bidirow->vis2log(j));
                                c = cell ? cell->c : 0;
                                base = _vte_unistr_get_base(c);
                        } else {
                                /* Pretend that visual column -1 contains a stop char. */
                                base = 0;
                        }
                } while (is_arabic(base));

                /* Extracted the Arabic run. Do the BiDi. */

                /* Convenience stuff, we no longer need the auto-growing wrapper. */
                count = vte_bidi_chars_get_size (fribidi_chars_array);
                fribidi_chars = vte_bidi_chars_get_data (fribidi_chars_array);

                workspace.reserve(count);

                /* Run the BiDi algorithm on the paragraph to get the embedding levels. */
                auto fribidi_chartypes = workspace.char_types_data();
                auto fribidi_brackettypes = workspace.bracket_types_data();
                auto fribidi_joiningtypes = workspace.joining_types_data();
                auto fribidi_levels = workspace.levels_data();

                fribidi_get_bidi_types (fribidi_chars, count, fribidi_chartypes);
                fribidi_get_bracket_types (fribidi_chars, count, fribidi_chartypes, fribidi_brackettypes);
                fribidi_get_joining_types (fribidi_chars, count, fribidi_joiningtypes);
                level = fribidi_get_par_embedding_levels_ex (fribidi_chartypes, fribidi_brackettypes, count, &pbase_dir, fribidi_levels) - 1;
                if (level == (FriBidiLevel)(-1)) {
                        /* Error. Skip shaping this word. */
                        i = j - 1;
                        continue;
                }

                /* Shaping. */
                fribidi_join_arabic (fribidi_chartypes, count, fribidi_levels, fribidi_joiningtypes);
                fribidi_shape_arabic (VTE_ARABIC_SHAPING_FLAGS, fribidi_levels, count, fribidi_joiningtypes, fribidi_chars);

                /* If we have the shortcut notation for the trivial LTR mapping, we need to
                 * expand it to the nontrivial notation, in order to store the shaped character. */
                if (bidirow->m_width == 0) {
                        bidirow->set_width(width);
                        for (int k = 0; k < width; k++) {
                                bidirow->m_log2vis[k] = bidirow->m_vis2log[k] = k;
                                bidirow->m_vis_rtl[k] = false;
                                bidirow->m_vis_shaped_base_char[k] = 0;
                        }
                }

                /* Walk through the Arabic word again. */
                j = i;
                while (count > 0) {
                        vte_assert_cmpint (j, >=, 0);
                        cell = _vte_row_data_get(row_data, bidirow->vis2log(j));
                        c = cell->c;
                        base = _vte_unistr_get_base(c);
                        if (*fribidi_chars != base) {
                                /* Shaping changed the codepoint, store it. */
                                bidirow->m_vis_shaped_base_char[j] = *fribidi_chars;
                        }
                        int len = _vte_unistr_strlen(c);
                        fribidi_chars += len;
                        count -= len;
                        j--;
                }

                /* Ready to look for the next word. Skip the stop char which isn't Arabic. */
                i = j - 1;
        }

        vte_bidi_chars_set_size (fribidi_chars_array, 0);
}
#endif /* WITH_FRIBIDI */

/* Set up the mapping according to explicit mode for a given line.
 *
 * If @do_shaping then perform Arabic shaping on the visual string, independently
 * from the paragraph direction (the @rtl parameter). This is done using
 * presentation form characters, until we have something better (e.g. HarfBuzz)
 * in place.
 */
void
BidiRunner::explicit_line(vte::grid::row_t row, bool rtl, bool do_shaping)
{
        int i;

        BidiRow *bidirow = m_ringview->get_bidirow_writable(row);
        if (G_UNLIKELY (bidirow == nullptr))
                return;
        bidirow->m_base_rtl = rtl;
        bidirow->m_has_foreign = false;

        auto width = m_ringview->get_width();

        if (G_LIKELY (!rtl)) {
                /* Shortcut notation: a width of 0 means the trivial LTR mapping. */
                bidirow->set_width(0);
        } else {
                /* Set up the explicit RTL mapping. */
                bidirow->set_width(width);
                for (i = 0; i < width; i++) {
                        bidirow->m_log2vis[i] = bidirow->m_vis2log[i] = width - 1 - i;
                        bidirow->m_vis_rtl[i] = true;
                        bidirow->m_vis_shaped_base_char[i] = 0;
                }
        }

#if WITH_FRIBIDI
        if (do_shaping)
                explicit_line_shape(row);
#endif
}

/* Figure out the mapping for the paragraph between the given rows. */
void
BidiRunner::paragraph(vte::grid::row_t start, vte::grid::row_t end,
                      bool do_bidi, bool do_shaping)
{
        const VteRowData *row_data = m_ringview->get_row(start);

        if (G_UNLIKELY (m_ringview->get_width() > G_MAXUSHORT)) {
                /* log2vis and vis2log mappings have 2 bytes per cell.
                 * Don't do BiDi for extremely wide terminals. */
                explicit_paragraph(start, end, false, false);
                return;
        }

        if (!do_bidi) {
                explicit_paragraph(start, end, false, do_shaping);
                return;
        }

#if WITH_FRIBIDI
        /* Have a consistent limit on the number of rows in a paragraph
         * that can get implicit BiDi treatment, which is independent from
         * the current scroll position. */
        if ((row_data->attr.bidi_flags & VTE_BIDI_FLAG_IMPLICIT) &&
            end - start <= VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX) {
                if (implicit_paragraph(start, end, do_shaping))
                        return;
        }
#endif

        explicit_paragraph(start, end, row_data->attr.bidi_flags & VTE_BIDI_FLAG_RTL, do_shaping);
}

/* Set up the mapping according to explicit mode, for all the lines
 * of a paragraph between the given lines. */
void
BidiRunner::explicit_paragraph(vte::grid::row_t start, vte::grid::row_t end,
                               bool rtl, bool do_shaping)
{
        for (; start < end; start++) {
                explicit_line(start, rtl, do_shaping);
        }
}

#if WITH_FRIBIDI
/* Figure out the mapping for the implicit paragraph between the given rows.
 * Returns success. */
bool
BidiRunner::implicit_paragraph(vte::grid::row_t start, vte::grid::row_t end, bool do_shaping)
{
        const VteCell *cell;
        const VteRowData *row_data;
        bool rtl;
        bool autodir;
        bool has_foreign;
        vte::grid::row_t row;
        FriBidiParType pbase_dir;
        FriBidiLevel level;
        FriBidiChar *fribidi_chars;
        FriBidiCharType *fribidi_chartypes;
        FriBidiBracketType *fribidi_brackettypes;
        FriBidiJoiningType *fribidi_joiningtypes;
        FriBidiLevel *fribidi_levels;
        FriBidiStrIndex *fribidi_map;
        FriBidiStrIndex *fribidi_to_term;
        BidiRow *bidirow;
        VteBidiChars *fribidi_chars_array;
        VteBidiIndexes *fribidi_map_array;
        VteBidiIndexes *fribidi_to_term_array;

        auto width = m_ringview->get_width();

        row_data = m_ringview->get_row(start);
        rtl = row_data->attr.bidi_flags & VTE_BIDI_FLAG_RTL;
        autodir = row_data->attr.bidi_flags & VTE_BIDI_FLAG_AUTO;

        int lines[VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX + 1];  /* offsets to the beginning of lines */
        lines[0] = 0;
        int line = 0;   /* line number within the paragraph */
        int count;      /* total character count */
        int tl, tv;     /* terminal logical and visual */
        int fl, fv;     /* fribidi logical and visual */
        unsigned int col;

        fribidi_chars_array = &m_fribidi_chars_array;
        vte_bidi_chars_set_size (fribidi_chars_array, 0);
        vte_bidi_chars_reserve (fribidi_chars_array, (end - start) * width);

        fribidi_map_array = &m_fribidi_map_array;
        vte_bidi_indexes_set_size (fribidi_map_array, 0);
        vte_bidi_indexes_reserve (fribidi_map_array, (end - start) * width);

        fribidi_to_term_array = &m_fribidi_to_term_array;
        vte_bidi_indexes_set_size (fribidi_to_term_array, 0);
        vte_bidi_indexes_reserve (fribidi_to_term_array, (end - start) * width);

        /* Extract the paragraph's contents, omitting unused and fragment cells. */

        /* Example of what is going on, showing the most important steps:
         *
         * Let's take the string produced by this command:
         *   echo -e "\u0041\u05e9\u05b8\u05c1\u05dc\u05d5\u05b9\u05dd\u0031\u0032\uff1c\u05d0"
         *
         * This string consists of:
         * - English letter A
         * - Hebrew word Shalom:
         *     - Letter Shin: ש
         *         - Combining accent Qamats
         *         - Combining accent Shin Dot
         *     - Letter Lamed: ל
         *     - Letter Vav: ו
         *         - Combining accent Holam
         *     - Letter Final Mem: ם
         * - Digits One and Two
         * - Full-width less-than sign U+ff1c: ＜
         * - Hebrew letter Alef: א
         *
         * Features of this example:
         * - Overall LTR direction for convenience (set up by the leading English letter)
         * - Combining accents within RTL
         * - Double width character with RTL resolved direction
         * - A mapping that is not its own inverse (due to the digits being LTR inside RTL inside LTR),
         *   to help catch if we'd look up something in the wrong direction
         *
         * Not demonstrated in this example:
         * - Wrapping a paragraph to lines
         * - Spacing marks
         *
         * Pre-BiDi (logical) order, using approximating glyphs ("Shalom" is "w7io", Alef is "x"):
         *   Aw7io12<x
         *
         * Post-BiDi (visual) order, using approximating glyphs ("Shalom" is "oi7w", note the mirrored less-than):
         *   Ax>12oi7w
         *
         * Terminal's logical cells:
         *                 [0]       [1]       [2]      [3]     [4]   [5]   [6]    [7]      [8]         [9]
         *     row_data:    A   Shin+qam+dot   Lam    Vav+hol   Mem   One   Two   Less   Less (cont)   Alef
         *
         * Extracted to pass to FriBidi (combining accents get -1, double wides' continuation cells are skipped):
         *                        [0]    [1]   [2]   [3]   [4]   [5]   [6]   [7]   [8]   [9]   [10]   [11]
         *     fribidi_chars:      A    Shin   qam   dot   Lam   Vav   hol   Mem   One   Two   Less   Alef
         *     fribidi_map:        0      1    -1    -1     4     5    -1     7     8     9     10     11
         *     fribidi_to_term:    0      1    -1    -1     2     3    -1     4     5     6      7      9
         *
         * Embedding levels and other properties (shaping etc.) are looked up:
         *                        [0]    [1]   [2]   [3]   [4]   [5]   [6]   [7]   [8]   [9]   [10]   [11]
         *     fribidi_levels:     0      1     1     1     1     1     1     1     2     2      1      1
         *
         * The steps above were per-paragraph. The steps below are per-line.
         *
         * After fribidi_reorder_line (only this array gets shuffled):
         *                        [0]    [1]   [2]   [3]   [4]   [5]   [6]   [7]   [8]   [9]   [10]   [11]
         *     fribidi_map:        0     11    10     8     9     7     5    -1     4     1     -1     -1
         *
         * To get the visual order: walk in the new fribidi_map, and for each real entry look up the
         * logical terminal column using fribidi_to_term:
         * - map[0] is 0, to_term[0] is 0, hence visual column 0 belongs to logical column 0 (A)
         * - map[1] is 11, to_term[11] is 9, hence visual column 1 belongs to logical column 9 (Alef)
         * - map[2] is 10, to_term[10] is 7, row_data[7] is the "<" sign
         *     - this is a double wide character, we need to map the next two visual cells to two logical cells
         *     - due to levels[10] being odd, this character has a resolved RTL direction
         *     - thus we map in reverse order: visual 2 <=> logical 8, visual 3 <=> logical 7
         *     - the glyph is also mirrorable, it'll be displayed accordingly
         * - [3] -> 8 -> 5, so visual 4 <=> logical 5 (One)
         * - [4] -> 9 -> 6, so visual 5 <=> logical 6 (Two)
         * - [5] -> 7 -> 4, so visual 6 <=> logical 4 (Mem, the last, leftmost letter of Shalom)
         * - [6] -> 5 -> 3, so visual 7 <=> logical 3 (Vav+hol)
         * - [7] -> -1, skipped
         * - [8] -> 4 -> 2, so visual 8 <=> logical 2 (Lam)
         * - [9] -> 1 -> 1, so visual 9 <=> logical 1 (Shin+qam+dot, the first, rightmost letter of Shalom)
         * - [10] -> -1, skipped
         * - [11] -> -1, skipped
         *
         * Silly FriBidi API almost allows us to skip one level of indirection, by placing the to_term values
         * in the map to be shuffled. However, we can't get the embedding levels then.
         * TODO: File an issue for a better API.
         */
        for (row = start; row < end; row++) {
                row_data = m_ringview->get_row(row);

                for (tl = 0; tl < row_data->len; tl++) {
                        auto prev_len = vte_bidi_chars_get_size(fribidi_chars_array);
                        FriBidiStrIndex val;

                        cell = _vte_row_data_get (row_data, tl);
                        if (cell->attr.fragment())
                                continue;

                        /* Extract the base character and combining accents.
                         * Convert mid-line erased cells to spaces.
                         * Note: see the static assert at the top of this file. */
                        _vte_unistr_append_to_gunichars (cell->c ? cell->c : ' ', fribidi_chars_array);
                        /* Make sure at least one character was produced. */
                        vte_assert_cmpint (vte_bidi_chars_get_size(fribidi_chars_array), >, prev_len);

                        /* Track the base character, assign to it its current index in fribidi_chars.
                         * Don't track combining accents, assign -1's to them. */
                        val = prev_len;
                        vte_bidi_indexes_append (fribidi_map_array, &val);
                        val = tl;
                        vte_bidi_indexes_append (fribidi_to_term_array, &val);
                        prev_len++;
                        val = -1;
                        while (prev_len++ < vte_bidi_chars_get_size(fribidi_chars_array)) {
                                vte_bidi_indexes_append (fribidi_map_array, &val);
                                vte_bidi_indexes_append (fribidi_to_term_array, &val);
                        }
                }

                lines[++line] = vte_bidi_chars_get_size(fribidi_chars_array);
        }

        /* Convenience stuff, we no longer need the auto-growing wrapper. */
        count = vte_bidi_chars_get_size(fribidi_chars_array);
        fribidi_chars = vte_bidi_chars_get_data(fribidi_chars_array);
        fribidi_map = vte_bidi_indexes_get_data(fribidi_map_array);
        fribidi_to_term = vte_bidi_indexes_get_data(fribidi_to_term_array);

        /* Run the BiDi algorithm on the paragraph to get the embedding levels. */
        static thread_local auto workspace = Workspace{};
        workspace.reserve(count);

        fribidi_chartypes = workspace.char_types_data();
        fribidi_brackettypes = workspace.bracket_types_data();
        fribidi_joiningtypes = workspace.joining_types_data();
        fribidi_levels = workspace.levels_data();

        pbase_dir = autodir ? (rtl ? FRIBIDI_PAR_WRTL : FRIBIDI_PAR_WLTR)
                            : (rtl ? FRIBIDI_PAR_RTL  : FRIBIDI_PAR_LTR );

        fribidi_get_bidi_types (fribidi_chars, count, fribidi_chartypes);
        fribidi_get_bracket_types (fribidi_chars, count, fribidi_chartypes, fribidi_brackettypes);
        fribidi_get_joining_types (fribidi_chars, count, fribidi_joiningtypes);
        level = fribidi_get_par_embedding_levels_ex (fribidi_chartypes, fribidi_brackettypes, count, &pbase_dir, fribidi_levels) - 1;

        if (level == (FriBidiLevel)(-1)) {
                /* error */
                vte_bidi_chars_set_size (fribidi_chars_array, 0);
                vte_bidi_indexes_set_size (fribidi_map_array, 0);
                vte_bidi_indexes_set_size (fribidi_to_term_array, 0);
                return false;
        }

        if (do_shaping) {
                /* Arabic shaping (on the entire paragraph in a single run). */
                fribidi_join_arabic (fribidi_chartypes, count, fribidi_levels, fribidi_joiningtypes);
                fribidi_shape_arabic (VTE_ARABIC_SHAPING_FLAGS, fribidi_levels, count, fribidi_joiningtypes, fribidi_chars);
        }

        /* For convenience, from now on this variable contains the resolved (i.e. possibly autodetected) value. */
        vte_assert_cmpint (pbase_dir, !=, FRIBIDI_PAR_ON);
        rtl = (pbase_dir == FRIBIDI_PAR_RTL || pbase_dir == FRIBIDI_PAR_WRTL);

        if (!rtl && level == 0) {
                /* Fast and memory saving shortcut for LTR-only paragraphs. */
                vte_bidi_chars_set_size (fribidi_chars_array, 0);
                vte_bidi_indexes_set_size (fribidi_map_array, 0);
                vte_bidi_indexes_set_size (fribidi_to_term_array, 0);
                explicit_paragraph (start, end, false, false);
                return true;
        }

        /* Check if the paragraph has a foreign directionality character. In fact, also catch
         * and treat it so if the paragraph has a mixture of multiple embedding levels, even if all
         * of them has the same parity (direction). */
        if (!rtl) {
                /* LTR. We already bailed out above if level == 0, so there must be a character
                 * with a higher embedding level. */
                has_foreign = true;
        } else {
                /* RTL. Check if any character has a level other than 1. Check the paragraph's
                 * maximum level as a shortcut, but note that in case of an empty paragraph
                 * its value is 0 rather than 1. */
                if (level <= 1) {
                        has_foreign = false;
                        for (int i = 0; i < count; i++) {
                                if (fribidi_levels[i] != 1) {
                                        has_foreign = true;
                                        break;
                                }
                        }
                } else {
                        has_foreign = true;
                }
        }

        /* Reshuffle line by line. */
        for (row = start, line = 0; row < end; row++, line++) {
                bidirow = m_ringview->get_bidirow_writable(row);
                if (bidirow == nullptr)
                        continue;

                bidirow->m_base_rtl = rtl;
                bidirow->m_has_foreign = has_foreign;
                bidirow->set_width(width);

                row_data = m_ringview->get_row(row);

                level = fribidi_reorder_line (FRIBIDI_FLAGS_DEFAULT,
                                              fribidi_chartypes,
                                              lines[line + 1] - lines[line],
                                              lines[line],
                                              pbase_dir,
                                              fribidi_levels,
                                              NULL,
                                              fribidi_map) - 1;

                if (level == (FriBidiLevel)(-1)) {
                        /* error, what should we do? */
                        explicit_line (row, rtl, true);
                        bidirow->m_has_foreign = has_foreign;
                        continue;
                }

                if (!rtl && level == 0) {
                        /* Fast shortcut for LTR-only lines. */
                        explicit_line (row, false, false);
                        bidirow->m_has_foreign = has_foreign;
                        continue;
                }

                /* Copy to our realm. Proceed in visual order.*/
                tv = 0;
                if (rtl) {
                        /* Unused cells on the left for RTL paragraphs */
                        int unused = width - row_data->len;
                        for (; tv < unused; tv++) {
                                bidirow->m_vis2log[tv] = width - 1 - tv;
                                bidirow->m_vis_rtl[tv] = true;
                                bidirow->m_vis_shaped_base_char[tv] = 0;
                        }
                }
                for (fv = lines[line]; fv < lines[line + 1]; fv++) {
                        /* Inflate fribidi's result by inserting fragments. */
                        fl = fribidi_map[fv];
                        if (fl == -1)
                                continue;
                        tl = fribidi_to_term[fl];
                        cell = _vte_row_data_get (row_data, tl);
                        g_assert (!cell->attr.fragment());
                        g_assert (cell->attr.columns() > 0);
                        if (FRIBIDI_LEVEL_IS_RTL(fribidi_levels[fl])) {
                                /* RTL character directionality. Map fragments in reverse order. */
                                for (col = 0; col < cell->attr.columns(); col++) {
                                        bidirow->m_vis2log[tv + col] = tl + cell->attr.columns() - 1 - col;
                                        bidirow->m_vis_rtl[tv + col] = true;
                                        bidirow->m_vis_shaped_base_char[tv + col] = fribidi_chars[fl];
                                }
                                tv += cell->attr.columns();
                                tl += cell->attr.columns();
                        } else {
                                /* LTR character directionality. */
                                for (col = 0; col < cell->attr.columns(); col++) {
                                        bidirow->m_vis2log[tv] = tl;
                                        bidirow->m_vis_rtl[tv] = false;
                                        bidirow->m_vis_shaped_base_char[tv] = fribidi_chars[fl];
                                        tv++;
                                        tl++;
                                }
                        }
                }
                if (!rtl) {
                        /* Unused cells on the right for LTR paragraphs */
                        vte_assert_cmpint (tv, ==, row_data->len);
                        for (; tv < width; tv++) {
                                bidirow->m_vis2log[tv] = tv;
                                bidirow->m_vis_rtl[tv] = false;
                                bidirow->m_vis_shaped_base_char[tv] = 0;
                        }
                }
                vte_assert_cmpint (tv, ==, width);

                /* From vis2log create the log2vis mapping too.
                 * In debug mode assert that we have a bijective mapping. */
                if (_vte_debug_on (VTE_DEBUG_BIDI)) {
                        for (tl = 0; tl < width; tl++) {
                                bidirow->m_log2vis[tl] = -1;
                        }
                }

                for (tv = 0; tv < width; tv++) {
                        bidirow->m_log2vis[bidirow->m_vis2log[tv]] = tv;
                }

                if (_vte_debug_on (VTE_DEBUG_BIDI)) {
                        for (tl = 0; tl < width; tl++) {
                                vte_assert_cmpint (bidirow->m_log2vis[tl], !=, -1);
                        }
                }
        }

        vte_bidi_chars_set_size (fribidi_chars_array, 0);
        vte_bidi_indexes_set_size (fribidi_map_array, 0);
        vte_bidi_indexes_set_size (fribidi_to_term_array, 0);
        return true;
}
#endif /* WITH_FRIBIDI */


/* Find the mirrored counterpart of a codepoint, just like
 * fribidi_get_mirror_char() or g_unichar_get_mirror_char() does.
 * Two additions:
 * - works with vteunistr, that is, preserves combining accents;
 * - optionally mirrors box drawing characters.
 */
gboolean
vte_bidi_get_mirror_char (vteunistr unistr, gboolean mirror_box_drawing, vteunistr *out)
{
        static const unsigned char mirrored_2500[0x80] = {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13,
                0x0c, 0x0d, 0x0e, 0x0f, 0x18, 0x19, 0x1a, 0x1b, 0x14, 0x15, 0x16, 0x17, 0x24, 0x25, 0x26, 0x27,
                0x28, 0x29, 0x2a, 0x2b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x2c, 0x2e, 0x2d, 0x2f,
                0x30, 0x32, 0x31, 0x33, 0x34, 0x36, 0x35, 0x37, 0x38, 0x3a, 0x39, 0x3b, 0x3c, 0x3e, 0x3d, 0x3f,
                0x40, 0x41, 0x42, 0x44, 0x43, 0x46, 0x45, 0x47, 0x48, 0x4a, 0x49, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
                0x50, 0x51, 0x55, 0x56, 0x57, 0x52, 0x53, 0x54, 0x5b, 0x5c, 0x5d, 0x58, 0x59, 0x5a, 0x61, 0x62,
                0x63, 0x5e, 0x5f, 0x60, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6e, 0x6d, 0x70,
                0x6f, 0x72, 0x71, 0x73, 0x76, 0x75, 0x74, 0x77, 0x7a, 0x79, 0x78, 0x7b, 0x7e, 0x7d, 0x7c, 0x7f };

        gunichar base_ch = _vte_unistr_get_base (unistr);
        gunichar base_ch_mirrored = base_ch;

        if (G_UNLIKELY (base_ch >= 0x2500 && base_ch < 0x2580)) {
                if (G_UNLIKELY (mirror_box_drawing))
                        base_ch_mirrored = 0x2500 + mirrored_2500[base_ch - 0x2500];
        } else {
#if WITH_FRIBIDI
                /* Prefer the FriBidi variant as that's more likely to be in sync with the rest of our BiDi stuff. */
                fribidi_get_mirror_char (base_ch, &base_ch_mirrored);
#else
                /* Fall back to glib, so that we still get mirrored characters in explicit RTL mode without BiDi support. */
                g_unichar_get_mirror_char (base_ch, &base_ch_mirrored);
#endif
        }

        vteunistr unistr_mirrored = _vte_unistr_replace_base (unistr, base_ch_mirrored);

        if (out)
                *out = unistr_mirrored;
        return unistr_mirrored == unistr;
}


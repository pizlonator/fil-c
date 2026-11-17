/*
 * Copyright © 2024 Christian Hergert
 * Copyright © 2002,2003 Red Hat, Inc.
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

#include "vteinternal.hh"
#include "vteaccess-gtk4.h"

#define GDK_ARRAY_NAME char_positions
#define GDK_ARRAY_TYPE_NAME CharPositions
#define GDK_ARRAY_ELEMENT_TYPE int
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 8
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

typedef struct _VteAccessibleTextContents
{
        /* A GdkArrayImpl of attributes per byte */
        VteCharAttrList attrs;

        /* The byte position within the UTF-8 string where each visible
         * character starts.
         */
        CharPositions characters;

        /* The character position within the UTF-8 string where each line
         * break occurs. To get byte offset, use @characters.
         */
        CharPositions linebreaks;

        /* The UTF-8 string encoded as bytes so that we may reference it
         * using GBytes for "substrings". However @string does include a
         * trailing NUL byte in it's size so that the a11y infrastructure
         * may elide some string copies.
         */
        GBytes *string;

        /* Number of bytes in @string excluding trailing NUL. */
        gsize n_bytes;

        /* Number of unicode characters in @string. */
        gsize n_chars;

        /* The character position (not bytes) of the caret in @string */
        gsize caret;

        /* Cached column/row of the caret updated each time we are notified
         * of the caret having moved. We cache this so that we can elide
         * extraneous notifications after snapshoting. We will update the
         * carent position synchronously when notified so @caret may always
         * be relied upon as correct.
         */
        long cached_caret_column;
        long cached_caret_row;
} VteAccessibleTextContents;

typedef struct _VteAccessibleText
{
        VteTerminal               *terminal;
        VteAccessibleTextContents  contents[2];
        guint                      contents_flip : 1;
} VteAccessibleText;

static inline gboolean
_pango_color_equal(const PangoColor *a,
                   const PangoColor *b)
{
        return a->red   == b->red &&
               a->green == b->green &&
               a->blue  == b->blue;
}

static void
vte_accessible_text_contents_init (VteAccessibleTextContents *contents)
{
        vte_char_attr_list_init (&contents->attrs);
        char_positions_init (&contents->characters);
        char_positions_init (&contents->linebreaks);
        contents->string = nullptr;
        contents->n_bytes = 0;
        contents->n_chars = 0;
        contents->caret = 0;
        contents->cached_caret_row = 0;
        contents->cached_caret_column = 0;
}

static void
vte_accessible_text_contents_clear (VteAccessibleTextContents *contents)
{
        vte_char_attr_list_clear (&contents->attrs);
        char_positions_clear (&contents->characters);
        char_positions_clear (&contents->linebreaks);
        g_clear_pointer (&contents->string, g_bytes_unref);
        contents->n_bytes = 0;
        contents->n_chars = 0;
        contents->caret = 0;
        contents->cached_caret_row = 0;
        contents->cached_caret_column = 0;
}

static void
vte_accessible_text_contents_reset (VteAccessibleTextContents *contents)
{
        vte_char_attr_list_set_size (&contents->attrs, 0);
        char_positions_set_size (&contents->characters, 0);
        char_positions_set_size (&contents->linebreaks, 0);
        g_clear_pointer (&contents->string, g_bytes_unref);
        contents->n_bytes = 0;
        contents->n_chars = 0;
        contents->caret = 0;
        contents->cached_caret_row = 0;
        contents->cached_caret_column = 0;
}

static const char *
vte_accessible_text_contents_get_string (VteAccessibleTextContents *contents,
                                         gsize                     *len)
{
        const char *ret;

        if (contents->string == nullptr || g_bytes_get_size (contents->string) == 0) {
                *len = 0;
                return "";
        }

        ret = (const char *)g_bytes_get_data (contents->string, len);

        if (*len > 0) {
                (*len)--;
        }

        return ret;
}

#if GTK_CHECK_VERSION(4, 15, 1)

static void
vte_accessible_text_contents_xy_from_offset (VteAccessibleTextContents *contents,
                                             int offset,
                                             int *x,
                                             int *y)
{
        int cur_offset = 0;
        int cur_x = -1;
        int cur_y = -1;
        int i;

        for (i = 0; i < int(char_positions_get_size (&contents->linebreaks)); i++) {
                int linebreak = *char_positions_index (&contents->linebreaks, i);

                if (offset < linebreak) {
                        cur_x = offset - cur_offset;
                        cur_y = i - 1;
                        break;
                }  else {
                        cur_offset = linebreak;
                }
        }

        if (i == int(char_positions_get_size (&contents->linebreaks))) {
                if (offset <= int(char_positions_get_size (&contents->characters))) {
                        cur_x = offset - cur_offset;
                        cur_y = i - 1;
                }
        }

        *x = cur_x;
        *y = cur_y;
}

#endif // gtk 4.16

static int
vte_accessible_text_contents_offset_from_xy (VteAccessibleTextContents *contents,
                                             int x,
                                             int y)
{
        int offset;
        int linebreak;
        int next_linebreak;

        if (y >= int(char_positions_get_size (&contents->linebreaks))) {
                y = int(char_positions_get_size (&contents->linebreaks)) - 1;
                if (y < 0) {
                        return 0;
                }
        }

        linebreak = *char_positions_index (&contents->linebreaks, y);
        if (y + 1 == int(char_positions_get_size (&contents->linebreaks))) {
                next_linebreak = int(char_positions_get_size (&contents->characters));
        } else {
                next_linebreak = *char_positions_index (&contents->linebreaks, y + 1);
        }

        offset = linebreak + x;
        if (offset >= next_linebreak) {
                offset = next_linebreak - 1;
        }

        return offset;
}

static gunichar
vte_accessible_text_contents_get_char_at (VteAccessibleTextContents *contents,
                                          guint                      offset)
{
        const char *str;

        if (contents->string == nullptr)
                return 0;

        if (offset >= contents->n_chars)
                return 0;

        g_assert (offset < char_positions_get_size (&contents->characters));

        str = (const char *)g_bytes_get_data (contents->string, nullptr);
        str += *char_positions_index (&contents->characters, offset);

        return g_utf8_get_char (str);

}

static GBytes *
_g_string_free_to_bytes_with_nul (GString *str)
{
        /* g_string_free_to_bytes() will have a trailing-NUL but not include it
         * in the size of the GBytes. We want the size included in our GBytes
         * so that GtkAccessibleText may avoid some copies.
         */
        gsize len = str->len + 1;
        return g_bytes_new_take (g_string_free (str, FALSE), len);
}

static inline gsize
vte_accessible_text_contents_find_caret (VteAccessibleTextContents *contents,
                                         long                       ccol,
                                         long                       crow)
{
        g_assert (contents != nullptr);

        /* Get the offsets to the beginnings of each line. */
        gsize caret = 0;
        for (gsize i = 0; i < char_positions_get_size (&contents->characters); i++) {
                /* Get the attributes for the current cell. */
                int offset = *char_positions_index (&contents->characters, i);
                const struct _VteCharAttributes *attrs = vte_char_attr_list_get (&contents->attrs, offset);

                /* If this cell is "before" the cursor, move the caret to be "here". */
                if ((attrs->row < crow) ||
                    ((attrs->row == crow) && (attrs->column < ccol))) {
                        caret = i + 1;
                }
        }

        return caret;
}

static void
vte_accessible_text_contents_snapshot (VteAccessibleTextContents *contents,
                                       VteTerminal               *terminal)
{
        auto impl = _vte_terminal_get_impl (terminal);
        GString *gstr = g_string_new (nullptr);

        try {
                impl->get_text_displayed_a11y (gstr, &contents->attrs);
        } catch (...) {
                g_string_truncate (gstr, 0);
        }

        if (vte_char_attr_list_get_size (&contents->attrs) >= G_MAXINT) {
                g_string_truncate (gstr, 0);
                return;
        }

        /* Get the offsets to the beginnings of each character. */
        int i = 0;
        const char *next = gstr->str;
        int n_attrs = int(vte_char_attr_list_get_size (&contents->attrs));
        while (i < n_attrs) {
                char_positions_append (&contents->characters, &i);
                next = g_utf8_next_char (next);
                if (next != nullptr) {
                        i = next - gstr->str;
                        continue;
                }
                break;
        }

        /* Find offsets for the beginning of lines. */
        gsize n_chars = char_positions_get_size (&contents->characters);
        int row;
        for (i = 0, row = 0; i < int(n_chars); i++) {
                /* Get the attributes for the current cell. */
                int offset = *char_positions_index (&contents->characters, i);
                const struct _VteCharAttributes *attrs = vte_char_attr_list_get (&contents->attrs, offset);

                /* If this character is on a row different from the row
                 * the character we looked at previously was on, then
                 * it's a new line and we need to keep track of where
                 * it is. */
                if ((i == 0) || (attrs->row != row)) {
                        _vte_debug_print (VTE_DEBUG_ALLY,
                                          "Row %d/%ld begins at %d.\n",
                                          int(char_positions_get_size (&contents->linebreaks)),
                                          attrs->row, i);
                        char_positions_append (&contents->linebreaks, &i);
                }

                row = attrs->row;
        }

        /* Add the final line break. */
        char_positions_append (&contents->linebreaks, &i);

        /* Update the caret position. */
        long ccol, crow;
        vte_terminal_get_cursor_position (terminal, &ccol, &crow);
        _vte_debug_print (VTE_DEBUG_ALLY, "Cursor at (%ld, " "%ld).\n", ccol, crow);
        gsize caret = vte_accessible_text_contents_find_caret (contents, ccol, crow);

        contents->n_bytes = gstr->len;
        contents->n_chars = n_chars;
        contents->string = _g_string_free_to_bytes_with_nul (gstr);
        contents->caret = caret;
        contents->cached_caret_column = ccol;
        contents->cached_caret_row = crow;

        _vte_debug_print (VTE_DEBUG_ALLY,
                          "Refreshed accessibility snapshot, "
                          "%ld cells, %ld characters.\n",
                          long(vte_char_attr_list_get_size(&contents->attrs)),
                          long(char_positions_get_size (&contents->characters)));
}

static GBytes *
vte_accessible_text_contents_slice (VteAccessibleTextContents *contents,
                                    guint                      start,
                                    guint                      end)
{
        static const char empty[] = {0};
        guint start_offset;
        guint end_offset;

        g_assert (contents != nullptr);

        if (contents->string == nullptr)
                return g_bytes_new_static (empty, sizeof empty);

        if (start > contents->n_chars)
                start = contents->n_chars;

        if (end > contents->n_chars)
                end = contents->n_chars;

        if (end < start)
                std::swap (end, start);

        g_assert (start <= char_positions_get_size (&contents->characters));
        g_assert (end <= char_positions_get_size (&contents->characters));

        if (start == char_positions_get_size (&contents->characters))
                start_offset = g_bytes_get_size (contents->string);
        else
                start_offset = *char_positions_index (&contents->characters, start);

        if (end == char_positions_get_size (&contents->characters))
                end_offset = g_bytes_get_size (contents->string);
        else
                end_offset = *char_positions_index (&contents->characters, end);

        g_assert (start_offset <= end_offset);

        if (start_offset == end_offset)
                return g_bytes_new_static (empty, sizeof empty);

        return g_bytes_new_from_bytes (contents->string, start_offset, end_offset - start_offset);
}

static void
vte_accessible_text_free (VteAccessibleText *state)
{
        vte_accessible_text_contents_clear (&state->contents[0]);
        vte_accessible_text_contents_clear (&state->contents[1]);
        state->terminal = nullptr;
        g_free (state);
}

static VteAccessibleText *
vte_accessible_text_get (VteTerminal *terminal)
{
        return (VteAccessibleText *)g_object_get_data (G_OBJECT (terminal), "VTE_ACCESSIBLE_TEXT");
}

static GBytes *
vte_accessible_text_get_contents (GtkAccessibleText *accessible,
                                  guint              start,
                                  guint              end)
{
        VteTerminal *terminal = VTE_TERMINAL (accessible);
        VteAccessibleText *state = vte_accessible_text_get (terminal);
        VteAccessibleTextContents *contents = nullptr;

        g_assert (VTE_IS_TERMINAL (terminal));
        g_assert (state != nullptr);
        g_assert (state->terminal == terminal);

        contents = &state->contents[state->contents_flip];

        return vte_accessible_text_contents_slice (contents, start, end);
}

static GBytes *
vte_accessible_text_get_contents_at (GtkAccessibleText            *accessible,
                                     guint                         offset,
                                     GtkAccessibleTextGranularity  granularity,
                                     guint                        *start,
                                     guint                        *end)
{
        VteTerminal *terminal = VTE_TERMINAL (accessible);
        VteAccessibleText *state = vte_accessible_text_get (terminal);
        VteAccessibleTextContents *contents;

        g_assert (VTE_IS_TERMINAL (terminal));
        g_assert (state != nullptr);
        g_assert (state->terminal == terminal);

        auto impl = _vte_terminal_get_impl (terminal);

        contents = &state->contents[state->contents_flip];

        if (contents->string == nullptr) {
                return nullptr;
        }

        if (offset > contents->n_chars) {
                offset = contents->n_chars;
        }

        switch (granularity) {
        case GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER: {
                *start = offset;
                *end = offset + 1;
                return vte_accessible_text_contents_slice (contents, offset, offset + 1);
        }

        case GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE: {
                guint char_offset = *char_positions_index (&contents->characters, offset);
                guint line;

                for (line = 0;
                     line < char_positions_get_size (&contents->linebreaks);
                     line++) {
                        guint line_offset = *char_positions_index (&contents->linebreaks, line);

                        if (line_offset > char_offset) {
                                line--;
                                break;
                        }
                }

                _vte_debug_print (VTE_DEBUG_ALLY,
                                  "Character %u is on line %u.\n",
                                  offset, line);

                *start = *char_positions_index (&contents->linebreaks, line);
                if (line + 1 < char_positions_get_size (&contents->linebreaks))
                        *end = *char_positions_index (&contents->linebreaks, line + 1);
                else
                        *end = contents->n_chars;

                return vte_accessible_text_contents_slice (contents, *start, *end);
        }

        case GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD: {
                gunichar ch = vte_accessible_text_contents_get_char_at (contents, offset);

                if (ch == 0 || !impl->is_word_char (ch))
                        break;

                *start = offset;
                *end = offset;

                while (*start > 0 &&
                       (ch = vte_accessible_text_contents_get_char_at (contents, *start - 1)) &&
                       impl->is_word_char (ch)) {
                        (*start)--;
                }

                while (*end < contents->n_chars &&
                       (ch = vte_accessible_text_contents_get_char_at (contents, *end + 1)) &&
                       impl->is_word_char (ch)) {
                        (*end)++;
                }

                return vte_accessible_text_contents_slice (contents, *start, *end);
        }

        case GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE:
        case GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH:
        default:
                break;
        }

        return nullptr;
}

static guint
vte_accessible_text_get_caret_position (GtkAccessibleText *accessible)
{
        VteTerminal *terminal = VTE_TERMINAL (accessible);
        VteAccessibleText *state = vte_accessible_text_get (terminal);

        g_assert (VTE_IS_TERMINAL (accessible));
        g_assert (state != nullptr);
        g_assert (state->terminal == terminal);

        return state->contents[state->contents_flip].caret;
}

static gboolean
vte_accessible_text_get_selection (GtkAccessibleText       *accessible,
                                   gsize                   *n_ranges,
                                   GtkAccessibleTextRange **ranges)
{
        VteTerminal *terminal = VTE_TERMINAL (accessible);
        VteAccessibleText *state = vte_accessible_text_get (terminal);

        g_assert (VTE_IS_TERMINAL (terminal));
        g_assert (ranges != nullptr);

        *n_ranges = 0;
        *ranges = nullptr;

        try {
                auto impl = _vte_terminal_get_impl (terminal);
                VteAccessibleTextContents *contents = &state->contents[state->contents_flip];
                GtkAccessibleTextRange range;

                if (impl->m_selection_resolved.empty() ||
                    impl->m_selection[vte::to_integral(vte::platform::ClipboardType::PRIMARY)] == nullptr)
                        return FALSE;

                auto start_column = impl->m_selection_resolved.start_column();
                auto start_row = impl->m_selection_resolved.start_row();
                auto end_column = impl->m_selection_resolved.end_column();
                auto end_row = impl->m_selection_resolved.end_row();

                auto start_offset = vte_accessible_text_contents_offset_from_xy (contents, start_column, start_row);
                auto end_offset = vte_accessible_text_contents_offset_from_xy (contents, end_column, end_row);

                range.start = gsize(start_offset);
                range.length = gsize(end_offset - start_offset);

                *n_ranges = 1;
                *ranges = (GtkAccessibleTextRange *)g_memdup2 (&range, sizeof range);

                return TRUE;
        } catch (...) { }

        return FALSE;
}

static gboolean
vte_accessible_text_get_attributes (GtkAccessibleText        *accessible,
                                    guint                     offset,
                                    gsize                    *n_ranges,
                                    GtkAccessibleTextRange  **ranges,
                                    char                   ***attribute_names,
                                    char                   ***attribute_values)
{
        VteTerminal *terminal = VTE_TERMINAL (accessible);
        VteAccessibleText *state = vte_accessible_text_get (terminal);
        VteAccessibleTextContents *contents;
        struct _VteCharAttributes cur_attr;
        struct _VteCharAttributes attr;
        GtkAccessibleTextRange range;
        struct {
                const char *name;
                const char *value;
        } attrs[4];
        char fg_color[16];
        char bg_color[16];
        guint n_attrs = 0;
        guint start = 0;
        guint end = 0;
        guint i;

        g_assert (VTE_IS_TERMINAL (accessible));
        g_assert (ranges != nullptr);
        g_assert (attribute_names != nullptr);
        g_assert (attribute_values != nullptr);

        contents = &state->contents[state->contents_flip];

        *n_ranges = 0;
        *ranges = nullptr;
        *attribute_names = nullptr;
        *attribute_values = nullptr;

        attr = *vte_char_attr_list_get (&contents->attrs, offset);
        start = 0;
        for (i = offset; i--;) {
                cur_attr = *vte_char_attr_list_get (&contents->attrs, i);
                if (!_pango_color_equal (&cur_attr.fore, &attr.fore) ||
                    !_pango_color_equal (&cur_attr.back, &attr.back) ||
                    cur_attr.underline != attr.underline ||
                    cur_attr.strikethrough != attr.strikethrough) {
                        start = i + 1;
                        break;
                }
        }
        end = vte_char_attr_list_get_size (&contents->attrs) - 1;
        for (i = offset + 1; i < vte_char_attr_list_get_size (&contents->attrs); i++) {
                cur_attr = *vte_char_attr_list_get (&contents->attrs, i);
                if (!_pango_color_equal (&cur_attr.fore, &attr.fore) ||
                    !_pango_color_equal (&cur_attr.back, &attr.back) ||
                    cur_attr.underline != attr.underline ||
                    cur_attr.strikethrough != attr.strikethrough) {
                        end = i - 1;
                        break;
                }
        }

        range.start = start;
        range.length = end - start;

        if (range.length == 0)
                return FALSE;

        if (attr.underline) {
                attrs[n_attrs].name = "underline";
                attrs[n_attrs].value = "true";
                n_attrs++;
        }

        if (attr.strikethrough) {
                attrs[n_attrs].name = "strikethrough";
                attrs[n_attrs].value = "true";
                n_attrs++;
        }

        g_snprintf (fg_color, sizeof fg_color, "%u,%u,%u",
                    attr.fore.red, attr.fore.green, attr.fore.blue);
        attrs[n_attrs].name = "fg-color";
        attrs[n_attrs].value = fg_color;
        n_attrs++;

        g_snprintf (bg_color, sizeof bg_color, "%u,%u,%u",
                    attr.back.red, attr.back.green, attr.back.blue);
        attrs[n_attrs].name = "bg-color";
        attrs[n_attrs].value = bg_color;
        n_attrs++;

        *attribute_names = g_new0 (char *, n_attrs + 1);
        *attribute_values = g_new0 (char *, n_attrs + 1);
        *n_ranges = 1;
        *ranges = (GtkAccessibleTextRange *)g_memdup2 (&range, sizeof range);

        for (i = 0; i < n_attrs; i++) {
                (*attribute_names)[i] = g_strdup (attrs[i].name);
                (*attribute_values)[i] = g_strdup (attrs[i].value);
        }

        return TRUE;
}

#if GTK_CHECK_VERSION(4, 15, 1)

static gboolean
vte_accessible_text_get_extents (GtkAccessibleText *accessible,
                                 unsigned int       start,
                                 unsigned int       end,
                                 graphene_rect_t   *extents)
{
        VteTerminal *terminal = VTE_TERMINAL (accessible);
        graphene_rect_t start_rect;
        graphene_rect_t end_rect;
        int start_x, start_y;
        int end_x, end_y;

        g_assert (VTE_IS_TERMINAL (terminal));
        g_assert (extents != nullptr);

        auto impl = _vte_terminal_get_impl (terminal);
        auto state = vte_accessible_text_get (terminal);
        auto contents = &state->contents[state->contents_flip];

        glong cell_width = vte_terminal_get_char_width (terminal);
        glong cell_height = vte_terminal_get_char_height (terminal);
        glong columns = vte_terminal_get_column_count (terminal);

        vte_accessible_text_contents_xy_from_offset (contents, start, &start_x, &start_y);
        vte_accessible_text_contents_xy_from_offset (contents, end, &end_x, &end_y);

        start_rect.origin.x = start_x * cell_width;
        start_rect.origin.y = start_y * cell_height;
        start_rect.size.width = cell_width;
        start_rect.size.height = cell_height;

        end_rect.origin.x = end_x * cell_width;
        end_rect.origin.y = end_y * cell_height;
        end_rect.size.width = cell_width;
        end_rect.size.height = cell_height;

        graphene_rect_union (&start_rect, &end_rect, extents);

        /* If the Y position of the two lines do not match, then we need
         * to extend the area to contain all possible wrap-around text
         * for the region.
         *
         * This does not attempt to try to find the earliest/latest character
         * on each line which could be an opportunity for shrinking the
         * included extents.
         */
        if (!_vte_double_equal (end_rect.origin.y, start_rect.origin.y)) {
                extents->origin.x = 0;
                extents->size.width = cell_width * columns;
        }

        extents->origin.x += impl->m_border.left;
        extents->origin.y += impl->m_border.top;

        return TRUE;
}

static gboolean
vte_accessible_text_get_offset (GtkAccessibleText      *accessible,
                                const graphene_point_t *point,
                                unsigned int           *offset)
{
        VteTerminal *terminal = VTE_TERMINAL (accessible);

        g_assert (VTE_IS_TERMINAL (terminal));

        auto impl = _vte_terminal_get_impl (terminal);
        auto state = vte_accessible_text_get (terminal);
        auto contents = &state->contents[state->contents_flip];

        glong cell_width = vte_terminal_get_char_width (terminal);
        glong cell_height = vte_terminal_get_char_height (terminal);

        int x = (point->x - impl->m_border.left) / cell_width;
        int y = (point->y - impl->m_border.top) / cell_height;

        *offset = vte_accessible_text_contents_offset_from_xy (contents, x, y);

        return TRUE;
}

#endif // gtk 4.16

void
_vte_accessible_text_iface_init (GtkAccessibleTextInterface *iface)
{
        iface->get_attributes = vte_accessible_text_get_attributes;
        iface->get_caret_position = vte_accessible_text_get_caret_position;
        iface->get_contents = vte_accessible_text_get_contents;
        iface->get_contents_at = vte_accessible_text_get_contents_at;
        iface->get_selection = vte_accessible_text_get_selection;

#if GTK_CHECK_VERSION(4, 15, 1)
        iface->get_offset = vte_accessible_text_get_offset;
        iface->get_extents = vte_accessible_text_get_extents;
#endif
}

static void
vte_accessible_text_contents_changed (VteTerminal       *terminal,
                                      VteAccessibleText *state)
{
        VteAccessibleTextContents *next = nullptr;
        VteAccessibleTextContents *prev = nullptr;
        const char *nextstr;
        const char *prevstr;
        gsize prevlen;
        gsize nextlen;

        g_assert (VTE_IS_TERMINAL (terminal));
        g_assert (state != nullptr);
        g_assert (state->terminal == terminal);

        if (!vte_terminal_get_enable_a11y (terminal))
          return;

        prev = &state->contents[state->contents_flip];
        next = &state->contents[!state->contents_flip];

        /* Get a new snapshot of contents so that we can compare this to the
         * previous contents. That way we can discover if it was a backspace
         * that occurred or if it's more than that.
         *
         * We do not filp state->contents_flip immediately so that we can
         * allow the AT context the ability to access the current contents
         * on DELETE operations.
         */
        vte_accessible_text_contents_reset (next);
        vte_accessible_text_contents_snapshot (next, state->terminal);

        nextstr = vte_accessible_text_contents_get_string (next, &nextlen);
        prevstr = vte_accessible_text_contents_get_string (prev, &prevlen);

        vte_assert_cmpint (char_positions_get_size (&prev->characters), ==, prev->n_chars);
        vte_assert_cmpint (char_positions_get_size (&next->characters), ==, next->n_chars);

        /* NOTE:
         *
         * The code below is based upon what vteaccess.cc did for GTK 3.
         * It does not do any sort of appropriate diffing to try to handle
         * scrolling correctly. That would be a good idea to implement in
         * the longer term.
         *
         * It just looks for a long prefix match, and then a long suffix
         * match and attempts to diff what is between those to end points.
         */

        const char *prevc = prevstr;
        const char *nextc = nextstr;
        gsize offset = 0;

        /* Find the beginning of changes */
        while ((offset < prev->n_chars) && (offset < next->n_chars)) {
                gunichar prevch = g_utf8_get_char (prevc);
                gunichar nextch = g_utf8_get_char (nextc);

                if (prevch != nextch) {
                        break;
                }

                offset++;

                prevc = g_utf8_next_char (prevc);
                nextc = g_utf8_next_char (nextc);
        }

        /* Find the end of changes */
        gsize next_end = next->n_chars;
        gsize prev_end = prev->n_chars;

        prevc = prevstr + prevlen;
        nextc = nextstr + nextlen;

        while ((next_end > offset) && (prev_end > offset)) {
                prevc = g_utf8_prev_char (prevc);
                nextc = g_utf8_prev_char (nextc);

                gunichar prevch = g_utf8_get_char (prevc);
                gunichar nextch = g_utf8_get_char (nextc);

                if (prevch != nextch) {
                        break;
                }

                next_end--;
                prev_end--;
        }

        if (offset < prev_end) {
                gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (terminal),
                                                     GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE,
                                                     offset, prev_end);
        }

        state->contents_flip = !state->contents_flip;

        if (offset < next_end) {
                gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (terminal),
                                                     GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                                     offset, next_end);
        }

        if (prev->caret != next->caret) {
                gtk_accessible_text_update_caret_position (GTK_ACCESSIBLE_TEXT (terminal));
        }
}

static void
vte_accessible_text_cursor_moved (VteTerminal       *terminal,
                                  VteAccessibleText *state)
{
        VteAccessibleTextContents *contents = nullptr;

        g_assert (VTE_IS_TERMINAL (terminal));
        g_assert (state != nullptr);
        g_assert (state->terminal == terminal);

        if (!vte_terminal_get_enable_a11y (terminal))
          return;

        contents = &state->contents[state->contents_flip];

        long ccol, crow;
        vte_terminal_get_cursor_position (terminal, &ccol, &crow);
        if (ccol == contents->cached_caret_column && crow == contents->cached_caret_row) {
                return;
        }

        _vte_debug_print (VTE_DEBUG_ALLY, "Cursor at (%ld, " "%ld).\n", ccol, crow);

        contents->cached_caret_column = ccol;
        contents->cached_caret_row = crow;
        contents->caret = vte_accessible_text_contents_find_caret (contents, ccol, crow);

        gtk_accessible_text_update_caret_position (GTK_ACCESSIBLE_TEXT (terminal));
}

static void
vte_accessible_text_window_title_changed (VteTerminal       *terminal,
                                          VteAccessibleText *state)
{
        const char *window_title;

        g_assert (VTE_IS_TERMINAL (terminal));
        g_assert (state != nullptr);
        g_assert (state->terminal == terminal);

        if (!vte_terminal_get_enable_a11y (terminal))
          return;

        window_title = vte_terminal_get_window_title (terminal);

        gtk_accessible_update_property (GTK_ACCESSIBLE (terminal),
                                        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, window_title ? window_title : "",
                                        GTK_ACCESSIBLE_VALUE_UNDEFINED);
}

static void
vte_accessible_text_selection_changed (VteTerminal       *terminal,
                                       VteAccessibleText *state)
{
        g_assert (VTE_IS_TERMINAL (terminal));
        g_assert (state != nullptr);
        g_assert (state->terminal == terminal);

        if (!vte_terminal_get_enable_a11y (terminal))
          return;

        gtk_accessible_text_update_caret_position (GTK_ACCESSIBLE_TEXT (terminal));
        gtk_accessible_text_update_selection_bound (GTK_ACCESSIBLE_TEXT (terminal));
}

void
_vte_accessible_text_init (GtkAccessibleText *accessible)
{
        VteTerminal *terminal = VTE_TERMINAL (accessible);
        VteAccessibleText *state;

        state = g_new0 (VteAccessibleText, 1);
        state->terminal = terminal;

        vte_accessible_text_contents_init (&state->contents[0]);
        vte_accessible_text_contents_init (&state->contents[1]);

        g_object_set_data_full (G_OBJECT (terminal),
                                "VTE_ACCESSIBLE_TEXT",
                                state,
                                (GDestroyNotify)vte_accessible_text_free);

        g_signal_connect (terminal,
                          "contents-changed",
                          G_CALLBACK (vte_accessible_text_contents_changed),
                          state);
        g_signal_connect (terminal,
                          "cursor-moved",
                          G_CALLBACK (vte_accessible_text_cursor_moved),
                          state);
        g_signal_connect (terminal,
                          "selection-changed",
                          G_CALLBACK (vte_accessible_text_selection_changed),
                          state);
        g_signal_connect (terminal,
                          "window-title-changed",
                          G_CALLBACK (vte_accessible_text_window_title_changed),
                          state);

        const char *window_title = vte_terminal_get_window_title (terminal);

        gtk_accessible_update_property (GTK_ACCESSIBLE (accessible),
                                        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, window_title ? window_title : "",
                                        GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                        GTK_ACCESSIBLE_PROPERTY_LABEL, "Terminal",
                                        GTK_ACCESSIBLE_PROPERTY_MULTI_LINE, TRUE,
                                        GTK_ACCESSIBLE_VALUE_UNDEFINED);
}

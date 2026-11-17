/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
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

#include <atk/atk.h>
#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>
#include <string.h>
#include "debug.h"
#include <vte/vte.h>
#include "vteaccess.h"
#include "vteinternal.hh"
#include "widget.hh"

#if __has_include(<locale.h>)
#include <locale.h>
#endif
#include <glib/gi18n-lib.h>

#define TERMINAL_FROM_ACCESSIBLE(a) (VTE_TERMINAL(gtk_accessible_get_widget(GTK_ACCESSIBLE(a))))

#define IMPL(t) (_vte_terminal_get_impl(t))
#define IMPL_FROM_WIDGET(w) (IMPL(VTE_TERMINAL(w)))
#define IMPL_FROM_ACCESSIBLE(a) (IMPL_FROM_WIDGET(gtk_accessible_get_widget(GTK_ACCESSIBLE(a))))

enum {
        ACTION_MENU,
        LAST_ACTION
};

typedef struct _VteTerminalAccessiblePrivate {
	gboolean snapshot_contents_invalid;	/* This data is stale. */
	gboolean snapshot_caret_invalid;	/* This data is stale. */
	GString *snapshot_text;		/* Pointer to UTF-8 text. */
	GArray *snapshot_characters;	/* Offsets to character begin points. */
        VteCharAttrList snapshot_attributes; /* Attributes, per byte. */
	GArray *snapshot_linebreaks;	/* Offsets to line breaks. */
	gint snapshot_caret;       /* Location of the cursor (in characters). */
        gboolean text_caret_moved_pending;

	char *action_descriptions[LAST_ACTION];
} VteTerminalAccessiblePrivate;

enum direction {
	direction_previous = -1,
	direction_current = 0,
	direction_next = 1
};

static gunichar vte_terminal_accessible_get_character_at_offset(AtkText *text,
								gint offset);

static const char *vte_terminal_accessible_action_names[] = {
        "menu",
        NULL
};

static const char *vte_terminal_accessible_action_descriptions[] = {
        "Popup context menu",
        NULL
};

static void vte_terminal_accessible_text_iface_init(AtkTextIface *iface);
static void vte_terminal_accessible_component_iface_init(AtkComponentIface *component);
static void vte_terminal_accessible_action_iface_init(AtkActionIface *action);

G_DEFINE_TYPE_WITH_CODE (VteTerminalAccessible, _vte_terminal_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_ADD_PRIVATE (VteTerminalAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, vte_terminal_accessible_text_iface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, vte_terminal_accessible_component_iface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, vte_terminal_accessible_action_iface_init))

static gint
offset_from_xy (VteTerminalAccessiblePrivate *priv,
		gint x, gint y)
{
	gint offset;
	gint linebreak;
	gint next_linebreak;

	if (y >= (gint) priv->snapshot_linebreaks->len)
		y = priv->snapshot_linebreaks->len -1;

	linebreak = g_array_index (priv->snapshot_linebreaks, int, y);
	if (y + 1 == (gint) priv->snapshot_linebreaks->len)
		next_linebreak = priv->snapshot_characters->len;
	else
		next_linebreak = g_array_index (priv->snapshot_linebreaks, int, y + 1);

	offset = linebreak + x;
	if (offset >= next_linebreak)
		offset = next_linebreak -1;
	return offset;
}

static void
xy_from_offset (VteTerminalAccessiblePrivate *priv,
		guint offset, gint *x, gint *y)
{
	guint i, linebreak;
	gint cur_x, cur_y;
	gint cur_offset = 0;

	cur_x = -1;
	cur_y = -1;
	for (i = 0; i < priv->snapshot_linebreaks->len; i++) {
		linebreak = g_array_index (priv->snapshot_linebreaks, int, i);
		if (offset < linebreak) {
			cur_x = offset - cur_offset;
			cur_y = i - 1;
			break;

		}  else {
			cur_offset = linebreak;
		}
	}
	if (i == priv->snapshot_linebreaks->len) {
		if (offset <= priv->snapshot_characters->len) {
			cur_x = offset - cur_offset;
			cur_y = i - 1;
		}
	}
	*x = cur_x;
	*y = cur_y;
}

static void
emit_text_caret_moved(GObject *object, glong caret)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS|VTE_DEBUG_ALLY,
			"Accessibility peer emitting "
			"`text-caret-moved'.\n");
	g_signal_emit_by_name(object, "text-caret-moved", caret);
}

static void
emit_text_changed_insert(GObject *object,
			 const char *text, glong offset, glong len)
{
	glong start, count;
	if (len == 0) {
		return;
	}
	/* Convert the byte offsets to character offsets. */
	start = g_utf8_pointer_to_offset (text, text + offset);
	count = g_utf8_pointer_to_offset (text + offset, text + offset + len);
	_vte_debug_print(VTE_DEBUG_SIGNALS|VTE_DEBUG_ALLY,
			"Accessibility peer emitting "
			"`text-changed::insert' (%ld, %ld) (%ld, %ld).\n"
			"Inserted text was `%.*s'.\n",
			offset, len, start, count,
			(int) len, text + offset);
	g_signal_emit_by_name(object, "text-changed::insert", start, count);
}

static void
emit_text_changed_delete(GObject *object,
			 const char *text, glong offset, glong len)
{
	glong start, count;
	if (len == 0) {
		return;
	}
	/* Convert the byte offsets to characters. */
	start = g_utf8_pointer_to_offset (text, text + offset);
	count = g_utf8_pointer_to_offset (text + offset, text + offset + len);
	_vte_debug_print(VTE_DEBUG_SIGNALS|VTE_DEBUG_ALLY,
			"Accessibility peer emitting "
			"`text-changed::delete' (%ld, %ld) (%ld, %ld).\n"
			"Deleted text was `%.*s'.\n",
			offset, len, start, count,
			(int) len, text + offset);
	g_signal_emit_by_name(object, "text-changed::delete", start, count);
}

static void
vte_terminal_accessible_update_private_data_if_needed(VteTerminalAccessible *accessible,
                                                      GString **old_text,
                                                      GArray **old_characters)
{
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	struct _VteCharAttributes attrs;
	char *next;
	long row, offset, caret;
	long ccol, crow;
	guint i;

	/* If nothing's changed, just return immediately. */
	if ((priv->snapshot_contents_invalid == FALSE) &&
	    (priv->snapshot_caret_invalid == FALSE)) {
                if (old_text) {
			if (priv->snapshot_text) {
                                *old_text = g_string_new_len(priv->snapshot_text->str,
                                                             priv->snapshot_text->len);
			} else {
                                *old_text = g_string_new("");
			}
                }
                if (old_characters) {
                        if (priv->snapshot_characters) {
                                *old_characters = g_array_sized_new(FALSE, FALSE, sizeof(int),
                                                                    priv->snapshot_characters->len);
                                g_array_append_vals(*old_characters,
                                                    priv->snapshot_characters->data,
                                                    priv->snapshot_characters->len);
                        } else {
                                *old_characters = g_array_new(FALSE, FALSE, sizeof(int));
			}
		}
		return;
	}

	/* Re-read the contents of the widget if the contents have changed. */
        VteTerminal* terminal = TERMINAL_FROM_ACCESSIBLE(accessible);
        auto impl = IMPL(terminal);
	if (priv->snapshot_contents_invalid) {
		/* Free the outdated snapshot data, unless the caller
		 * wants it. */
                if (old_text) {
			if (priv->snapshot_text != NULL) {
                                *old_text = priv->snapshot_text;
			} else {
                                *old_text = g_string_new("");
			}
		} else {
			if (priv->snapshot_text != NULL) {
				g_string_free(priv->snapshot_text, TRUE);
			}
		}
		priv->snapshot_text = NULL;

                /* Free the character offsets unless the caller wants it,
                 * and allocate a new array to hold them. */
                if (old_characters) {
                        if (priv->snapshot_characters != NULL) {
                                *old_characters = priv->snapshot_characters;
                        } else {
                                *old_characters = g_array_new(FALSE, FALSE, sizeof(int));
                        }
                } else {
                        if (priv->snapshot_characters != NULL) {
                                g_array_free(priv->snapshot_characters, TRUE);
                        }
		}
		priv->snapshot_characters = g_array_new(FALSE, FALSE, sizeof(int));

		/* Free the attribute lists and allocate a new array to hold
		 * them. */
                vte_char_attr_list_set_size(&priv->snapshot_attributes, 0);

		/* Free the linebreak offsets and allocate a new array to hold
		 * them. */
		if (priv->snapshot_linebreaks != NULL) {
			g_array_free(priv->snapshot_linebreaks, TRUE);
		}
		priv->snapshot_linebreaks = g_array_new(FALSE, FALSE, sizeof(int));

		/* Get a new view of the uber-label. */
                auto text = g_string_new(nullptr);
                try {
                        impl->get_text_displayed_a11y(text,
                                                      &priv->snapshot_attributes);
                } catch (...) {
                        g_string_truncate(text, 0);
                }
                priv->snapshot_text = text;
		/* Get the offsets to the beginnings of each character. */
		i = 0;
		next = priv->snapshot_text->str;
		while (i < vte_char_attr_list_get_size(&priv->snapshot_attributes)) {
			g_array_append_val(priv->snapshot_characters, i);
			next = g_utf8_next_char(next);
			if (next == NULL) {
				break;
			} else {
				i = next - priv->snapshot_text->str;
			}
		}
		/* Find offsets for the beginning of lines. */
		for (i = 0, row = 0; i < priv->snapshot_characters->len; i++) {
			/* Get the attributes for the current cell. */
			offset = g_array_index(priv->snapshot_characters,
					       int, i);
                        attrs = *vte_char_attr_list_get(&priv->snapshot_attributes, offset);
			/* If this character is on a row different from the row
			 * the character we looked at previously was on, then
			 * it's a new line and we need to keep track of where
			 * it is. */
			if ((i == 0) || (attrs.row != row)) {
				_vte_debug_print(VTE_DEBUG_ALLY,
						"Row %d/%ld begins at %u.\n",
						priv->snapshot_linebreaks->len,
						attrs.row, i);
				g_array_append_val(priv->snapshot_linebreaks, i);
			}
			row = attrs.row;
		}
		/* Add the final line break. */
		g_array_append_val(priv->snapshot_linebreaks, i);
		/* We're finished updating this. */
		priv->snapshot_contents_invalid = FALSE;
	}

	/* Update the caret position. */
	vte_terminal_get_cursor_position(terminal, &ccol, &crow);
	_vte_debug_print(VTE_DEBUG_ALLY,
			"Cursor at (%ld, " "%ld).\n", ccol, crow);

	/* Get the offsets to the beginnings of each line. */
	caret = 0;
	for (i = 0; i < priv->snapshot_characters->len; i++) {
		/* Get the attributes for the current cell. */
		offset = g_array_index(priv->snapshot_characters,
				       int, i);
                attrs = *vte_char_attr_list_get(&priv->snapshot_attributes, offset);
		/* If this cell is "before" the cursor, move the
		 * caret to be "here". */
		if ((attrs.row < crow) ||
		    ((attrs.row == crow) && (attrs.column < ccol))) {
			caret = i + 1;
		}
	}

        /* Make a note that we'll need to notify observers if the caret moved.
         * But only notify them after sending text-changed. */
	if (caret != priv->snapshot_caret) {
		priv->snapshot_caret = caret;
                priv->text_caret_moved_pending = TRUE;
	}

	/* Done updating the caret position, whether we needed to or not. */
	priv->snapshot_caret_invalid = FALSE;

	_vte_debug_print(VTE_DEBUG_ALLY,
			"Refreshed accessibility snapshot, "
			"%ld cells, %ld characters.\n",
			(long)vte_char_attr_list_get_size(&priv->snapshot_attributes),
			(long)priv->snapshot_characters->len);
}

static void
vte_terminal_accessible_maybe_emit_text_caret_moved(VteTerminalAccessible *accessible)
{
        VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);

        if (priv->text_caret_moved_pending) {
                emit_text_caret_moved(G_OBJECT(accessible), priv->snapshot_caret);
                priv->text_caret_moved_pending = FALSE;
        }
}

void
_vte_terminal_accessible_text_inserted(VteTerminalAccessible* accessible)
{
        _vte_terminal_accessible_text_modified(accessible);
}

void
_vte_terminal_accessible_text_deleted (VteTerminalAccessible* accessible)
{
        _vte_terminal_accessible_text_modified(accessible);
}

void
_vte_terminal_accessible_text_modified(VteTerminalAccessible* accessible)
{
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
        GString *old_text;
        GArray *old_characters;
	char *old, *current;
	glong offset, caret_offset, olen, clen;
	gint old_snapshot_caret;

        auto widget = gtk_accessible_get_widget(GTK_ACCESSIBLE(accessible));
        auto terminal = VTE_TERMINAL(widget);

        if (!vte_terminal_get_enable_a11y (terminal))
                return;

	old_snapshot_caret = priv->snapshot_caret;
	priv->snapshot_contents_invalid = TRUE;
	vte_terminal_accessible_update_private_data_if_needed(accessible,
                                                              &old_text,
                                                              &old_characters);
        g_assert(old_text != NULL);
        g_assert(old_characters != NULL);

	current = priv->snapshot_text->str;
	clen = priv->snapshot_text->len;
        old = old_text->str;
        olen = old_text->len;

	if ((guint) priv->snapshot_caret < priv->snapshot_characters->len) {
		caret_offset = g_array_index(priv->snapshot_characters,
				int, priv->snapshot_caret);
	} else {
		/* caret was not in the line */
		caret_offset = clen;
	}

	/* Find the offset where they don't match. */
	offset = 0;
	while ((offset < olen) && (offset < clen)) {
		if (old[offset] != current[offset]) {
			break;
		}
		offset++;
	}

        /* Check if we just backspaced over a space. */
	if ((olen == offset) &&
		       	(caret_offset < olen && old[caret_offset] == ' ') &&
			(old_snapshot_caret == priv->snapshot_caret + 1)) {
                GString *saved_text = priv->snapshot_text;
                GArray *saved_characters = priv->snapshot_characters;

                priv->snapshot_text = old_text;
                priv->snapshot_characters = old_characters;
		emit_text_changed_delete(G_OBJECT(accessible),
					 old, caret_offset, 1);
                priv->snapshot_text = saved_text;
                priv->snapshot_characters = saved_characters;
		emit_text_changed_insert(G_OBJECT(accessible),
					 old, caret_offset, 1);
	}


	/* At least one of them had better have more data, right? */
	if ((offset < olen) || (offset < clen)) {
		/* Back up from both end points until we find the *last* point
		 * where they differed. */
		gchar *op = old + olen;
		gchar *cp = current + clen;
		while (op > old + offset && cp > current + offset) {
			gchar *opp = g_utf8_prev_char (op);
			gchar *cpp = g_utf8_prev_char (cp);
			if (g_utf8_get_char (opp) != g_utf8_get_char (cpp)) {
				break;
			}
			op = opp;
			cp = cpp;
		}
		/* recompute the respective lengths */
		olen = op - old;
		clen = cp - current;
		/* At least one of them has to have text the other
		 * doesn't. */
		g_assert((clen > offset) || (olen > offset));
		g_assert((clen >= 0) && (olen >= 0));
		/* Now emit a deleted signal for text that was in the old
		 * string but isn't in the new one... */
		if (olen > offset) {
                        GString *saved_text = priv->snapshot_text;
                        GArray *saved_characters = priv->snapshot_characters;

                        priv->snapshot_text = old_text;
                        priv->snapshot_characters = old_characters;
			emit_text_changed_delete(G_OBJECT(accessible),
						 old,
						 offset,
						 olen - offset);
                        priv->snapshot_text = saved_text;
                        priv->snapshot_characters = saved_characters;
		}
		/* .. and an inserted signal for text that wasn't in the old
		 * string but is in the new one. */
		if (clen > offset) {
			emit_text_changed_insert(G_OBJECT(accessible),
						 current,
						 offset,
						 clen - offset);
		}
	}

        vte_terminal_accessible_maybe_emit_text_caret_moved(accessible);

        g_string_free(old_text, TRUE);
        g_array_free(old_characters, TRUE);
}

/* Compute a simple diff between the `before` string (before_len bytes) and the
   `after` string (after_len bytes). We here only check for common head and
   tail.

   If the strings are equal, this returns FALSE.

   If the strings differ, this returns TRUE, and *head_len is set to the number
   of bytes that are identical at the beginning of `before` and `after`, and
   *tail_len is set to the number of bytes that are identical at the end of
   `before` and `after`. */
static gboolean
check_diff(const gchar *before, guint before_len,
	   const gchar *after, guint after_len,
	   guint *head_len, guint *tail_len)
{
	guint i;

	for (i = 0; i < before_len && i < after_len; i++)
		if (before[i] != after[i])
			break;

	if (i == before_len && i == after_len)
		/* They are identical. */
		return FALSE;

	/* They started diverging at i. */
	*head_len = i;

	for (i = 1; i <= before_len - *head_len && i <= after_len - *head_len; i++)
		if (before[before_len - i] != after[after_len - i])
			break;

	/* They finished diverging here. */
	*tail_len = i - 1;

	return TRUE;
}

void
_vte_terminal_accessible_text_scrolled(VteTerminalAccessible* accessible,
                                       long howmuch)
{
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	struct _VteCharAttributes attr;
	long delta, row_count;
	guint drop, old_len, new_len, old_common, new_common;

        /* TODOegmont: Fix this for smooth scrolling */
        /* g_assert(howmuch != 0); */
        if (howmuch == 0) return;

        auto widget = gtk_accessible_get_widget(GTK_ACCESSIBLE(accessible));
        auto terminal = VTE_TERMINAL(widget);

        if (!vte_terminal_get_enable_a11y (terminal))
                return;

        row_count = vte_terminal_get_row_count(terminal);
	if (((howmuch < 0) && (howmuch <= -row_count)) ||
	    ((howmuch > 0) && (howmuch >= row_count))) {
		/* All of the text was removed. */
		if (priv->snapshot_text != NULL) {
			if (priv->snapshot_text->str != NULL) {
				emit_text_changed_delete(G_OBJECT(accessible),
							 priv->snapshot_text->str,
							 0,
							 priv->snapshot_text->len);
			}
		}
		priv->snapshot_contents_invalid = TRUE;
		vte_terminal_accessible_update_private_data_if_needed(accessible,
								      NULL,
								      NULL);
		/* All of the present text was added. */
		if (priv->snapshot_text != NULL) {
			if (priv->snapshot_text->str != NULL) {
				emit_text_changed_insert(G_OBJECT(accessible),
							 priv->snapshot_text->str,
							 0,
							 priv->snapshot_text->len);
			}
		}
                vte_terminal_accessible_maybe_emit_text_caret_moved(accessible);
		return;
	}

	/* Find the start point. */
	delta = 0;
        if (vte_char_attr_list_get_size(&priv->snapshot_attributes) > 0) {
                attr = *vte_char_attr_list_get(&priv->snapshot_attributes, 0);
                delta = attr.row;
        }
	/* We scrolled up, so text was added at the top and removed
	 * from the bottom. */
	if ((howmuch < 0) && (howmuch > -row_count)) {
		howmuch = -howmuch;
		if (priv->snapshot_text != NULL) {
			old_len = vte_char_attr_list_get_size(&priv->snapshot_attributes);

			/* Find the first byte that scrolled off. */
			for (old_common = 0; old_common < old_len; old_common++) {
				attr = *vte_char_attr_list_get(&priv->snapshot_attributes, old_common);
				if (attr.row >= delta + row_count - howmuch) {
					break;
				}
			}
			if (old_common < old_len) {
				drop = old_len - old_common;

				GString *old_text;
				GArray *old_characters;

				/* Refresh. */
				priv->snapshot_contents_invalid = TRUE;
				vte_terminal_accessible_update_private_data_if_needed(accessible,
										      &old_text,
										      &old_characters);

				GString *new_text = priv->snapshot_text;
				GArray *new_characters = priv->snapshot_characters;
				new_len = new_text->len;

				if (old_common > new_len)
					/* Not only did we scroll, but we even removed some text */
					new_common = new_len;
				else
					new_common = old_common;

				guint head_len, tail_len;
				gboolean diff;

				/* Check for more difference than just the addition at top and removal at bottom */
				diff = check_diff(old_text->str, old_common,
						  new_text->str + new_len - new_common, new_common,
						  &head_len, &tail_len);

				/* Note: to keep offsets correct, we need to
				 * notify deletions from bottom to top and
				 * additions from top to bottom
				 */

				/* Temporarily switch to old text to emit deletion events */
				priv->snapshot_text = old_text;
				priv->snapshot_characters = old_characters;

				/* Delete bottom first */
				emit_text_changed_delete(G_OBJECT(accessible),
						old_text->str,
						old_common,
						drop);

				if (diff)
					/* Not only did we scroll, but we also modified some bits */
					/* Delete middle */
					emit_text_changed_delete(G_OBJECT(accessible),
								 old_text->str,
								 head_len,
								 old_common - tail_len - head_len);

				/* Switch to new text */
				priv->snapshot_text = new_text;
				priv->snapshot_characters = new_characters;

				g_string_free(old_text, TRUE);
				g_array_free(old_characters, TRUE);

				/* If we now have more text than before, the initial portion
				 * was added. */
				if (new_len > new_common)
					/* Add top */
					emit_text_changed_insert(G_OBJECT(accessible),
								 new_text->str,
								 0,
								 new_len - new_common);

				if (diff)
					/* Add middle */
					emit_text_changed_insert(G_OBJECT(accessible),
								 new_text->str,
								 new_len - new_common + head_len,
								 new_common - tail_len - head_len);
			}
		} else {
			/* Just refresh. */
			priv->snapshot_contents_invalid = TRUE;
			vte_terminal_accessible_update_private_data_if_needed(accessible,
									      NULL,
									      NULL);
		}

                vte_terminal_accessible_maybe_emit_text_caret_moved(accessible);
		return;
	}
	/* We scrolled down, so text was added at the bottom and removed
	 * from the top. */
	if ((howmuch > 0) && (howmuch < row_count)) {
		if (priv->snapshot_text != NULL) {
			/* Leave trailing '\n' untouched, so that the exposed text always contains a trailing '\n',
			   insertion happens in front of it: bug 657960 */
			old_len = vte_char_attr_list_get_size(&priv->snapshot_attributes) - 1;

			/* Find the first byte that wasn't scrolled off the top. */
			for (drop = 0; drop < old_len; drop++) {
				attr = *vte_char_attr_list_get(&priv->snapshot_attributes, drop);
				if (attr.row >= delta + howmuch) {
					break;
				}
			}

			/* Figure out how much text was common, and refresh. */
			old_common = old_len - drop;

			GString *old_text;
			GArray *old_characters;

			priv->snapshot_contents_invalid = TRUE;
			vte_terminal_accessible_update_private_data_if_needed(accessible,
									      &old_text,
									      &old_characters);

			GString *new_text = priv->snapshot_text;
			GArray *new_characters = priv->snapshot_characters;
			new_len = new_text->len - 1;

			if (old_common > new_len)
				/* Not only did we scroll, but we even removed some text */
				new_common = new_len;
			else
				new_common = old_common;

			guint head_len, tail_len;
			guint extra_add = 0;
			gboolean diff;

			/* Check for more difference than just the removal at top and addition at bottom */
			diff = check_diff(old_text->str + drop, old_common,
				       new_text->str, new_common,
				       &head_len, &tail_len);

			/* Note: to keep offsets correct, we need to
			 * notify deletions from bottom to top and
			 * additions from top to bottom
			 */

			/* Temporarily switch to old text to emit deletion events */
			priv->snapshot_text = old_text;
			priv->snapshot_characters = old_characters;

			if (diff)
				/* Not only did we scroll, but we also modified some bits */
				/* Delete middle first */
				emit_text_changed_delete(G_OBJECT(accessible),
							 old_text->str,
							 drop + head_len,
							 old_common - tail_len - head_len);

			/* Delete top */
			emit_text_changed_delete(G_OBJECT(accessible),
					old_text->str,
					0,
                                        drop);

			/* Switch to new text */
			priv->snapshot_text = new_text;
			priv->snapshot_characters = new_characters;

			g_string_free(old_text, TRUE);
			g_array_free(old_characters, TRUE);

			if (diff)
			{
				/* Add middle */
				if (tail_len)
					emit_text_changed_insert(G_OBJECT(accessible),
								 new_text->str,
								 head_len,
								 new_common - head_len - tail_len);
				else
					/* Diff was at bottom, merge with addition below */
					extra_add = new_common - head_len - tail_len;
			}

			/* Add bottom */
			if (new_len > new_common - extra_add)
				emit_text_changed_insert(G_OBJECT(accessible),
							 priv->snapshot_text->str,
							 new_common - extra_add,
							 new_len - (new_common - extra_add));
		} else {
			/* Just refresh. */
			priv->snapshot_contents_invalid = TRUE;
			vte_terminal_accessible_update_private_data_if_needed(accessible,
									      NULL,
									      NULL);
		}

                vte_terminal_accessible_maybe_emit_text_caret_moved(accessible);
		return;
	}
	g_assert_not_reached();
}

/* A signal handler to catch "cursor-moved" signals. */
static void
vte_terminal_accessible_invalidate_cursor(VteTerminal *terminal, gpointer data)
{
        VteTerminalAccessible *accessible = (VteTerminalAccessible *)data;
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);

        if (!vte_terminal_get_enable_a11y (terminal))
                return;

	_vte_debug_print(VTE_DEBUG_ALLY,
			"Invalidating accessibility cursor.\n");
	priv->snapshot_caret_invalid = TRUE;
	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
        vte_terminal_accessible_maybe_emit_text_caret_moved(accessible);
}

/* Handle title changes by resetting the description. */
static void
vte_terminal_accessible_title_changed(VteTerminal *terminal, gpointer data)
{
        VteTerminalAccessible *accessible = (VteTerminalAccessible *)data;

        if (!vte_terminal_get_enable_a11y (terminal))
                return;

        auto const title = vte_terminal_get_window_title(terminal);
        atk_object_set_description(ATK_OBJECT(accessible), title ? title : "");
}

/* Reflect visibility-notify events. */
static gboolean
vte_terminal_accessible_visibility_notify(VteTerminal *terminal,
					  GdkEventVisibility *event,
					  gpointer data)
{
        VteTerminalAccessible *accessible = (VteTerminalAccessible *)data;
	GtkWidget *widget;
	gboolean visible;

        if (!vte_terminal_get_enable_a11y (terminal))
                return FALSE;

	visible = event->state != GDK_VISIBILITY_FULLY_OBSCURED;
	/* The VISIBLE state indicates that this widget is "visible". */
	atk_object_notify_state_change(ATK_OBJECT(accessible),
				       ATK_STATE_VISIBLE,
				       visible);
	widget = &terminal->widget;
	while (visible) {
		if (gtk_widget_get_toplevel(widget) == widget) {
			break;
		}
		if (widget == NULL) {
			break;
		}
		visible = visible && (gtk_widget_get_visible(widget));
		widget = gtk_widget_get_parent(widget);
	}
	/* The SHOWING state indicates that this widget, and all of its
	 * parents up to the toplevel, are "visible". */
	atk_object_notify_state_change(ATK_OBJECT(accessible),
				       ATK_STATE_SHOWING,
				       visible);

	return FALSE;
}

static void
vte_terminal_accessible_selection_changed (VteTerminal *terminal,
					   gpointer data)
{
        VteTerminalAccessible *accessible = (VteTerminalAccessible *)data;

        if (!vte_terminal_get_enable_a11y (terminal))
                return;

	g_signal_emit_by_name (accessible, "text_selection_changed");
}

static void
vte_terminal_accessible_initialize(AtkObject *obj,
                                   gpointer data)
{
        auto accessible = VTE_TERMINAL_ACCESSIBLE(obj);
        VteTerminal *terminal = VTE_TERMINAL(data);
        const char *window_title;

	ATK_OBJECT_CLASS (_vte_terminal_accessible_parent_class)->initialize (obj, data);

        auto impl = IMPL(terminal);
        try {
                impl->set_accessible(accessible);
        } catch (...) {
        }

	g_signal_connect(terminal, "cursor-moved",
			 G_CALLBACK(vte_terminal_accessible_invalidate_cursor),
			 obj);
	g_signal_connect(terminal, "window-title-changed",
			 G_CALLBACK(vte_terminal_accessible_title_changed),
			 obj);

	g_signal_connect(terminal, "visibility-notify-event",
		G_CALLBACK(vte_terminal_accessible_visibility_notify), obj);
	g_signal_connect(terminal, "selection-changed",
		G_CALLBACK(vte_terminal_accessible_selection_changed), obj);

	atk_object_set_name(obj, "Terminal");
        window_title = vte_terminal_get_window_title(terminal);
	atk_object_set_description(obj, window_title ? window_title : "");

	atk_object_notify_state_change(obj,
				       ATK_STATE_FOCUSABLE, TRUE);
	atk_object_notify_state_change(obj,
				       ATK_STATE_EXPANDABLE, FALSE);
	atk_object_notify_state_change(obj,
				       ATK_STATE_RESIZABLE, TRUE);
        atk_object_set_role(obj, ATK_ROLE_TERMINAL);
}

static void
_vte_terminal_accessible_init (VteTerminalAccessible *accessible)
{
        VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private (accessible);

	_vte_debug_print(VTE_DEBUG_ALLY, "Initialising accessible peer.\n");

	priv->snapshot_text = NULL;
	priv->snapshot_characters = NULL;
	priv->snapshot_linebreaks = NULL;
	priv->snapshot_caret = -1;
	priv->snapshot_contents_invalid = TRUE;
	priv->snapshot_caret_invalid = TRUE;
        priv->text_caret_moved_pending = FALSE;

	vte_char_attr_list_init(&priv->snapshot_attributes);
}

static void
vte_terminal_accessible_finalize(GObject *object)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(object);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
        GtkWidget *widget;
	gint i;

	_vte_debug_print(VTE_DEBUG_ALLY, "Finalizing accessible peer.\n");

        widget = gtk_accessible_get_widget (GTK_ACCESSIBLE(accessible));

	if (widget != NULL) {
		g_signal_handlers_disconnect_matched(widget,
						     (GSignalMatchType)(G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA),
						     0, 0, NULL,
						     (void *)vte_terminal_accessible_invalidate_cursor,
						     object);
		g_signal_handlers_disconnect_matched(widget,
						     (GSignalMatchType)(G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA),
						     0, 0, NULL,
						     (void *)vte_terminal_accessible_title_changed,
						     object);
		g_signal_handlers_disconnect_matched(widget,
						     (GSignalMatchType)(G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA),
						     0, 0, NULL,
						     (void *)vte_terminal_accessible_visibility_notify,
						     object);

                auto terminal = VTE_TERMINAL(widget);
                auto impl = IMPL(terminal);
                try {
                        impl->set_accessible(nullptr);
                } catch (...) {
                }
	}

	if (priv->snapshot_text != NULL) {
		g_string_free(priv->snapshot_text, TRUE);
	}
	if (priv->snapshot_characters != NULL) {
		g_array_free(priv->snapshot_characters, TRUE);
	}
        vte_char_attr_list_clear(&priv->snapshot_attributes);
	if (priv->snapshot_linebreaks != NULL) {
		g_array_free(priv->snapshot_linebreaks, TRUE);
	}
	for (i = 0; i < LAST_ACTION; i++) {
		g_free (priv->action_descriptions[i]);
	}

	G_OBJECT_CLASS(_vte_terminal_accessible_parent_class)->finalize(object);
}

static gchar *
vte_terminal_accessible_get_text(AtkText *text,
				 gint start_offset, gint end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	int start, end;
	gchar *ret;

	g_assert(VTE_IS_TERMINAL_ACCESSIBLE(accessible));

        /* Swap around if start is greater than end */
        if (start_offset > end_offset && end_offset != -1) {
                gint tmp;

                tmp = start_offset;
                start_offset = end_offset;
                end_offset = tmp;
        }

	g_assert((start_offset >= 0) && (end_offset >= -1));

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);

	_vte_debug_print(VTE_DEBUG_ALLY,
			"Getting text from %d to %d of %d.\n",
			start_offset, end_offset,
			priv->snapshot_characters->len);

	/* If the requested area is after all of the text, just return an
	 * empty string. */
	if (start_offset >= (int) priv->snapshot_characters->len) {
		return g_strdup("");
	}

	/* Map the offsets to, er, offsets. */
	start = g_array_index(priv->snapshot_characters, int, start_offset);
	if ((end_offset == -1) || (end_offset >= (int) priv->snapshot_characters->len) ) {
		/* Get everything up to the end of the buffer. */
		end = priv->snapshot_text->len;
	} else {
		/* Map the stopping point. */
		end = g_array_index(priv->snapshot_characters, int, end_offset);
	}
	if (end <= start) {
		ret = g_strdup ("");
	} else {
		ret = (char *)g_malloc(end - start + 1);
		memcpy(ret, priv->snapshot_text->str + start, end - start);
		ret[end - start] = '\0';
	}
	return ret;
}

/* Map a subsection of the text with before/at/after char/word/line specs
 * into a run of Unicode characters.  (The interface is specifying characters,
 * not bytes, plus that saves us from having to deal with parts of multibyte
 * characters, which are icky.) */
static gchar *
vte_terminal_accessible_get_text_somewhere(AtkText *text,
					   gint offset,
					   AtkTextBoundary boundary_type,
					   enum direction direction,
					   gint *start_offset,
					   gint *end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	gunichar current, prev, next;
	guint start, end, line;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);

        auto impl = IMPL_FROM_ACCESSIBLE(text);

	_vte_debug_print(VTE_DEBUG_ALLY,
			"Getting %s %s at %d of %" G_GSIZE_FORMAT " .\n",
			(direction == direction_current) ? "this" :
			((direction == direction_next) ? "next" : "previous"),
			(boundary_type == ATK_TEXT_BOUNDARY_CHAR) ? "char" :
			((boundary_type == ATK_TEXT_BOUNDARY_LINE_START) ? "line (start)" :
			((boundary_type == ATK_TEXT_BOUNDARY_LINE_END) ? "line (end)" :
			((boundary_type == ATK_TEXT_BOUNDARY_WORD_START) ? "word (start)" :
			((boundary_type == ATK_TEXT_BOUNDARY_WORD_END) ? "word (end)" :
			((boundary_type == ATK_TEXT_BOUNDARY_SENTENCE_START) ? "sentence (start)" :
			((boundary_type == ATK_TEXT_BOUNDARY_SENTENCE_END) ? "sentence (end)" : "unknown")))))),
			offset, vte_char_attr_list_get_size(&priv->snapshot_attributes));
	g_assert(priv->snapshot_text != NULL);
	g_assert(priv->snapshot_characters != NULL);
	if (offset >= (int) priv->snapshot_characters->len) {
		return g_strdup("");
	}
	g_assert(offset < (int) priv->snapshot_characters->len);
	g_assert(offset >= 0);

	switch (boundary_type) {
		case ATK_TEXT_BOUNDARY_CHAR:
			/* We're either looking at the character at this
			 * position, the one before it, or the one after it. */
			offset += direction;
			start = MAX(offset, 0);
			end = MIN(offset + 1, (int) vte_char_attr_list_get_size(&priv->snapshot_attributes));
			break;
		case ATK_TEXT_BOUNDARY_WORD_START:
			/* Back up to the previous non-word-word transition. */
			while (offset > 0) {
				prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
				if (impl->is_word_char(prev)) {
					offset--;
				} else {
					break;
				}
			}
			start = offset;
			/* If we started in a word and we're looking for the
			 * word before this one, keep searching by backing up
			 * to the previous non-word character and then searching
			 * for the word-start before that. */
			if (direction == direction_previous) {
				while (offset > 0) {
					prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
					if (!impl->is_word_char(prev)) {
						offset--;
					} else {
						break;
					}
				}
				while (offset > 0) {
					prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
					if (impl->is_word_char(prev)) {
						offset--;
					} else {
						break;
					}
				}
				start = offset;
			}
			/* If we're looking for the word after this one,
			 * search forward by scanning forward for the next
			 * non-word character, then the next word character
			 * after that. */
			if (direction == direction_next) {
				while (offset < (int) priv->snapshot_characters->len) {
					next = vte_terminal_accessible_get_character_at_offset(text, offset);
					if (impl->is_word_char(next)) {
						offset++;
					} else {
						break;
					}
				}
				while (offset < (int) priv->snapshot_characters->len) {
					next = vte_terminal_accessible_get_character_at_offset(text, offset);
					if (!impl->is_word_char(next)) {
						offset++;
					} else {
						break;
					}
				}
				start = offset;
			}
			/* Now find the end of this word. */
			while (offset < (int) priv->snapshot_characters->len) {
				current = vte_terminal_accessible_get_character_at_offset(text, offset);
				if (impl->is_word_char(current)) {
					offset++;
				} else {
					break;
				}

			}
			/* Now find the next non-word-word transition */
			while (offset < (int) priv->snapshot_characters->len) {
				next = vte_terminal_accessible_get_character_at_offset(text, offset);
				if (!impl->is_word_char(next)) {
					offset++;
				} else {
					break;
				}
			}
			end = offset;
			break;
		case ATK_TEXT_BOUNDARY_WORD_END:
			/* Back up to the previous word-non-word transition. */
			current = vte_terminal_accessible_get_character_at_offset(text, offset);
			while (offset > 0) {
				prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
				if (impl->is_word_char(prev) &&
				    !impl->is_word_char(current)) {
					break;
				} else {
					offset--;
					current = prev;
				}
			}
			start = offset;
			/* If we're looking for the word end before this one, 
			 * keep searching by backing up to the previous word 
			 * character and then searching for the word-end 
			 * before that. */
			if (direction == direction_previous) {
				while (offset > 0) {
					prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
					if (impl->is_word_char(prev)) {
						offset--;
					} else {
						break;
					}
				}
				current = vte_terminal_accessible_get_character_at_offset(text, offset);
				while (offset > 0) {
					prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
					if (impl->is_word_char(prev) &&
					    !impl->is_word_char(current)) {
						break;
					} else {
						offset--;
						current = prev;
					}
				}
				start = offset;
			}
			/* If we're looking for the word end after this one,
			 * search forward by scanning forward for the next
			 * word character, then the next non-word character
			 * after that. */
			if (direction == direction_next) {
				while (offset < (int) priv->snapshot_characters->len) {
					current = vte_terminal_accessible_get_character_at_offset(text, offset);
					if (!impl->is_word_char(current)) {
						offset++;
					} else {
						break;
					}
				}
				while (offset < (int) priv->snapshot_characters->len) {
					current = vte_terminal_accessible_get_character_at_offset(text, offset);
					if (impl->is_word_char(current)) {
						offset++;
					} else {
						break;
					}
				}
				start = offset;
			}
			/* Now find the next word end. */
			while (offset < (int) priv->snapshot_characters->len) {
				current = vte_terminal_accessible_get_character_at_offset(text, offset);
				if (!impl->is_word_char(current)) {
					offset++;
				} else {
					break;
				}
			}
			while (offset < (int) priv->snapshot_characters->len) {
				current = vte_terminal_accessible_get_character_at_offset(text, offset);
				if (impl->is_word_char(current)) {
					offset++;
				} else {
					break;
				}
			}
			end = offset;
			break;
		case ATK_TEXT_BOUNDARY_LINE_START:
		case ATK_TEXT_BOUNDARY_LINE_END:
			/* Figure out which line we're on.  If the start of the
			 * i'th line is before the offset, then i could be the
			 * line we're looking for. */
			line = 0;
			for (line = 0;
			     line < priv->snapshot_linebreaks->len;
			     line++) {
				if (g_array_index(priv->snapshot_linebreaks,
						  int, line) > offset) {
					line--;
					break;
				}
			}
			_vte_debug_print(VTE_DEBUG_ALLY,
					"Character %d is on line %d.\n",
					offset, line);
			/* Perturb the line number to handle before/at/after. */
			line += direction;
			line = MIN(line, priv->snapshot_linebreaks->len - 1);
			/* Read the offsets for this line. */
			start = g_array_index(priv->snapshot_linebreaks,
						      int, line);
			line++;
			line = MIN(line, priv->snapshot_linebreaks->len - 1);
			end = g_array_index(priv->snapshot_linebreaks,
						    int, line);
			_vte_debug_print(VTE_DEBUG_ALLY,
					"Line runs from %d to %d.\n",
					start, end);
			break;
		case ATK_TEXT_BOUNDARY_SENTENCE_START:
		case ATK_TEXT_BOUNDARY_SENTENCE_END:
			/* This doesn't make sense.  Fall through. */
		default:
			start = end = 0;
			break;
	}
	*start_offset = start = MIN(start, priv->snapshot_characters->len - 1);
	*end_offset = end = CLAMP(end, start, priv->snapshot_characters->len);
	return vte_terminal_accessible_get_text(text, start, end);
}

static gchar *
vte_terminal_accessible_get_text_before_offset(AtkText *text, gint offset,
					       AtkTextBoundary boundary_type,
					       gint *start_offset,
					       gint *end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	return vte_terminal_accessible_get_text_somewhere(text,
							  offset,
							  boundary_type,
							  direction_previous,
							  start_offset,
							  end_offset);
}

static gchar *
vte_terminal_accessible_get_text_after_offset(AtkText *text, gint offset,
					      AtkTextBoundary boundary_type,
					      gint *start_offset,
					      gint *end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	return vte_terminal_accessible_get_text_somewhere(text,
							  offset,
							  boundary_type,
							  direction_next,
							  start_offset,
							  end_offset);
}

static gchar *
vte_terminal_accessible_get_text_at_offset(AtkText *text, gint offset,
					   AtkTextBoundary boundary_type,
					   gint *start_offset,
					   gint *end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	return vte_terminal_accessible_get_text_somewhere(text,
							  offset,
							  boundary_type,
							  direction_current,
							  start_offset,
							  end_offset);
}

static gunichar
vte_terminal_accessible_get_character_at_offset(AtkText *text, gint offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
#ifndef G_DISABLE_ASSERT
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
#endif
	char *unichar;
	gunichar ret;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);

	g_assert(offset < (int) priv->snapshot_characters->len);

	unichar = vte_terminal_accessible_get_text(text, offset, offset + 1);
	ret = g_utf8_get_char(unichar);
	g_free(unichar);

	return ret;
}

static gint
vte_terminal_accessible_get_caret_offset(AtkText *text)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);

	return priv->snapshot_caret;
}

static AtkAttributeSet *
get_attribute_set (struct _VteCharAttributes attr)
{
	AtkAttributeSet *set = NULL;
	AtkAttribute *at;

	if (attr.underline) {
		at = g_new (AtkAttribute, 1);
		at->name = g_strdup ("underline");
		at->value = g_strdup ("true");
		set = g_slist_append (set, at);
	}
	if (attr.strikethrough) {
		at = g_new (AtkAttribute, 1);
		at->name = g_strdup ("strikethrough");
		at->value = g_strdup ("true");
		set = g_slist_append (set, at);
	}
	at = g_new (AtkAttribute, 1);
	at->name = g_strdup ("fg-color");
	at->value = g_strdup_printf ("%u,%u,%u",
				     attr.fore.red, attr.fore.green, attr.fore.blue);
	set = g_slist_append (set, at);

	at = g_new (AtkAttribute, 1);
	at->name = g_strdup ("bg-color");
	at->value = g_strdup_printf ("%u,%u,%u",
				     attr.back.red, attr.back.green, attr.back.blue);
	set = g_slist_append (set, at);

	return set;
}

static gboolean
_pango_color_equal(const PangoColor *a,
                   const PangoColor *b)
{
        return a->red   == b->red &&
               a->green == b->green &&
               a->blue  == b->blue;
}

static AtkAttributeSet *
vte_terminal_accessible_get_run_attributes(AtkText *text, gint offset,
					   gint *start_offset, gint *end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	guint i;
	struct _VteCharAttributes cur_attr;
	struct _VteCharAttributes attr;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);

        attr = *vte_char_attr_list_get(&priv->snapshot_attributes, offset);
	*start_offset = 0;
	for (i = offset; i--;) {
                cur_attr = *vte_char_attr_list_get(&priv->snapshot_attributes, i);
		if (!_pango_color_equal (&cur_attr.fore, &attr.fore) ||
		    !_pango_color_equal (&cur_attr.back, &attr.back) ||
		    cur_attr.underline != attr.underline ||
		    cur_attr.strikethrough != attr.strikethrough) {
			*start_offset = i + 1;
			break;
		}
	}
	*end_offset = vte_char_attr_list_get_size(&priv->snapshot_attributes) - 1;
	for (i = offset + 1; i < vte_char_attr_list_get_size(&priv->snapshot_attributes); i++) {
                cur_attr = *vte_char_attr_list_get(&priv->snapshot_attributes, i);
		if (!_pango_color_equal (&cur_attr.fore, &attr.fore) ||
		    !_pango_color_equal (&cur_attr.back, &attr.back) ||
		    cur_attr.underline != attr.underline ||
		    cur_attr.strikethrough != attr.strikethrough) {
			*end_offset = i - 1;
			break;
		}
	}

	return get_attribute_set (attr);
}

static AtkAttributeSet *
vte_terminal_accessible_get_default_attributes(AtkText *text)
{
	return NULL;
}

static void
vte_terminal_accessible_get_character_extents(AtkText *text, gint offset,
					      gint *x, gint *y,
					      gint *width, gint *height,
					      AtkCoordType coords)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	VteTerminal *terminal;
	glong cell_width, cell_height;
	gint base_x, base_y, w, h;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	terminal = VTE_TERMINAL (gtk_accessible_get_widget (GTK_ACCESSIBLE (text)));

	atk_component_get_extents (ATK_COMPONENT (text), &base_x, &base_y, &w, &h, coords);
	xy_from_offset (priv, offset, x, y);
	cell_width = vte_terminal_get_char_width (terminal);
	cell_height = vte_terminal_get_char_height (terminal);
	*x *= cell_width;
	*y *= cell_height;
	*width = cell_width;
	*height = cell_height;
	*x += base_x;
	*y += base_y;
}

static gint
vte_terminal_accessible_get_character_count(AtkText *text)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);

	return vte_char_attr_list_get_size(&priv->snapshot_attributes);
}

static gint
vte_terminal_accessible_get_offset_at_point(AtkText *text,
					    gint x, gint y,
					    AtkCoordType coords)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	VteTerminal *terminal;
	glong cell_width, cell_height;
	gint base_x, base_y, w, h;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	terminal = VTE_TERMINAL (gtk_accessible_get_widget (GTK_ACCESSIBLE (text)));

	atk_component_get_extents (ATK_COMPONENT (text), &base_x, &base_y, &w, &h, coords);
	cell_width = vte_terminal_get_char_width (terminal);
	cell_height = vte_terminal_get_char_height (terminal);
	x -= base_x;
	y -= base_y;
	x /= cell_width;
	y /= cell_height;
	return offset_from_xy (priv, x, y);
}

static gint
vte_terminal_accessible_get_n_selections(AtkText *text)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	GtkWidget *widget;
	VteTerminal *terminal;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE(text));
	if (widget == NULL) {
		/* State is defunct */
		return -1;
	}

	terminal = VTE_TERMINAL (widget);
	return (vte_terminal_get_has_selection (terminal)) ? 1 : 0;
}

static gchar *
vte_terminal_accessible_get_selection(AtkText *text, gint selection_number,
				      gint *start_offset, gint *end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	GtkWidget *widget;

	if (selection_number != 0)
		return NULL;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE(text));
	if (widget == NULL) {
		/* State is defunct */
		return NULL;
	}

        try {
                auto impl = IMPL_FROM_WIDGET(widget);
                if (impl->m_selection_resolved.empty() || impl->m_selection[vte::to_integral(vte::platform::ClipboardType::PRIMARY)] == nullptr)
                        return nullptr;

                *start_offset = offset_from_xy (priv, impl->m_selection_resolved.start_column(), impl->m_selection_resolved.start_row());
                *end_offset = offset_from_xy (priv, impl->m_selection_resolved.end_column(), impl->m_selection_resolved.end_row());

                return g_strdup(impl->m_selection[vte::to_integral(vte::platform::ClipboardType::PRIMARY)]->str);
        } catch (...) {
                return nullptr;
        }
}

static gboolean
vte_terminal_accessible_add_selection(AtkText *text,
				      gint start_offset, gint end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);
	GtkWidget *widget;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE(text));
	if (widget == NULL) {
		/* State is defunct */
		return FALSE;
	}

	int start_x, start_y, end_x, end_y;
	xy_from_offset (priv, start_offset, &start_x, &start_y);
	xy_from_offset (priv, end_offset, &end_x, &end_y);
	IMPL_FROM_WIDGET(widget)->select_text(start_x, start_y, end_x, end_y);
	return TRUE;
}

static gboolean
vte_terminal_accessible_remove_selection(AtkText *text,
					 gint selection_number)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	GtkWidget *widget;
	VteTerminal *terminal;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE(text));
	if (widget == NULL) {
		/* State is defunct */
		return FALSE;
	}

	terminal = VTE_TERMINAL (widget);
        auto impl = IMPL_FROM_WIDGET(widget);
	if (selection_number == 0 && vte_terminal_get_has_selection (terminal)) {
                try {
                        impl->deselect_all();
                } catch (...) {
                }
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean
vte_terminal_accessible_set_selection(AtkText *text, gint selection_number,
				      gint start_offset, gint end_offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);
	GtkWidget *widget;
	VteTerminal *terminal;

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE(text));
	if (widget == NULL) {
		/* State is defunct */
		return FALSE;
	}

	terminal = VTE_TERMINAL (widget);
        auto impl = IMPL_FROM_WIDGET(widget);
	if (selection_number != 0) {
		return FALSE;
	}
	if (vte_terminal_get_has_selection (terminal)) {
                try {
                        impl->deselect_all();
                } catch (...) {
                }
	}

	return vte_terminal_accessible_add_selection (text, start_offset, end_offset);
}

static gboolean
vte_terminal_accessible_set_caret_offset(AtkText *text, gint offset)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(text);

	vte_terminal_accessible_update_private_data_if_needed(accessible,
							      NULL, NULL);
	/* Whoa, very not allowed. */
	return FALSE;
}

static void
vte_terminal_accessible_text_iface_init(AtkTextIface *text)
{
	text->get_text = vte_terminal_accessible_get_text;
	text->get_text_after_offset = vte_terminal_accessible_get_text_after_offset;
	text->get_text_at_offset = vte_terminal_accessible_get_text_at_offset;
	text->get_character_at_offset = vte_terminal_accessible_get_character_at_offset;
	text->get_text_before_offset = vte_terminal_accessible_get_text_before_offset;
	text->get_caret_offset = vte_terminal_accessible_get_caret_offset;
	text->get_run_attributes = vte_terminal_accessible_get_run_attributes;
	text->get_default_attributes = vte_terminal_accessible_get_default_attributes;
	text->get_character_extents = vte_terminal_accessible_get_character_extents;
	text->get_character_count = vte_terminal_accessible_get_character_count;
	text->get_offset_at_point = vte_terminal_accessible_get_offset_at_point;
	text->get_n_selections = vte_terminal_accessible_get_n_selections;
	text->get_selection = vte_terminal_accessible_get_selection;
	text->add_selection = vte_terminal_accessible_add_selection;
	text->remove_selection = vte_terminal_accessible_remove_selection;
	text->set_selection = vte_terminal_accessible_set_selection;
	text->set_caret_offset = vte_terminal_accessible_set_caret_offset;
}

static gboolean
vte_terminal_accessible_set_extents(AtkComponent *component,
				    gint x, gint y,
				    gint width, gint height,
				    AtkCoordType coord_type)
{
	/* FIXME?  We can change the size, but our position is controlled
	 * by the parent container. */
	return FALSE;
}

static gboolean
vte_terminal_accessible_set_position(AtkComponent *component,
				     gint x, gint y,
				     AtkCoordType coord_type)
{
	/* Controlled by the parent container, if there is one. */
	return FALSE;
}

static gboolean
vte_terminal_accessible_set_size(AtkComponent *component,
				 gint width, gint height)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(component);
	GtkWidget *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE(accessible));
	if (widget == NULL)
		return FALSE;

        VteTerminal *terminal = VTE_TERMINAL(widget);
        auto impl = IMPL(terminal);

        /* If the size is an exact multiple of the cell size, use that,
         * otherwise round down. */
        try {
                width -= impl->m_border.left + impl->m_border.right;
                height -= impl->m_border.top + impl->m_border.bottom;

                auto columns = width / impl->m_cell_width;
                auto rows = height / impl->m_cell_height;
                if (columns <= 0 || rows <= 0)
                        return FALSE;

                vte_terminal_set_size(terminal, columns, rows);
                return (vte_terminal_get_row_count (terminal) == rows) &&
                        (vte_terminal_get_column_count (terminal) == columns);
        } catch (...) {
                return false;
        }
}

static AtkObject *
vte_terminal_accessible_ref_accessible_at_point(AtkComponent *component,
						gint x, gint y,
						AtkCoordType coord_type)
{
	/* There are no children. */
	return NULL;
}

static void
vte_terminal_accessible_component_iface_init(AtkComponentIface *component)
{
	component->ref_accessible_at_point = vte_terminal_accessible_ref_accessible_at_point;
	component->set_extents = vte_terminal_accessible_set_extents;
	component->set_position = vte_terminal_accessible_set_position;
	component->set_size = vte_terminal_accessible_set_size;
}

/* AtkAction interface */

static gboolean
vte_terminal_accessible_do_action (AtkAction *accessible, int i)
{
	GtkWidget *widget;
	gboolean retval = FALSE;

	g_return_val_if_fail (i < LAST_ACTION, FALSE);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return FALSE;
	}

        switch (i) {
        case ACTION_MENU :
		g_signal_emit_by_name (widget, "popup_menu", &retval);
                break;
        default :
                g_warning ("Invalid action passed to VteTerminalAccessible::do_action");
                return FALSE;
        }
        return retval;
}

static int
vte_terminal_accessible_get_n_actions (AtkAction *accessible)
{
	return LAST_ACTION;
}

static const char *
vte_terminal_accessible_action_get_description (AtkAction *action, int i)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(action);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);

        g_return_val_if_fail (i < LAST_ACTION, NULL);

        if (priv->action_descriptions[i]) {
                return priv->action_descriptions[i];
        } else {
                return vte_terminal_accessible_action_descriptions[i];
        }
}

static const char *
vte_terminal_accessible_action_get_name (AtkAction *accessible, int i)
{
        g_return_val_if_fail (i < LAST_ACTION, NULL);

        return vte_terminal_accessible_action_names[i];
}

static const char *
vte_terminal_accessible_action_get_keybinding (AtkAction *accessible, int i)
{
        g_return_val_if_fail (i < LAST_ACTION, NULL);

        return NULL;
}

static gboolean
vte_terminal_accessible_action_set_description (AtkAction *action,
                                                int i,
                                                const char *description)
{
        VteTerminalAccessible *accessible = VTE_TERMINAL_ACCESSIBLE(action);
	VteTerminalAccessiblePrivate *priv = (VteTerminalAccessiblePrivate *)_vte_terminal_accessible_get_instance_private(accessible);

        g_return_val_if_fail (i < LAST_ACTION, FALSE);

        if (priv->action_descriptions[i]) {
                g_free (priv->action_descriptions[i]);
        }
        priv->action_descriptions[i] = g_strdup (description);

        return TRUE;
}

static void
vte_terminal_accessible_action_iface_init(AtkActionIface *action)
{
	action->do_action = vte_terminal_accessible_do_action;
	action->get_n_actions = vte_terminal_accessible_get_n_actions;
	action->get_description = vte_terminal_accessible_action_get_description;
	action->get_name = vte_terminal_accessible_action_get_name;
	action->get_keybinding = vte_terminal_accessible_action_get_keybinding;
	action->set_description = vte_terminal_accessible_action_set_description;
}

static void
_vte_terminal_accessible_class_init(VteTerminalAccessibleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

	gobject_class->finalize = vte_terminal_accessible_finalize;

	atk_object_class->initialize = vte_terminal_accessible_initialize;
}

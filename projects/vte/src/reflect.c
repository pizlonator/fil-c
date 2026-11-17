/*
 * Copyright (C) 2003 Red Hat, Inc.
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
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <atk/atk.h>
#ifdef USE_VTE
#include <vte/vte.h>
#endif

static GArray *contents = NULL;

#ifdef USE_TEXT_VIEW
/*
 * Implementation for a TextView widget.
 */
static void
terminal_init_text_view(GtkWidget **widget)
{
	*widget = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(*widget), TRUE);
}
static void
terminal_shell_text_view(GtkWidget *widget)
{
	/* no-op */
}
static GtkAdjustment *
terminal_adjustment_text_view(GtkWidget *terminal)
{
        return gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(terminal));
}
#endif
#ifdef USE_VTE
/*
 * Implementation for a VteTerminal widget.
 */
static void
terminal_init_vte(GtkWidget **terminal)
{
	*terminal = vte_terminal_new();
	g_signal_connect(G_OBJECT(*terminal), "eof",
			 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(*terminal), "child-exited",
			 G_CALLBACK(gtk_main_quit), NULL);
}
static void
terminal_shell_vte(GtkWidget *terminal)
{
        char *argv[2];

        argv[0] = vte_get_user_shell ();
        argv[1] = NULL;
	vte_terminal_spawn_sync(VTE_TERMINAL(terminal),
                                       VTE_PTY_DEFAULT,
                                       g_get_home_dir() ? g_get_home_dir() : NULL,
                                       argv,
                                       NULL,
                                       0, NULL, NULL,
                                       NULL,
                                       NULL,
                                       NULL);
}
#endif

/*
 * Update the contents of the widget with the data from our contents array.
 */
static void
update_contents(AtkObject *obj, GtkWidget *widget)
{
	guint caret, i;
	GString *s;

	caret = (guint)atk_text_get_caret_offset(ATK_TEXT(obj));
	s = g_string_new(NULL);
	for (i = 0; i < contents->len; i++) {
		if (i == caret) {
			s = g_string_append(s, "[CARET]");
		}
		s = g_string_append_unichar(s,
					    g_array_index(contents,
							  gunichar,
							  i));
	}
	if (i == caret) {
		s = g_string_append(s, "[CARET]");
	}
	if (GTK_IS_LABEL(widget)) {
		gtk_label_set_text(GTK_LABEL(widget), s->str);
		gtk_label_set_selectable(GTK_LABEL(widget),
					 atk_text_get_n_selections(ATK_TEXT(obj)) > 0);
		if (gtk_label_get_selectable(GTK_LABEL(widget))) {
			int selection_start, selection_end;
			atk_text_get_selection(ATK_TEXT(obj), 0,
					       &selection_start,
					       &selection_end);
			gtk_label_select_region(GTK_LABEL(widget),
						selection_start, selection_end);
		}
	}
	g_string_free(s, TRUE);
}

/* Handle inserted text by inserting the text into our gunichar array. */
static void
text_changed_insert(AtkObject *obj, gint offset, gint length, gpointer data)
{
	char *inserted, *p;
	gunichar c;
	int i;

	inserted = atk_text_get_text(ATK_TEXT(obj), offset, offset + length);

	if (!g_utf8_validate(inserted, -1, NULL)) {
		g_free(inserted);
		g_error("UTF-8 validation error");
		return;
	}

	p = inserted;
	i = 0;
	while (i < length) {
		c = g_utf8_get_char(p);
		if ((guint)(offset + i) >= contents->len) {
			g_array_append_val(contents, c);
		} else {
			g_array_insert_val(contents, offset + i, c);
		}
		i++;
		p = g_utf8_next_char(p);
	}

#if VTE_DEBUG
	if ((getenv("REFLECT_VERBOSE") != NULL) &&
	    (atol(getenv("REFLECT_VERBOSE")) != 0)) {
		g_printerr("Inserted %d chars ('%.*s') at %d,",
			length, (int)(p - inserted), inserted, offset);
		g_printerr(" buffer contains %d characters.\n",
			contents->len);
	}
#endif

	g_free(inserted);

	update_contents(obj, GTK_WIDGET(data));
}

/* Handle deleted text by removing the text from our gunichar array. */
static void
text_changed_delete(AtkObject *obj, gint offset, gint length, gpointer data)
{
	int i;
	for (i = offset + length - 1; i >= offset; i--) {
		if ((guint)i > contents->len - 1) {
			g_warning("Invalid character %d was deleted.\n", i);
		}
		g_array_remove_index(contents, i);
	}
#if VTE_DEBUG
	if ((getenv("REFLECT_VERBOSE") != NULL) &&
	    (atol(getenv("REFLECT_VERBOSE")) != 0)) {
		g_printerr("Deleted %d chars at %d.\n", length, offset);
	}
#endif
	update_contents(obj, GTK_WIDGET(data));
}

static void
text_caret_moved(AtkObject *obj, gint offset, gpointer data)
{
	update_contents(obj, GTK_WIDGET(data));
}

static void
text_selection_changed(AtkObject *obj, gpointer data)
{
	update_contents(obj, GTK_WIDGET(data));
}

/* Wrapper versions. */
static void
terminal_init(GtkWidget **terminal)
{
	*terminal = NULL;
#ifdef USE_TEXT_VIEW
	terminal_init_text_view(terminal);
	return;
#endif
#ifdef USE_VTE
	terminal_init_vte(terminal);
	return;
#endif
	g_assert_not_reached();
}
static void
terminal_shell(GtkWidget *terminal)
{
#ifdef USE_TEXT_VIEW
	terminal_shell_text_view(terminal);
	return;
#endif
#ifdef USE_VTE
	terminal_shell_vte(terminal);
	return;
#endif
	g_assert_not_reached();
}
static GtkAdjustment *
terminal_adjustment(GtkWidget *terminal)
{
#ifdef USE_TEXT_VIEW
	return terminal_adjustment_text_view(terminal);
#endif
#ifdef USE_VTE
	return gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(terminal));
#endif
	g_assert_not_reached();
}

int
main(int argc, char **argv)
{
	GtkWidget *label, *terminal, *tophalf, *pane, *window, *sw;
	AtkObject *obj;
	char *text, *p;
	gunichar c;
	guint count;

	gtk_init(&argc, &argv);

	contents = g_array_new(TRUE, FALSE, sizeof(gunichar));

	terminal_init(&terminal);

#ifdef USE_TEXT_VIEW
	tophalf = gtk_scrolled_window_new(NULL, terminal_adjustment(terminal));
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tophalf),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(tophalf), terminal);
#else
        tophalf = gtk_grid_new();

        gtk_grid_attach(GTK_GRID(tophalf), terminal, 0, 0, 1, 1);
	gtk_widget_show(terminal);

        GtkWidget* scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL,
                                                 terminal_adjustment(terminal));
        gtk_grid_attach(GTK_GRID(tophalf), scrollbar, 1, 0, 1, 1);
	gtk_widget_show(scrollbar);
#endif
	gtk_widget_show(terminal);

	label = gtk_label_new("");
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_label_set_yalign(GTK_LABEL(label), 0.0);

	sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_add(GTK_CONTAINER(sw), label);
	gtk_widget_show(label);

	pane = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_paned_pack1(GTK_PANED(pane), tophalf, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(pane), sw, TRUE, FALSE);
	gtk_widget_show(tophalf);
	gtk_widget_show(sw);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_main_quit), NULL);
	gtk_container_add(GTK_CONTAINER(window), pane);
	gtk_widget_show(pane);

	obj = gtk_widget_get_accessible(terminal);
	g_assert(obj != NULL);
	g_signal_connect(G_OBJECT(obj), "text-changed::insert",
			 G_CALLBACK(text_changed_insert), label);
	g_signal_connect(G_OBJECT(obj), "text-changed::delete",
			 G_CALLBACK(text_changed_delete), label);
	g_signal_connect(G_OBJECT(obj), "text-caret-moved",
			 G_CALLBACK(text_caret_moved), label);
	g_signal_connect(G_OBJECT(obj), "text-selection-changed",
			 G_CALLBACK(text_selection_changed), label);

	count = (guint)atk_text_get_character_count(ATK_TEXT(obj));
	if (count > 0) {
		text = atk_text_get_text(ATK_TEXT(obj), 0, count);
		if (text != NULL) {
			for (p = text;
			     contents->len < count;
			     p = g_utf8_next_char(p)) {
				c = g_utf8_get_char(p);
				g_array_append_val(contents, c);
			}
			g_free(text);
		}
	}
	terminal_shell(terminal);

	gtk_window_set_default_size(GTK_WINDOW(window), 600, 450);
	gtk_widget_show(window);

	update_contents(obj, terminal);

	gtk_main();

	g_array_free(contents, TRUE);
	contents = NULL;

	return 0;
}

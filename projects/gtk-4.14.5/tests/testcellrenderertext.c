/* GTK - The GIMP Toolkit
 * testcellrenderertext.c: Tests for the various properties of GtkCellRendererText
 * Copyright (C) 2005, Novell, Inc.
 *
 * Authors:
 *   Federico Mena-Quintero <federico@novell.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define COL_BACKGROUND 15
#define COL_LINE_NUM   16
#define NUM_COLS       17 /* change this when adding columns */

struct cell_params {
  const char *description;			/* 0 */
  const char *test;				/* 1 */
  int xpad;				/* 2 */
  int ypad;				/* 3 */
  double xalign;			/* 4 */
  double yalign;			/* 5 */
  gboolean sensitive;			/* 6 */
  int width;				/* 7 */
  int height;				/* 8 */
  int width_chars;			/* 9 */
  int wrap_width;			/* 10 */
  PangoWrapMode wrap_mode;		/* 11 */
  gboolean single_paragraph_mode;	/* 12 */
  PangoEllipsizeMode ellipsize;		/* 13 */
  PangoAlignment alignment;			/* 14 */
  /* COL_BACKGROUND	 */		/* 15 */
  /* COL_LINE_NUM */			/* 16 */
};

#define WO PANGO_WRAP_WORD
#define CH PANGO_WRAP_CHAR
#define WC PANGO_WRAP_WORD_CHAR

#define NO PANGO_ELLIPSIZE_NONE
#define ST PANGO_ELLIPSIZE_START
#define MI PANGO_ELLIPSIZE_MIDDLE
#define EN PANGO_ELLIPSIZE_END

#define AL PANGO_ALIGN_LEFT
#define AC PANGO_ALIGN_CENTER
#define AR PANGO_ALIGN_RIGHT

#define TESTL "LEFT JUSTIFIED This is really truly verily some very long text\n\330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205 \330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205 \330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205"

#define TESTC "CENTERED This is really truly verily some very long text\n\330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205 \330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205 \330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205"

#define TESTR "RIGHT JUSTIFIED This is really truly verily some very long text\n\330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205 \330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205 \330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205"


/* DO NOT CHANGE THE ROWS!  They are numbered so that we can refer to
 * problematic rows in bug reports.  If you need a different test, just add a
 * new row at the bottom.  Also, please add your new row numbers to this column -------------------------------+
 * to keep things tidy.                                                                                        v
 */
static const struct cell_params cell_params[] = {
  { "xp yp xa ya se wi he wc ww wm sp el", "",    0,  0, 0.0, 0.5, TRUE,  -1, -1, -1, -1, CH, FALSE, NO },  /* 0 */

  /* Test alignment */

  { "0  0  0  0  T  -1 -1 -1 -1 CH F  NO", TESTL,  0,  0, 0.0, 0.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 1 */
  { "0  0  .5 0  T  -1 -1 -1 -1 CH F  NO", TESTC,  0,  0, 0.5, 0.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 2 */
  { "0  0  1  0  T  -1 -1 -1 -1 CH F  NO", TESTR,  0,  0, 1.0, 0.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 3 */
  { "0  0  0  .5 T  -1 -1 -1 -1 CH F  NO", TESTL,  0,  0, 0.0, 0.5, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 4 */
  { "0  0  .5 .5 T  -1 -1 -1 -1 CH F  NO", TESTC,  0,  0, 0.5, 0.5, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 5 */
  { "0  0  1  .5 T  -1 -1 -1 -1 CH F  NO", TESTR,  0,  0, 1.0, 0.5, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 6 */
  { "0  0  0  1  T  -1 -1 -1 -1 CH F  NO", TESTL,  0,  0, 0.0, 1.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 7 */
  { "0  0  .5 1  T  -1 -1 -1 -1 CH F  NO", TESTC,  0,  0, 0.5, 1.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 8 */
  { "0  0  1  1  T  -1 -1 -1 -1 CH F  NO", TESTR,  0,  0, 1.0, 1.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 9 */

  /* Test padding */

  { "10 10 0  0  T  -1 -1 -1 -1 CH F  NO", TESTL, 10, 10, 0.0, 0.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 10 */
  { "10 10 .5 0  T  -1 -1 -1 -1 CH F  NO", TESTC, 10, 10, 0.5, 0.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 11 */
  { "10 10 1  0  T  -1 -1 -1 -1 CH F  NO", TESTR, 10, 10, 1.0, 0.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 12 */
  { "10 10 0  .5 T  -1 -1 -1 -1 CH F  NO", TESTL, 10, 10, 0.0, 0.5, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 13 */
  { "10 10 .5 .5 T  -1 -1 -1 -1 CH F  NO", TESTC, 10, 10, 0.5, 0.5, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 14 */
  { "10 10 1  .5 T  -1 -1 -1 -1 CH F  NO", TESTR, 10, 10, 1.0, 0.5, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 15 */
  { "10 10 0  1  T  -1 -1 -1 -1 CH F  NO", TESTL, 10, 10, 0.0, 1.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 16 */
  { "10 10 .5 1  T  -1 -1 -1 -1 CH F  NO", TESTC, 10, 10, 0.5, 1.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 17 */
  { "10 10 1  1  T  -1 -1 -1 -1 CH F  NO", TESTR, 10, 10, 1.0, 1.0, TRUE,  -1, -1, -1, -1, CH, FALSE, NO , AL }, /* 18 */

  /* Test Pango alignment (not xalign) */
  { "0  0  0  0  T  -1 -1 -1 -1 CH F  NO AL", TESTL,  0,  0, 0.0, 0.0, TRUE,  -1, -1, -1, 20, WO, FALSE, NO , AL }, /* 19 */
  { "0  0  0  0  T  -1 -1 -1 -1 CH F  NO AC", TESTC,  0,  0, 0.0, 0.0, TRUE,  -1, -1, -1, 20, WO, FALSE, NO , AC }, /* 20 */
  { "0  0  0  0  T  -1 -1 -1 -1 CH F  NO AR", TESTR,  0,  0, 0.0, 0.0, TRUE,  -1, -1, -1, 20, WO, FALSE, NO , AR }, /* 21 */
};

static GtkListStore *
create_list_store (void)
{
  GtkListStore *list_store;
  int i;

  list_store = gtk_list_store_new (NUM_COLS,
				   G_TYPE_STRING,		/* 0 */ 
				   G_TYPE_STRING,		/* 1 */ 
				   G_TYPE_INT,			/* 2 */ 
				   G_TYPE_INT,			/* 3 */ 
				   G_TYPE_DOUBLE,		/* 4 */ 
				   G_TYPE_DOUBLE,		/* 5 */ 
				   G_TYPE_BOOLEAN,		/* 6 */ 
				   G_TYPE_INT,			/* 7 */ 
				   G_TYPE_INT,			/* 8 */ 
				   G_TYPE_INT,			/* 9 */ 
				   G_TYPE_INT,			/* 10 */
				   PANGO_TYPE_WRAP_MODE,	/* 11 */
				   G_TYPE_BOOLEAN,		/* 12 */
				   PANGO_TYPE_ELLIPSIZE_MODE,	/* 13 */
				   PANGO_TYPE_ALIGNMENT,	/* 14 */
				   G_TYPE_STRING,		/* 15 */
				   G_TYPE_STRING);		/* 16 */

  for (i = 0; i < G_N_ELEMENTS (cell_params); i++)
    {
      const struct cell_params *p;
      GtkTreeIter iter;
      char buf[50];

      p = cell_params + i;

      g_snprintf (buf, sizeof (buf), "%d", i);

      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter,
			  0, p->description,
			  1, p->test,
			  2, p->xpad,
			  3, p->ypad,
			  4, p->xalign,
			  5, p->yalign,
			  6, p->sensitive,
			  7, p->width,
			  8, p->height,
			  9, p->width_chars,
			  10, p->wrap_width,
			  11, p->wrap_mode,
			  12, p->single_paragraph_mode,
			  13, p->ellipsize,
			  14, p->alignment,
			  15, (i % 2 == 0) ? "gray50" : "gray80",
			  16, buf,
			  -1);
    }

  return list_store;
}

static GtkWidget *
create_tree (gboolean rtl)
{
  GtkWidget *sw;
  GtkWidget *treeview;
  GtkListStore *list_store;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GdkPixbuf *pixbuf;

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_widget_set_direction (sw, rtl ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);

  list_store = create_list_store ();

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
  gtk_widget_set_direction (treeview, rtl ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), treeview);

  /* Line number */

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("#",
						     renderer,
						     "text", COL_LINE_NUM,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* Description */

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
		"font", "monospace",
		NULL);
  column = gtk_tree_view_column_new_with_attributes ("Description",
						     renderer,
						     "text", 0,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* Test text */

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Test",
						     renderer,
						     "text", 1,
						     "xpad", 2,
						     "ypad", 3,
						     "xalign", 4,
						     "yalign", 5,
						     "sensitive", 6,
						     "width", 7,
						     "height", 8,
						     "width_chars", 9,
						     "wrap_width", 10,
						     "wrap_mode", 11,
						     "single_paragraph_mode", 12,
						     "ellipsize", 13,
						     "alignment", 14,
						     "cell_background", 15,
						     NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* Empty column */

  pixbuf = gdk_pixbuf_new_from_file ("apple-red.png", NULL);

  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer,
		"pixbuf", pixbuf,
		"xpad", 10,
		"ypad", 10,
		NULL);
  column = gtk_tree_view_column_new_with_attributes ("Empty",
						     renderer,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  return sw;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *tree;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy",
		    G_CALLBACK (quit_cb), &done);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  /* LTR */

  label = gtk_label_new ("Left to right");
  gtk_box_append (GTK_BOX (vbox), label);

  tree = create_tree (FALSE);
  gtk_widget_set_vexpand (tree, TRUE);
  gtk_box_append (GTK_BOX (vbox), tree);

  /* RTL */

  label = gtk_label_new ("Right to left");
  gtk_box_append (GTK_BOX (vbox), label);

  tree = create_tree (TRUE);
  gtk_widget_set_vexpand (tree, TRUE);
  gtk_box_append (GTK_BOX (vbox), tree);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}

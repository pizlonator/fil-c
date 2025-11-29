  /* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */


#include "config.h"

#undef	G_LOG_DOMAIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

#include "gtk/gtk.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#ifdef G_OS_WIN32
#define sleep(n) _sleep(n)
#endif

#include "test.xpm"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static gboolean done = FALSE;

static gboolean
file_exists (const char *filename)
{
  struct stat statbuf;

  return stat (filename, &statbuf) == 0;
}

static GtkWidget *
build_option_menu (const char           *items[],
		   int              num_items,
		   int              history,
		   void           (*func) (GtkWidget *widget, gpointer data),
		   gpointer         data);

/* macro, structure and variables used by tree window demos */
#define DEFAULT_NUMBER_OF_ITEM  3
#define DEFAULT_RECURSION_LEVEL 3

struct {
  GSList* selection_mode_group;
  GtkWidget* single_button;
  GtkWidget* browse_button;
  GtkWidget* multiple_button;
  GtkWidget* draw_line_button;
  GtkWidget* view_line_button;
  GtkWidget* no_root_item_button;
  GtkWidget* nb_item_spinner;
  GtkWidget* recursion_spinner;
} sTreeSampleSelection;

typedef struct sTreeButtons {
  guint nb_item_add;
  GtkWidget* add_button;
  GtkWidget* remove_button;
  GtkWidget* subtree_button;
} sTreeButtons;
/* end of tree section */

static GtkWidget *
build_option_menu (const char           *items[],
		   int              num_items,
		   int              history,
		   void           (*func)(GtkWidget *widget, gpointer data),
		   gpointer         data)
{
  GtkWidget *omenu;
  int i;

  omenu = gtk_combo_box_text_new ();
  g_signal_connect (omenu, "changed",
		    G_CALLBACK (func), data);
      
  for (i = 0; i < num_items; i++)
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (omenu), items[i]);

  gtk_combo_box_set_active (GTK_COMBO_BOX (omenu), history);
  
  return omenu;
}

/*
 * Windows with an alpha channel
 */
static GtkWidget *
build_alpha_widgets (void)
{
  GtkWidget *grid;
  GtkWidget *radio_button;
  GtkWidget *check_button;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *group;

  grid = gtk_grid_new ();
  gtk_widget_set_vexpand (grid, TRUE);

  radio_button = gtk_check_button_new_with_label ("Red");
  gtk_widget_set_hexpand (radio_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), radio_button, 0, 0, 1, 1);
  group = radio_button;

  radio_button = gtk_check_button_new_with_label ("Green");
  gtk_widget_set_hexpand (radio_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), radio_button, 0, 1, 1, 1);
  gtk_check_button_set_group (GTK_CHECK_BUTTON (radio_button), GTK_CHECK_BUTTON (group));

  radio_button = gtk_check_button_new_with_label ("Blue"),
  gtk_widget_set_hexpand (radio_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), radio_button, 0, 2, 1, 1);
  gtk_check_button_set_group (GTK_CHECK_BUTTON (radio_button), GTK_CHECK_BUTTON (group));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (group), TRUE);

  check_button = gtk_check_button_new_with_label ("Sedentary"),
  gtk_widget_set_hexpand (check_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 0, 1, 1);

  check_button = gtk_check_button_new_with_label ("Nocturnal"),
  gtk_widget_set_hexpand (check_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 1, 1, 1);

  check_button = gtk_check_button_new_with_label ("Compulsive"),
  gtk_widget_set_hexpand (check_button, TRUE);
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 2, 1, 1);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<i>Entry: </i>");
  gtk_box_append (GTK_BOX (hbox), label);
  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_box_append (GTK_BOX (hbox), entry);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_grid_attach (GTK_GRID (grid), hbox, 0, 3, 2, 1);

  return grid;
}

static void
on_composited_changed (GdkDisplay *display,
                       GParamSpec *pspec,
                       GtkLabel   *label)
{
  gboolean is_composited = gdk_display_is_composited (display);

  if (is_composited)
    gtk_label_set_text (label, "Composited");
  else
    gtk_label_set_text (label, "Not composited");

  /* We draw a different background on the GdkSurface */
  gtk_widget_queue_draw (GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (label))));
}

static void
create_alpha_window (GtkWidget *widget)
{
  static GtkWidget *window;

  if (!window)
    {
      static GtkCssProvider *provider = NULL;
      GtkWidget *content_area;
      GtkWidget *vbox;
      GtkWidget *label;
      GdkDisplay *display;
      
      window = gtk_dialog_new_with_buttons ("Alpha Window",
					    GTK_WINDOW (gtk_widget_get_root (widget)), 0,
					    "_Close", 0,
					    NULL);
      gtk_widget_add_css_class (window, "alpha");
      if (provider == NULL)
        {
          provider = gtk_css_provider_new ();
          gtk_css_provider_load_from_data (provider,
                                           "dialog.alpha {\n"
                                           "  background: radial-gradient(ellipse at center, #FFBF00, #FFBF0000);\n"
                                           "}\n",
                                           -1);
          gtk_style_context_add_provider_for_display (gtk_widget_get_display (window),
                                                      GTK_STYLE_PROVIDER (provider),
                                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
          g_object_unref (provider);
        }

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_top (vbox, 12);
      gtk_widget_set_margin_bottom (vbox, 12);
      gtk_widget_set_margin_start (vbox, 12);
      gtk_widget_set_margin_end (vbox, 12);
      gtk_widget_set_vexpand (vbox, TRUE);
      label = gtk_label_new (NULL);
      gtk_box_append (GTK_BOX (content_area), vbox);
      gtk_box_append (GTK_BOX (vbox), label);
      display = gtk_widget_get_display (window);
      on_composited_changed (display, NULL, GTK_LABEL (label));
      g_signal_connect (display, "notify::composited", G_CALLBACK (on_composited_changed), label);

      gtk_box_append (GTK_BOX (vbox), build_alpha_widgets ());
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_window_destroy),
                        NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkButton
 */

static void
button_window (GtkWidget *widget,
              GtkWidget *button)
{
  gtk_widget_set_visible (button, !gtk_widget_get_visible (button));
}

static void
create_buttons (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *grid;
  GtkWidget *separator;
  GtkWidget *button[10];
  int button_x[9] = { 0, 1, 2, 0, 2, 1, 1, 2, 0 };
  int button_y[9] = { 0, 1, 2, 2, 0, 2, 0, 1, 1 };
  guint i;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkButton");

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      grid = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 5);

      gtk_widget_set_margin_top (grid, 10);
      gtk_widget_set_margin_bottom (grid, 10);
      gtk_widget_set_margin_start (grid, 10);
      gtk_widget_set_margin_end (grid, 10);
      gtk_box_append (GTK_BOX (box1), grid);

      button[0] = gtk_button_new_with_label ("button1");
      button[1] = gtk_button_new_with_mnemonic ("_button2");
      button[2] = gtk_button_new_with_mnemonic ("_button3");
      button[3] = gtk_button_new_with_mnemonic ("_button4");
      button[4] = gtk_button_new_with_label ("button5");
      button[5] = gtk_button_new_with_label ("button6");
      button[6] = gtk_button_new_with_label ("button7");
      button[7] = gtk_button_new_with_label ("button8");
      button[8] = gtk_button_new_with_label ("button9");

      for (i = 0; i < 9; i++)
        {
          g_signal_connect (button[i], "clicked",
                            G_CALLBACK (button_window),
                            button[(i + 1) % 9]);
          gtk_widget_set_hexpand (button[i], TRUE);
          gtk_widget_set_vexpand (button[i], TRUE);

          gtk_grid_attach (GTK_GRID (grid), button[i],
                           button_x[i], button_y[i] + 1, 1, 1);
        }

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button[9] = gtk_button_new_with_label ("close");
      gtk_widget_set_margin_top (button[9], 10);
      gtk_widget_set_margin_bottom (button[9], 10);
      gtk_widget_set_margin_start (button[9], 10);
      gtk_widget_set_margin_end (button[9], 10);
      g_signal_connect_swapped (button[9], "clicked",
				G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (box2), button[9]);
      gtk_window_set_default_widget (GTK_WINDOW (window), button[9]);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkToggleButton
 */

static void
create_toggle_buttons (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkToggleButton");

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_toggle_button_new_with_label ("button3");
      gtk_box_append (GTK_BOX (box2), button);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (box2), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

static GtkWidget *
create_widget_grid (gboolean group)
{
  GtkWidget *grid;
  GtkWidget *group_widget = NULL;
  int i, j;
  
  grid = gtk_grid_new ();
  
  for (i = 0; i < 5; i++)
    {
      for (j = 0; j < 5; j++)
	{
	  GtkWidget *widget;
	  char *tmp;
	  
	  if (i == 0 && j == 0)
	    {
	      widget = NULL;
	    }
	  else if (i == 0)
	    {
	      tmp = g_strdup_printf ("%d", j);
	      widget = gtk_label_new (tmp);
	      g_free (tmp);
	    }
	  else if (j == 0)
	    {
	      tmp = g_strdup_printf ("%c", 'A' + i - 1);
	      widget = gtk_label_new (tmp);
	      g_free (tmp);
	    }
	  else
	    {
	      widget = gtk_check_button_new ();
              if (group)
		{
		  if (!group_widget)
		    group_widget = widget;
		  else
                    gtk_check_button_set_group (GTK_CHECK_BUTTON (widget), GTK_CHECK_BUTTON (group_widget));
		}
	    }
	  
	  if (widget)
	    gtk_grid_attach (GTK_GRID (grid), widget, i, j, 1, 1);
	}
    }

  return grid;
}

/*
 * GtkCheckButton
 */

static void
create_check_buttons (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *table;
  
  if (!window)
    {
      window = gtk_dialog_new_with_buttons ("Check Buttons",
                                            NULL, 0,
                                            "_Close",
                                            GTK_RESPONSE_NONE,
                                            NULL);

      gtk_window_set_display (GTK_WINDOW (window), 
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_window_destroy),
                        NULL);

      box1 = gtk_dialog_get_content_area (GTK_DIALOG (window));

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_widget_set_hexpand (box2, TRUE);
      gtk_widget_set_margin_start (box2, 10);
      gtk_widget_set_margin_end (box2, 10);
      gtk_widget_set_margin_top (box2, 10);
      gtk_widget_set_margin_bottom (box2, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_check_button_new_with_mnemonic ("_button1");
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_check_button_new_with_label ("button2");
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_check_button_new_with_label ("button3");
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_check_button_new_with_label ("inconsistent");
      gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), TRUE);
      gtk_box_append (GTK_BOX (box2), button);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      table = create_widget_grid (FALSE);
      gtk_widget_set_vexpand (table, TRUE);
      gtk_box_append (GTK_BOX (box2), table);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

static void
create_radio_buttons (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *table;
  GtkWidget *group;

  if (!window)
    {
      window = gtk_dialog_new_with_buttons ("Radio Buttons",
                                            NULL, 0,
                                            "_Close",
                                            GTK_RESPONSE_NONE,
                                            NULL);

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_window_destroy),
                        NULL);

      box1 = gtk_dialog_get_content_area (GTK_DIALOG (window));

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_check_button_new_with_label ("button1");
      gtk_box_append (GTK_BOX (box2), button);
      group = button;

      button = gtk_check_button_new_with_label ("button2");
      gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);
      gtk_check_button_set_group (GTK_CHECK_BUTTON (button), GTK_CHECK_BUTTON (group));
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_check_button_new_with_label ("button3");
      gtk_check_button_set_group (GTK_CHECK_BUTTON (button), GTK_CHECK_BUTTON (group));
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_check_button_new_with_label ("inconsistent");
      gtk_check_button_set_group (GTK_CHECK_BUTTON (button), GTK_CHECK_BUTTON (group));
      gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), TRUE);
      gtk_box_append (GTK_BOX (box2), button);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_check_button_new_with_label ("button4");
      gtk_box_append (GTK_BOX (box2), button);
      group = button;

      button = gtk_check_button_new_with_label ("button5");
      gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);
      gtk_check_button_set_group (GTK_CHECK_BUTTON (button), GTK_CHECK_BUTTON (group));
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_check_button_new_with_label ("button6");
      gtk_check_button_set_group (GTK_CHECK_BUTTON (button), GTK_CHECK_BUTTON (group));
      gtk_box_append (GTK_BOX (box2), button);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      table = create_widget_grid (TRUE);
      gtk_box_append (GTK_BOX (box1), table);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkToolBar
 */

static GtkWidget*
new_pixbuf (const char *filename,
            GdkSurface *window)
{
  GtkWidget *widget;
  GdkPixbuf *pixbuf;

  if (strcmp (filename, "test.xpm") == 0)
    pixbuf = NULL;
  else
    pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

  if (pixbuf == NULL)
    pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) openfile);
  
  widget = gtk_image_new_from_pixbuf (pixbuf);

  g_object_unref (pixbuf);

  return widget;
}

/*
 * GtkStatusBar
 */

static guint statusbar_counter = 1;

static void
statusbar_push (GtkWidget *button,
		GtkStatusbar *statusbar)
{
  char text[1024];

  sprintf (text, "something %d", statusbar_counter++);

  gtk_statusbar_push (statusbar, 1, text);
}

static void
statusbar_push_long (GtkWidget *button,
                     GtkStatusbar *statusbar)
{
  char text[1024];

  sprintf (text, "Just because a system has menu choices written with English words, phrases or sentences, that is no guarantee, that it is comprehensible. Individual words may not be familiar to some users (for example, \"repaginate\"), and two menu items may appear to satisfy the users's needs, whereas only one does (for example, \"put away\" or \"eject\").");

  gtk_statusbar_push (statusbar, 1, text);
}

static void
statusbar_pop (GtkWidget *button,
	       GtkStatusbar *statusbar)
{
  gtk_statusbar_pop (statusbar, 1);
}

static void
statusbar_steal (GtkWidget *button,
	         GtkStatusbar *statusbar)
{
  gtk_statusbar_remove (statusbar, 1, 4);
}

static void
statusbar_popped (GtkStatusbar  *statusbar,
		  guint          context_id,
		  const char	*text)
{
  if (!text)
    statusbar_counter = 1;
}

static void
statusbar_contexts (GtkStatusbar *statusbar)
{
  const char *string;

  string = "any context";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
  
  string = "idle messages";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
  
  string = "some text";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));

  string = "hit the mouse";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));

  string = "hit the mouse2";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
}

static void
create_statusbar (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *statusbar;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "statusbar");

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      statusbar = gtk_statusbar_new ();
      g_signal_connect (statusbar,
			"text_popped",
			G_CALLBACK (statusbar_popped),
			NULL);

      button = g_object_new (gtk_button_get_type (),
			       "label", "push something",
			       NULL);
      gtk_box_append (GTK_BOX (box2), button);
      g_object_connect (button,
			"signal::clicked", statusbar_push, statusbar,
			NULL);

      button = g_object_connect (g_object_new (gtk_button_get_type (),
						 "label", "pop",
						 NULL),
				 "signal_after::clicked", statusbar_pop, statusbar,
				 NULL);
      gtk_box_append (GTK_BOX (box2), button);

      button = g_object_connect (g_object_new (gtk_button_get_type (),
						 "label", "steal #4",
						 NULL),
				 "signal_after::clicked", statusbar_steal, statusbar,
				 NULL);
      gtk_box_append (GTK_BOX (box2), button);

      button = g_object_connect (g_object_new (gtk_button_get_type (),
						 "label", "test contexts",
						 NULL),
				 "swapped_signal_after::clicked", statusbar_contexts, statusbar,
				 NULL);
      gtk_box_append (GTK_BOX (box2), button);

      button = g_object_connect (g_object_new (gtk_button_get_type (),
						 "label", "push something long",
						 NULL),
				 "signal_after::clicked", statusbar_push_long, statusbar,
				 NULL);
      gtk_box_append (GTK_BOX (box2), button);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);
      gtk_box_append (GTK_BOX (box1), statusbar);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (box2), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/* 
 * Label Demo
 */
static void
sensitivity_toggled (GtkWidget *toggle,
                     GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static GtkWidget*
create_sensitivity_control (GtkWidget *widget)
{
  GtkWidget *button;

  button = gtk_toggle_button_new_with_label ("Sensitive");  

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                gtk_widget_is_sensitive (widget));
  
  g_signal_connect (button,
                    "toggled",
                    G_CALLBACK (sensitivity_toggled),
                    widget);

  return button;
}

static void
set_selectable_recursive (GtkWidget *widget,
                          gboolean   setting)
{
  if (GTK_IS_LABEL (widget))
    gtk_label_set_selectable (GTK_LABEL (widget), setting);
  else
    {
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (widget);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        set_selectable_recursive (child, setting);
    }
}

static void
selectable_toggled (GtkWidget *toggle,
                    GtkWidget *widget)
{
  set_selectable_recursive (widget,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static GtkWidget*
create_selectable_control (GtkWidget *widget)
{
  GtkWidget *button;

  button = gtk_toggle_button_new_with_label ("Selectable");  

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                FALSE);
  
  g_signal_connect (button,
                    "toggled",
                    G_CALLBACK (selectable_toggled),
                    widget);

  return button;
}

static void
dialog_response (GtkWidget *dialog, int response_id, GtkLabel *label)
{
  const char *text;

  gtk_window_destroy (GTK_WINDOW (dialog));

  text = "Some <a href=\"http://en.wikipedia.org/wiki/Text\" title=\"plain text\">text</a> may be marked up\n"
         "as hyperlinks, which can be clicked\n"
         "or activated via <a href=\"keynav\">keynav</a>.\n"
         "The links remain the same.";
  gtk_label_set_markup (label, text);
}

static gboolean
activate_link (GtkWidget *label, const char *uri, gpointer data)
{
  if (g_strcmp0 (uri, "keynav") == 0)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_root (label)),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_INFO,
                                       GTK_BUTTONS_OK,
                                       "The term <i>keynav</i> is a shorthand for "
                                       "keyboard navigation and refers to the process of using a program "
                                       "(exclusively) via keyboard input.");

      gtk_window_present (GTK_WINDOW (dialog));

      g_signal_connect (dialog, "response", G_CALLBACK (dialog_response), label);

      return TRUE;
    }

  return FALSE;
}

static void create_labels (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_window_new ();

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "Label");

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_window_set_child (GTK_WINDOW (window), vbox);


      button = create_sensitivity_control (hbox);

      gtk_box_append (GTK_BOX (vbox), button);

      button = create_selectable_control (hbox);

      gtk_box_append (GTK_BOX (vbox), button);
      gtk_box_append (GTK_BOX (vbox), hbox);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

      gtk_box_append (GTK_BOX (hbox), vbox);

      frame = gtk_frame_new ("Normal Label");
      label = gtk_label_new ("This is a Normal label");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Multi-line Label");
      label = gtk_label_new ("This is a Multi-line label.\nSecond line\nThird line");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Left Justified Label");
      label = gtk_label_new ("This is a Left-Justified\nMulti-line label.\nThird      line");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Right Justified Label");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
      label = gtk_label_new ("This is a Right-Justified\nMulti-line label.\nFourth line, (j/k)");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Internationalized Label");
      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
			    "French (Fran\303\247ais) Bonjour, Salut\n"
			    "Korean (\355\225\234\352\270\200)   \354\225\210\353\205\225\355\225\230\354\204\270\354\232\224, \354\225\210\353\205\225\355\225\230\354\213\255\353\213\210\352\271\214\n"
			    "Russian (\320\240\321\203\321\201\321\201\320\272\320\270\320\271) \320\227\320\264\321\200\320\260\320\262\321\201\321\202\320\262\321\203\320\271\321\202\320\265!\n"
			    "Chinese (Simplified) <span lang=\"zh-cn\">\345\205\203\346\260\224	\345\274\200\345\217\221</span>\n"
			    "Chinese (Traditional) <span lang=\"zh-tw\">\345\205\203\346\260\243	\351\226\213\347\231\274</span>\n"
			    "Japanese <span lang=\"ja\">\345\205\203\346\260\227	\351\226\213\347\231\272</span>");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Bidirection Label");
      label = gtk_label_new ("\342\200\217Arabic	\330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205\n"
			     "\342\200\217Hebrew	\327\251\327\234\327\225\327\235");
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Links in a label");
      label = gtk_label_new ("Some <a href=\"http://en.wikipedia.org/wiki/Text\" title=\"plain text\">text</a> may be marked up\n"
                             "as hyperlinks, which can be clicked\n"
                             "or activated via <a href=\"keynav\">keynav</a>");
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);
      g_signal_connect (label, "activate-link", G_CALLBACK (activate_link), NULL);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_box_append (GTK_BOX (hbox), vbox);
      frame = gtk_frame_new ("Line wrapped label");
      label = gtk_label_new ("This is an example of a line-wrapped label.  It should not be taking "\
			     "up the entire             "/* big space to test spacing */\
			     "width allocated to it, but automatically wraps the words to fit.  "\
			     "The time has come, for all good men, to come to the aid of their party.  "\
			     "The sixth sheik's six sheep's sick.\n"\
			     "     It supports multiple paragraphs correctly, and  correctly   adds "\
			     "many          extra  spaces. ");

      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Filled, wrapped label");
      label = gtk_label_new ("This is an example of a line-wrapped, filled label.  It should be taking "\
			     "up the entire              width allocated to it.  Here is a seneance to prove "\
			     "my point.  Here is another sentence. "\
			     "Here comes the sun, do de do de do.\n"\
			     "    This is a new paragraph.\n"\
			     "    This is another newer, longer, better paragraph.  It is coming to an end, "\
			     "unfortunately.");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Underlined label");
      label = gtk_label_new ("This label is underlined!\n"
			     "This one is underlined (\343\201\223\343\202\223\343\201\253\343\201\241\343\201\257) in quite a funky fashion");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);

      frame = gtk_frame_new ("Markup label");
      label = gtk_label_new (NULL);

      /* There's also a gtk_label_set_markup() without accel if you
       * don't have an accelerator key
       */
      gtk_label_set_markup_with_mnemonic (GTK_LABEL (label),
					  "This <span foreground=\"blue\" background=\"orange\">label</span> has "
					  "<b>markup</b> _such as "
					  "<big><i>Big Italics</i></big>\n"
					  "<tt>Monospace font</tt>\n"
					  "<u>Underline!</u>\n"
					  "foo\n"
					  "<span foreground=\"green\" background=\"red\">Ugly colors</span>\n"
					  "and nothing on this line,\n"
					  "or this.\n"
					  "or this either\n"
					  "or even on this one\n"
					  "la <big>la <big>la <big>la <big>la</big></big></big></big>\n"
					  "but this _word is <span foreground=\"purple\"><big>purple</big></span>\n"
					  "<span underline=\"double\">We like <sup>superscript</sup> and <sub>subscript</sub> too</span>");

      g_assert (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)) == GDK_KEY_s);

      gtk_frame_set_child (GTK_FRAME (frame), label);
      gtk_box_append (GTK_BOX (vbox), frame);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

#define DEFAULT_TEXT_RADIUS 200

static void
on_rotated_text_unrealize (GtkWidget *widget)
{
  g_object_set_data (G_OBJECT (widget), "text-gc", NULL);
}

static void
on_rotated_text_draw (GtkDrawingArea *drawing_area,
                      cairo_t        *cr,
                      int             width,
                      int             height,
                      gpointer        tile_pixbuf)
{
  static const char *words[] = { "The", "grand", "old", "Duke", "of", "York",
                                  "had", "10,000", "men" };
  int n_words;
  int i;
  double radius;
  PangoLayout *layout;
  PangoContext *context;
  PangoFontDescription *desc;

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  if (tile_pixbuf)
    {
      gdk_cairo_set_source_pixbuf (cr, tile_pixbuf, 0, 0);
      cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
    }
  else
    cairo_set_source_rgb (cr, 0, 0, 0);

  radius = MIN (width, height) / 2.;

  cairo_translate (cr,
                   radius + (width - 2 * radius) / 2,
                   radius + (height - 2 * radius) / 2);
  cairo_scale (cr, radius / DEFAULT_TEXT_RADIUS, radius / DEFAULT_TEXT_RADIUS);

  context = gtk_widget_get_pango_context (GTK_WIDGET (drawing_area));
  layout = pango_layout_new (context);
  desc = pango_font_description_from_string ("Sans Bold 30");
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);
    
  n_words = G_N_ELEMENTS (words);
  for (i = 0; i < n_words; i++)
    {
      int layout_width, layout_height;

      cairo_save (cr);

      cairo_rotate (cr, 2 * G_PI * i / n_words);
      pango_cairo_update_layout (cr, layout);

      pango_layout_set_text (layout, words[i], -1);
      pango_layout_get_size (layout, &layout_width, &layout_height);

      cairo_move_to (cr, - layout_width / 2 / PANGO_SCALE, - DEFAULT_TEXT_RADIUS);
      pango_cairo_show_layout (cr, layout);

      cairo_restore (cr);
    }
  
  g_object_unref (layout);
}

static void
create_rotated_text (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *content_area;
      GtkWidget *drawing_area;
      GdkPixbuf *tile_pixbuf;

      window = gtk_dialog_new_with_buttons ("Rotated Text",
					    GTK_WINDOW (gtk_widget_get_root (widget)), 0,
					    "_Close", GTK_RESPONSE_CLOSE,
					    NULL);

      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

      gtk_window_set_display (GTK_WINDOW (window),
			      gtk_widget_get_display (widget));

      g_signal_connect (window, "response",
			G_CALLBACK (gtk_window_destroy), NULL);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      drawing_area = gtk_drawing_area_new ();
      gtk_widget_set_hexpand (drawing_area, TRUE);
      gtk_widget_set_vexpand (drawing_area, TRUE);
      gtk_box_append (GTK_BOX (content_area), drawing_area);

      tile_pixbuf = gdk_pixbuf_new_from_file ("marble.xpm", NULL);

      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area),
			              on_rotated_text_draw,
                                      tile_pixbuf,
                                      g_object_unref);
      g_signal_connect (drawing_area, "unrealize",
			G_CALLBACK (on_rotated_text_unrealize), NULL);

      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (drawing_area), DEFAULT_TEXT_RADIUS * 2);
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (drawing_area), DEFAULT_TEXT_RADIUS * 2);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}


/*
 * GtkPixmap
 */

static void
create_pixbuf (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *pixbufwid;
  GdkSurface *gdk_surface;

  if (!window)
    {
      window = gtk_window_new ();

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkPixmap");
      gtk_widget_realize(window);

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_button_new ();
      gtk_box_append (GTK_BOX (box2), button);

      gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));

      pixbufwid = new_pixbuf ("test.xpm", gdk_surface);

      label = gtk_label_new ("Pixbuf\ntest");
      box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box3), pixbufwid);
      gtk_box_append (GTK_BOX (box3), label);
      gtk_button_set_child (GTK_BUTTON (button), box3);

      button = gtk_button_new ();
      gtk_box_append (GTK_BOX (box2), button);

      pixbufwid = new_pixbuf ("test.xpm", gdk_surface);

      label = gtk_label_new ("Pixbuf\ntest");
      box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box3), pixbufwid);
      gtk_box_append (GTK_BOX (box3), label);
      gtk_button_set_child (GTK_BUTTON (button), box3);

      gtk_widget_set_sensitive (button, FALSE);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (box2), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

static void
create_tooltips (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *toggle;
  GtkWidget *frame;
  GtkWidget *separator;

  if (!window)
    {
      window =
	g_object_new (gtk_window_get_type (),
			"GtkWindow::title", "Tooltips",
			"GtkWindow::resizable", FALSE,
			NULL);

      gtk_window_set_display (GTK_WINDOW (window),
			      gtk_widget_get_display (widget));

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_append (GTK_BOX (box2), button);

      gtk_widget_set_tooltip_text (button, "This is button 1");

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_append (GTK_BOX (box2), button);

      gtk_widget_set_tooltip_text (button,
        "This is button 2. This is also a really long tooltip which probably "
        "won't fit on a single line and will therefore need to be wrapped. "
        "Hopefully the wrapping will work correctly.");

      toggle = gtk_toggle_button_new_with_label ("Override TipsQuery Label");
      gtk_box_append (GTK_BOX (box2), toggle);

      gtk_widget_set_tooltip_text (toggle, "Toggle TipsQuery view.");

      box3 =
	g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
			"homogeneous", FALSE,
			"spacing", 5,
			NULL);

      button =
	g_object_new (gtk_button_get_type (),
			"label", "[?]",
			NULL);
      gtk_box_append (GTK_BOX (box3), button);
      gtk_widget_set_tooltip_text (button, "Start the Tooltips Inspector");

      frame = g_object_new (gtk_frame_get_type (),
			      "label", "ToolTips Inspector",
			      "label_xalign", (double) 0.5,
			      NULL);
      gtk_box_append (GTK_BOX (box2), frame);
      gtk_frame_set_child (GTK_FRAME (frame), box3);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (box2), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);

      gtk_widget_set_tooltip_text (button, "Push this button to close window");
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkImage
 */

static void
pack_image (GtkWidget *box,
            const char *text,
            GtkWidget *image)
{
  gtk_box_append (GTK_BOX (box),
                      gtk_label_new (text));

  gtk_box_append (GTK_BOX (box),
                      image);
}

static void
create_image (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (window == NULL)
    {
      GtkWidget *vbox;
      GdkPixbuf *pixbuf;
        
      window = gtk_window_new ();
      
      gtk_window_set_display (GTK_WINDOW (window),
			      gtk_widget_get_display (widget));

      /* this is bogus for testing drawing when allocation < request,
       * don't copy into real code
       */
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

      gtk_window_set_child (GTK_WINDOW (window), vbox);

      pack_image (vbox, "Stock Warning Dialog", gtk_image_new_from_icon_name ("dialog-warning"));

      pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) openfile);
      
      pack_image (vbox, "Pixbuf",
                  gtk_image_new_from_pixbuf (pixbuf));

      g_object_unref (pixbuf);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * ListBox demo
 */

static int
list_sort_cb (GtkListBoxRow *a, GtkListBoxRow *b, gpointer data)
{
  int aa = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a), "value"));
  int bb = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (b), "value"));
  return aa - bb;
}

static gboolean
list_filter_all_cb (GtkListBoxRow *row, gpointer data)
{
  return FALSE;
}

static gboolean
list_filter_odd_cb (GtkListBoxRow *row, gpointer data)
{
  int value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "value"));

  return value % 2 == 0;
}

static void
list_sort_clicked_cb (GtkButton *button,
                      gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_sort_func (list, list_sort_cb, NULL, NULL);
}

static void
list_filter_odd_clicked_cb (GtkButton *button,
                            gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_filter_func (list, list_filter_odd_cb, NULL, NULL);
}

static void
list_filter_all_clicked_cb (GtkButton *button,
                            gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_filter_func (list, list_filter_all_cb, NULL, NULL);
}


static void
list_unfilter_clicked_cb (GtkButton *button,
                          gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_filter_func (list, NULL, NULL, NULL);
}

static void
add_placeholder_clicked_cb (GtkButton *button,
                            gpointer data)
{
  GtkListBox *list = data;
  GtkWidget *label;

  label = gtk_label_new ("You filtered everything!!!");
  gtk_list_box_set_placeholder (GTK_LIST_BOX (list), label);
}

static void
remove_placeholder_clicked_cb (GtkButton *button,
                            gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_placeholder (GTK_LIST_BOX (list), NULL);
}


static void
create_listbox (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *hbox, *vbox, *scrolled, *scrolled_box, *list, *label, *button;
      GdkDisplay *display = gtk_widget_get_display (widget);
      int i;

      window = gtk_window_new ();
      gtk_window_set_hide_on_close (GTK_WINDOW (window), TRUE);
      gtk_window_set_display (GTK_WINDOW (window), display);

      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "listbox");

      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), hbox);

      scrolled = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_box_append (GTK_BOX (hbox), scrolled);

      scrolled_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), scrolled_box);

      label = gtk_label_new ("This is \na LABEL\nwith rows");
      gtk_box_append (GTK_BOX (scrolled_box), label);

      list = gtk_list_box_new();
      gtk_list_box_set_adjustment (GTK_LIST_BOX (list), gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled)));
      gtk_box_append (GTK_BOX (scrolled_box), list);

      for (i = 0; i < 1000; i++)
        {
          int value = g_random_int_range (0, 10000);
          label = gtk_label_new (g_strdup_printf ("Value %u", value));
          gtk_list_box_insert (GTK_LIST_BOX (list), label, -1);
          g_object_set_data (G_OBJECT (gtk_widget_get_parent (label)), "value", GINT_TO_POINTER (value));
        }

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_append (GTK_BOX (hbox), vbox);

      button = gtk_button_new_with_label ("sort");
      gtk_box_append (GTK_BOX (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (list_sort_clicked_cb), list);

      button = gtk_button_new_with_label ("filter odd");
      gtk_box_append (GTK_BOX (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (list_filter_odd_clicked_cb), list);

      button = gtk_button_new_with_label ("filter all");
      gtk_box_append (GTK_BOX (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (list_filter_all_clicked_cb), list);

      button = gtk_button_new_with_label ("unfilter");
      gtk_box_append (GTK_BOX (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (list_unfilter_clicked_cb), list);

      button = gtk_button_new_with_label ("add placeholder");
      gtk_box_append (GTK_BOX (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (add_placeholder_clicked_cb), list);

      button = gtk_button_new_with_label ("remove placeholder");
      gtk_box_append (GTK_BOX (vbox), button);
      g_signal_connect (button, "clicked", G_CALLBACK (remove_placeholder_clicked_cb), list);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 create_modal_window
 */

static gboolean
cmw_destroy_cb(GtkWidget *widget)
{
  done = TRUE;
  g_main_context_wakeup (NULL);

  return FALSE;
}

static void
cmw_color (GtkWidget *widget, GtkWidget *parent)
{
    GtkWidget *csd;

    csd = gtk_color_chooser_dialog_new ("This is a modal color selection dialog", GTK_WINDOW (parent));

    /* Set as modal */
    gtk_window_set_modal (GTK_WINDOW(csd),TRUE);

    g_signal_connect (csd, "destroy",
		      G_CALLBACK (cmw_destroy_cb), NULL);
    g_signal_connect (csd, "response",
                      G_CALLBACK (gtk_window_destroy), NULL);
    
    /* wait until destroy calls gtk_main_quit */
    gtk_window_present (GTK_WINDOW (csd));
    while (!done)
      g_main_context_iteration (NULL, TRUE);
}

static void
cmw_file (GtkWidget *widget, GtkWidget *parent)
{
    GtkWidget *fs;

    fs = gtk_file_chooser_dialog_new ("This is a modal file selection dialog",
                                      GTK_WINDOW (parent),
                                      GTK_FILE_CHOOSER_ACTION_OPEN,
                                      "_Open", GTK_RESPONSE_ACCEPT,
                                      "_Cancel", GTK_RESPONSE_CANCEL,
                                      NULL);
    gtk_window_set_display (GTK_WINDOW (fs), gtk_widget_get_display (parent));
    gtk_window_set_modal (GTK_WINDOW (fs), TRUE);

    g_signal_connect (fs, "destroy",
                      G_CALLBACK (cmw_destroy_cb), NULL);
    g_signal_connect_swapped (fs, "response",
                      G_CALLBACK (gtk_window_destroy), fs);

    /* wait until destroy calls gtk_main_quit */
    gtk_widget_show (fs);
    while (!done)
      g_main_context_iteration (NULL, TRUE);
}


static void
create_modal_window (GtkWidget *widget)
{
  GtkWidget *window = NULL;
  GtkWidget *box1,*box2;
  GtkWidget *frame1;
  GtkWidget *btnColor,*btnFile,*btnClose;

  /* Create modal window (Here you can use any window descendent )*/
  window = gtk_window_new ();
  gtk_window_set_display (GTK_WINDOW (window),
			  gtk_widget_get_display (widget));

  gtk_window_set_title (GTK_WINDOW(window),"This window is modal");

  /* Set window as modal */
  gtk_window_set_modal (GTK_WINDOW(window),TRUE);

  /* Create widgets */
  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  frame1 = gtk_frame_new ("Standard dialogs in modal form");
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_set_homogeneous (GTK_BOX (box2), TRUE);
  btnColor = gtk_button_new_with_label ("Color");
  btnFile = gtk_button_new_with_label ("File Selection");
  btnClose = gtk_button_new_with_label ("Close");

  /* Pack widgets */
  gtk_window_set_child (GTK_WINDOW (window), box1);
  gtk_box_append (GTK_BOX (box1), frame1);
  gtk_frame_set_child (GTK_FRAME (frame1), box2);
  gtk_box_append (GTK_BOX (box2), btnColor);
  gtk_box_append (GTK_BOX (box2), btnFile);
  gtk_box_append (GTK_BOX (box1), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_box_append (GTK_BOX (box1), btnClose);

  /* connect signals */
  g_signal_connect_swapped (btnClose, "clicked",
			    G_CALLBACK (gtk_window_destroy), window);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (cmw_destroy_cb), NULL);

  g_signal_connect (btnColor, "clicked",
                    G_CALLBACK (cmw_color), window);
  g_signal_connect (btnFile, "clicked",
                    G_CALLBACK (cmw_file), window);

  /* Show widgets */
  gtk_window_present (GTK_WINDOW (window));

  /* wait until dialog get destroyed */
  while (!done)
    g_main_context_iteration (NULL, TRUE);
}

/*
 * GtkMessageDialog
 */

static void
make_message_dialog (GdkDisplay     *display,
		     GtkWidget     **dialog,
                     GtkMessageType  type,
                     GtkButtonsType  buttons,
		     guint           default_response)
{
  if (*dialog)
    {
      gtk_window_destroy (GTK_WINDOW (*dialog));

      return;
    }

  *dialog = gtk_message_dialog_new (NULL, 0, type, buttons,
                                    "This is a message dialog; it can wrap long lines. This is a long line. La la la. Look this line is wrapped. Blah blah blah blah blah blah. (Note: testgtk has a nonstandard gtkrc that changes some of the message dialog icons.)");

  gtk_window_set_display (GTK_WINDOW (*dialog), display);

  g_signal_connect_swapped (*dialog,
			    "response",
			    G_CALLBACK (gtk_window_destroy),
			    *dialog);
  
  g_object_add_weak_pointer (G_OBJECT (*dialog), (gpointer)dialog);

  gtk_dialog_set_default_response (GTK_DIALOG (*dialog), default_response);

  gtk_widget_show (*dialog);
}

static void
create_message_dialog (GtkWidget *widget)
{
  static GtkWidget *info = NULL;
  static GtkWidget *warning = NULL;
  static GtkWidget *error = NULL;
  static GtkWidget *question = NULL;
  GdkDisplay *display = gtk_widget_get_display (widget);

  make_message_dialog (display, &info, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, GTK_RESPONSE_OK);
  make_message_dialog (display, &warning, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, GTK_RESPONSE_CLOSE);
  make_message_dialog (display, &error, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK);
  make_message_dialog (display, &question, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, GTK_RESPONSE_NO);
}

/*
 * GtkScrolledWindow
 */

static GtkWidget *sw_parent = NULL;
static GtkWidget *sw_float_parent;
static gulong sw_destroyed_handler = 0;

static gboolean
scrolled_windows_delete_cb (GtkWidget *widget,
                            GtkWidget *scrollwin)
{
  g_object_ref (scrollwin);
  gtk_box_remove (GTK_BOX (gtk_widget_get_parent (scrollwin)), scrollwin);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw_parent), scrollwin);
  g_object_unref (scrollwin);

  g_signal_handler_disconnect (sw_parent, sw_destroyed_handler);
  sw_float_parent = NULL;
  sw_parent = NULL;
  sw_destroyed_handler = 0;

  return FALSE;
}

static void
scrolled_windows_destroy_cb (GtkWidget *widget, GtkWidget *scrollwin)
{
  gtk_window_destroy (GTK_WINDOW (sw_float_parent));

  sw_float_parent = NULL;
  sw_parent = NULL;
  sw_destroyed_handler = 0;
}

static void
scrolled_windows_remove (GtkWidget *dialog, int response, GtkWidget *scrollwin)
{
  if (response != GTK_RESPONSE_APPLY)
    {
      gtk_window_destroy (GTK_WINDOW (dialog));
      return;
    }

  if (sw_parent)
    {
      g_object_ref (scrollwin);
      gtk_box_remove (GTK_BOX (gtk_widget_get_parent (scrollwin)), scrollwin);
      gtk_window_set_child (GTK_WINDOW (sw_float_parent), scrollwin);
      g_object_unref (scrollwin);


      gtk_window_destroy (GTK_WINDOW (sw_float_parent));

      g_signal_handler_disconnect (sw_parent, sw_destroyed_handler);
      sw_float_parent = NULL;
      sw_parent = NULL;
      sw_destroyed_handler = 0;
    }
  else
    {
      sw_parent = gtk_widget_get_parent (scrollwin);
      sw_float_parent = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (sw_float_parent),
			      gtk_widget_get_display (dialog));
      
      gtk_window_set_default_size (GTK_WINDOW (sw_float_parent), 200, 200);
      
      g_object_ref (scrollwin);
      gtk_box_remove (GTK_BOX (gtk_widget_get_parent (scrollwin)), scrollwin);
      gtk_window_set_child (GTK_WINDOW (sw_float_parent), scrollwin);
      g_object_unref (scrollwin);


      gtk_widget_show (sw_float_parent);

      sw_destroyed_handler =
	g_signal_connect (sw_parent, "destroy",
			  G_CALLBACK (scrolled_windows_destroy_cb), scrollwin);
      g_signal_connect (sw_float_parent, "close-request",
			G_CALLBACK (scrolled_windows_delete_cb), scrollwin);
    }
}

static void
create_scrolled_windows (GtkWidget *widget)
{
  static GtkWidget *window;
  GtkWidget *content_area;
  GtkWidget *scrolled_window;
  GtkWidget *button;
  GtkWidget *grid;
  char buffer[32];
  int i, j;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "dialog");

      scrolled_window = gtk_scrolled_window_new ();
      gtk_widget_set_hexpand (scrolled_window, TRUE);
      gtk_widget_set_vexpand (scrolled_window, TRUE);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_append (GTK_BOX (content_area), scrolled_window);

      grid = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), grid);
      gtk_viewport_set_scroll_to_focus (GTK_VIEWPORT (gtk_widget_get_parent (grid)), TRUE);

      for (i = 0; i < 20; i++)
	for (j = 0; j < 20; j++)
	  {
	    sprintf (buffer, "button (%d,%d)\n", i, j);
	    button = gtk_toggle_button_new_with_label (buffer);
	    gtk_grid_attach (GTK_GRID (grid), button, i, j, 1, 1);
	  }

      gtk_dialog_add_button (GTK_DIALOG (window),
                             "Close",
                             GTK_RESPONSE_CLOSE);

      gtk_dialog_add_button (GTK_DIALOG (window),
                             "Reparent Out",
                             GTK_RESPONSE_APPLY);

      g_signal_connect (window, "response",
			G_CALLBACK (scrolled_windows_remove),
			scrolled_window);

      gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkEntry
 */

static void
entry_toggle_frame (GtkWidget *checkbutton,
                    GtkWidget *entry)
{
   gtk_entry_set_has_frame (GTK_ENTRY(entry),
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
entry_toggle_sensitive (GtkWidget *checkbutton,
			GtkWidget *entry)
{
   gtk_widget_set_sensitive (entry,
                             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton)));
}

static gboolean
entry_progress_timeout (gpointer data)
{
  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (data), "progress-pulse")))
    {
      gtk_entry_progress_pulse (GTK_ENTRY (data));
    }
  else
    {
      double fraction;

      fraction = gtk_entry_get_progress_fraction (GTK_ENTRY (data));

      fraction += 0.05;
      if (fraction > 1.0001)
        fraction = 0.0;

      gtk_entry_set_progress_fraction (GTK_ENTRY (data), fraction);
    }

  return G_SOURCE_CONTINUE;
}

static void
entry_remove_timeout (gpointer data)
{
  g_source_remove (GPOINTER_TO_UINT (data));
}

static void
entry_toggle_progress (GtkWidget *checkbutton,
                       GtkWidget *entry)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)))
    {
      guint timeout = g_timeout_add (100, entry_progress_timeout, entry);
      g_object_set_data_full (G_OBJECT (entry), "timeout-id",
                              GUINT_TO_POINTER (timeout),
                              entry_remove_timeout);
    }
  else
    {
      g_object_set_data (G_OBJECT (entry), "timeout-id",
                         GUINT_TO_POINTER (0));

      gtk_entry_set_progress_fraction (GTK_ENTRY (entry), 0.0);
    }
}

static void
entry_toggle_pulse (GtkWidget *checkbutton,
                    GtkWidget *entry)
{
  g_object_set_data (G_OBJECT (entry), "progress-pulse",
                     GUINT_TO_POINTER ((guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton))));
}

static void
create_entry (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *hbox;
  GtkWidget *has_frame_check;
  GtkWidget *sensitive_check;
  GtkWidget *progress_check;
  GtkWidget *entry;
  GtkComboBoxText *cb;
  GtkWidget *cb_entry;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "entry");


      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);


      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_widget_set_margin_top (box2, 10);
      gtk_widget_set_margin_bottom (box2, 10);
      gtk_widget_set_margin_start (box2, 10);
      gtk_widget_set_margin_end (box2, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_box_append (GTK_BOX (box2), hbox);

      entry = gtk_entry_new ();
      gtk_editable_set_text (GTK_EDITABLE (entry), "hello world \330\247\331\204\330\263\331\204\330\247\331\205 \330\271\331\204\331\212\331\203\331\205");
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, 5);
      gtk_widget_set_hexpand (entry, TRUE);
      gtk_box_append (GTK_BOX (hbox), entry);

      cb = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());

      gtk_combo_box_text_append_text (cb, "item0");
      gtk_combo_box_text_append_text (cb, "item0");
      gtk_combo_box_text_append_text (cb, "item1 item1");
      gtk_combo_box_text_append_text (cb, "item2 item2 item2");
      gtk_combo_box_text_append_text (cb, "item3 item3 item3 item3");
      gtk_combo_box_text_append_text (cb, "item4 item4 item4 item4 item4");
      gtk_combo_box_text_append_text (cb, "item5 item5 item5 item5 item5 item5");
      gtk_combo_box_text_append_text (cb, "item6 item6 item6 item6 item6");
      gtk_combo_box_text_append_text (cb, "item7 item7 item7 item7");
      gtk_combo_box_text_append_text (cb, "item8 item8 item8");
      gtk_combo_box_text_append_text (cb, "item9 item9");

      cb_entry = gtk_combo_box_get_child (GTK_COMBO_BOX (cb));
      gtk_editable_set_text (GTK_EDITABLE (cb_entry), "hello world \n\n\n foo");
      gtk_editable_select_region (GTK_EDITABLE (cb_entry), 0, -1);
      gtk_box_append (GTK_BOX (box2), GTK_WIDGET (cb));

      sensitive_check = gtk_check_button_new_with_label ("Sensitive");
      gtk_box_append (GTK_BOX (box2), sensitive_check);
      g_signal_connect (sensitive_check, "toggled",
			G_CALLBACK (entry_toggle_sensitive), entry);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sensitive_check), TRUE);

      has_frame_check = gtk_check_button_new_with_label("Has Frame");
      gtk_box_append (GTK_BOX (box2), has_frame_check);
      g_signal_connect (has_frame_check, "toggled",
			G_CALLBACK (entry_toggle_frame), entry);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (has_frame_check), TRUE);

      progress_check = gtk_check_button_new_with_label("Show Progress");
      gtk_box_append (GTK_BOX (box2), progress_check);
      g_signal_connect (progress_check, "toggled",
			G_CALLBACK (entry_toggle_progress), entry);

      progress_check = gtk_check_button_new_with_label("Pulse Progress");
      gtk_box_append (GTK_BOX (box2), progress_check);
      g_signal_connect (progress_check, "toggled",
			G_CALLBACK (entry_toggle_pulse), entry);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_widget_set_margin_top (box2, 10);
      gtk_widget_set_margin_bottom (box2, 10);
      gtk_widget_set_margin_start (box2, 10);
      gtk_widget_set_margin_end (box2, 10);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (box2), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

static void
create_expander (GtkWidget *widget)
{
  GtkWidget *box1;
  GtkWidget *expander;
  GtkWidget *hidden;
  static GtkWidget *window = NULL;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "expander");

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      expander = gtk_expander_new ("The Hidden");

      gtk_box_append (GTK_BOX (box1), expander);

      hidden = gtk_label_new ("Revealed!");

      gtk_expander_set_child (GTK_EXPANDER (expander), hidden);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}


/*
 * GtkSizeGroup
 */

#define SIZE_GROUP_INITIAL_SIZE 50

static void
size_group_hsize_changed (GtkSpinButton *spin_button,
			  GtkWidget     *button)
{
  gtk_widget_set_size_request (gtk_button_get_child (GTK_BUTTON (button)),
			       gtk_spin_button_get_value_as_int (spin_button),
			       -1);
}

static void
size_group_vsize_changed (GtkSpinButton *spin_button,
			  GtkWidget     *button)
{
  gtk_widget_set_size_request (gtk_button_get_child (GTK_BUTTON (button)),
			       -1,
			       gtk_spin_button_get_value_as_int (spin_button));
}

static GtkWidget *
create_size_group_window (GdkDisplay   *display,
			  GtkSizeGroup *master_size_group)
{
  GtkWidget *content_area;
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *grid;
  GtkWidget *main_button;
  GtkWidget *button;
  GtkWidget *spin_button;
  GtkWidget *hbox;
  GtkSizeGroup *hgroup1;
  GtkSizeGroup *hgroup2;
  GtkSizeGroup *vgroup1;
  GtkSizeGroup *vgroup2;

  window = gtk_dialog_new_with_buttons ("GtkSizeGroup",
					NULL, 0,
					"_Close",
					GTK_RESPONSE_NONE,
					NULL);

  gtk_window_set_display (GTK_WINDOW (window), display);

  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

  g_signal_connect (window, "response",
		    G_CALLBACK (gtk_window_destroy),
		    NULL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (content_area), vbox);

  grid = gtk_grid_new ();
  gtk_box_append (GTK_BOX (content_area), grid);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
  gtk_widget_set_size_request (grid, 250, 250);

  hgroup1 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  hgroup2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  vgroup1 = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  vgroup2 = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  main_button = gtk_button_new_with_label ("X");
  gtk_widget_set_hexpand (main_button, TRUE);
  gtk_widget_set_vexpand (main_button, TRUE);
  gtk_widget_set_halign (main_button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (main_button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (grid), main_button, 0, 0, 1, 1);
  
  gtk_size_group_add_widget (master_size_group, main_button);
  gtk_size_group_add_widget (hgroup1, main_button);
  gtk_size_group_add_widget (vgroup1, main_button);
  gtk_widget_set_size_request (gtk_button_get_child (GTK_BUTTON (main_button)),
			       SIZE_GROUP_INITIAL_SIZE,
			       SIZE_GROUP_INITIAL_SIZE);

  button = gtk_button_new ();
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (grid), button, 1, 0, 1, 1);

  gtk_size_group_add_widget (vgroup1, button);
  gtk_size_group_add_widget (vgroup2, button);

  button = gtk_button_new ();
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 1, 1, 1);

  gtk_size_group_add_widget (hgroup1, button);
  gtk_size_group_add_widget (hgroup2, button);

  button = gtk_button_new ();
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (grid), button, 1, 1, 1, 1);

  gtk_size_group_add_widget (hgroup2, button);
  gtk_size_group_add_widget (vgroup2, button);

  g_object_unref (hgroup1);
  g_object_unref (hgroup2);
  g_object_unref (vgroup1);
  g_object_unref (vgroup2);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_append (GTK_BOX (content_area), hbox);

  spin_button = gtk_spin_button_new_with_range (1, 100, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), SIZE_GROUP_INITIAL_SIZE);
  gtk_widget_set_hexpand (spin_button, TRUE);
  gtk_box_append (GTK_BOX (hbox), spin_button);
  g_signal_connect (spin_button, "value_changed",
		    G_CALLBACK (size_group_hsize_changed), main_button);

  spin_button = gtk_spin_button_new_with_range (1, 100, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), SIZE_GROUP_INITIAL_SIZE);
  gtk_widget_set_hexpand (spin_button, TRUE);
  gtk_box_append (GTK_BOX (hbox), spin_button);
  g_signal_connect (spin_button, "value_changed",
		    G_CALLBACK (size_group_vsize_changed), main_button);

  return window;
}

static void
create_size_groups (GtkWidget *widget)
{
  static GtkWidget *window1 = NULL;
  static GtkWidget *window2 = NULL;
  static GtkSizeGroup *master_size_group;

  if (!master_size_group)
    master_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  if (!window1)
    {
      window1 = create_size_group_window (gtk_widget_get_display (widget),
                                          master_size_group);
      g_object_add_weak_pointer (G_OBJECT (window1), (gpointer *)&window1);
    }

  if (!window2)
    {
      window2 = create_size_group_window (gtk_widget_get_display (widget),
                                          master_size_group);
      g_object_add_weak_pointer (G_OBJECT (window2), (gpointer *)&window2);
    }

  if (gtk_widget_get_visible (window1) && gtk_widget_get_visible (window2))
    {
      gtk_window_destroy (GTK_WINDOW (window1));
      gtk_window_destroy (GTK_WINDOW (window2));
    }
  else
    {
      if (!gtk_widget_get_visible (window1))
	gtk_widget_show (window1);
      if (!gtk_widget_get_visible (window2))
	gtk_widget_show (window2);
    }
}

/*
 * GtkSpinButton
 */

static GtkWidget *spinner1;

static void
toggle_snap (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_snap_to_ticks (spin,
                                     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
toggle_numeric (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_numeric (spin,
                               gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
change_digits (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinner1),
			      gtk_spin_button_get_value_as_int (spin));
}

static void
get_value (GtkWidget *widget, gpointer data)
{
  char buf[32];
  GtkLabel *label;
  GtkSpinButton *spin;

  spin = GTK_SPIN_BUTTON (spinner1);
  label = GTK_LABEL (g_object_get_data (G_OBJECT (widget), "user_data"));
  if (GPOINTER_TO_INT (data) == 1)
    sprintf (buf, "%d", gtk_spin_button_get_value_as_int (spin));
  else
    sprintf (buf, "%0.*f",
             gtk_spin_button_get_digits (spin),
             gtk_spin_button_get_value (spin));

  gtk_label_set_text (label, buf);
}

static void
get_spin_value (GtkWidget *widget, gpointer data)
{
  char *buffer;
  GtkLabel *label;
  GtkSpinButton *spin;

  spin = GTK_SPIN_BUTTON (widget);
  label = GTK_LABEL (data);

  buffer = g_strdup_printf ("%0.*f",
                            gtk_spin_button_get_digits (spin),
			    gtk_spin_button_get_value (spin));
  gtk_label_set_text (label, buffer);

  g_free (buffer);
}

static int
spin_button_time_output_func (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  static char buf[6];
  double hours;
  double minutes;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  hours = gtk_adjustment_get_value (adjustment) / 60.0;
  minutes = (fabs(floor (hours) - hours) < 1e-5) ? 0.0 : 30;
  sprintf (buf, "%02.0f:%02.0f", floor (hours), minutes);
  if (strcmp (buf, gtk_editable_get_text (GTK_EDITABLE (spin_button))))
    gtk_editable_set_text (GTK_EDITABLE (spin_button), buf);
  return TRUE;
}

static int
spin_button_month_input_func (GtkSpinButton *spin_button,
			      double        *new_val)
{
  int i;
  static const char *month[12] = { "January", "February", "March", "April",
			      "May", "June", "July", "August",
			      "September", "October", "November", "December" };
  char *tmp1, *tmp2;
  gboolean found = FALSE;

  for (i = 1; i <= 12; i++)
    {
      tmp1 = g_ascii_strup (month[i - 1], -1);
      tmp2 = g_ascii_strup (gtk_editable_get_text (GTK_EDITABLE (spin_button)), -1);
      if (strstr (tmp1, tmp2) == tmp1)
	found = TRUE;
      g_free (tmp1);
      g_free (tmp2);
      if (found)
	break;
    }
  if (!found)
    {
      *new_val = 0.0;
      return GTK_INPUT_ERROR;
    }
  *new_val = (double) i;
  return TRUE;
}

static int
spin_button_month_output_func (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  double value;
  int i;
  static const char *month[12] = { "January", "February", "March", "April",
			      "May", "June", "July", "August", "September",
			      "October", "November", "December" };

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  value = gtk_adjustment_get_value (adjustment);
  for (i = 1; i <= 12; i++)
    if (fabs (value - (double)i) < 1e-5)
      {
        if (strcmp (month[i-1], gtk_editable_get_text (GTK_EDITABLE (spin_button))))
          gtk_editable_set_text (GTK_EDITABLE (spin_button), month[i-1]);
      }
  return TRUE;
}

static int
spin_button_hex_input_func (GtkSpinButton *spin_button,
			    double        *new_val)
{
  const char *buf;
  char *err;
  double res;

  buf = gtk_editable_get_text (GTK_EDITABLE (spin_button));
  res = strtol(buf, &err, 16);
  *new_val = res;
  if (*err)
    return GTK_INPUT_ERROR;
  else
    return TRUE;
}

static int
spin_button_hex_output_func (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  static char buf[7];
  double val;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  val = gtk_adjustment_get_value (adjustment);
  if (fabs (val) < 1e-5)
    sprintf (buf, "0x00");
  else
    sprintf (buf, "0x%.2X", (int) val);
  if (strcmp (buf, gtk_editable_get_text (GTK_EDITABLE (spin_button))))
    gtk_editable_set_text (GTK_EDITABLE (spin_button), buf);

  return TRUE;
}

static void
create_spins (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *spinner2;
  GtkWidget *spinner;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *val_label;
  GtkAdjustment *adjustment;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkSpinButton");

      main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_window_set_child (GTK_WINDOW (window), main_vbox);

      frame = gtk_frame_new ("Not accelerated");
      gtk_box_append (GTK_BOX (main_vbox), frame);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_frame_set_child (GTK_FRAME (frame), vbox);

      /* Time, month, hex spinners */

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (vbox), hbox);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_hexpand (vbox2, TRUE);
      gtk_box_append (GTK_BOX (hbox), vbox2);

      label = gtk_label_new ("Time :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox2), label);

      adjustment = gtk_adjustment_new (0, 0, 1410, 30, 60, 0);
      spinner = gtk_spin_button_new (adjustment, 0, 0);
      gtk_editable_set_editable (GTK_EDITABLE (spinner), FALSE);
      g_signal_connect (spinner,
			"output",
			G_CALLBACK (spin_button_time_output_func),
			NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_editable_set_width_chars (GTK_EDITABLE (spinner), 5);
      gtk_box_append (GTK_BOX (vbox2), spinner);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_hexpand (vbox2, TRUE);
      gtk_box_append (GTK_BOX (hbox), vbox2);

      label = gtk_label_new ("Month :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox2), label);

      adjustment = gtk_adjustment_new (1.0, 1.0, 12.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adjustment, 0, 0);
      gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinner),
					 GTK_UPDATE_IF_VALID);
      g_signal_connect (spinner,
			"input",
			G_CALLBACK (spin_button_month_input_func),
			NULL);
      g_signal_connect (spinner,
			"output",
			G_CALLBACK (spin_button_month_output_func),
			NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_editable_set_width_chars (GTK_EDITABLE (spinner), 9);
      gtk_box_append (GTK_BOX (vbox2), spinner);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_hexpand (vbox2, TRUE);
      gtk_box_append (GTK_BOX (hbox), vbox2);

      label = gtk_label_new ("Hex :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox2), label);

      adjustment = gtk_adjustment_new (0, 0, 255, 1, 16, 0);
      spinner = gtk_spin_button_new (adjustment, 0, 0);
      gtk_editable_set_editable (GTK_EDITABLE (spinner), TRUE);
      g_signal_connect (spinner,
			"input",
			G_CALLBACK (spin_button_hex_input_func),
			NULL);
      g_signal_connect (spinner,
			"output",
			G_CALLBACK (spin_button_hex_output_func),
			NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_editable_set_width_chars (GTK_EDITABLE (spinner), 4);
      gtk_box_append (GTK_BOX (vbox2), spinner);

      frame = gtk_frame_new ("Accelerated");
      gtk_box_append (GTK_BOX (main_vbox), frame);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_frame_set_child (GTK_FRAME (frame), vbox);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (vbox), hbox);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_append (GTK_BOX (hbox), vbox2);

      label = gtk_label_new ("Value :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox2), label);

      adjustment = gtk_adjustment_new (0.0, -10000.0, 10000.0,
						  0.5, 100.0, 0.0);
      spinner1 = gtk_spin_button_new (adjustment, 1.0, 2);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
      gtk_box_append (GTK_BOX (vbox2), spinner1);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_append (GTK_BOX (hbox), vbox2);

      label = gtk_label_new ("Digits :");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox2), label);

      adjustment = gtk_adjustment_new (2, 1, 15, 1, 1, 0);
      spinner2 = gtk_spin_button_new (adjustment, 0.0, 0);
      g_signal_connect (adjustment, "value_changed",
			G_CALLBACK (change_digits),
			spinner2);
      gtk_box_append (GTK_BOX (vbox2), spinner2);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (vbox), hbox);

      button = gtk_check_button_new_with_label ("Snap to 0.5-ticks");
      g_signal_connect (button, "clicked",
			G_CALLBACK (toggle_snap),
			spinner1);
      gtk_box_append (GTK_BOX (vbox), button);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

      button = gtk_check_button_new_with_label ("Numeric only input mode");
      g_signal_connect (button, "clicked",
			G_CALLBACK (toggle_numeric),
			spinner1);
      gtk_box_append (GTK_BOX (vbox), button);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

      val_label = gtk_label_new ("");

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (vbox), hbox);

      button = gtk_button_new_with_label ("Value as Int");
      gtk_widget_set_hexpand (button, TRUE);
      g_object_set_data (G_OBJECT (button), "user_data", val_label);
      g_signal_connect (button, "clicked",
			G_CALLBACK (get_value),
			GINT_TO_POINTER (1));
      gtk_box_append (GTK_BOX (hbox), button);

      button = gtk_button_new_with_label ("Value as Float");
      gtk_widget_set_hexpand (button, TRUE);
      g_object_set_data (G_OBJECT (button), "user_data", val_label);
      g_signal_connect (button, "clicked",
			G_CALLBACK (get_value),
			GINT_TO_POINTER (2));
      gtk_box_append (GTK_BOX (hbox), button);

      gtk_box_append (GTK_BOX (vbox), val_label);
      gtk_label_set_text (GTK_LABEL (val_label), "0");

      frame = gtk_frame_new ("Using Convenience Constructor");
      gtk_box_append (GTK_BOX (main_vbox), frame);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_frame_set_child (GTK_FRAME (frame), hbox);

      val_label = gtk_label_new ("0.0");
      gtk_widget_set_hexpand (val_label, TRUE);

      spinner = gtk_spin_button_new_with_range (0.0, 10.0, 0.009);
      gtk_widget_set_hexpand (spinner, TRUE);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinner), 0.0);
      g_signal_connect (spinner, "value_changed",
			G_CALLBACK (get_spin_value), val_label);
      gtk_box_append (GTK_BOX (hbox), spinner);
      gtk_box_append (GTK_BOX (hbox), val_label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (main_vbox), hbox);

      button = gtk_button_new_with_label ("Close");
      gtk_widget_set_hexpand (button, TRUE);
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (hbox), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}


/*
 * Cursors
 */

static void
cursor_draw (GtkDrawingArea *darea,
	     cairo_t        *cr,
             int             width,
             int             height,
	     gpointer        user_data)
{
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_rectangle (cr, width / 3, height / 3, width / 3, height / 3);
  cairo_clip (cr);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_rectangle (cr, 0, 0, width, height / 2);
  cairo_fill (cr);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_rectangle (cr, 0, height / 2, width, height / 2);
  cairo_fill (cr);
}

static const char *cursor_names[] = {
  "none",
  "default",
  "help",
  "pointer",
  "context-menu",
  "progress",
  "wait",
  "cell",
  "crosshair",
  "text",
  "vertical-text",
  "alias",
  "copy",
  "no-drop",
  "move",
  "not-allowed",
  "grab",
  "grabbing",
  "all-scroll",
  "col-resize",
  "row-resize",
  "n-resize",
  "e-resize",
  "s-resize",
  "w-resize",
  "ne-resize",
  "nw-resize",
  "sw-resize",
  "se-resize",
  "ew-resize",
  "ns-resize",
  "nesw-resize",
  "nwse-resize",
  "zoom-in",
  "zoom-out",
  NULL
};

static GtkTreeModel *
cursor_model (void)
{
  GtkListStore *store;
  int i;
  store = gtk_list_store_new (1, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (cursor_names); i++)
    gtk_list_store_insert_with_values (store, NULL, -1, 0, cursor_names[i], -1);

  return (GtkTreeModel *)store;
}

static void
cursor_pressed_cb (GtkGesture *gesture,
                   int         n_pressed,
                   double      x,
                   double      y,
                   GtkWidget  *entry)
{
  GtkWidget *widget;
  const char *name;
  int i;
  const int n = G_N_ELEMENTS (cursor_names);
  int button;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  name = (const char *)g_object_get_data (G_OBJECT (widget), "name");
  if (name != NULL)
    {
      for (i = 0; i < n; i++)
        if (strcmp (name, cursor_names[i]) == 0)
          break;
    }
  else
    i = 0;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  if (button == GDK_BUTTON_PRIMARY)
    i = (i + 1) % n;
  else
    i = (i + n - 1) % n;

  gtk_editable_set_text (GTK_EDITABLE (entry), cursor_names[i]);
}

static void
set_cursor_from_name (GtkWidget *entry,
                      GtkWidget *widget)
{
  const char *name;

  name = gtk_editable_get_text (GTK_EDITABLE (entry));
  gtk_widget_set_cursor_from_name (widget, name);

  g_object_set_data_full (G_OBJECT (widget), "name", g_strdup (name), g_free);
}

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

static void
change_cursor_theme (GtkWidget *widget,
                     gpointer   data)
{
#if defined(GDK_WINDOWING_X11) || defined (GDK_WINDOWING_WAYLAND)
  const char *theme;
  int size;
  GdkDisplay *display;
  GtkWidget *entry;
  GtkWidget *spin;

  entry = gtk_widget_get_next_sibling (gtk_widget_get_first_child (GTK_WIDGET (data)));
  spin = gtk_widget_get_next_sibling (entry);

  theme = gtk_editable_get_text (GTK_EDITABLE (entry));
  size = (int) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

  display = gtk_widget_get_display (widget);
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    gdk_x11_display_set_cursor_theme (display, theme, size);
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    gdk_wayland_display_set_cursor_theme (display, theme, size);
#endif
#endif
}


static void
create_cursors (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *darea;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *any;
  GtkWidget *entry;
  GtkWidget *size;
  GtkEntryCompletion *completion;
  GtkTreeModel *model;
  gboolean cursor_demo = FALSE;
  GtkGesture *gesture;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "Cursors");

      main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_window_set_child (GTK_WINDOW (window), main_vbox);

      vbox = g_object_new (GTK_TYPE_BOX,
                           "orientation", GTK_ORIENTATION_VERTICAL,
                           "homogeneous", FALSE,
                           "spacing", 5,
                           NULL);
      gtk_box_append (GTK_BOX (main_vbox), vbox);

#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (vbox)))
        cursor_demo = TRUE;
#endif
#ifdef GDK_WINDOWING_WAYLAND
      if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (vbox)))
        cursor_demo = TRUE;
#endif

    if (cursor_demo)
        {
          hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
          gtk_widget_set_margin_top (hbox, 5);
          gtk_widget_set_margin_bottom (hbox, 5);
          gtk_widget_set_margin_start (hbox, 5);
          gtk_widget_set_margin_end (hbox, 5);
          gtk_box_append (GTK_BOX (vbox), hbox);

          label = gtk_label_new ("Cursor Theme:");
          gtk_widget_set_halign (label, GTK_ALIGN_START);
          gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
          gtk_box_append (GTK_BOX (hbox), label);

          entry = gtk_entry_new ();
          gtk_editable_set_text (GTK_EDITABLE (entry), "default");
          gtk_box_append (GTK_BOX (hbox), entry);

          size = gtk_spin_button_new_with_range (1.0, 128.0, 1.0);
          gtk_spin_button_set_value (GTK_SPIN_BUTTON (size), 24.0);
          gtk_widget_set_hexpand (size, TRUE);
          gtk_box_append (GTK_BOX (hbox), size);

          g_signal_connect (entry, "changed",
                            G_CALLBACK (change_cursor_theme), hbox);
          g_signal_connect (size, "value-changed",
                            G_CALLBACK (change_cursor_theme), hbox);
        }

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_widget_set_margin_top (hbox, 5);
      gtk_widget_set_margin_bottom (hbox, 5);
      gtk_widget_set_margin_start (hbox, 5);
      gtk_widget_set_margin_end (hbox, 5);
      gtk_box_append (GTK_BOX (vbox), hbox);

      label = gtk_label_new ("Cursor Name:");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (hbox), label);

      entry = gtk_entry_new ();

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

      completion = gtk_entry_completion_new ();
      model = cursor_model ();
      gtk_entry_completion_set_model (completion, model);
      gtk_entry_completion_set_text_column (completion, 0);
      gtk_entry_set_completion (GTK_ENTRY (entry), completion);

G_GNUC_END_IGNORE_DEPRECATIONS

      g_object_unref (model);
      gtk_widget_set_hexpand (entry, TRUE);
      gtk_box_append (GTK_BOX (hbox), entry);

      frame =
	g_object_new (gtk_frame_get_type (),
			"label_xalign", 0.5,
			"label", "Cursor Area",
			NULL);
      gtk_box_append (GTK_BOX (vbox), frame);

      darea = gtk_drawing_area_new ();
      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (darea), 80);
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (darea), 80);
      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (darea), cursor_draw, NULL, NULL);
      gtk_frame_set_child (GTK_FRAME (frame), darea);
      gesture = gtk_gesture_click_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
      g_signal_connect (gesture, "pressed", G_CALLBACK (cursor_pressed_cb), entry);
      gtk_widget_add_controller (darea, GTK_EVENT_CONTROLLER (gesture));

      g_signal_connect (entry, "changed",
                        G_CALLBACK (set_cursor_from_name), darea);


      any = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (main_vbox), any);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_margin_top (hbox, 10);
      gtk_widget_set_margin_bottom (hbox, 10);
      gtk_widget_set_margin_start (hbox, 10);
      gtk_widget_set_margin_end (hbox, 10);
      gtk_box_append (GTK_BOX (main_vbox), hbox);

      button = gtk_button_new_with_label ("Close");
      gtk_widget_set_hexpand (button, TRUE);
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (hbox), button);

      gtk_window_present (GTK_WINDOW (window));

      gtk_editable_set_text (GTK_EDITABLE (entry), "arrow");
    }
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkColorSelection
 */

static void
create_color_selection (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *picker;
      GtkWidget *hbox;
      GtkWidget *label;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkColorButton");

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_widget_set_margin_top (hbox, 5);
      gtk_widget_set_margin_bottom (hbox, 5);
      gtk_widget_set_margin_start (hbox, 5);
      gtk_widget_set_margin_end (hbox, 5);
      gtk_window_set_child (GTK_WINDOW (window), hbox);
      
      label = gtk_label_new ("Pick a color");
      gtk_box_append (GTK_BOX (hbox), label);

      picker = gtk_color_button_new ();
      gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (picker), TRUE);
      gtk_box_append (GTK_BOX (hbox), picker);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

static void
flipping_toggled_cb (GtkWidget *widget, gpointer data)
{
  int state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  int new_direction = state ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;

  gtk_widget_set_default_direction (new_direction);
}

static void
orientable_toggle_orientation (GtkOrientable *orientable)
{
  GtkOrientation orientation;
  GtkWidget *child;

  orientation = gtk_orientable_get_orientation (orientable);
  gtk_orientable_set_orientation (orientable,
                                  orientation == GTK_ORIENTATION_HORIZONTAL ?
                                  GTK_ORIENTATION_VERTICAL :
                                  GTK_ORIENTATION_HORIZONTAL);


  for (child = gtk_widget_get_first_child (GTK_WIDGET (orientable));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_ORIENTABLE (child))
        orientable_toggle_orientation (GTK_ORIENTABLE (child));
    }
}

static void
flipping_orientation_toggled_cb (GtkWidget *widget, gpointer data)
{
  GtkWidget *content_area;
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (toplevel));
  orientable_toggle_orientation (GTK_ORIENTABLE (content_area));
}

static void
set_direction_recurse (GtkWidget *widget,
                       gpointer   data)
{
  GtkTextDirection *dir = data;
  GtkWidget *child;

  gtk_widget_set_direction (widget, *dir);
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    set_direction_recurse (child, data);
}

static GtkWidget *
create_forward_back (const char       *title,
		     GtkTextDirection  text_dir)
{
  GtkWidget *frame = gtk_frame_new (title);
  GtkWidget *bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *back_button = gtk_button_new_with_label ("Back");
  GtkWidget *forward_button = gtk_button_new_with_label ("Forward");

  gtk_frame_set_child (GTK_FRAME (frame), bbox);
  gtk_box_append (GTK_BOX (bbox), back_button);
  gtk_box_append (GTK_BOX (bbox), forward_button);

  set_direction_recurse (frame, &text_dir);

  return frame;
}

static void
create_flipping (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *check_button;
  GtkWidget *content_area;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "Bidirectional Flipping");

      check_button = gtk_check_button_new_with_label ("Right-to-left global direction");
      gtk_box_append (GTK_BOX (content_area), check_button);

      if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);

      g_signal_connect (check_button, "toggled",
			G_CALLBACK (flipping_toggled_cb), NULL);

      check_button = gtk_check_button_new_with_label ("Toggle orientation of all boxes");
      gtk_box_append (GTK_BOX (content_area), check_button);

      g_signal_connect (check_button, "toggled",
			G_CALLBACK (flipping_orientation_toggled_cb), NULL);

      gtk_box_append (GTK_BOX (content_area),
			  create_forward_back ("Default", GTK_TEXT_DIR_NONE));

      gtk_box_append (GTK_BOX (content_area),
			  create_forward_back ("Left-to-Right", GTK_TEXT_DIR_LTR));

      gtk_box_append (GTK_BOX (content_area),
			  create_forward_back ("Right-to-Left", GTK_TEXT_DIR_RTL));

      gtk_dialog_add_button (GTK_DIALOG (window), "Close", GTK_RESPONSE_CLOSE);
      g_signal_connect (window, "response", G_CALLBACK (gtk_window_destroy), NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkFontSelection
 */

static void
create_font_selection (GtkWidget *widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *picker;
      GtkWidget *hbox;
      GtkWidget *label;
      
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkFontButton");

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_widget_set_margin_top (hbox, 8);
      gtk_widget_set_margin_bottom (hbox, 8);
      gtk_widget_set_margin_start (hbox, 8);
      gtk_widget_set_margin_end (hbox, 8);
      gtk_window_set_child (GTK_WINDOW (window), hbox);
      
      label = gtk_label_new ("Pick a font");
      gtk_box_append (GTK_BOX (hbox), label);

      picker = gtk_font_button_new ();
      gtk_font_button_set_use_font (GTK_FONT_BUTTON (picker), TRUE);
      gtk_box_append (GTK_BOX (hbox), picker);
    }
  
  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkDialog
 */

static GtkWidget *dialog_window = NULL;

static void
dialog_response_cb (GtkWidget *widget, int response, gpointer unused)
{
  GtkWidget *content_area;
  GtkWidget *child;

  if (response == GTK_RESPONSE_APPLY)
    {
      content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog_window));

      for (child = gtk_widget_get_first_child (content_area);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          if (GTK_IS_LABEL (child))
            {
              gtk_box_remove (GTK_BOX (content_area), child);
              break;
            }
        }

      /* no label removed, so add one */
      if (child == NULL)
        {
          GtkWidget *label;

          label = gtk_label_new ("Dialog Test");
          gtk_widget_set_margin_start (label, 10);
          gtk_widget_set_margin_end (label, 10);
          gtk_widget_set_margin_top (label, 10);
          gtk_widget_set_margin_bottom (label, 10);
          gtk_box_append (GTK_BOX (content_area), label);
        }
    }
}

static void
create_dialog (GtkWidget *widget)
{
  if (!dialog_window)
    {
      /* This is a terrible example; it's much simpler to create
       * dialogs than this. Don't use testgtk for example code,
       * use gtk-demo ;-)
       */
      
      dialog_window = gtk_dialog_new ();
      gtk_window_set_display (GTK_WINDOW (dialog_window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (dialog_window), (gpointer *)&dialog_window);

      gtk_window_set_title (GTK_WINDOW (dialog_window), "GtkDialog");

      gtk_dialog_add_button (GTK_DIALOG (dialog_window),
                             "OK",
                             GTK_RESPONSE_OK);

      gtk_dialog_add_button (GTK_DIALOG (dialog_window),
                             "Toggle",
                             GTK_RESPONSE_APPLY);
      
      g_signal_connect (dialog_window, "response",
			G_CALLBACK (dialog_response_cb),
			NULL);
    }

  if (!gtk_widget_get_visible (dialog_window))
    gtk_widget_show (dialog_window);
  else
    gtk_window_destroy (GTK_WINDOW (dialog_window));
}

/* Display & Screen test 
 */

typedef struct
{
  GtkWidget *combo;
  GtkWidget *entry;
  GtkWidget *toplevel;
  GtkWidget *dialog_window;
} ScreenDisplaySelection;

static void
screen_display_check (GtkWidget *widget, ScreenDisplaySelection *data)
{
  const char *display_name;
  GdkDisplay *display;
  GtkWidget *dialog;
  GdkDisplay *new_display = NULL;
  GdkDisplay *current_display = gtk_widget_get_display (widget);
  
  display_name = gtk_editable_get_text (GTK_EDITABLE (data->entry));
  display = gdk_display_open (display_name);
      
  if (!display)
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_root (widget)),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       "The display :\n%s\ncannot be opened",
                                       display_name);
      gtk_window_set_display (GTK_WINDOW (dialog), current_display);
      gtk_widget_show (dialog);
      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_window_destroy),
                        NULL);
    }
  else
    {
      GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (data->combo));
      int i = 0;
      GtkTreeIter iter;
      gboolean found = FALSE;
      while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i++))
        {
          char *name;
          gtk_tree_model_get (model, &iter, 0, &name, -1);
          found = !g_ascii_strcasecmp (display_name, name);
          g_free (name);

          if (found)
            break;
        }
      if (!found)
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (data->combo), display_name);
      new_display = display;

      gtk_window_set_display (GTK_WINDOW (data->toplevel), new_display);
      gtk_window_destroy (GTK_WINDOW (data->dialog_window));
    }
}

static void
screen_display_destroy_diag (GtkWidget *widget, GtkWidget *data)
{
  gtk_window_destroy (GTK_WINDOW (data));
}

static void
create_display_screen (GtkWidget *widget)
{
  GtkWidget *grid, *frame, *window, *combo_dpy, *vbox;
  GtkWidget *label_dpy, *applyb, *cancelb;
  GtkWidget *bbox;
  ScreenDisplaySelection *scr_dpy_data;
  GdkDisplay *display = gtk_widget_get_display (widget);

  window = g_object_new (gtk_window_get_type (),
			 "display", display,
			 "title", "Screen or Display selection",
                         NULL);
  g_signal_connect (window, "destroy", 
		    G_CALLBACK (gtk_window_destroy), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_window_set_child (GTK_WINDOW (window), vbox);
  
  frame = gtk_frame_new ("Select display");
  gtk_box_append (GTK_BOX (vbox), frame);
  
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 3);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 3);

  gtk_frame_set_child (GTK_FRAME (frame), grid);

  label_dpy = gtk_label_new ("move to another X display");
  combo_dpy = gtk_combo_box_text_new_with_entry ();
  gtk_widget_set_hexpand (combo_dpy, TRUE);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_dpy), "diabolo:0.0");
  gtk_editable_set_text (GTK_EDITABLE (gtk_combo_box_get_child (GTK_COMBO_BOX (combo_dpy))),
                         "<hostname>:<X Server Num>.<Screen Num>");

  gtk_grid_attach (GTK_GRID (grid), label_dpy, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), combo_dpy, 0, 1, 1, 1);

  bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (bbox, GTK_ALIGN_END);
  applyb = gtk_button_new_with_mnemonic ("_Apply");
  cancelb = gtk_button_new_with_mnemonic ("_Cancel");

  gtk_box_append (GTK_BOX (vbox), bbox);

  gtk_box_append (GTK_BOX (bbox), applyb);
  gtk_box_append (GTK_BOX (bbox), cancelb);

  scr_dpy_data = g_new0 (ScreenDisplaySelection, 1);

  scr_dpy_data->entry = gtk_combo_box_get_child (GTK_COMBO_BOX (combo_dpy));
  scr_dpy_data->toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  scr_dpy_data->dialog_window = window;

  g_signal_connect (cancelb, "clicked", 
		    G_CALLBACK (screen_display_destroy_diag), window);
  g_signal_connect (applyb, "clicked", 
		    G_CALLBACK (screen_display_check), scr_dpy_data);
  gtk_window_present (GTK_WINDOW (window));
}

/*
 * GtkRange
 */

static char *
reformat_value (GtkScale *scale,
                double    value)
{
  return g_strdup_printf ("-->%0.*g<--",
                          gtk_scale_get_digits (scale), value);
}

static void
create_range_controls (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *scrollbar;
  GtkWidget *scale;
  GtkWidget *separator;
  GtkAdjustment *adjustment;
  GtkWidget *hbox;

  if (!window)
    {
      window = gtk_window_new ();

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "range controls");


      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);


      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);


      adjustment = gtk_adjustment_new (0.0, 0.0, 101.0, 0.1, 1.0, 1.0);

      scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (GTK_WIDGET (scale), 150, -1);
      gtk_scale_set_digits (GTK_SCALE (scale), 1);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_append (GTK_BOX (box2), scale);

      scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (adjustment));
      gtk_box_append (GTK_BOX (box2), scrollbar);

      scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_scale_set_format_value_func (GTK_SCALE (scale),
                                       (GtkScaleFormatValueFunc) reformat_value,
                                       NULL, NULL);
      gtk_box_append (GTK_BOX (box2), scale);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

      scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (scale, -1, 200);
      gtk_scale_set_digits (GTK_SCALE (scale), 2);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_append (GTK_BOX (hbox), scale);

      scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_size_request (scale, -1, 200);
      gtk_scale_set_digits (GTK_SCALE (scale), 2);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_range_set_inverted (GTK_RANGE (scale), TRUE);
      gtk_box_append (GTK_BOX (hbox), scale);

      scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_scale_set_format_value_func (GTK_SCALE (scale),
                                       (GtkScaleFormatValueFunc) reformat_value,
                                       NULL, NULL);
      gtk_box_append (GTK_BOX (hbox), scale);


      gtk_box_append (GTK_BOX (box2), hbox);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);


      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);


      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (box2), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkNotebook
 */

static const char * book_open_xpm[] = {
"16 16 4 1",
"       c None s None",
".      c black",
"X      c #808080",
"o      c white",
"                ",
"  ..            ",
" .Xo.    ...    ",
" .Xoo. ..oo.    ",
" .Xooo.Xooo...  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
"  .Xoo.Xoo..X.  ",
"   .Xo.o..ooX.  ",
"    .X..XXXXX.  ",
"    ..X.......  ",
"     ..         ",
"                "};

static const char * book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "};

GdkPixbuf *book_open;
GdkPixbuf *book_closed;
GtkWidget *sample_notebook;

static void
set_page_image (GtkNotebook *notebook, int page_num, GdkPixbuf *pixbuf)
{
  GtkWidget *page_widget;
  GtkWidget *pixwid;

  page_widget = gtk_notebook_get_nth_page (notebook, page_num);

  pixwid = g_object_get_data (G_OBJECT (page_widget), "tab_pixmap");
  gtk_image_set_from_pixbuf (GTK_IMAGE (pixwid), pixbuf);
  
  pixwid = g_object_get_data (G_OBJECT (page_widget), "menu_pixmap");
  gtk_image_set_from_pixbuf (GTK_IMAGE (pixwid), pixbuf);
}

static void
page_switch (GtkWidget *widget, gpointer *page, int page_num)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  int old_page_num = gtk_notebook_get_current_page (notebook);
 
  if (page_num == old_page_num)
    return;

  set_page_image (notebook, page_num, book_open);

  if (old_page_num != -1)
    set_page_image (notebook, old_page_num, book_closed);
}

static void
tab_fill (GtkToggleButton *button, GtkWidget *child)
{
  GtkNotebookPage *page = gtk_notebook_get_page (GTK_NOTEBOOK (sample_notebook), child);
  g_object_set (page, "tab-fill", gtk_toggle_button_get_active (button), NULL);
}

static void
tab_expand (GtkToggleButton *button, GtkWidget *child)
{
  GtkNotebookPage *page = gtk_notebook_get_page (GTK_NOTEBOOK (sample_notebook), child);
  g_object_set (page, "tab-expand", gtk_toggle_button_get_active (button), NULL);
}

static void
create_pages (GtkNotebook *notebook, int start, int end)
{
  GtkWidget *child = NULL;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *label_box;
  GtkWidget *menu_box;
  GtkWidget *pixwid;
  int i;
  char buffer[32];
  char accel_buffer[32];

  for (i = start; i <= end; i++)
    {
      sprintf (buffer, "Page %d", i);
      sprintf (accel_buffer, "Page _%d", i);

      child = gtk_frame_new (buffer);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
      gtk_frame_set_child (GTK_FRAME (child), vbox);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
      gtk_box_append (GTK_BOX (vbox), hbox);

      button = gtk_check_button_new_with_label ("Fill Tab");
      gtk_box_append (GTK_BOX (hbox), button);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      g_signal_connect (button, "toggled",
			G_CALLBACK (tab_fill), child);

      button = gtk_check_button_new_with_label ("Expand Tab");
      gtk_box_append (GTK_BOX (hbox), button);
      g_signal_connect (button, "toggled",
			G_CALLBACK (tab_expand), child);

      button = gtk_button_new_with_label ("Hide Page");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_widget_hide),
				child);

      label_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      pixwid = gtk_image_new_from_pixbuf (book_closed);
      g_object_set_data (G_OBJECT (child), "tab_pixmap", pixwid);

      gtk_box_append (GTK_BOX (label_box), pixwid);
      gtk_widget_set_margin_start (pixwid, 3);
      gtk_widget_set_margin_end (pixwid, 3);
      gtk_widget_set_margin_bottom (pixwid, 1);
      gtk_widget_set_margin_top (pixwid, 1);
      label = gtk_label_new_with_mnemonic (accel_buffer);
      gtk_box_append (GTK_BOX (label_box), label);


      menu_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      pixwid = gtk_image_new_from_pixbuf (book_closed);
      g_object_set_data (G_OBJECT (child), "menu_pixmap", pixwid);

      gtk_box_append (GTK_BOX (menu_box), pixwid);
      gtk_widget_set_margin_start (pixwid, 3);
      gtk_widget_set_margin_end (pixwid, 3);
      gtk_widget_set_margin_bottom (pixwid, 1);
      gtk_widget_set_margin_top (pixwid, 1);
      label = gtk_label_new (buffer);
      gtk_box_append (GTK_BOX (menu_box), label);

      gtk_notebook_append_page_menu (notebook, child, label_box, menu_box);
    }
}

static void
rotate_notebook (GtkButton   *button,
		 GtkNotebook *notebook)
{
  gtk_notebook_set_tab_pos (notebook, (gtk_notebook_get_tab_pos (notebook) + 1) % 4);
}

static void
show_all_pages (GtkButton   *button,
		GtkNotebook *notebook)
{
  int i;

  for (i = 0; i < gtk_notebook_get_n_pages (notebook); i++)
    gtk_widget_show (gtk_notebook_get_nth_page (notebook, i)); 
}

static void
notebook_type_changed (GtkWidget *optionmenu,
		       gpointer   data)
{
  GtkNotebook *notebook;
  int i, c;

  enum {
    STANDARD,
    NOTABS,
    BORDERLESS,
    SCROLLABLE
  };

  notebook = GTK_NOTEBOOK (data);

  c = gtk_combo_box_get_active (GTK_COMBO_BOX (optionmenu));

  switch (c)
    {
    case STANDARD:
      /* standard notebook */
      gtk_notebook_set_show_tabs (notebook, TRUE);
      gtk_notebook_set_show_border (notebook, TRUE);
      gtk_notebook_set_scrollable (notebook, FALSE);
      break;

    case NOTABS:
      /* notabs notebook */
      gtk_notebook_set_show_tabs (notebook, FALSE);
      gtk_notebook_set_show_border (notebook, TRUE);
      break;

    case BORDERLESS:
      /* borderless */
      gtk_notebook_set_show_tabs (notebook, FALSE);
      gtk_notebook_set_show_border (notebook, FALSE);
      break;

    case SCROLLABLE:  
      /* scrollable */
      gtk_notebook_set_show_tabs (notebook, TRUE);
      gtk_notebook_set_show_border (notebook, TRUE);
      gtk_notebook_set_scrollable (notebook, TRUE);
      if (gtk_notebook_get_n_pages (notebook) == 5)
	create_pages (notebook, 6, 15);

      return;
      break;
    default:
      g_assert_not_reached ();
    }

  if (gtk_notebook_get_n_pages (notebook) == 15)
    for (i = 0; i < 10; i++)
      gtk_notebook_remove_page (notebook, 5);
}

static void
notebook_popup (GtkToggleButton *button,
		GtkNotebook     *notebook)
{
  if (gtk_toggle_button_get_active (button))
    gtk_notebook_popup_enable (notebook);
  else
    gtk_notebook_popup_disable (notebook);
}

static void
create_notebook (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *omenu;
  GtkWidget *label;

  static const char *items[] =
  {
    "Standard",
    "No tabs",
    "Borderless",
    "Scrollable"
  };
  
  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "notebook");

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      sample_notebook = gtk_notebook_new ();
      g_signal_connect (sample_notebook, "switch_page",
                        G_CALLBACK (page_switch), NULL);
      gtk_notebook_set_tab_pos (GTK_NOTEBOOK (sample_notebook), GTK_POS_TOP);
      gtk_widget_set_vexpand (sample_notebook, TRUE);
      gtk_box_append (GTK_BOX (box1), sample_notebook);

      gtk_widget_realize (sample_notebook);

      if (!book_open)
	book_open = gdk_pixbuf_new_from_xpm_data (book_open_xpm);

      if (!book_closed)
	book_closed = gdk_pixbuf_new_from_xpm_data (book_closed_xpm);

      create_pages (GTK_NOTEBOOK (sample_notebook), 1, 5);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_check_button_new_with_label ("popup menu");
      gtk_box_append (GTK_BOX (box2), button);
      g_signal_connect (button, "clicked",
			G_CALLBACK (notebook_popup),
			sample_notebook);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_box_append (GTK_BOX (box1), box2);

      label = gtk_label_new ("Notebook Style :");
      gtk_box_append (GTK_BOX (box2), label);

      omenu = build_option_menu (items, G_N_ELEMENTS (items), 0,
				 notebook_type_changed,
				 sample_notebook);
      gtk_box_append (GTK_BOX (box2), omenu);

      button = gtk_button_new_with_label ("Show all Pages");
      gtk_box_append (GTK_BOX (box2), button);
      g_signal_connect (button, "clicked",
			G_CALLBACK (show_all_pages), sample_notebook);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_set_homogeneous (GTK_BOX (box2), TRUE);
      gtk_box_append (GTK_BOX (box1), box2);

      button = gtk_button_new_with_label ("prev");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_notebook_prev_page),
				sample_notebook);
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_button_new_with_label ("next");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_notebook_next_page),
				sample_notebook);
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_button_new_with_label ("rotate");
      g_signal_connect (button, "clicked",
			G_CALLBACK (rotate_notebook), sample_notebook);
      gtk_box_append (GTK_BOX (box2), button);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
			        G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (box1), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkPanes
 */

static void
toggle_resize (GtkWidget *widget, GtkWidget *child)
{
  GtkPaned *paned = GTK_PANED (gtk_widget_get_parent (child));

  if (child == gtk_paned_get_start_child (paned))
    gtk_paned_set_resize_start_child (paned, !gtk_paned_get_resize_start_child (paned));
  else
    gtk_paned_set_resize_end_child (paned, !gtk_paned_get_resize_end_child (paned));
}

static void
toggle_shrink (GtkWidget *widget, GtkWidget *child)
{
  GtkPaned *paned = GTK_PANED (gtk_widget_get_parent (child));

  if (child == gtk_paned_get_start_child (paned))
    gtk_paned_set_shrink_start_child (paned, !gtk_paned_get_shrink_start_child (paned));
  else
    gtk_paned_set_shrink_end_child (paned, !gtk_paned_get_shrink_end_child (paned));
}

static GtkWidget *
create_pane_options (GtkPaned    *paned,
		     const char *frame_label,
		     const char *label1,
		     const char *label2)
{
  GtkWidget *child1, *child2;
  GtkWidget *frame;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *check_button;

  child1 = gtk_paned_get_start_child (paned);
  child2 = gtk_paned_get_end_child (paned);

  frame = gtk_frame_new (frame_label);
  
  grid = gtk_grid_new ();
  gtk_frame_set_child (GTK_FRAME (frame), grid);
  
  label = gtk_label_new (label1);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  
  check_button = gtk_check_button_new_with_label ("Resize");
  gtk_grid_attach (GTK_GRID (grid), check_button, 0, 1, 1, 1);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_resize),
                    child1);

  check_button = gtk_check_button_new_with_label ("Shrink");
  gtk_grid_attach (GTK_GRID (grid), check_button, 0, 2, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_shrink),
                    child1);

  label = gtk_label_new (label2);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  
  check_button = gtk_check_button_new_with_label ("Resize");
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 1, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_resize),
                    child2);

  check_button = gtk_check_button_new_with_label ("Shrink");
  gtk_grid_attach (GTK_GRID (grid), check_button, 1, 2, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (toggle_shrink),
                    child2);

  return frame;
}

static void
create_panes (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hpaned;
  GtkWidget *vpaned;
  GtkWidget *button;
  GtkWidget *vbox;

  if (!window)
    {
      window = gtk_window_new ();

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "Panes");

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
      gtk_box_append (GTK_BOX (vbox), vpaned);

      hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_paned_set_start_child (GTK_PANED (vpaned), hpaned);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_size_request (frame, 60, 60);
      gtk_paned_set_start_child (GTK_PANED (hpaned), frame);
      
      button = gtk_button_new_with_label ("Hi there");
      gtk_frame_set_child (GTK_FRAME (frame), button);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_size_request (frame, 80, 60);
      gtk_paned_set_end_child (GTK_PANED (hpaned), frame);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_size_request (frame, 60, 80);
      gtk_paned_set_end_child (GTK_PANED (vpaned), frame);

      /* Now create toggle buttons to control sizing */

      gtk_box_append (GTK_BOX (vbox),
			  create_pane_options (GTK_PANED (hpaned),
					       "Horizontal",
					       "Left",
					       "Right"));

      gtk_box_append (GTK_BOX (vbox),
			  create_pane_options (GTK_PANED (vpaned),
					       "Vertical",
					       "Top",
					       "Bottom"));
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * Paned keyboard navigation
 */

static GtkWidget*
paned_keyboard_window1 (GtkWidget *widget)
{
  GtkWidget *window1;
  GtkWidget *hpaned1;
  GtkWidget *frame1;
  GtkWidget *vbox1;
  GtkWidget *button7;
  GtkWidget *button8;
  GtkWidget *button9;
  GtkWidget *vpaned1;
  GtkWidget *frame2;
  GtkWidget *frame5;
  GtkWidget *hbox1;
  GtkWidget *button5;
  GtkWidget *button6;
  GtkWidget *frame3;
  GtkWidget *frame4;
  GtkWidget *grid1;
  GtkWidget *button1;
  GtkWidget *button2;
  GtkWidget *button3;
  GtkWidget *button4;

  window1 = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window1), "Basic paned navigation");
  gtk_window_set_display (GTK_WINDOW (window1), 
			  gtk_widget_get_display (widget));

  hpaned1 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_window_set_child (GTK_WINDOW (window1), hpaned1);

  frame1 = gtk_frame_new (NULL);
  gtk_paned_set_start_child (GTK_PANED (hpaned1), frame1);
  gtk_paned_set_resize_start_child (GTK_PANED (hpaned1), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (hpaned1), TRUE);

  vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_frame_set_child (GTK_FRAME (frame1), vbox1);

  button7 = gtk_button_new_with_label ("button7");
  gtk_box_append (GTK_BOX (vbox1), button7);

  button8 = gtk_button_new_with_label ("button8");
  gtk_box_append (GTK_BOX (vbox1), button8);

  button9 = gtk_button_new_with_label ("button9");
  gtk_box_append (GTK_BOX (vbox1), button9);

  vpaned1 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_paned_set_end_child (GTK_PANED (hpaned1), vpaned1);
  gtk_paned_set_resize_end_child (GTK_PANED (hpaned1), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (hpaned1), TRUE);

  frame2 = gtk_frame_new (NULL);
  gtk_paned_set_start_child (GTK_PANED (vpaned1), frame2);
  gtk_paned_set_resize_start_child (GTK_PANED (vpaned1), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (vpaned1), TRUE);

  frame5 = gtk_frame_new (NULL);
  gtk_frame_set_child (GTK_FRAME (frame2), frame5);

  hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_frame_set_child (GTK_FRAME (frame5), hbox1);

  button5 = gtk_button_new_with_label ("button5");
  gtk_box_append (GTK_BOX (hbox1), button5);

  button6 = gtk_button_new_with_label ("button6");
  gtk_box_append (GTK_BOX (hbox1), button6);

  frame3 = gtk_frame_new (NULL);
  gtk_paned_set_end_child (GTK_PANED (vpaned1), frame3);
  gtk_paned_set_resize_end_child (GTK_PANED (vpaned1), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (vpaned1), TRUE);

  frame4 = gtk_frame_new ("Buttons");
  gtk_frame_set_child (GTK_FRAME (frame3), frame4);

  grid1 = gtk_grid_new ();
  gtk_frame_set_child (GTK_FRAME (frame4), grid1);

  button1 = gtk_button_new_with_label ("button1");
  gtk_grid_attach (GTK_GRID (grid1), button1, 0, 0, 1, 1);

  button2 = gtk_button_new_with_label ("button2");
  gtk_grid_attach (GTK_GRID (grid1), button2, 1, 0, 1, 1);

  button3 = gtk_button_new_with_label ("button3");
  gtk_grid_attach (GTK_GRID (grid1), button3, 0, 1, 1, 1);

  button4 = gtk_button_new_with_label ("button4");
  gtk_grid_attach (GTK_GRID (grid1), button4, 1, 1, 1, 1);

  return window1;
}

static GtkWidget*
paned_keyboard_window2 (GtkWidget *widget)
{
  GtkWidget *window2;
  GtkWidget *hpaned2;
  GtkWidget *frame6;
  GtkWidget *button13;
  GtkWidget *hbox2;
  GtkWidget *vpaned2;
  GtkWidget *frame7;
  GtkWidget *button12;
  GtkWidget *frame8;
  GtkWidget *button11;
  GtkWidget *button10;

  window2 = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window2), "\"button 10\" is not inside the horizontal pane");

  gtk_window_set_display (GTK_WINDOW (window2), 
			  gtk_widget_get_display (widget));

  hpaned2 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_window_set_child (GTK_WINDOW (window2), hpaned2);

  frame6 = gtk_frame_new (NULL);
  gtk_paned_set_start_child (GTK_PANED (hpaned2), frame6);
  gtk_paned_set_resize_start_child (GTK_PANED (hpaned2), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (hpaned2), TRUE);

  button13 = gtk_button_new_with_label ("button13");
  gtk_frame_set_child (GTK_FRAME (frame6), button13);

  hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_paned_set_end_child (GTK_PANED (hpaned2), hbox2);
  gtk_paned_set_resize_end_child (GTK_PANED (hpaned2), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (hpaned2), TRUE);

  vpaned2 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (hbox2), vpaned2);

  frame7 = gtk_frame_new (NULL);
  gtk_paned_set_start_child (GTK_PANED (vpaned2), frame7);
  gtk_paned_set_resize_start_child (GTK_PANED (vpaned2), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (vpaned2), TRUE);

  button12 = gtk_button_new_with_label ("button12");
  gtk_frame_set_child (GTK_FRAME (frame7), button12);

  frame8 = gtk_frame_new (NULL);
  gtk_paned_set_end_child (GTK_PANED (vpaned2), frame8);
  gtk_paned_set_resize_end_child (GTK_PANED (vpaned2), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (vpaned2), TRUE);

  button11 = gtk_button_new_with_label ("button11");
  gtk_frame_set_child (GTK_FRAME (frame8), button11);

  button10 = gtk_button_new_with_label ("button10");
  gtk_box_append (GTK_BOX (hbox2), button10);

  return window2;
}

static GtkWidget*
paned_keyboard_window3 (GtkWidget *widget)
{
  GtkWidget *window3;
  GtkWidget *vbox2;
  GtkWidget *label1;
  GtkWidget *hpaned3;
  GtkWidget *frame9;
  GtkWidget *button14;
  GtkWidget *hpaned4;
  GtkWidget *frame10;
  GtkWidget *button15;
  GtkWidget *hpaned5;
  GtkWidget *frame11;
  GtkWidget *button16;
  GtkWidget *frame12;
  GtkWidget *button17;

  window3 = gtk_window_new ();
  g_object_set_data (G_OBJECT (window3), "window3", window3);
  gtk_window_set_title (GTK_WINDOW (window3), "Nested panes");

  gtk_window_set_display (GTK_WINDOW (window3),
			  gtk_widget_get_display (widget));


  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window3), vbox2);

  label1 = gtk_label_new ("Three panes nested inside each other");
  gtk_box_append (GTK_BOX (vbox2), label1);

  hpaned3 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_append (GTK_BOX (vbox2), hpaned3);

  frame9 = gtk_frame_new (NULL);
  gtk_paned_set_start_child (GTK_PANED (hpaned3), frame9);
  gtk_paned_set_resize_start_child (GTK_PANED (hpaned3), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (hpaned3), TRUE);

  button14 = gtk_button_new_with_label ("button14");
  gtk_frame_set_child (GTK_FRAME (frame9), button14);

  hpaned4 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_end_child (GTK_PANED (hpaned3), hpaned4);
  gtk_paned_set_resize_end_child (GTK_PANED (hpaned3), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (hpaned3), TRUE);

  frame10 = gtk_frame_new (NULL);
  gtk_paned_set_start_child (GTK_PANED (hpaned4), frame10);
  gtk_paned_set_resize_start_child (GTK_PANED (hpaned4), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (hpaned4), TRUE);

  button15 = gtk_button_new_with_label ("button15");
  gtk_frame_set_child (GTK_FRAME (frame10), button15);

  hpaned5 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_end_child (GTK_PANED (hpaned4), hpaned5);
  gtk_paned_set_resize_end_child (GTK_PANED (hpaned4), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (hpaned4), TRUE);

  frame11 = gtk_frame_new (NULL);
  gtk_paned_set_start_child (GTK_PANED (hpaned5), frame11);
  gtk_paned_set_resize_start_child (GTK_PANED (hpaned5), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (hpaned5), TRUE);

  button16 = gtk_button_new_with_label ("button16");
  gtk_frame_set_child (GTK_FRAME (frame11), button16);

  frame12 = gtk_frame_new (NULL);
  gtk_paned_set_end_child (GTK_PANED (hpaned5), frame12);
  gtk_paned_set_resize_end_child (GTK_PANED (hpaned5), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (hpaned5), TRUE);

  button17 = gtk_button_new_with_label ("button17");
  gtk_frame_set_child (GTK_FRAME (frame12), button17);

  return window3;
}

static GtkWidget*
paned_keyboard_window4 (GtkWidget *widget)
{
  GtkWidget *window4;
  GtkWidget *vbox3;
  GtkWidget *label2;
  GtkWidget *hpaned6;
  GtkWidget *vpaned3;
  GtkWidget *button19;
  GtkWidget *button18;
  GtkWidget *hbox3;
  GtkWidget *vpaned4;
  GtkWidget *button21;
  GtkWidget *button20;
  GtkWidget *vpaned5;
  GtkWidget *button23;
  GtkWidget *button22;
  GtkWidget *vpaned6;
  GtkWidget *button25;
  GtkWidget *button24;

  window4 = gtk_window_new ();
  g_object_set_data (G_OBJECT (window4), "window4", window4);
  gtk_window_set_title (GTK_WINDOW (window4), "window4");

  gtk_window_set_display (GTK_WINDOW (window4),
                          gtk_widget_get_display (widget));

  vbox3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window4), vbox3);

  label2 = gtk_label_new ("Widget tree:\n\nhpaned \n - vpaned\n - hbox\n    - vpaned\n    - vpaned\n    - vpaned\n");
  gtk_box_append (GTK_BOX (vbox3), label2);
  gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);

  hpaned6 = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_append (GTK_BOX (vbox3), hpaned6);

  vpaned3 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_paned_set_start_child (GTK_PANED (hpaned6), vpaned3);
  gtk_paned_set_resize_start_child (GTK_PANED (hpaned6), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (hpaned6), TRUE);

  button19 = gtk_button_new_with_label ("button19");
  gtk_paned_set_start_child (GTK_PANED (vpaned3), button19);
  gtk_paned_set_resize_start_child (GTK_PANED (vpaned3), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (vpaned3), TRUE);

  button18 = gtk_button_new_with_label ("button18");
  gtk_paned_set_end_child (GTK_PANED (vpaned3), button18);
  gtk_paned_set_resize_end_child (GTK_PANED (vpaned3), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (vpaned3), TRUE);

  hbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_paned_set_end_child (GTK_PANED (hpaned6), hbox3);
  gtk_paned_set_resize_end_child (GTK_PANED (hpaned6), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (hpaned6), TRUE);

  vpaned4 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (hbox3), vpaned4);

  button21 = gtk_button_new_with_label ("button21");
  gtk_paned_set_start_child (GTK_PANED (vpaned4), button21);
  gtk_paned_set_resize_start_child (GTK_PANED (vpaned4), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (vpaned4), TRUE);

  button20 = gtk_button_new_with_label ("button20");
  gtk_paned_set_end_child (GTK_PANED (vpaned4), button20);
  gtk_paned_set_resize_end_child (GTK_PANED (vpaned4), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (vpaned4), TRUE);

  vpaned5 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (hbox3), vpaned5);

  button23 = gtk_button_new_with_label ("button23");
  gtk_paned_set_start_child (GTK_PANED (vpaned5), button23);
  gtk_paned_set_resize_start_child (GTK_PANED (vpaned5), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (vpaned5), TRUE);

  button22 = gtk_button_new_with_label ("button22");
  gtk_paned_set_end_child (GTK_PANED (vpaned5), button22);
  gtk_paned_set_resize_end_child (GTK_PANED (vpaned5), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (vpaned5), TRUE);

  vpaned6 = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (hbox3), vpaned6);

  button25 = gtk_button_new_with_label ("button25");
  gtk_paned_set_start_child (GTK_PANED (vpaned6), button25);
  gtk_paned_set_resize_start_child (GTK_PANED (vpaned6), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (vpaned6), TRUE);

  button24 = gtk_button_new_with_label ("button24");
  gtk_paned_set_end_child (GTK_PANED (vpaned6), button24);
  gtk_paned_set_resize_end_child (GTK_PANED (vpaned6), TRUE);
  gtk_paned_set_shrink_end_child (GTK_PANED (vpaned6), TRUE);

  return window4;
}

static void
create_paned_keyboard_navigation (GtkWidget *widget)
{
  static GtkWidget *window1 = NULL;
  static GtkWidget *window2 = NULL;
  static GtkWidget *window3 = NULL;
  static GtkWidget *window4 = NULL;

  if (window1 && 
     (gtk_widget_get_display (window1) != gtk_widget_get_display (widget)))
    {
      gtk_window_destroy (GTK_WINDOW (window1));
      gtk_window_destroy (GTK_WINDOW (window2));
      gtk_window_destroy (GTK_WINDOW (window3));
      gtk_window_destroy (GTK_WINDOW (window4));
    }
  
  if (!window1)
    {
      window1 = paned_keyboard_window1 (widget);
      g_object_add_weak_pointer (G_OBJECT (window1), (gpointer *)&window1);
    }

  if (!window2)
    {
      window2 = paned_keyboard_window2 (widget);
      g_object_add_weak_pointer (G_OBJECT (window2), (gpointer *)&window2);
    }

  if (!window3)
    {
      window3 = paned_keyboard_window3 (widget);
      g_object_add_weak_pointer (G_OBJECT (window3), (gpointer *)&window3);
    }

  if (!window4)
    {
      window4 = paned_keyboard_window4 (widget);
      g_object_add_weak_pointer (G_OBJECT (window4), (gpointer *)&window4);
    }

  if (gtk_widget_get_visible (window1))
    gtk_window_destroy (GTK_WINDOW (window1));
  else
    gtk_widget_show (GTK_WIDGET (window1));

  if (gtk_widget_get_visible (window2))
    gtk_window_destroy (GTK_WINDOW (window2));
  else
    gtk_widget_show (GTK_WIDGET (window2));

  if (gtk_widget_get_visible (window3))
    gtk_window_destroy (GTK_WINDOW (window3));
  else
    gtk_widget_show (GTK_WIDGET (window3));

  if (gtk_widget_get_visible (window4))
    gtk_window_destroy (GTK_WINDOW (window4));
  else
    gtk_widget_show (GTK_WIDGET (window4));
}

/*
 * WM Hints demo
 */

static void
create_wmhints (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *button;
  GtkWidget *box1;
  GtkWidget *box2;
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GList *list;

  if (!window)
    {
      window = gtk_window_new ();

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "WM Hints");

      gtk_widget_realize (window);

      pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) openfile);
      texture = gdk_texture_new_for_pixbuf (pixbuf);

      list = g_list_prepend (NULL, texture);

      g_list_free (list);
      g_object_unref (texture);
      g_object_unref (pixbuf);

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      label = gtk_label_new ("Try iconizing me!");
      gtk_widget_set_size_request (label, 150, 50);

      gtk_box_append (GTK_BOX (box1), label);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box1), separator);


      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (box1), box2);


      button = gtk_button_new_with_label ("close");

      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_window_destroy),
				window);

      gtk_box_append (GTK_BOX (box2), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * Window state tracking
 */

static void
surface_state_callback (GdkSurface  *window,
                       GParamSpec *pspec,
                       GtkWidget  *label)
{
  char *msg;
  GdkToplevelState new_state;

  new_state = gdk_toplevel_get_state (GDK_TOPLEVEL (window));
  msg = g_strconcat ((const char *)g_object_get_data (G_OBJECT (label), "title"), ": ",
                     (new_state & GDK_TOPLEVEL_STATE_MINIMIZED) ?
                     "minimized" : "not minimized", ", ",
                     (new_state & GDK_TOPLEVEL_STATE_SUSPENDED) ?
                     "suspended" : "not suspended", ", ",
                     (new_state & GDK_TOPLEVEL_STATE_STICKY) ?
                     "sticky" : "not sticky", ", ",
                     (new_state & GDK_TOPLEVEL_STATE_MAXIMIZED) ?
                     "maximized" : "not maximized", ", ",
                     (new_state & GDK_TOPLEVEL_STATE_FULLSCREEN) ?
                     "fullscreen" : "not fullscreen", ", ",
                     (new_state & GDK_TOPLEVEL_STATE_ABOVE) ?
                     "above" : "not above", ", ",
                     (new_state & GDK_TOPLEVEL_STATE_BELOW) ?
                     "below" : "not below", ", ",
                     NULL);

  gtk_label_set_text (GTK_LABEL (label), msg);

  g_free (msg);
}

static GtkWidget*
tracking_label (GtkWidget *window)
{
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *button;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);

  g_signal_connect_object (hbox,
			   "destroy",
			   G_CALLBACK (gtk_window_destroy),
			   window,
			   G_CONNECT_SWAPPED);

  label = gtk_label_new ("<no window state events received>");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_box_append (GTK_BOX (hbox), label);

  g_object_set_data (G_OBJECT (label), "title", (gpointer)gtk_window_get_title (GTK_WINDOW (window)));
  g_signal_connect_object (gtk_native_get_surface (GTK_NATIVE (window)), "notify::state",
                           G_CALLBACK (surface_state_callback),
                           label,
                           0);

  button = gtk_button_new_with_label ("Unminimize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_unminimize),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_button_new_with_label ("Minimize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_minimize),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_button_new_with_label ("Fullscreen");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_fullscreen),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_button_new_with_label ("Unfullscreen");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_unfullscreen),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_button_new_with_label ("Present");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_present),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_button_new_with_label ("Show");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_show),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (hbox), button);

  return hbox;
}

static GtkWidget*
get_state_controls (GtkWidget *window)
{
  GtkWidget *vbox;
  GtkWidget *button;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  button = gtk_button_new_with_label ("Maximize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_maximize),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Unmaximize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_unmaximize),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Minimize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_minimize),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Fullscreen");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_window_fullscreen),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Unfullscreen");
  g_signal_connect_object (button,
			   "clicked",
                           G_CALLBACK (gtk_window_unfullscreen),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Hide (withdraw)");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_hide),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  return vbox;
}

static void
create_surface_states (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *box1;
  GtkWidget *iconified;
  GtkWidget *normal;
  GtkWidget *controls;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (window), "Surface states");
      
      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box1);

      iconified = gtk_window_new ();

      gtk_window_set_display (GTK_WINDOW (iconified),
			      gtk_widget_get_display (widget));
      
      g_signal_connect_object (iconified, "destroy",
			       G_CALLBACK (gtk_window_destroy),
			       window,
			       G_CONNECT_SWAPPED);
      gtk_window_minimize (GTK_WINDOW (iconified));
      gtk_window_set_title (GTK_WINDOW (iconified), "Minimized initially");
      controls = get_state_controls (iconified);
      gtk_window_set_child (GTK_WINDOW (iconified), controls);
      
      normal = gtk_window_new ();

      gtk_window_set_display (GTK_WINDOW (normal),
			      gtk_widget_get_display (widget));
      
      g_signal_connect_object (normal, "destroy",
			       G_CALLBACK (gtk_window_destroy),
			       window,
			       G_CONNECT_SWAPPED);
      
      gtk_window_set_title (GTK_WINDOW (normal), "Unminimized initially");
      controls = get_state_controls (normal);
      gtk_window_set_child (GTK_WINDOW (normal), controls);
      
      gtk_widget_realize (iconified);
      gtk_widget_realize (normal);

      label = tracking_label (iconified);
      gtk_box_append (GTK_BOX (box1), label);

      label = tracking_label (normal);
      gtk_box_append (GTK_BOX (box1), label);

      gtk_widget_show (iconified);
      gtk_widget_show (normal);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * Window sizing
 */

static void
get_ints (GtkWidget *window,
          int       *a,
          int       *b)
{
  GtkWidget *spin1;
  GtkWidget *spin2;

  spin1 = g_object_get_data (G_OBJECT (window), "spin1");
  spin2 = g_object_get_data (G_OBJECT (window), "spin2");

  *a = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin1));
  *b = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin2));
}

static void
unset_default_size_callback (GtkWidget *widget,
                             gpointer   data)
{
  gtk_window_set_default_size (g_object_get_data (data, "target"),
                               -1, -1);
}

static void
set_default_size_callback (GtkWidget *widget,
                           gpointer   data)
{
  int w, h;
  
  get_ints (data, &w, &h);

  gtk_window_set_default_size (g_object_get_data (data, "target"),
                               w, h);
}

static void
unset_size_request_callback (GtkWidget *widget,
			     gpointer   data)
{
  gtk_widget_set_size_request (g_object_get_data (data, "target"),
                               -1, -1);
}

static void
set_size_request_callback (GtkWidget *widget,
			   gpointer   data)
{
  int w, h;
  
  get_ints (data, &w, &h);

  gtk_widget_set_size_request (g_object_get_data (data, "target"),
                               w, h);
}

static void
resizable_callback (GtkWidget *widget,
                     gpointer   data)
{
  g_object_set (g_object_get_data (data, "target"),
                "resizable", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
                NULL);
}

static GtkWidget*
window_controls (GtkWidget *window)
{
  GtkWidget *control_window;
  GtkWidget *label;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *spin;
  GtkAdjustment *adjustment;
  
  control_window = gtk_window_new ();

  gtk_window_set_display (GTK_WINDOW (control_window),
			  gtk_widget_get_display (window));

  gtk_window_set_title (GTK_WINDOW (control_window), "Size controls");
  
  g_object_set_data (G_OBJECT (control_window),
                     "target",
                     window);
  
  g_signal_connect_object (control_window,
			   "destroy",
			   G_CALLBACK (gtk_window_destroy),
                           window,
			   G_CONNECT_SWAPPED);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  gtk_window_set_child (GTK_WINDOW (control_window), vbox);

  label = gtk_label_new ("<no size>");
  gtk_box_append (GTK_BOX (vbox), label);

  adjustment = gtk_adjustment_new (10.0, -2000.0, 2000.0, 1.0, 5.0, 0.0);
  spin = gtk_spin_button_new (adjustment, 0, 0);

  gtk_box_append (GTK_BOX (vbox), spin);

  g_object_set_data (G_OBJECT (control_window), "spin1", spin);

  adjustment = gtk_adjustment_new (10.0, -2000.0, 2000.0, 1.0, 5.0, 0.0);
  spin = gtk_spin_button_new (adjustment, 0, 0);

  gtk_box_append (GTK_BOX (vbox), spin);

  g_object_set_data (G_OBJECT (control_window), "spin2", spin);

  button = gtk_button_new_with_label ("Queue resize");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_queue_resize),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Set default size");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (set_default_size_callback),
		    control_window);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Unset default size");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (unset_default_size_callback),
                    control_window);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Set size request");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (set_size_request_callback),
		    control_window);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_label ("Unset size request");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (unset_size_request_callback),
                    control_window);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_check_button_new_with_label ("Allow resize");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_connect (button,
		    "toggled",
		    G_CALLBACK (resizable_callback),
                    control_window);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_mnemonic ("_Show");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_show),
			   window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  button = gtk_button_new_with_mnemonic ("_Hide");
  g_signal_connect_object (button,
			   "clicked",
			   G_CALLBACK (gtk_widget_hide),
                           window,
			   G_CONNECT_SWAPPED);
  gtk_box_append (GTK_BOX (vbox), button);

  return control_window;
}

static void
create_window_sizing (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  static GtkWidget *target_window = NULL;

  if (!target_window)
    {
      GtkWidget *label;
      
      target_window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (target_window),
			      gtk_widget_get_display (widget));
      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), "<span foreground=\"purple\"><big>Window being resized</big></span>\nBlah blah blah blah\nblah blah blah\nblah blah blah blah blah");
      gtk_window_set_child (GTK_WINDOW (target_window), label);
      gtk_widget_show (target_window);

      g_object_add_weak_pointer (G_OBJECT (target_window), (gpointer *)&target_window);

      window = window_controls (target_window);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      gtk_window_set_title (GTK_WINDOW (target_window), "Window to size");
    }

  /* don't show target window by default, we want to allow testing
   * of behavior on first show.
   */
  
  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * GtkProgressBar
 */

typedef struct _ProgressData {
  GtkWidget *window;
  GtkWidget *pbar;
  GtkWidget *block_spin;
  GtkWidget *x_align_spin;
  GtkWidget *y_align_spin;
  GtkWidget *step_spin;
  GtkWidget *act_blocks_spin;
  GtkWidget *label;
  GtkWidget *omenu1;
  GtkWidget *elmenu;
  GtkWidget *omenu2;
  GtkWidget *entry;
  int timer;
  gboolean activity;
} ProgressData;

static gboolean
progress_timeout (gpointer data)
{
  ProgressData *pdata = data;
  double new_val;
  char *text;

  if (pdata->activity)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pdata->pbar));

      text = g_strdup_printf ("%s", "???");
    }
  else
    {
      new_val = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (pdata->pbar)) + 0.01;
      if (new_val > 1.00)
        new_val = 0.00;
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pdata->pbar), new_val);

      text = g_strdup_printf ("%.0f%%", 100 * new_val);
    }

  gtk_label_set_text (GTK_LABEL (pdata->label), text);
  g_free (text);

  return TRUE;
}

static void
destroy_progress (GtkWidget     *widget,
		  ProgressData **pdata)
{
  if ((*pdata)->timer)
    {
      g_source_remove ((*pdata)->timer);
      (*pdata)->timer = 0;
    }
  (*pdata)->window = NULL;
  g_free (*pdata);
  *pdata = NULL;
}

static void
progressbar_toggle_orientation (GtkWidget *widget, gpointer data)
{
  ProgressData *pdata;
  int i;

  pdata = (ProgressData *) data;

  if (!gtk_widget_get_mapped (widget))
    return;

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  if (i == 0 || i == 1)
    gtk_orientable_set_orientation (GTK_ORIENTABLE (pdata->pbar), GTK_ORIENTATION_HORIZONTAL);
  else
    gtk_orientable_set_orientation (GTK_ORIENTABLE (pdata->pbar), GTK_ORIENTATION_VERTICAL);
 
  if (i == 1 || i == 2)
    gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (pdata->pbar), TRUE);
  else
    gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (pdata->pbar), FALSE);
}

static void
toggle_show_text (GtkWidget *widget, ProgressData *pdata)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (pdata->pbar), active);
}

static void
progressbar_toggle_ellipsize (GtkWidget *widget,
                              gpointer   data)
{
  ProgressData *pdata = data;
  if (gtk_widget_is_drawable (widget))
    {
      int i = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
      gtk_progress_bar_set_ellipsize (GTK_PROGRESS_BAR (pdata->pbar), i);
    }
}

static void
toggle_activity_mode (GtkWidget *widget, ProgressData *pdata)
{
  pdata->activity = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
toggle_running (GtkWidget *widget, ProgressData *pdata)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      if (pdata->timer == 0)
        pdata->timer = g_timeout_add (100, (GSourceFunc)progress_timeout, pdata);
    }
  else
    {
      if (pdata->timer != 0)
        {
          g_source_remove (pdata->timer);
          pdata->timer = 0;
        }
    }
}

static void
entry_changed (GtkWidget *widget, ProgressData *pdata)
{
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (pdata->pbar),
			  gtk_editable_get_text (GTK_EDITABLE (pdata->entry)));
}

static void
create_progress_bar (GtkWidget *widget)
{
  GtkWidget *content_area;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *check;
  GtkWidget *frame;
  GtkWidget *grid;
  GtkWidget *label;
  static ProgressData *pdata = NULL;

  static const char *items1[] =
  {
    "Left-Right",
    "Right-Left",
    "Bottom-Top",
    "Top-Bottom"
  };

    static const char *ellipsize_items[] = {
    "None",     // PANGO_ELLIPSIZE_NONE,
    "Start",    // PANGO_ELLIPSIZE_START,
    "Middle",   // PANGO_ELLIPSIZE_MIDDLE,
    "End",      // PANGO_ELLIPSIZE_END
  };
  
  if (!pdata)
    pdata = g_new0 (ProgressData, 1);

  if (!pdata->window)
    {
      pdata->window = gtk_dialog_new ();

      gtk_window_set_display (GTK_WINDOW (pdata->window),
	 		      gtk_widget_get_display (widget));

      gtk_window_set_resizable (GTK_WINDOW (pdata->window), TRUE);

      g_signal_connect (pdata->window, "destroy",
			G_CALLBACK (destroy_progress),
			&pdata);
      pdata->timer = 0;

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (pdata->window));

      gtk_window_set_title (GTK_WINDOW (pdata->window), "GtkProgressBar");

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_box_append (GTK_BOX (content_area), vbox);

      frame = gtk_frame_new ("Progress");
      gtk_box_append (GTK_BOX (vbox), frame);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_frame_set_child (GTK_FRAME (frame), vbox2);

      pdata->pbar = gtk_progress_bar_new ();
      gtk_progress_bar_set_ellipsize (GTK_PROGRESS_BAR (pdata->pbar),
                                      PANGO_ELLIPSIZE_MIDDLE);
      gtk_widget_set_halign (pdata->pbar, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (pdata->pbar, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox2), pdata->pbar);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox2), hbox);
      label = gtk_label_new ("Label updated by user :");
      gtk_box_append (GTK_BOX (hbox), label);
      pdata->label = gtk_label_new ("");
      gtk_box_append (GTK_BOX (hbox), pdata->label);

      frame = gtk_frame_new ("Options");
      gtk_box_append (GTK_BOX (vbox), frame);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_frame_set_child (GTK_FRAME (frame), vbox2);

      grid = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
      gtk_box_append (GTK_BOX (vbox2), grid);

      label = gtk_label_new ("Orientation :");
      gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

      pdata->omenu1 = build_option_menu (items1, 4, 0,
					 progressbar_toggle_orientation,
					 pdata);
      gtk_grid_attach (GTK_GRID (grid), pdata->omenu1, 1, 0, 1, 1);
      
      check = gtk_check_button_new_with_label ("Running");
      g_signal_connect (check, "toggled",
			G_CALLBACK (toggle_running),
			pdata);
      gtk_grid_attach (GTK_GRID (grid), check, 0, 1, 2, 1);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

      check = gtk_check_button_new_with_label ("Show text");
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_show_text),
			pdata);
      gtk_grid_attach (GTK_GRID (grid), check, 0, 2, 1, 1);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_grid_attach (GTK_GRID (grid), hbox, 1, 2, 1, 1);

      label = gtk_label_new ("Text: ");
      gtk_box_append (GTK_BOX (hbox), label);

      pdata->entry = gtk_entry_new ();
      gtk_widget_set_hexpand (pdata->entry, TRUE);
      g_signal_connect (pdata->entry, "changed",
			G_CALLBACK (entry_changed),
			pdata);
      gtk_box_append (GTK_BOX (hbox), pdata->entry);
      gtk_widget_set_size_request (pdata->entry, 100, -1);

      label = gtk_label_new ("Ellipsize text :");
      gtk_grid_attach (GTK_GRID (grid), label, 0, 10, 1, 1);

      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      pdata->elmenu = build_option_menu (ellipsize_items,
                                         sizeof (ellipsize_items) / sizeof (ellipsize_items[0]),
                                         2, // PANGO_ELLIPSIZE_MIDDLE
					 progressbar_toggle_ellipsize,
					 pdata);
      gtk_grid_attach (GTK_GRID (grid), pdata->elmenu, 1, 10, 1, 1);

      check = gtk_check_button_new_with_label ("Activity mode");
      g_signal_connect (check, "clicked",
			G_CALLBACK (toggle_activity_mode), pdata);
      gtk_grid_attach (GTK_GRID (grid), check, 0, 15, 1, 1);

      gtk_dialog_add_button (GTK_DIALOG (pdata->window), "Close", GTK_RESPONSE_CLOSE);
      g_signal_connect (pdata->window, "response",
			G_CALLBACK (gtk_window_destroy),
			NULL);
    }

  if (!gtk_widget_get_visible (pdata->window))
    gtk_widget_show (pdata->window);
  else
    gtk_window_destroy (GTK_WINDOW (pdata->window));
}

/*
 * Timeout Test
 */

static int timer = 0;

static int
timeout_test (GtkWidget *label)
{
  static int count = 0;
  static char buffer[32];

  sprintf (buffer, "count: %d", ++count);
  gtk_label_set_text (GTK_LABEL (label), buffer);

  return TRUE;
}

static void
start_timeout_test (GtkWidget *widget,
		    GtkWidget *label)
{
  if (!timer)
    {
      timer = g_timeout_add (100, (GSourceFunc)timeout_test, label);
    }
}

static void
stop_timeout_test (GtkWidget *widget,
		   gpointer   data)
{
  if (timer)
    {
      g_source_remove (timer);
      timer = 0;
    }
}

static void
destroy_timeout_test (GtkWidget  *widget,
		      GtkWidget **window)
{
  stop_timeout_test (NULL, NULL);

  *window = NULL;
}

static void
create_timeout_test (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *action_area, *content_area;
  GtkWidget *button;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_display (GTK_WINDOW (window),
			      gtk_widget_get_display (widget));

      g_signal_connect (window, "destroy",
			G_CALLBACK (destroy_timeout_test),
			&window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));
      action_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      gtk_window_set_title (GTK_WINDOW (window), "Timeout Test");

      label = gtk_label_new ("count: 0");
      gtk_widget_set_margin_start (label, 10);
      gtk_widget_set_margin_end (label, 10);
      gtk_widget_set_margin_top (label, 10);
      gtk_widget_set_margin_bottom (label, 10);
      gtk_box_append (GTK_BOX (content_area), label);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK (gtk_window_destroy),
				window);
      gtk_box_append (GTK_BOX (action_area), button);
      gtk_window_set_default_widget (GTK_WINDOW (window), button);

      button = gtk_button_new_with_label ("start");
      g_signal_connect (button, "clicked",
			G_CALLBACK(start_timeout_test),
			label);
      gtk_box_append (GTK_BOX (action_area), button);

      button = gtk_button_new_with_label ("stop");
      g_signal_connect (button, "clicked",
			G_CALLBACK (stop_timeout_test),
			NULL);
      gtk_box_append (GTK_BOX (action_area), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

static void
show_native (GtkWidget *button,
             GtkFileChooserNative *native)
{
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

static void
hide_native (GtkWidget *button,
             GtkFileChooserNative *native)
{
  gtk_native_dialog_hide (GTK_NATIVE_DIALOG (native));
}

static void
native_response (GtkNativeDialog *self,
                 int response_id,
                 GtkWidget *label)
{
  static int count = 0;
  char *res;
  GString *s;
  char *response;
  GtkFileFilter *filter;
  GListModel *files;
  guint i, n;

  files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (self));
  filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (self));
  s = g_string_new ("");
  n = g_list_model_get_n_items (files);
  for (i = 0; i < n; i++)
    {
      GFile *file = g_list_model_get_item (files, i);
      char *uri = g_file_get_uri (file);
      g_string_prepend (s, uri);
      g_string_prepend (s, "\n");
      g_free (uri);
      g_object_unref (file);
    }
  g_object_unref (files);

  switch (response_id)
    {
    case GTK_RESPONSE_NONE:
      response = g_strdup ("GTK_RESPONSE_NONE");
      break;
    case GTK_RESPONSE_ACCEPT:
      response = g_strdup ("GTK_RESPONSE_ACCEPT");
      break;
    case GTK_RESPONSE_CANCEL:
      response = g_strdup ("GTK_RESPONSE_CANCEL");
      break;
    case GTK_RESPONSE_DELETE_EVENT:
      response = g_strdup ("GTK_RESPONSE_DELETE_EVENT");
      break;
    default:
      response = g_strdup_printf ("%d", response_id);
      break;
    }

  if (filter)
    res = g_strdup_printf ("Response #%d: %s\n"
                           "Filter: %s\n"
                           "Files:\n"
                           "%s",
                           ++count,
                           response,
                           gtk_file_filter_get_name (filter),
                           s->str);
  else
    res = g_strdup_printf ("Response #%d: %s\n"
                           "NO Filter\n"
                           "Files:\n"
                           "%s",
                           ++count,
                           response,
                           s->str);
  gtk_label_set_text (GTK_LABEL (label), res);
  g_free (response);
  g_string_free (s, TRUE);
}

static void
native_modal_toggle (GtkWidget *checkbutton,
                     GtkFileChooserNative *native)
{
  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (native),
                               gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton)));
}

static void
native_multi_select_toggle (GtkWidget *checkbutton,
                            GtkFileChooserNative *native)
{
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (native),
                                        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton)));
}

static void
native_visible_notify_show (GObject	*object,
                            GParamSpec	*pspec,
                            GtkWidget   *show_button)
{
  GtkFileChooserNative *native = GTK_FILE_CHOOSER_NATIVE (object);
  gboolean visible;

  visible = gtk_native_dialog_get_visible (GTK_NATIVE_DIALOG (native));
  gtk_widget_set_sensitive (show_button, !visible);
}

static void
native_visible_notify_hide (GObject	*object,
                            GParamSpec	*pspec,
                            GtkWidget   *hide_button)
{
  GtkFileChooserNative *native = GTK_FILE_CHOOSER_NATIVE (object);
  gboolean visible;

  visible = gtk_native_dialog_get_visible (GTK_NATIVE_DIALOG (native));
  gtk_widget_set_sensitive (hide_button, visible);
}

static GFile *
get_some_file (void)
{
  GFile *dir = g_file_new_for_path (g_get_current_dir ());
  GFileEnumerator *e;
  GFile *res = NULL;

  e = g_file_enumerate_children (dir, "*", 0, NULL, NULL);
  if (e)
    {
      GFileInfo *info;

      while (res == NULL)
        {
          info = g_file_enumerator_next_file (e, NULL, NULL);
          if (info)
            {
              if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR)
                {
                  GFile *child = g_file_enumerator_get_child (e, info);
                  res = g_steal_pointer (&child);
                }
              g_object_unref (info);
            }
          else
            break;
        }
    }

  return res;
}

static void
native_action_changed (GtkWidget *combo,
                       GtkFileChooserNative *native)
{
  int i;
  gboolean save_as = FALSE;
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  if (i == 4) /* Save as */
    {
      save_as = TRUE;
      i = GTK_FILE_CHOOSER_ACTION_SAVE;
    }

  gtk_file_chooser_set_action (GTK_FILE_CHOOSER (native),
                               (GtkFileChooserAction) i);


  if (i == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      if (save_as)
        {
          GFile *file = get_some_file ();
          gtk_file_chooser_set_file (GTK_FILE_CHOOSER (native), file, NULL);
        }
      else
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (native), "newname.txt");
    }
}

static void
native_filter_changed (GtkWidget *combo,
                       GtkFileChooserNative *native)
{
  int i;
  GListModel *filters;
  GtkFileFilter *filter;

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  filters = gtk_file_chooser_get_filters (GTK_FILE_CHOOSER (native));
  while (g_list_model_get_n_items (filters) > 0)
    {
      GtkFileFilter *f = g_list_model_get_item (filters, 0);
      gtk_file_chooser_remove_filter (GTK_FILE_CHOOSER (native), f);
      g_object_unref (f);
    }
  g_object_unref (filters);

  switch (i)
    {
    case 0:
      break;
    case 1:   /* pattern */
      filter = gtk_file_filter_new ();
      gtk_file_filter_set_name (filter, "Text");
      gtk_file_filter_add_suffix (filter, "doc");
      gtk_file_filter_add_suffix (filter, "txt");
      gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);
      g_object_unref (filter);

      filter = gtk_file_filter_new ();
      gtk_file_filter_set_name (filter, "Images");
      gtk_file_filter_add_pixbuf_formats (filter);
      gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (native), filter);
      g_object_unref (filter);

      filter = gtk_file_filter_new ();
      gtk_file_filter_set_name (filter, "All");
      gtk_file_filter_add_pattern (filter, "*");
      gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);
      g_object_unref (filter);
      break;

    case 2:   /* mimetype */
      filter = gtk_file_filter_new ();
      gtk_file_filter_set_name (filter, "Text");
      gtk_file_filter_add_mime_type (filter, "text/plain");
      gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);
      g_object_unref (filter);

      filter = gtk_file_filter_new ();
      gtk_file_filter_set_name (filter, "All");
      gtk_file_filter_add_pattern (filter, "*");
      gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (native), filter);
      g_object_unref (filter);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
destroy_native (GtkFileChooserNative *native)
{
  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
  g_object_unref (native);
}

static void
create_native_dialogs (GtkWidget *widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box, *label;
  GtkWidget *show_button, *hide_button, *check_button;
  GtkFileChooserNative *native;
  GtkWidget *combo;

  if (!window)
    {
      GFile *path;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (widget));

      native = gtk_file_chooser_native_new ("Native title",
                                            GTK_WINDOW (window),
                                            GTK_FILE_CHOOSER_ACTION_OPEN,
                                            "_accept&native",
                                            "_cancel__native");

      g_signal_connect_swapped (G_OBJECT (window), "destroy", G_CALLBACK (destroy_native), native);

      path = g_file_new_for_path (g_get_current_dir ());
      gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (native), path, NULL);
      g_object_unref (path);

      gtk_window_set_title (GTK_WINDOW(window), "Native dialog parent");

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_window_set_child (GTK_WINDOW (window), box);

      label = gtk_label_new ("");
      gtk_box_append (GTK_BOX (box), label);

      combo = gtk_combo_box_text_new ();

      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Open");
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Save");
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Select Folder");
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Create Folder");
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Save as");

      g_signal_connect (combo, "changed",
                        G_CALLBACK (native_action_changed), native);
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), GTK_FILE_CHOOSER_ACTION_OPEN);
      gtk_box_append (GTK_BOX (box), combo);

      combo = gtk_combo_box_text_new ();

      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "No filters");
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Pattern filter");
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Mimetype filter");

      g_signal_connect (combo, "changed",
                        G_CALLBACK (native_filter_changed), native);
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
      gtk_box_append (GTK_BOX (box), combo);

      check_button = gtk_check_button_new_with_label ("Modal");
      g_signal_connect (check_button, "toggled",
                        G_CALLBACK (native_modal_toggle), native);
      gtk_box_append (GTK_BOX (box), check_button);

      check_button = gtk_check_button_new_with_label ("Multiple select");
      g_signal_connect (check_button, "toggled",
                        G_CALLBACK (native_multi_select_toggle), native);
      gtk_box_append (GTK_BOX (box), check_button);

      show_button = gtk_button_new_with_label ("Show");
      hide_button = gtk_button_new_with_label ("Hide");
      gtk_widget_set_sensitive (hide_button, FALSE);

      gtk_box_append (GTK_BOX (box), show_button);
      gtk_box_append (GTK_BOX (box), hide_button);

      /* connect signals */
      g_signal_connect (native, "response",
                        G_CALLBACK (native_response), label);
      g_signal_connect (show_button, "clicked",
                        G_CALLBACK (show_native), native);
      g_signal_connect (hide_button, "clicked",
                        G_CALLBACK (hide_native), native);

      g_signal_connect (native, "notify::visible",
                        G_CALLBACK (native_visible_notify_show), show_button);
      g_signal_connect (native, "notify::visible",
                        G_CALLBACK (native_visible_notify_hide), hide_button);

      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

/*
 * Main Window and Exit
 */

static void
do_exit (GtkWidget *widget, GtkWidget *window)
{
  gtk_window_destroy (GTK_WINDOW (window));
  done = TRUE;
  g_main_context_wakeup (NULL);
}

struct {
  const char *label;
  void (*func) (GtkWidget *widget);
  gboolean do_not_benchmark;
} buttons[] =
{
  { "alpha window", create_alpha_window },
  { "buttons", create_buttons },
  { "check buttons", create_check_buttons },
  { "color selection", create_color_selection },
  { "cursors", create_cursors },
  { "dialog", create_dialog },
  { "display", create_display_screen, TRUE },
  { "entry", create_entry },
  { "expander", create_expander },
  { "flipping", create_flipping },
  { "font selection", create_font_selection },
  { "image", create_image },
  { "labels", create_labels },
  { "listbox", create_listbox },
  { "message dialog", create_message_dialog },
  { "modal window", create_modal_window, TRUE },
  { "native dialogs", create_native_dialogs },
  { "notebook", create_notebook },
  { "panes", create_panes },
  { "paned keyboard", create_paned_keyboard_navigation },
  { "pixbuf", create_pixbuf },
  { "progress bar", create_progress_bar },
  { "radio buttons", create_radio_buttons },
  { "range controls", create_range_controls },
  { "rotated text", create_rotated_text },
  { "scrolled windows", create_scrolled_windows },
  { "size groups", create_size_groups },
  { "spinbutton", create_spins },
  { "statusbar", create_statusbar },
  { "surface states", create_surface_states },
  { "test timeout", create_timeout_test },
  { "toggle buttons", create_toggle_buttons },
  { "tooltips", create_tooltips },
  { "WM hints", create_wmhints },
  { "window sizing", create_window_sizing }
};
int nbuttons = sizeof (buttons) / sizeof (buttons[0]);

static void
quit_cb (GtkWidget *widget,
         gpointer   user_data)
{
  gboolean *is_done = user_data;

  *is_done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
create_main_window (void)
{
  GtkWidget *window;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *scrolled_window;
  GtkWidget *button;
  GtkWidget *label;
  char buffer[64];
  GtkWidget *separator;
  int i;

  window = gtk_window_new ();
  gtk_widget_set_name (window, "main_window");
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 400);

  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box1);

  if (gtk_get_micro_version () > 0)
    sprintf (buffer,
	     "Gtk+ v%d.%d.%d",
	     gtk_get_major_version (),
	     gtk_get_minor_version (),
	     gtk_get_micro_version ());
  else
    sprintf (buffer,
	     "Gtk+ v%d.%d",
	     gtk_get_major_version (),
	     gtk_get_minor_version ());

  label = gtk_label_new (buffer);
  gtk_box_append (GTK_BOX (box1), label);
  gtk_widget_set_name (label, "testgtk-version-label");

  scrolled_window = gtk_scrolled_window_new ();
  gtk_widget_set_margin_top (scrolled_window, 10);
  gtk_widget_set_margin_bottom (scrolled_window, 10);
  gtk_widget_set_margin_start (scrolled_window, 10);
  gtk_widget_set_margin_end (scrolled_window, 10);
  gtk_widget_set_vexpand (scrolled_window, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
     		                  GTK_POLICY_NEVER, 
                                  GTK_POLICY_AUTOMATIC);
  gtk_box_append (GTK_BOX (box1), scrolled_window);

  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_top (box2, 10);
  gtk_widget_set_margin_bottom (box2, 10);
  gtk_widget_set_margin_start (box2, 10);
  gtk_widget_set_margin_end (box2, 10);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), box2);
  gtk_viewport_set_scroll_to_focus (GTK_VIEWPORT (gtk_widget_get_parent (box2)), TRUE);

  for (i = 0; i < nbuttons; i++)
    {
      button = gtk_button_new_with_label (buttons[i].label);
      if (buttons[i].func)
        g_signal_connect (button,
			  "clicked",
			  G_CALLBACK(buttons[i].func),
			  NULL);
      else
        gtk_widget_set_sensitive (button, FALSE);
      gtk_box_append (GTK_BOX (box2), button);
    }

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_append (GTK_BOX (box1), separator);

  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_append (GTK_BOX (box1), box2);

  button = gtk_button_new_with_mnemonic ("_Close");
  gtk_widget_set_margin_top (button, 10);
  gtk_widget_set_margin_bottom (button, 10);
  gtk_widget_set_margin_start (button, 10);
  gtk_widget_set_margin_end (button, 10);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (do_exit),
		    window);
  gtk_box_append (GTK_BOX (box2), button);
  gtk_window_set_default_widget (GTK_WINDOW (window), button);

  gtk_window_present (GTK_WINDOW (window));
}

static char *
pad (const char *str, int to)
{
  static char buf[256];
  int len = strlen (str);
  int i;

  for (i = 0; i < to; i++)
    buf[i] = ' ';

  buf[to] = '\0';

  memcpy (buf, str, len);

  return buf;
}

static void
bench_iteration (GtkWidget *widget, void (* fn) (GtkWidget *widget))
{
  fn (widget); /* on */
  while (g_main_context_iteration (NULL, FALSE));
  fn (widget); /* off */
  while (g_main_context_iteration (NULL, FALSE));
}

static void
do_real_bench (GtkWidget *widget, void (* fn) (GtkWidget *widget), const char *name, int num)
{
  gint64 t0, t1;
  double dt_first;
  double dt;
  int n;
  static gboolean printed_headers = FALSE;

  if (!printed_headers) {
    g_print ("Test                 Iters      First      Other\n");
    g_print ("-------------------- ----- ---------- ----------\n");
    printed_headers = TRUE;
  }

  t0 = g_get_monotonic_time ();
  bench_iteration (widget, fn);
  t1 = g_get_monotonic_time ();

  dt_first = ((double)(t1 - t0)) / 1000.0;

  t0 = g_get_monotonic_time ();
  for (n = 0; n < num - 1; n++)
    bench_iteration (widget, fn);
  t1 = g_get_monotonic_time ();
  dt = ((double)(t1 - t0)) / 1000.0;

  g_print ("%s %5d ", pad (name, 20), num);
  if (num > 1)
    g_print ("%10.1f %10.1f\n", dt_first, dt/(num-1));
  else
    g_print ("%10.1f\n", dt_first);
}

static void
do_bench (char* what, int num)
{
  int i;
  GtkWidget *widget;
  void (* fn) (GtkWidget *widget);
  fn = NULL;
  widget = gtk_window_new ();

  if (g_ascii_strcasecmp (what, "ALL") == 0)
    {
      for (i = 0; i < nbuttons; i++)
	{
	  if (!buttons[i].do_not_benchmark)
	    do_real_bench (widget, buttons[i].func, buttons[i].label, num);
	}

      return;
    }
  else
    {
      for (i = 0; i < nbuttons; i++)
	{
	  if (strcmp (buttons[i].label, what) == 0)
	    {
	      fn = buttons[i].func;
	      break;
	    }
	}
      
      if (!fn)
	g_print ("Can't bench: \"%s\" not found.\n", what);
      else
	do_real_bench (widget, fn, buttons[i].label, num);
    }
}

static void G_GNUC_NORETURN
usage (void)
{
  fprintf (stderr, "Usage: testgtk [--bench ALL|<bench>[:<count>]]\n");
  exit (1);
}

int
main (int argc, char *argv[])
{
  GtkCssProvider *provider, *memory_provider;
  GdkDisplay *display;
  int i;
  gboolean done_benchmarks = FALSE;

  srand (time (NULL));

  g_set_application_name ("GTK Test Program");

#ifdef GTK_SRCDIR
  g_chdir (GTK_SRCDIR);
#endif

  gtk_init ();

  provider = gtk_css_provider_new ();

  /* Check to see if we are being run from the correct
   * directory.
   */
  if (file_exists ("testgtk.css"))
    gtk_css_provider_load_from_path (provider, "testgtk.css");
  else if (file_exists ("tests/testgtk.css"))
    gtk_css_provider_load_from_path (provider, "tests/testgtk.css");
  else
    g_warning ("Couldn't find file \"testgtk.css\".");

  display = gdk_display_get_default ();

  gtk_style_context_add_provider_for_display (display, GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  /*  benchmarking
   */
  for (i = 1; i < argc; i++)
    {
      if (strncmp (argv[i], "--bench", strlen("--bench")) == 0)
        {
          int num = 1;
	  char *nextarg;
	  char *what;
	  char *count;
	  
	  nextarg = strchr (argv[i], '=');
	  if (nextarg)
	    nextarg++;
	  else
	    {
	      i++;
	      if (i == argc)
		usage ();
	      nextarg = argv[i];
	    }

	  count = strchr (nextarg, ':');
	  if (count)
	    {
	      what = g_strndup (nextarg, count - nextarg);
	      count++;
	      num = atoi (count);
	      if (num <= 0)
		usage ();
	    }
	  else
	    what = g_strdup (nextarg);

          do_bench (what, num ? num : 1);
	  done_benchmarks = TRUE;
        }
      else
	usage ();
    }
  if (done_benchmarks)
    return 0;

  memory_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (memory_provider,
                                   "#testgtk-version-label {\n"
                                   "  color: #f00;\n"
                                   "  font-family: Sans;\n"
                                   "  font-size: 18px;\n"
                                   "}",
                                   -1);
  gtk_style_context_add_provider_for_display (display, GTK_STYLE_PROVIDER (memory_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

  create_main_window ();

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  if (1)
    {
      while (g_main_context_pending (NULL))
	g_main_context_iteration (NULL, FALSE);
#if 0
      sleep (1);
      while (g_main_context_pending (NULL))
	g_main_context_iteration (NULL, FALSE);
#endif
    }
  return 0;
}

/* 
 * GTK - The GIMP Toolkit
 * Copyright (C) 1999  Red Hat, Inc.
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003  Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2005  Carlos Garnacho Parro <carlosg@gnome.org>
 *
 * All rights reserved.
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

static GtkWidget*
get_test_page (const char *text)
{
  return gtk_label_new (text);
}

typedef struct {
  GtkAssistant *assistant;
  GtkWidget    *page;
} PageData;

static void
complete_cb (GtkWidget *check, 
	     gpointer   data)
{
  PageData *pdata = data;
  gboolean complete;

  complete = gtk_check_button_get_active (GTK_CHECK_BUTTON (check));

  gtk_assistant_set_page_complete (pdata->assistant,
				   pdata->page,
				   complete);
}
	     
static GtkWidget *
add_completion_test_page (GtkWidget   *assistant,
			  const char *text, 
			  gboolean     visible,
			  gboolean     complete)
{
  GtkWidget *page;
  GtkWidget *check;
  PageData *pdata;

  page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  check = gtk_check_button_new_with_label ("Complete");

  gtk_box_append (GTK_BOX (page), gtk_label_new (text));
  gtk_box_append (GTK_BOX (page), check);
  
  gtk_check_button_set_active (GTK_CHECK_BUTTON (check), complete);

  pdata = g_new (PageData, 1);
  pdata->assistant = GTK_ASSISTANT (assistant);
  pdata->page = page;
  g_signal_connect (G_OBJECT (check), "toggled", 
		    G_CALLBACK (complete_cb), pdata);


  gtk_widget_set_visible (page, visible);

  gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
  gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, text);
  gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, complete);

  return page;
}

static void
cancel_callback (GtkWidget *widget)
{
  g_print ("cancel\n");

  gtk_widget_hide (widget);
}

static void
close_callback (GtkWidget *widget)
{
  g_print ("close\n");

  gtk_widget_hide (widget);
}

static void
apply_callback (GtkWidget *widget)
{
  g_print ("apply\n");
}

static gboolean
progress_timeout (GtkWidget *assistant)
{
  GtkWidget *progress;
  int current_page;
  double value;

  current_page = gtk_assistant_get_current_page (GTK_ASSISTANT (assistant));
  progress = gtk_assistant_get_nth_page (GTK_ASSISTANT (assistant), current_page);

  value  = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (progress));
  value += 0.1;
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), value);

  if (value >= 1.0)
    {
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), progress, TRUE);
      return FALSE;
    }

  return TRUE;
}

static void
prepare_callback (GtkWidget *widget, GtkWidget *page)
{
  if (GTK_IS_LABEL (page))
    g_print ("prepare: %s\n", gtk_label_get_text (GTK_LABEL (page)));
  else if (gtk_assistant_get_page_type (GTK_ASSISTANT (widget), page) == GTK_ASSISTANT_PAGE_PROGRESS)
    {
      gtk_assistant_set_page_complete (GTK_ASSISTANT (widget), page, FALSE);
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (page), 0.0);
      g_timeout_add (300, (GSourceFunc) progress_timeout, widget);
    }
  else
    g_print ("prepare: %d\n", gtk_assistant_get_current_page (GTK_ASSISTANT (widget)));
}

static void
create_simple_assistant (GtkWidget *widget)
{
  static GtkWidget *assistant = NULL;

  if (!assistant)
    {
      GtkWidget *page;

      assistant = gtk_assistant_new ();
      gtk_window_set_default_size (GTK_WINDOW (assistant), 400, 300);

      g_signal_connect (G_OBJECT (assistant), "cancel",
			G_CALLBACK (cancel_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "close",
			G_CALLBACK (close_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "apply",
			G_CALLBACK (apply_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "prepare",
			G_CALLBACK (prepare_callback), NULL);

      page = get_test_page ("Page 1");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 1");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Page 2");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 2");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_CONFIRM);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);
    }

  if (!gtk_widget_get_visible (assistant))
    gtk_window_present (GTK_WINDOW (assistant));
  else
    {
      gtk_window_destroy (GTK_WINDOW (assistant));
      assistant = NULL;
    }
}

static void
create_anonymous_assistant (GtkWidget *widget)
{
  static GtkWidget *assistant = NULL;

  if (!assistant)
    {
      GtkWidget *page;

      assistant = gtk_assistant_new ();
      gtk_window_set_default_size (GTK_WINDOW (assistant), 400, 300);

      g_signal_connect (G_OBJECT (assistant), "cancel",
			G_CALLBACK (cancel_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "close",
			G_CALLBACK (close_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "apply",
			G_CALLBACK (apply_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "prepare",
			G_CALLBACK (prepare_callback), NULL);

      page = get_test_page ("Page 1");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Page 2");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_CONFIRM);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);
    }

  if (!gtk_widget_get_visible (assistant))
    gtk_widget_show (assistant);
  else
    {
      gtk_window_destroy (GTK_WINDOW (assistant));
      assistant = NULL;
    }
}

static void
visible_cb (GtkWidget *check, 
	    gpointer   data)
{
  GtkWidget *page = data;
  gboolean visible;

  visible = gtk_check_button_get_active (GTK_CHECK_BUTTON (check));

  g_object_set (G_OBJECT (page), "visible", visible, NULL);
}

static void
create_generous_assistant (GtkWidget *widget)
{
  static GtkWidget *assistant = NULL;

  if (!assistant)
    {
      GtkWidget *page, *next, *check;
      PageData  *pdata;

      assistant = gtk_assistant_new ();
      gtk_window_set_default_size (GTK_WINDOW (assistant), 400, 300);

      g_signal_connect (G_OBJECT (assistant), "cancel",
			G_CALLBACK (cancel_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "close",
			G_CALLBACK (close_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "apply",
			G_CALLBACK (apply_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "prepare",
			G_CALLBACK (prepare_callback), NULL);

      page = get_test_page ("Introduction");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Introduction");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_INTRO);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = add_completion_test_page (assistant, "Content", TRUE, FALSE);
      next = add_completion_test_page (assistant, "More Content", TRUE, TRUE);

      check = gtk_check_button_new_with_label ("Next page visible");
      gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
      g_signal_connect (G_OBJECT (check), "toggled", 
                        G_CALLBACK (visible_cb), next);
      gtk_box_append (GTK_BOX (page), check);
      
      add_completion_test_page (assistant, "Even More Content", TRUE, TRUE);

      page = get_test_page ("Confirmation");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Confirmation");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_CONFIRM);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = gtk_progress_bar_new ();
      gtk_widget_set_halign (page, GTK_ALIGN_FILL);
      gtk_widget_set_valign (page, GTK_ALIGN_CENTER);
      gtk_widget_set_margin_start (page, 20);
      gtk_widget_set_margin_end (page, 20);
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Progress");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_PROGRESS);

      page = gtk_check_button_new_with_label ("Summary complete");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Summary");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_SUMMARY);

      gtk_check_button_set_active (GTK_CHECK_BUTTON (page),
                                   gtk_assistant_get_page_complete (GTK_ASSISTANT (assistant),
                                                                    page));

      pdata = g_new (PageData, 1);
      pdata->assistant = GTK_ASSISTANT (assistant);
      pdata->page = page;
      g_signal_connect (page, "toggled",
                      G_CALLBACK (complete_cb), pdata);
    }

  if (!gtk_widget_get_visible (assistant))
    gtk_widget_show (assistant);
  else
    {
      gtk_window_destroy (GTK_WINDOW (assistant));
      assistant = NULL;
    }
}

static char selected_branch = 'A';

static void
select_branch (GtkWidget *widget, char branch)
{
  selected_branch = branch;
}

static int
nonlinear_assistant_forward_page (int current_page, gpointer data)
{
  switch (current_page)
    {
    case 0:
      if (selected_branch == 'A')
	return 1;
      else
	return 2;
    case 1:
    case 2:
      return 3;
    default:
      return -1;
    }
}

static void
create_nonlinear_assistant (GtkWidget *widget)
{
  static GtkWidget *assistant = NULL;

  if (!assistant)
    {
      GtkWidget *page, *button, *group;

      assistant = gtk_assistant_new ();
      gtk_window_set_default_size (GTK_WINDOW (assistant), 400, 300);

      g_signal_connect (G_OBJECT (assistant), "cancel",
			G_CALLBACK (cancel_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "close",
			G_CALLBACK (close_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "apply",
			G_CALLBACK (apply_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "prepare",
			G_CALLBACK (prepare_callback), NULL);

      gtk_assistant_set_forward_page_func (GTK_ASSISTANT (assistant),
					   nonlinear_assistant_forward_page,
					   NULL, NULL);

      page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

      button = gtk_check_button_new_with_label ("branch A");
      gtk_box_append (GTK_BOX (page), button);
      g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (select_branch), GINT_TO_POINTER ('A'));
      gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);
      group = button;

      button = gtk_check_button_new_with_label ("branch B");
      gtk_check_button_set_group (GTK_CHECK_BUTTON (button), GTK_CHECK_BUTTON (group));
      gtk_box_append (GTK_BOX (page), button);
      g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (select_branch), GINT_TO_POINTER ('B'));

      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 1");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Page 2A");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 2");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Page 2B");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 2");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Confirmation");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Confirmation");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_CONFIRM);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);
    }

  if (!gtk_widget_get_visible (assistant))
    gtk_widget_show (assistant);
  else
    {
      gtk_window_destroy (GTK_WINDOW (assistant));
      assistant = NULL;
    }
}

static int
looping_assistant_forward_page (int current_page, gpointer data)
{
  switch (current_page)
    {
    case 0:
      return 1;
    case 1:
      return 2;
    case 2:
      return 3;
    case 3:
      {
	GtkAssistant *assistant;
	GtkWidget *page;

	assistant = (GtkAssistant*) data;
	page = gtk_assistant_get_nth_page (assistant, current_page);

	if (gtk_check_button_get_active (GTK_CHECK_BUTTON (page)))
	  return 0;
	else
	  return 4;
      }
    case 4:
    default:
      return -1;
    }
}

static void
create_looping_assistant (GtkWidget *widget)
{
  static GtkWidget *assistant = NULL;

  if (!assistant)
    {
      GtkWidget *page;

      assistant = gtk_assistant_new ();
      gtk_window_set_default_size (GTK_WINDOW (assistant), 400, 300);

      g_signal_connect (G_OBJECT (assistant), "cancel",
			G_CALLBACK (cancel_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "close",
			G_CALLBACK (close_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "apply",
			G_CALLBACK (apply_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "prepare",
			G_CALLBACK (prepare_callback), NULL);

      gtk_assistant_set_forward_page_func (GTK_ASSISTANT (assistant),
					   looping_assistant_forward_page,
					   assistant, NULL);

      page = get_test_page ("Introduction");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Introduction");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_INTRO);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Content");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Content");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("More content");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "More content");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = gtk_check_button_new_with_label ("Loop?");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Loop?");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);
      
      page = get_test_page ("Confirmation");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Confirmation");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_CONFIRM);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);
    }

  if (!gtk_widget_get_visible (assistant))
    gtk_widget_show (assistant);
  else
    {
      gtk_window_destroy (GTK_WINDOW (assistant));
      assistant = NULL;
    }
}

static void
toggle_invisible (GtkButton *button, GtkAssistant *assistant)
{
  GtkWidget *page;

  page = gtk_assistant_get_nth_page (assistant, 1);

  gtk_widget_set_visible (page, !gtk_widget_get_visible (page));
}

static void
create_full_featured_assistant (GtkWidget *widget)
{
  static GtkWidget *assistant = NULL;

  if (!assistant)
    {
      GtkWidget *page, *button;

      assistant = gtk_assistant_new ();
      gtk_window_set_default_size (GTK_WINDOW (assistant), 400, 300);

      button = gtk_button_new_with_label ("_Stop");
      gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
      gtk_assistant_add_action_widget (GTK_ASSISTANT (assistant), button);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (toggle_invisible), assistant);

      g_signal_connect (G_OBJECT (assistant), "cancel",
			G_CALLBACK (cancel_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "close",
			G_CALLBACK (close_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "apply",
			G_CALLBACK (apply_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "prepare",
			G_CALLBACK (prepare_callback), NULL);

      page = get_test_page ("Page 1");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 1");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Invisible page");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 2");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_OPEN);
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Filechooser");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Page 3");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 3");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_CONFIRM);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);
    }

  if (!gtk_widget_get_visible (assistant))
    gtk_widget_show (assistant);
  else
    {
      gtk_window_destroy (GTK_WINDOW (assistant));
      assistant = NULL;
    }
}

static void
flip_pages (GtkButton *button, GtkAssistant *assistant)
{
  GtkWidget *page;
  char *title;

  page = gtk_assistant_get_nth_page (assistant, 1);

  g_object_ref (page);

  title = g_strdup (gtk_assistant_get_page_title (assistant, page));

  gtk_assistant_remove_page (assistant, 1);
  gtk_assistant_insert_page (assistant, page, 2);

  gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, title);
  gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

  g_object_unref (page);
  g_free (title);
}


static void
create_page_flipping_assistant (GtkWidget *widget)
{
  static GtkWidget *assistant = NULL;

  if (!assistant)
    {
      GtkWidget *page, *button;

      assistant = gtk_assistant_new ();
      gtk_window_set_default_size (GTK_WINDOW (assistant), 400, 300);

      button = gtk_button_new_with_label ("_Flip");
      gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
      gtk_assistant_add_action_widget (GTK_ASSISTANT (assistant), button);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (flip_pages), assistant);

      g_signal_connect (G_OBJECT (assistant), "cancel",
			G_CALLBACK (cancel_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "close",
			G_CALLBACK (close_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "apply",
			G_CALLBACK (apply_callback), NULL);
      g_signal_connect (G_OBJECT (assistant), "prepare",
			G_CALLBACK (prepare_callback), NULL);

      page = get_test_page ("Page 1");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 1");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_append (GTK_BOX (page),
                          get_test_page ("Page 2"));
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 2");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Page 3");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Page 3");
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);

      page = get_test_page ("Summary");
      gtk_assistant_append_page (GTK_ASSISTANT (assistant), page);
      gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, "Summary");
      gtk_assistant_set_page_type  (GTK_ASSISTANT (assistant), page, GTK_ASSISTANT_PAGE_SUMMARY);
      gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), page, TRUE);
    }

  if (!gtk_widget_get_visible (assistant))
    gtk_widget_show (assistant);
  else
    {
      gtk_window_destroy (GTK_WINDOW (assistant));
      assistant = NULL;
    }
}

struct {
  const char *text;
  void  (*func) (GtkWidget *widget);
} buttons[] =
  {
    { "simple assistant",        create_simple_assistant },
    { "anonymous assistant",        create_anonymous_assistant },
    { "generous assistant",      create_generous_assistant },
    { "nonlinear assistant",     create_nonlinear_assistant },
    { "looping assistant",       create_looping_assistant },
    { "full featured assistant", create_full_featured_assistant },
    { "page-flipping assistant", create_page_flipping_assistant },
  };

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *button;
  int i;
  gboolean done = FALSE;

  gtk_init ();

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  window = gtk_window_new ();
  gtk_window_set_hide_on_close (GTK_WINDOW (window), TRUE);

  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (quit_cb), &done);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_window_set_child (GTK_WINDOW (window), box);

  for (i = 0; i < G_N_ELEMENTS (buttons); i++)
    {
      button = gtk_button_new_with_label (buttons[i].text);

      if (buttons[i].func)
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (buttons[i].func), NULL);

      gtk_box_append (GTK_BOX (box), button);
    }

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}

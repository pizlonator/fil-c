/* example-start calendar calendar.c */
/*
 * Copyright (C) 1998 Cesar Miquel, Shawn T. Amundson, Mattias Grönlund
 * Copyright (C) 2000 Tony Gale
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#define DEF_PAD_SMALL 6

#define TM_YEAR_BASE 1900

typedef struct _CalendarData
{
  GtkWidget *calendar_widget;
  GtkWidget *prev2_sig;
  GtkWidget *prev_sig;
  GtkWidget *last_sig;
  GtkWidget *month;
} CalendarData;

enum
{
  calendar_show_header,
  calendar_show_days,
  calendar_month_change, 
  calendar_show_week,
  calendar_monday_first
};

/*
 * GtkCalendar
 */

static char *
calendar_date_to_string (CalendarData *data,
                         const char   *format)
{
  GDateTime *date;
  char *str;

  date = gtk_calendar_get_date (GTK_CALENDAR (data->calendar_widget));
  str = g_date_time_format (date, format);

  g_date_time_unref (date);

  return str;
}

static void
calendar_set_signal_strings (char         *sig_str,
				  CalendarData *data)
{
  const char *prev_sig;

  prev_sig = gtk_label_get_text (GTK_LABEL (data->prev_sig));
  gtk_label_set_text (GTK_LABEL (data->prev2_sig), prev_sig);

  prev_sig = gtk_label_get_text (GTK_LABEL (data->last_sig));
  gtk_label_set_text (GTK_LABEL (data->prev_sig), prev_sig);
  gtk_label_set_text (GTK_LABEL (data->last_sig), sig_str);
}

static void
calendar_day_selected (GtkWidget    *widget,
                            CalendarData *data)
{
  char *str = calendar_date_to_string (data, "day-selected: %c");
  calendar_set_signal_strings (str, data);
  g_free (str);
}

static void
calendar_prev_month (GtkWidget    *widget,
                          CalendarData *data)
{
  char *str = calendar_date_to_string (data, "prev-month: %c");
  calendar_set_signal_strings (str, data);
  g_free (str);
}

static void
calendar_next_month (GtkWidget    *widget,
                     CalendarData *data)
{
  char *str = calendar_date_to_string (data, "next-month: %c");
  calendar_set_signal_strings (str, data);
  g_free (str);

}

static void
calendar_prev_year (GtkWidget    *widget,
                    CalendarData *data)
{
  char *str = calendar_date_to_string (data, "prev-year: %c");
  calendar_set_signal_strings (str, data);
  g_free (str);
}

static void
calendar_next_year (GtkWidget    *widget,
                    CalendarData *data)
{
  char *str = calendar_date_to_string (data, "next-year: %c");
  calendar_set_signal_strings (str, data);
  g_free (str);
}

static void
flag_toggled_cb (GtkCheckButton *button,
                 gpointer        user_data)
{
  struct {
    const char *prop_name;
    const char *label;
    GtkWidget *calendar;
  } *data = user_data;

  g_object_set (G_OBJECT (data->calendar), data->prop_name,
                gtk_check_button_get_active (GTK_CHECK_BUTTON (button)),
                NULL);
}

static GtkWidget*
create_frame (const char *caption,
              GtkWidget  *child,
              GtkAlign    halign,
              GtkAlign    valign)
{
  GtkWidget *frame = gtk_frame_new ("");
  GtkWidget *label = gtk_frame_get_label_widget (GTK_FRAME (frame));

  g_object_set (child,
                "margin-top", 6,
                "margin-bottom", 0,
                "margin-start", 18,
                "margin-end", 0,
                "halign", halign,
                "valign", valign,
                NULL);
  gtk_label_set_markup (GTK_LABEL (label), caption);

  gtk_frame_set_child (GTK_FRAME (frame), child);

  return frame;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
create_calendar(void)
{
  static CalendarData calendar_data;

  GtkWidget *window, *hpaned, *vbox, *rpane, *hbox;
  GtkWidget *calendar = gtk_calendar_new ();
  GtkWidget *button;
  GtkWidget *frame, *label, *bbox;
  int i;
  struct {
    const char *prop_name;
    const char *label;
    GtkWidget *calendar;
  } flags[] = {
    { "show-heading", "Show Heading", calendar },
    { "show-day-names", "Show Day Names", calendar },
    { "show-week-numbers", "Show Week Numbers", calendar },
  };
  gboolean done = FALSE;

  window = gtk_window_new ();
  gtk_window_set_hide_on_close (GTK_WINDOW (window), TRUE);
  gtk_window_set_title (GTK_WINDOW (window), "GtkCalendar Example");
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_vexpand (hpaned, TRUE);

  /* Calendar widget */

  calendar_data.calendar_widget = calendar;
  frame = create_frame ("<b>Calendar</b>", calendar, GTK_ALIGN_CENTER, GTK_ALIGN_CENTER);
  gtk_paned_set_start_child (GTK_PANED (hpaned), frame);
  gtk_paned_set_resize_start_child (GTK_PANED (hpaned), TRUE);
  gtk_paned_set_shrink_start_child (GTK_PANED (hpaned), FALSE);

  gtk_calendar_mark_day (GTK_CALENDAR (calendar), 19);	

  g_signal_connect (calendar, "day-selected", 
		    G_CALLBACK (calendar_day_selected),
		    &calendar_data);
  g_signal_connect (calendar, "prev-month", 
		    G_CALLBACK (calendar_prev_month),
		    &calendar_data);
  g_signal_connect (calendar, "next-month", 
		    G_CALLBACK (calendar_next_month),
		    &calendar_data);
  g_signal_connect (calendar, "prev-year", 
		    G_CALLBACK (calendar_prev_year),
		    &calendar_data);
  g_signal_connect (calendar, "next-year", 
		    G_CALLBACK (calendar_next_year),
		    &calendar_data);

  rpane = gtk_box_new (GTK_ORIENTATION_VERTICAL, DEF_PAD_SMALL);
  gtk_paned_set_end_child (GTK_PANED (hpaned), rpane);
  gtk_paned_set_resize_end_child (GTK_PANED (hpaned), FALSE);
  gtk_paned_set_shrink_end_child (GTK_PANED (hpaned), FALSE);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, DEF_PAD_SMALL);
  frame = create_frame ("<b>Options</b>", vbox, GTK_ALIGN_FILL, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (rpane), frame);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, DEF_PAD_SMALL);
  gtk_widget_set_halign (hbox, GTK_ALIGN_START);
  gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (vbox), hbox);

  /* Build the Right frame with the flags in */

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (rpane), vbox);

  for (i = 0; i < G_N_ELEMENTS (flags); i++)
    {
      GtkWidget *toggle = gtk_check_button_new_with_mnemonic (flags[i].label);
      gboolean value;

      gtk_box_append (GTK_BOX (vbox), toggle);

      g_object_get (G_OBJECT (calendar), flags[i].prop_name, &value, NULL);
      gtk_check_button_set_active (GTK_CHECK_BUTTON (toggle), value);

      g_signal_connect (toggle, "toggled", G_CALLBACK (flag_toggled_cb), &flags[i]);
    }

  /*
   *  Build the Signal-event part.
   */

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, DEF_PAD_SMALL);
  gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
  frame = create_frame ("<b>Signal Events</b>", vbox, GTK_ALIGN_FILL, GTK_ALIGN_CENTER);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_append (GTK_BOX (vbox), hbox);
  label = gtk_label_new ("Signal:");
  gtk_box_append (GTK_BOX (hbox), label);
  calendar_data.last_sig = gtk_label_new ("");
  gtk_box_append (GTK_BOX (hbox), calendar_data.last_sig);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_append (GTK_BOX (vbox), hbox);
  label = gtk_label_new ("Previous signal:");
  gtk_box_append (GTK_BOX (hbox), label);
  calendar_data.prev_sig = gtk_label_new ("");
  gtk_box_append (GTK_BOX (hbox), calendar_data.prev_sig);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_append (GTK_BOX (vbox), hbox);
  label = gtk_label_new ("Second previous signal:");
  gtk_box_append (GTK_BOX (hbox), label);
  calendar_data.prev2_sig = gtk_label_new ("");
  gtk_box_append (GTK_BOX (hbox), calendar_data.prev2_sig);

  /*
   *  Glue everything together
   */

  bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (bbox, GTK_ALIGN_END);

  button = gtk_button_new_with_label ("Close");
  g_signal_connect (button, "clicked", G_CALLBACK (quit_cb), &done);
  gtk_box_append (GTK_BOX (bbox), button);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, DEF_PAD_SMALL);

  gtk_box_append (GTK_BOX (vbox), hpaned);
  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_box_append (GTK_BOX (vbox), frame);
  gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  gtk_box_append (GTK_BOX (vbox), bbox);

  gtk_window_set_child (GTK_WINDOW (window), vbox);

  gtk_window_set_default_widget (GTK_WINDOW (window), button);

  gtk_window_set_default_size (GTK_WINDOW (window), 600, 0);
  g_signal_connect (window, "close-request", G_CALLBACK (quit_cb), &done);
  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);
}


int main(int   argc,
         char *argv[] )
{
  gtk_init ();

  if (g_getenv ("GTK_RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  create_calendar();

  return(0);
}
/* example-end */

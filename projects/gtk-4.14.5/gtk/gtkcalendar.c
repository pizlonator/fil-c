/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GTK Calendar Widget
 * Copyright (C) 1998 Cesar Miquel, Shawn T. Amundson and Mattias Groenlund
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

/**
 * GtkCalendar:
 *
 * `GtkCalendar` is a widget that displays a Gregorian calendar, one month
 * at a time.
 *
 * ![An example GtkCalendar](calendar.png)
 *
 * A `GtkCalendar` can be created with [ctor@Gtk.Calendar.new].
 *
 * The date that is currently displayed can be altered with
 * [method@Gtk.Calendar.select_day].
 *
 * To place a visual marker on a particular day, use
 * [method@Gtk.Calendar.mark_day] and to remove the marker,
 * [method@Gtk.Calendar.unmark_day]. Alternative, all
 * marks can be cleared with [method@Gtk.Calendar.clear_marks].
 *
 * The selected date can be retrieved from a `GtkCalendar` using
 * [method@Gtk.Calendar.get_date].
 *
 * Users should be aware that, although the Gregorian calendar is the
 * legal calendar in most countries, it was adopted progressively
 * between 1582 and 1929. Display before these dates is likely to be
 * historically incorrect.
 *
 * # CSS nodes
 *
 * ```
 * calendar.view
 * ├── header
 * │   ├── button
 * │   ├── stack.month
 * │   ├── button
 * │   ├── button
 * │   ├── label.year
 * │   ╰── button
 * ╰── grid
 *     ╰── label[.day-name][.week-number][.day-number][.other-month][.today]
 * ```
 *
 * `GtkCalendar` has a main node with name calendar. It contains a subnode
 * called header containing the widgets for switching between years and months.
 *
 * The grid subnode contains all day labels, including week numbers on the left
 * (marked with the .week-number css class) and day names on top (marked with the
 * .day-name css class).
 *
 * Day labels that belong to the previous or next month get the .other-month
 * style class. The label of the current day get the .today style class.
 *
 * Marked day labels get the :selected state assigned.
 */

#include "config.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
#include <langinfo.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <glib.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include "gtkcalendar.h"
#include "gtkdroptarget.h"
#include <glib/gi18n-lib.h>
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtktooltip.h"
#include "gtkprivate.h"
#include "gtkrendericonprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"
#include "gtkgestureclick.h"
#include "gtkgesturedrag.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkdragsource.h"
#include "gtknative.h"
#include "gtkdragicon.h"
#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkboxlayout.h"
#include "gtkorientable.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkgrid.h"

static const guint month_length[2][13] =
{
  { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static gboolean
leap (guint year)
{
  return ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0));
}

static guint
day_of_week (guint year, guint mm, guint dd)
{
  GDateTime *dt;
  guint days;

  dt = g_date_time_new_local (year, mm, dd, 1, 1, 1);
  if (dt == NULL)
    return 0;

  days = g_date_time_get_day_of_week (dt);
  g_date_time_unref (dt);

  return days;
}

static guint
week_of_year (guint year, guint mm, guint dd)
{
  GDateTime *dt;
  guint week;

  dt = g_date_time_new_local (year, mm, dd, 1, 1, 1);
  if (dt == NULL)
    return 1;

  week = g_date_time_get_week_of_year (dt);
  g_date_time_unref (dt);

  return week;
}

enum {
  MONTH_PREV,
  MONTH_CURRENT,
  MONTH_NEXT
};

enum {
  DAY_SELECTED_SIGNAL,
  PREV_MONTH_SIGNAL,
  NEXT_MONTH_SIGNAL,
  PREV_YEAR_SIGNAL,
  NEXT_YEAR_SIGNAL,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_YEAR,
  PROP_MONTH,
  PROP_DAY,
  PROP_SHOW_HEADING,
  PROP_SHOW_DAY_NAMES,
  PROP_SHOW_WEEK_NUMBERS,
};

static guint gtk_calendar_signals[LAST_SIGNAL] = { 0 };

typedef struct _GtkCalendarClass   GtkCalendarClass;
typedef struct _GtkCalendarPrivate GtkCalendarPrivate;

struct _GtkCalendar
{
  GtkWidget widget;

  guint show_week_numbers : 1;
  guint show_heading      : 1;
  guint show_day_names    : 1;
  guint year_before       : 1;

  GtkWidget *header_box;
  GtkWidget *year_label;
  GtkWidget *month_name_stack;
  GtkWidget *arrow_widgets[4];

  GtkWidget *grid;
  GtkWidget *day_name_labels[7];
  GtkWidget *week_number_labels[6];
  GtkWidget *day_number_labels[6][7];

  GDateTime *date;

  int   day_month[6][7];
  int   day[6][7];

  int   num_marked_dates;
  int   marked_date[31];

  int   focus_row;
  int   focus_col;

  int week_start;
};

struct _GtkCalendarClass
{
  GtkWidgetClass parent_class;

  void (* day_selected)                 (GtkCalendar *calendar);
  void (* prev_month)                   (GtkCalendar *calendar);
  void (* next_month)                   (GtkCalendar *calendar);
  void (* prev_year)                    (GtkCalendar *calendar);
  void (* next_year)                    (GtkCalendar *calendar);
};

static void gtk_calendar_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec);
static void gtk_calendar_get_property (GObject      *object,
                                       guint         prop_id,
                                       GValue       *value,
                                       GParamSpec   *pspec);

static void     gtk_calendar_button_press   (GtkGestureClick *gesture,
                                             int              n_press,
                                             double           x,
                                             double           y,
                                             gpointer         user_data);
static gboolean gtk_calendar_key_controller_key_pressed (GtkEventControllerKey *controller,
                                                         guint                  keyval,
                                                         guint                  keycode,
                                                         GdkModifierType        state,
                                                         GtkWidget             *widget);
static void     gtk_calendar_focus_controller_focus     (GtkEventController    *controller,
                                                         GtkWidget             *widget);

static void calendar_select_day_internal (GtkCalendar *calendar,
                                          GDateTime   *date,
                                          gboolean     emit_day_signal);

static void calendar_invalidate_day     (GtkCalendar *widget,
                                         int        row,
                                         int        col);
static void calendar_invalidate_day_num (GtkCalendar *widget,
                                         int        day);

static gboolean gtk_calendar_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                                       double                    dx,
                                                       double                    dy,
                                                       GtkWidget                *widget);

static void     calendar_set_month_prev (GtkCalendar *calendar);
static void     calendar_set_month_next (GtkCalendar *calendar);
static void     calendar_set_year_prev  (GtkCalendar *calendar);
static void     calendar_set_year_next  (GtkCalendar *calendar);


static char    *default_abbreviated_dayname[7];
static char    *default_monthname[12];

G_DEFINE_TYPE (GtkCalendar, gtk_calendar, GTK_TYPE_WIDGET)

static void
gtk_calendar_drag_notify_value (GtkDropTarget  *target,
                                GParamSpec    **pspec,
                                GtkCalendar    *calendar)
{
  GDate *date;
  const GValue *value;

  value = gtk_drop_target_get_value (target);
  if (value == NULL)
    return;

  date = g_date_new ();
  g_date_set_parse (date, g_value_get_string (value));
  if (!g_date_valid (date))
    gtk_drop_target_reject (target);
  g_date_free (date);
}

static gboolean
gtk_calendar_drag_drop (GtkDropTarget  *dest,
                        const GValue   *value,
                        double          x,
                        double          y,
                        GtkCalendar    *calendar)
{
  GDate *date;
  GDateTime *datetime;

  date = g_date_new ();
  g_date_set_parse (date, g_value_get_string (value));

  if (!g_date_valid (date))
    {
      g_warning ("Received invalid date data");
      g_date_free (date);
      return FALSE;
    }

  datetime = g_date_time_new_local (g_date_get_year (date),
                                    g_date_get_month (date),
                                    g_date_get_day (date),
                                    0, 0, 0);
  g_date_free (date);

  calendar_select_day_internal (calendar, datetime, TRUE);
  g_date_time_unref (datetime);

  return TRUE;
}

static void
gtk_calendar_dispose (GObject *object)
{
  GtkCalendar *calendar = GTK_CALENDAR (object);

  g_clear_pointer (&calendar->date, g_date_time_unref);
  g_clear_pointer (&calendar->header_box, gtk_widget_unparent);
  g_clear_pointer (&calendar->grid, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_calendar_parent_class)->dispose (object);
}

static void
gtk_calendar_class_init (GtkCalendarClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*)  class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->dispose = gtk_calendar_dispose;
  gobject_class->set_property = gtk_calendar_set_property;
  gobject_class->get_property = gtk_calendar_get_property;

  /**
   * GtkCalendar:year:
   *
   * The selected year.
   *
   * This property gets initially set to the current year.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_YEAR,
                                   g_param_spec_int ("year", NULL, NULL,
                                                     1, 9999, 1,
                                                     G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar:month:
   *
   * The selected month (as a number between 0 and 11).
   *
   * This property gets initially set to the current month.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MONTH,
                                   g_param_spec_int ("month", NULL, NULL,
                                                     0, 11, 0,
                                                     G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar:day:
   *
   * The selected day (as a number between 1 and 31).
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DAY,
                                   g_param_spec_int ("day", NULL, NULL,
                                                     1, 31, 1,
                                                     G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar:show-heading: (attributes org.gtk.Property.get=gtk_calendar_get_show_heading org.gtk.Property.set=gtk_calendar_set_show_heading)
   *
   * Determines whether a heading is displayed.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_HEADING,
                                   g_param_spec_boolean ("show-heading", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar:show-day-names: (attributes org.gtk.Property.get=gtk_calendar_get_show_day_names org.gtk.Property.set=gtk_calendar_set_show_day_names)
   *
   * Determines whether day names are displayed.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_DAY_NAMES,
                                   g_param_spec_boolean ("show-day-names", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkCalendar:show-week-numbers: (attributes org.gtk.Property.get=gtk_calendar_get_show_week_numbers org.gtk.Property.set=gtk_calendar_set_show_week_numbers)
   *
   * Determines whether week numbers are displayed.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_WEEK_NUMBERS,
                                   g_param_spec_boolean ("show-week-numbers", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCalendar::day-selected:
   * @calendar: the object which received the signal.
   *
   * Emitted when the user selects a day.
   */
  gtk_calendar_signals[DAY_SELECTED_SIGNAL] =
    g_signal_new (I_("day-selected"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, day_selected),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCalendar::prev-month:
   * @calendar: the object which received the signal.
   *
   * Emitted when the user switched to the previous month.
   */
  gtk_calendar_signals[PREV_MONTH_SIGNAL] =
    g_signal_new (I_("prev-month"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, prev_month),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCalendar::next-month:
   * @calendar: the object which received the signal.
   *
   * Emitted when the user switched to the next month.
   */
  gtk_calendar_signals[NEXT_MONTH_SIGNAL] =
    g_signal_new (I_("next-month"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, next_month),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCalendar::prev-year:
   * @calendar: the object which received the signal.
   *
   * Emitted when user switched to the previous year.
   */
  gtk_calendar_signals[PREV_YEAR_SIGNAL] =
    g_signal_new (I_("prev-year"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, prev_year),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCalendar::next-year:
   * @calendar: the object which received the signal.
   *
   * Emitted when user switched to the next year.
   */
  gtk_calendar_signals[NEXT_YEAR_SIGNAL] =
    g_signal_new (I_("next-year"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCalendarClass, next_year),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("calendar"));
}

static GdkContentProvider *
gtk_calendar_drag_prepare (GtkDragSource *source,
                           double         x,
                           double         y,
                           GtkCalendar   *calendar)
{
  GDate *date;
  char str[128];

  date = g_date_new_dmy (g_date_time_get_day_of_month (calendar->date),
                         g_date_time_get_month (calendar->date),
                         g_date_time_get_year (calendar->date));
  g_date_strftime (str, 127, "%x", date);
  g_free (date);

  return gdk_content_provider_new_typed (G_TYPE_STRING, str);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static void
gtk_calendar_init (GtkCalendar *calendar)
{
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkEventController *controller;
  GtkGesture *gesture;
  GtkDragSource *source;
  GtkDropTarget *target;
  int i;
#ifdef G_OS_WIN32
  wchar_t wbuffer[100];
#else
  static const char *month_format = NULL;
  char buffer[255];
  time_t tmp_time;
#endif
  char *year_before;
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  union { unsigned int word; char *string; } langinfo;
  int week_1stday = 0;
  int first_weekday = 1;
  guint week_origin;
#else
  char *week_start;
#endif
  int min_year_width;
  GDateTime *now;

  gtk_widget_set_focusable (widget, TRUE);

  gtk_widget_add_css_class (GTK_WIDGET (calendar), "view");

  calendar->header_box = g_object_new (GTK_TYPE_BOX,
                                   "css-name", "header",
                                   NULL);
  calendar->year_label = gtk_label_new ("");
  gtk_widget_add_css_class (calendar->year_label, "year");
  calendar->month_name_stack = gtk_stack_new ();
  gtk_widget_add_css_class (calendar->month_name_stack, "month");
  calendar->arrow_widgets[0] = gtk_button_new_from_icon_name ("pan-start-symbolic");
  g_signal_connect_swapped (calendar->arrow_widgets[0], "clicked", G_CALLBACK (calendar_set_month_prev), calendar);
  calendar->arrow_widgets[1] = gtk_button_new_from_icon_name ("pan-end-symbolic");
  g_signal_connect_swapped (calendar->arrow_widgets[1], "clicked", G_CALLBACK (calendar_set_month_next), calendar);
  gtk_widget_set_hexpand (calendar->arrow_widgets[1], TRUE);
  gtk_widget_set_halign (calendar->arrow_widgets[1], GTK_ALIGN_START);
  calendar->arrow_widgets[2] = gtk_button_new_from_icon_name ("pan-start-symbolic");
  g_signal_connect_swapped (calendar->arrow_widgets[2], "clicked", G_CALLBACK (calendar_set_year_prev), calendar);
  calendar->arrow_widgets[3] = gtk_button_new_from_icon_name ("pan-end-symbolic");
  g_signal_connect_swapped (calendar->arrow_widgets[3], "clicked", G_CALLBACK (calendar_set_year_next), calendar);

  gtk_box_append (GTK_BOX (calendar->header_box), calendar->arrow_widgets[0]);
  gtk_box_append (GTK_BOX (calendar->header_box), calendar->month_name_stack);
  gtk_box_append (GTK_BOX (calendar->header_box), calendar->arrow_widgets[1]);
  gtk_box_append (GTK_BOX (calendar->header_box), calendar->arrow_widgets[2]);
  gtk_box_append (GTK_BOX (calendar->header_box), calendar->year_label);
  gtk_box_append (GTK_BOX (calendar->header_box), calendar->arrow_widgets[3]);

  gtk_widget_set_parent (calendar->header_box, GTK_WIDGET (calendar));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (gtk_calendar_button_press), calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), GTK_EVENT_CONTROLLER (gesture));

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (gtk_calendar_drag_prepare), calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), GTK_EVENT_CONTROLLER (source));

  controller =
    gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                                     GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (gtk_calendar_scroll_controller_scroll),
                    calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), controller);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_calendar_key_controller_key_pressed),
                    calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), controller);
  controller = gtk_event_controller_focus_new ();
  g_signal_connect (controller, "enter",
                    G_CALLBACK (gtk_calendar_focus_controller_focus),
                    calendar);
  g_signal_connect (controller, "leave",
                    G_CALLBACK (gtk_calendar_focus_controller_focus),
                    calendar);
  gtk_widget_add_controller (GTK_WIDGET (calendar), controller);

#ifdef G_OS_WIN32
  calendar->week_start = 0;
  week_start = NULL;

  if (GetLocaleInfoW (GetThreadLocale (), LOCALE_IFIRSTDAYOFWEEK,
                      wbuffer, G_N_ELEMENTS (wbuffer)))
    week_start = g_utf16_to_utf8 (wbuffer, -1, NULL, NULL, NULL);

  if (week_start != NULL)
    {
      calendar->week_start = (week_start[0] - '0' + 1) % 7;
      g_free(week_start);
    }
#else
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  langinfo.string = nl_langinfo (_NL_TIME_FIRST_WEEKDAY);
  first_weekday = langinfo.string[0];
  langinfo.string = nl_langinfo (_NL_TIME_WEEK_1STDAY);
  week_origin = langinfo.word;
  if (week_origin == 19971130) /* Sunday */
    week_1stday = 0;
  else if (week_origin == 19971201) /* Monday */
    week_1stday = 1;
  else
    g_warning ("Unknown value of _NL_TIME_WEEK_1STDAY.");

  calendar->week_start = (week_1stday + first_weekday - 1) % 7;
#else
  /* Translate to calendar:week_start:0 if you want Sunday to be the
   * first day of the week to calendar:week_start:1 if you want Monday
   * to be the first day of the week, and so on.
   */
  week_start = _("calendar:week_start:0");

  if (strncmp (week_start, "calendar:week_start:", 20) == 0)
    calendar->week_start = *(week_start + 20) - '0';
  else
    calendar->week_start = -1;

  if (calendar->week_start < 0 || calendar->week_start > 6)
    {
      g_warning ("Whoever translated calendar:week_start:0 did so wrongly.");
      calendar->week_start = 0;
    }
#endif
#endif

  if (!default_abbreviated_dayname[0])
    for (i=0; i<7; i++)
      {
#ifndef G_OS_WIN32
        tmp_time= (i+3)*86400; /* epoch was a Thursday, so add 3 days for Sunday */
        strftime (buffer, sizeof (buffer), "%a", gmtime (&tmp_time));
        default_abbreviated_dayname[i] = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
#else
        if (!GetLocaleInfoW (GetThreadLocale (), LOCALE_SABBREVDAYNAME1 + (i+6)%7,
                             wbuffer, G_N_ELEMENTS (wbuffer)))
          default_abbreviated_dayname[i] = g_strdup_printf ("(%d)", i);
        else
          default_abbreviated_dayname[i] = g_utf16_to_utf8 (wbuffer, -1, NULL, NULL, NULL);
#endif
      }

  if (!default_monthname[0])
    for (i=0; i<12; i++)
      {
#ifndef G_OS_WIN32
        tmp_time=i*2764800;
        if (G_UNLIKELY (month_format == NULL))
          {
            buffer[0] = '\0';
            month_format = "%OB";
            strftime (buffer, sizeof (buffer), month_format, gmtime (&tmp_time));
            /* "%OB" is not supported in Linux with glibc < 2.27  */
            if (!strcmp (buffer, "%OB") || !strcmp (buffer, "OB") || !strcmp (buffer, ""))
              {
                month_format = "%B";
                strftime (buffer, sizeof (buffer), month_format, gmtime (&tmp_time));
              }
          }
        else
          strftime (buffer, sizeof (buffer), month_format, gmtime (&tmp_time));

        default_monthname[i] = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
#else
        if (!GetLocaleInfoW (GetThreadLocale (), LOCALE_SMONTHNAME1 + i,
                             wbuffer, G_N_ELEMENTS (wbuffer)))
          default_monthname[i] = g_strdup_printf ("(%d)", i);
        else
          default_monthname[i] = g_utf16_to_utf8 (wbuffer, -1, NULL, NULL, NULL);
#endif
      }

  for (i = 0; i < 12; i ++)
    {
      GtkWidget *month_label = gtk_label_new (default_monthname[i]);

      gtk_stack_add_named (GTK_STACK (calendar->month_name_stack), month_label, default_monthname[i]);
    }

  calendar->grid = gtk_grid_new ();
  gtk_grid_set_row_homogeneous (GTK_GRID (calendar->grid), TRUE);
  gtk_grid_set_column_homogeneous (GTK_GRID (calendar->grid), TRUE);
  /* Day name labels */
  for (i = 0; i < 7; i ++)
    {
      int day;
      GtkWidget *label;

      day = (i + calendar->week_start) % 7;
      label = gtk_label_new (default_abbreviated_dayname[day]);

      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_set_vexpand (label, TRUE);
      gtk_widget_add_css_class (label, "day-name");
      gtk_grid_attach (GTK_GRID (calendar->grid), label, 1 + i, 0, 1, 1);

      calendar->day_name_labels[i] = label;
    }

  /* Week number labels */
  for (i = 0; i < 6; i ++)
    {
      GtkWidget *label = gtk_label_new ("");

      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_set_vexpand (label, TRUE);
      gtk_widget_add_css_class (label, "week-number");
      gtk_grid_attach (GTK_GRID (calendar->grid), label, 0, 1 + i, 1, 1);

      calendar->week_number_labels[i] = label;
      gtk_widget_set_visible (label, FALSE);
    }

  {
    int x, y;

    for (y = 0; y < 6; y ++)
      for (x = 0; x < 7; x ++)
        {
          GtkWidget *label = gtk_label_new ("");

          gtk_widget_set_hexpand (label, TRUE);
          gtk_widget_set_vexpand (label, TRUE);
          gtk_widget_add_css_class (label, "day-number");
          gtk_grid_attach (GTK_GRID (calendar->grid), label, 1 + x, 1 + y, 1, 1);

          calendar->day_number_labels[y][x] = label;
        }
  }

  gtk_widget_set_hexpand (calendar->grid, TRUE);
  gtk_widget_set_vexpand (calendar->grid, TRUE);
  gtk_widget_set_parent (calendar->grid, GTK_WIDGET (calendar));

  for (i=0;i<31;i++)
    calendar->marked_date[i] = FALSE;
  calendar->num_marked_dates = 0;

  calendar->show_heading = TRUE;
  calendar->show_day_names = TRUE;

  calendar->focus_row = -1;
  calendar->focus_col = -1;

  target = gtk_drop_target_new (G_TYPE_STRING, GDK_ACTION_COPY);
  gtk_drop_target_set_preload (target, TRUE);
  g_signal_connect (target, "notify::value", G_CALLBACK (gtk_calendar_drag_notify_value), calendar);
  g_signal_connect (target, "drop", G_CALLBACK (gtk_calendar_drag_drop), calendar);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (target));

  calendar->year_before = 0;

  /* Translate to calendar:YM if you want years to be displayed
   * before months; otherwise translate to calendar:MY.
   * Do *not* translate it to anything else, if it
   * it isn't calendar:YM or calendar:MY it will not work.
   *
   * Note that the ordering described here is logical order, which is
   * further influenced by BIDI ordering. Thus, if you have a default
   * text direction of RTL and specify "calendar:YM", then the year
   * will appear to the right of the month.
   */
  year_before = _("calendar:MY");
  if (strcmp (year_before, "calendar:YM") == 0)
    calendar->year_before = 1;
  else if (strcmp (year_before, "calendar:MY") != 0)
    g_warning ("Whoever translated calendar:MY did so wrongly.");

  gtk_orientable_set_orientation (GTK_ORIENTABLE (gtk_widget_get_layout_manager (GTK_WIDGET (calendar))),
                                  GTK_ORIENTATION_VERTICAL);

  /* Select current day */
  calendar->date = g_date_time_new_from_unix_local (0);
  now = g_date_time_new_now_local ();
  calendar_select_day_internal (calendar, now, FALSE);
  g_date_time_unref (now);

  /* We just initialized the year label, now add some space to it so
   * changing the year does not increase the calendar width */
  gtk_widget_measure (calendar->year_label, GTK_ORIENTATION_HORIZONTAL, -1,
                      &min_year_width, NULL, NULL, NULL);
  gtk_widget_set_size_request (calendar->year_label, min_year_width + 10, -1);
}

#pragma GCC diagnostic pop

static void
calendar_queue_refresh (GtkCalendar *calendar)
{
  gtk_widget_queue_resize (GTK_WIDGET (calendar));
}

static void
calendar_set_month_prev (GtkCalendar *calendar)
{
  GDateTime *new_date;

  new_date = g_date_time_add_months (calendar->date, -1);

  calendar_select_day_internal (calendar, new_date, FALSE);
  g_date_time_unref (new_date);

  g_signal_emit (calendar, gtk_calendar_signals[PREV_MONTH_SIGNAL], 0);
}

static void
calendar_set_month_next (GtkCalendar *calendar)
{
  GDateTime *new_date;

  new_date = g_date_time_add_months (calendar->date, 1);

  calendar_select_day_internal (calendar, new_date, FALSE);
  g_date_time_unref (new_date);

  g_signal_emit (calendar, gtk_calendar_signals[NEXT_MONTH_SIGNAL], 0);
}

static void
calendar_set_year_prev (GtkCalendar *calendar)
{
  GDateTime *new_date;

  new_date = g_date_time_add_years (calendar->date, -1);

  calendar_select_day_internal (calendar, new_date, FALSE);
  g_date_time_unref (new_date);

  g_signal_emit (calendar, gtk_calendar_signals[PREV_YEAR_SIGNAL], 0);
}

static void
calendar_set_year_next (GtkCalendar *calendar)
{
  GDateTime *new_date;

  new_date = g_date_time_add_years (calendar->date, 1);

  calendar_select_day_internal (calendar, new_date, FALSE);
  g_date_time_unref (new_date);

  g_signal_emit (calendar, gtk_calendar_signals[NEXT_YEAR_SIGNAL], 0);
}

static void
calendar_compute_days (GtkCalendar *calendar)
{
  const int month = g_date_time_get_month (calendar->date);
  const int year = g_date_time_get_year (calendar->date);
  int ndays_in_month;
  int ndays_in_prev_month;
  int first_day;
  int row;
  int col;
  int day;

  ndays_in_month = month_length[leap (year)][month];

  first_day = day_of_week (year, month, 1);
  first_day = (first_day + 7 - calendar->week_start) % 7;
  if (first_day == 0)
    first_day = 7;

  /* Compute days of previous month */
  if (month > 1)
    ndays_in_prev_month = month_length[leap (year)][month - 1];
  else
    ndays_in_prev_month = month_length[leap (year - 1)][12];
  day = ndays_in_prev_month - first_day+ 1;

  for (col = 0; col < first_day; col++)
    {
      calendar->day[0][col] = day;
      calendar->day_month[0][col] = MONTH_PREV;
      day++;
    }

  /* Compute days of current month */
  row = first_day / 7;
  col = first_day % 7;
  for (day = 1; day <= ndays_in_month; day++)
    {
      calendar->day[row][col] = day;
      calendar->day_month[row][col] = MONTH_CURRENT;

      col++;
      if (col == 7)
        {
          row++;
          col = 0;
        }
    }

  /* Compute days of next month */
  day = 1;
  for (; row <= 5; row++)
    {
      for (; col <= 6; col++)
        {
          calendar->day[row][col] = day;
          calendar->day_month[row][col] = MONTH_NEXT;
          day++;
        }
      col = 0;
    }
}

static void
calendar_select_day_internal (GtkCalendar *calendar,
                              GDateTime   *date,
                              gboolean     emit_day_signal)
{
  GDateTime *today;
  int new_day, new_month, new_year;
  gboolean day_changed, month_changed, year_changed;
  char buffer[255];
  char *str;
  time_t tmp_time;
  struct tm *tm;
  int i;
  int x, y;
  int today_day;

  day_changed = g_date_time_get_day_of_month (calendar->date) != g_date_time_get_day_of_month (date);
  month_changed = g_date_time_get_month (calendar->date) != g_date_time_get_month (date);
  year_changed = g_date_time_get_year (calendar->date) != g_date_time_get_year (date);

  if (!day_changed && !month_changed && !year_changed)
    return;

  new_year = g_date_time_get_year (date);
  new_month = g_date_time_get_month (date);
  new_day = g_date_time_get_day_of_month (date);

  g_date_time_unref (calendar->date);
  calendar->date = g_date_time_ref (date);

  tmp_time = 1;  /* Jan 1 1970, 00:00:01 UTC */
  tm = gmtime (&tmp_time);
  tm->tm_year = new_year - 1900;

  /* Translators: This dictates how the year is displayed in
   * gtkcalendar widget.  See strftime() manual for the format.
   * Use only ASCII in the translation.
   *
   * "%Y" is appropriate for most locales.
   */
  strftime (buffer, sizeof (buffer), C_("calendar year format", "%Y"), tm);
  str = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
  gtk_label_set_label (GTK_LABEL (calendar->year_label), str);
  g_free (str);

  /* Update month */

  calendar_compute_days (calendar);
  gtk_stack_set_visible_child_name (GTK_STACK (calendar->month_name_stack),
                                    default_monthname[new_month - 1]);

  today = g_date_time_new_now_local ();

  if (g_date_time_get_year (calendar->date) == g_date_time_get_year (today) &&
      g_date_time_get_month (calendar->date) == g_date_time_get_month (today))
    today_day = g_date_time_get_day_of_month (today);
  else
    today_day = -1;

  g_date_time_unref (today);

  /* Update day labels */
  for (y = 0; y < 6; y ++)
    for (x = 0; x < 7; x ++)
      {
        const int day = calendar->day[y][x];
        GtkWidget *label = calendar->day_number_labels[y][x];
        /* Translators: this defines whether the day numbers should use
         * localized digits or the ones used in English (0123...).
         *
         * Translate to "%Id" if you want to use localized digits, or
         * translate to "%d" otherwise.
         *
         * Note that translating this doesn't guarantee that you get localized
         * digits. That needs support from your system and locale definition
         * too.
         */
        g_snprintf (buffer, sizeof (buffer), C_("calendar:day:digits", "%d"), day);

        gtk_label_set_label (GTK_LABEL (label), buffer);

        if (calendar->day_month[y][x] == MONTH_PREV ||
            calendar->day_month[y][x] == MONTH_NEXT)
          gtk_widget_add_css_class (label, "other-month");
        else
          gtk_widget_remove_css_class (label, "other-month");

        if (calendar->marked_date[day-1] &&
            calendar->day_month[y][x] == MONTH_CURRENT)
          gtk_widget_set_state_flags (label, GTK_STATE_FLAG_CHECKED, FALSE);
        else
          gtk_widget_unset_state_flags (label, GTK_STATE_FLAG_CHECKED);

        if (new_day == day &&
            calendar->day_month[y][x] == MONTH_CURRENT)
          gtk_widget_set_state_flags (label, GTK_STATE_FLAG_SELECTED, FALSE);
        else
          gtk_widget_unset_state_flags (label, GTK_STATE_FLAG_SELECTED);

        if (calendar->focus_row == y && calendar->focus_col == x)
          gtk_widget_set_state_flags (label, GTK_STATE_FLAG_FOCUSED, FALSE);
        else
          gtk_widget_unset_state_flags (label, GTK_STATE_FLAG_FOCUSED);

        if (day == today_day &&
            calendar->day_month[y][x] == MONTH_CURRENT)
          gtk_widget_add_css_class (label, "today");
        else
          gtk_widget_remove_css_class (label, "today");
      }

  /* Update week number labels.
   * We simply get the week number of calendar->date and add the others.
   * simple. */
  for (i = 0; i < 6; i ++)
    {
      int year = new_year;
      int month, week;

      month = new_month + calendar->day_month[i][6] - MONTH_CURRENT;

      if (month < 1)
        {
          month += 12;
          year -= 1;
        }
      else if (month > 12)
        {
          month -= 12;
          year += 1;
        }

      week = week_of_year (year, month, calendar->day[i][6]);

      /* Translators: this defines whether the week numbers should use
       * localized digits or the ones used in English (0123...).
       *
       * Translate to "%Id" if you want to use localized digits, or
       * translate to "%d" otherwise.
       * Note that translating this doesn't guarantee that you get localized
       * digits. That needs support from your system and locale definition
       * too. */
      g_snprintf (buffer, sizeof (buffer), C_("calendar:week:digits", "%d"), week);

      gtk_label_set_label (GTK_LABEL (calendar->week_number_labels[i]), buffer);
    }

  if (day_changed)
    {
      g_object_notify (G_OBJECT (calendar), "day");

      if (emit_day_signal)
        g_signal_emit (calendar, gtk_calendar_signals[DAY_SELECTED_SIGNAL], 0);
    }

  if (month_changed)
    g_object_notify (G_OBJECT (calendar), "month");

  if (year_changed)
    g_object_notify (G_OBJECT (calendar), "year");
}

static void
calendar_select_and_focus_day (GtkCalendar *calendar,
                               int          day)
{
  GDateTime *new_date;
  int row;
  int col;

  for (row = 0; row < 6; row ++)
    for (col = 0; col < 7; col++)
      {
        if (calendar->day_month[row][col] == MONTH_CURRENT &&
            calendar->day[row][col] == day)
          {
            calendar->focus_row = row;
            calendar->focus_col = col;
            break;
          }
      }

  new_date = g_date_time_new_local (g_date_time_get_year (calendar->date),
                                    g_date_time_get_month (calendar->date),
                                    day,
                                    0, 0, 0);

  calendar_select_day_internal (calendar, new_date, TRUE);
  g_date_time_unref (new_date);
}

static void
gtk_calendar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkCalendar *calendar = GTK_CALENDAR (object);

  switch (prop_id)
    {
    case PROP_YEAR:
      gtk_calendar_set_year (calendar, g_value_get_int (value));
      break;
    case PROP_MONTH:
      gtk_calendar_set_month (calendar, g_value_get_int (value));
      break;
    case PROP_DAY:
      gtk_calendar_set_day (calendar, g_value_get_int (value));
      break;
    case PROP_SHOW_HEADING:
      gtk_calendar_set_show_heading (calendar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_DAY_NAMES:
      gtk_calendar_set_show_day_names (calendar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_WEEK_NUMBERS:
      gtk_calendar_set_show_week_numbers (calendar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_calendar_get_property (GObject      *object,
                           guint         prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  GtkCalendar *calendar = GTK_CALENDAR (object);

  switch (prop_id)
    {
    case PROP_YEAR:
      g_value_set_int (value, gtk_calendar_get_year (calendar));
      break;
    case PROP_MONTH:
      g_value_set_int (value, gtk_calendar_get_month (calendar));
      break;
    case PROP_DAY:
      g_value_set_int (value, gtk_calendar_get_day (calendar));
      break;
    case PROP_SHOW_HEADING:
      g_value_set_boolean (value, gtk_calendar_get_show_heading (calendar));
      break;
    case PROP_SHOW_DAY_NAMES:
      g_value_set_boolean (value, gtk_calendar_get_show_day_names (calendar));
      break;
    case PROP_SHOW_WEEK_NUMBERS:
      g_value_set_boolean (value, gtk_calendar_get_show_week_numbers (calendar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
calendar_invalidate_day_num (GtkCalendar *calendar,
                             int          day)
{
  gtk_widget_queue_draw (GTK_WIDGET (calendar));
}

static void
calendar_invalidate_day (GtkCalendar *calendar,
                         int          row,
                         int          col)
{
  gtk_widget_queue_draw (GTK_WIDGET (calendar));
}

static void
gtk_calendar_button_press (GtkGestureClick *gesture,
                           int              n_press,
                           double           x,
                           double           y,
                           gpointer         user_data)
{
  GtkCalendar *calendar = user_data;
  GtkWidget *widget = GTK_WIDGET (calendar);
  GtkWidget *label;
  int row = -1, col = -1;
  int ix, iy;
  int day_month;
  int day;

  label = gtk_widget_pick (widget, x, y, GTK_PICK_DEFAULT);
  for (iy = 0; iy < 6; iy ++)
    for (ix = 0; ix < 7; ix ++)
      {
        if (label == calendar->day_number_labels[iy][ix])
          {
            row = iy;
            col = ix;
          }
      }

  /* If row or column isn't found, just return. */
  if (row == -1 || col == -1)
    return;

  day_month = calendar->day_month[row][col];
  day = calendar->day[row][col];

  if (day_month == MONTH_PREV)
    calendar_set_month_prev (calendar);
  else if (day_month == MONTH_NEXT)
    calendar_set_month_next (calendar);

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  calendar_select_and_focus_day (calendar, day);
}

static gboolean
gtk_calendar_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                       double                    dx,
                                       double                    dy,
                                       GtkWidget                *widget)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (dy < 0)
    calendar_set_month_prev (calendar);
  else if (dy > 0)
    calendar_set_month_next (calendar);

  return GDK_EVENT_STOP;
}


/****************************************
 *             Key handling              *
 ****************************************/

static void
move_focus (GtkCalendar *calendar,
            int          direction,
            int          updown)
{
  GtkTextDirection text_dir = gtk_widget_get_direction (GTK_WIDGET (calendar));
  int x, y;

  if (updown == 1)
    {
      if (calendar->focus_row > 0)
        calendar->focus_row--;
      if (calendar->focus_row < 0)
        calendar->focus_row = 5;
      if (calendar->focus_col < 0)
        calendar->focus_col = 6;
    }
  else if (updown == -1)
    {
      if (calendar->focus_row < 5)
        calendar->focus_row++;
      if (calendar->focus_col < 0)
        calendar->focus_col = 0;
    }
  else if ((text_dir == GTK_TEXT_DIR_LTR && direction == -1) ||
           (text_dir == GTK_TEXT_DIR_RTL && direction == 1))
    {
      if (calendar->focus_col > 0)
          calendar->focus_col--;
      else if (calendar->focus_row > 0)
        {
          calendar->focus_col = 6;
          calendar->focus_row--;
        }

      if (calendar->focus_col < 0)
        calendar->focus_col = 6;
      if (calendar->focus_row < 0)
        calendar->focus_row = 5;
    }
  else
    {
      if (calendar->focus_col < 6)
        calendar->focus_col++;
      else if (calendar->focus_row < 5)
        {
          calendar->focus_col = 0;
          calendar->focus_row++;
        }

      if (calendar->focus_col < 0)
        calendar->focus_col = 0;
      if (calendar->focus_row < 0)
        calendar->focus_row = 0;
    }

  for (y = 0; y < 6; y ++)
    for (x = 0; x < 7; x ++)
      {
        GtkWidget *label = calendar->day_number_labels[y][x];

        if (calendar->focus_row == y && calendar->focus_col == x)
          gtk_widget_set_state_flags (label, GTK_STATE_FLAG_FOCUSED, FALSE);
        else
          gtk_widget_unset_state_flags (label, GTK_STATE_FLAG_FOCUSED);
      }
}

static gboolean
gtk_calendar_key_controller_key_pressed (GtkEventControllerKey *controller,
                                         guint                  keyval,
                                         guint                  keycode,
                                         GdkModifierType        state,
                                         GtkWidget             *widget)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);
  int return_val;
  int old_focus_row;
  int old_focus_col;
  int row, col, day;

  return_val = FALSE;

  old_focus_row = calendar->focus_row;
  old_focus_col = calendar->focus_col;

  switch (keyval)
    {
    case GDK_KEY_KP_Left:
    case GDK_KEY_Left:
      return_val = TRUE;
      if (state & GDK_CONTROL_MASK)
        calendar_set_month_prev (calendar);
      else
        {
          move_focus (calendar, -1, 0);
          calendar_invalidate_day (calendar, old_focus_row, old_focus_col);
          calendar_invalidate_day (calendar, calendar->focus_row, calendar->focus_col);
        }
      break;
    case GDK_KEY_KP_Right:
    case GDK_KEY_Right:
      return_val = TRUE;
      if (state & GDK_CONTROL_MASK)
        calendar_set_month_next (calendar);
      else
        {
          move_focus (calendar, 1, 0);
          calendar_invalidate_day (calendar, old_focus_row, old_focus_col);
          calendar_invalidate_day (calendar, calendar->focus_row, calendar->focus_col);
        }
      break;
    case GDK_KEY_KP_Up:
    case GDK_KEY_Up:
      return_val = TRUE;
      if (state & GDK_CONTROL_MASK)
        calendar_set_year_prev (calendar);
      else
        {
          move_focus (calendar, 0, 1);
          calendar_invalidate_day (calendar, old_focus_row, old_focus_col);
          calendar_invalidate_day (calendar, calendar->focus_row, calendar->focus_col);
        }
      break;
    case GDK_KEY_KP_Down:
    case GDK_KEY_Down:
      return_val = TRUE;
      if (state & GDK_CONTROL_MASK)
        calendar_set_year_next (calendar);
      else
        {
          move_focus (calendar, 0, -1);
          calendar_invalidate_day (calendar, old_focus_row, old_focus_col);
          calendar_invalidate_day (calendar, calendar->focus_row, calendar->focus_col);
        }
      break;
    case GDK_KEY_KP_Space:
    case GDK_KEY_space:
      row = calendar->focus_row;
      col = calendar->focus_col;

      if (row > -1 && col > -1)
        {
          return_val = TRUE;

          day = calendar->day[row][col];
          if (calendar->day_month[row][col] == MONTH_PREV)
            calendar_set_month_prev (calendar);
          else if (calendar->day_month[row][col] == MONTH_NEXT)
            calendar_set_month_next (calendar);

          calendar_select_and_focus_day (calendar, day);
        }
      break;
    default:
      break;
    }

  return return_val;
}

static void
gtk_calendar_focus_controller_focus (GtkEventController     *controller,
                                     GtkWidget              *widget)
{
  GtkCalendar *calendar = GTK_CALENDAR (widget);

  calendar_queue_refresh (calendar);
}


/****************************************
 *              Public API              *
 ****************************************/

/**
 * gtk_calendar_new:
 *
 * Creates a new calendar, with the current date being selected.
 *
 * Returns: a newly `GtkCalendar` widget
 */
GtkWidget*
gtk_calendar_new (void)
{
  return g_object_new (GTK_TYPE_CALENDAR, NULL);
}

/**
 * gtk_calendar_select_day:
 * @self: a `GtkCalendar`.
 * @date: (transfer none): a `GDateTime` representing the day to select
 *
 * Switches to @date's year and month and select its day.
 */
void
gtk_calendar_select_day (GtkCalendar *calendar,
                         GDateTime   *date)
{
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  g_return_if_fail (date != NULL);

  calendar_select_day_internal (calendar, date, TRUE);
}

/**
 * gtk_calendar_clear_marks:
 * @calendar: a `GtkCalendar`
 *
 * Remove all visual markers.
 */
void
gtk_calendar_clear_marks (GtkCalendar *calendar)
{
  guint day;

  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  for (int y = 0; y < 6; y ++)
    for (int x = 0; x < 7; x ++)
      {
        GtkWidget *label = calendar->day_number_labels[y][x];

        gtk_widget_unset_state_flags (label, GTK_STATE_FLAG_CHECKED);
      }

  for (day = 0; day < 31; day++)
    {
      calendar->marked_date[day] = FALSE;
    }

  calendar->num_marked_dates = 0;
  calendar_queue_refresh (calendar);
}

static void
update_mark_state (GtkCalendar *calendar,
                   guint        day,
                   gboolean     mark)
{
  for (int y = 0; y < 6; y ++)
    for (int x = 0; x < 7; x ++)
      {
        GtkWidget *label = calendar->day_number_labels[y][x];

        if (day != calendar->day[y][x])
          continue;

        if (mark && calendar->marked_date[day-1] &&
            calendar->day_month[y][x] == MONTH_CURRENT)
          gtk_widget_set_state_flags (label, GTK_STATE_FLAG_CHECKED, FALSE);
        else
          gtk_widget_unset_state_flags (label, GTK_STATE_FLAG_CHECKED);
      }
}

/**
 * gtk_calendar_mark_day:
 * @calendar: a `GtkCalendar`
 * @day: the day number to mark between 1 and 31.
 *
 * Places a visual marker on a particular day of the current month.
 */
void
gtk_calendar_mark_day (GtkCalendar *calendar,
                       guint        day)
{
  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  if (day >= 1 && day <= 31 && !calendar->marked_date[day-1])
    {
      calendar->marked_date[day - 1] = TRUE;
      calendar->num_marked_dates++;
      update_mark_state (calendar, day, TRUE);
      calendar_invalidate_day_num (calendar, day);
    }
}

/**
 * gtk_calendar_get_day_is_marked:
 * @calendar: a `GtkCalendar`
 * @day: the day number between 1 and 31.
 *
 * Returns if the @day of the @calendar is already marked.
 *
 * Returns: whether the day is marked.
 */
gboolean
gtk_calendar_get_day_is_marked (GtkCalendar *calendar,
                                guint        day)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), FALSE);

  if (day >= 1 && day <= 31)
    return calendar->marked_date[day - 1];

  return FALSE;
}

/**
 * gtk_calendar_unmark_day:
 * @calendar: a `GtkCalendar`.
 * @day: the day number to unmark between 1 and 31.
 *
 * Removes the visual marker from a particular day.
 */
void
gtk_calendar_unmark_day (GtkCalendar *calendar,
                         guint        day)
{
  g_return_if_fail (GTK_IS_CALENDAR (calendar));

  if (day >= 1 && day <= 31 && calendar->marked_date[day-1])
    {
      calendar->marked_date[day - 1] = FALSE;
      calendar->num_marked_dates--;
      update_mark_state (calendar, day, FALSE);
      calendar_invalidate_day_num (calendar, day);
    }
}

/**
 * gtk_calendar_get_date:
 * @self: a `GtkCalendar`
 *
 * Returns a `GDateTime` representing the shown
 * year, month and the selected day.
 *
 * The returned date is in the local time zone.
 *
 * Returns: (transfer full): the `GDateTime` representing the shown date
 */
GDateTime *
gtk_calendar_get_date (GtkCalendar *self)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (self), NULL);

  return g_date_time_ref (self->date);
}

/**
 * gtk_calendar_set_show_week_numbers: (attributes org.gtk.Method.set_property=show-week-numbers)
 * @self: a `GtkCalendar`
 * @value: whether to show week numbers on the left of the days
 *
 * Sets whether week numbers are shown in the calendar.
 */
void
gtk_calendar_set_show_week_numbers (GtkCalendar *self,
                                    gboolean     value)
{
  int i;

  g_return_if_fail (GTK_IS_CALENDAR (self));

  if (self->show_week_numbers == value)
    return;

  self->show_week_numbers = value;

  for (i = 0; i < 6; i ++)
    gtk_widget_set_visible (self->week_number_labels[i], value);

  g_object_notify (G_OBJECT (self), "show-week-numbers");
}

/**
 * gtk_calendar_get_show_week_numbers: (attributes org.gtk.Method.get_property=show-week-numbers)
 * @self: a `GtkCalendar`
 *
 * Returns whether @self is showing week numbers right
 * now.
 *
 * This is the value of the [property@Gtk.Calendar:show-week-numbers]
 * property.
 *
 * Return: Whether the calendar is showing week numbers.
 */
gboolean
gtk_calendar_get_show_week_numbers (GtkCalendar *self)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (self), FALSE);

  return self->show_week_numbers;
}

/**
 * gtk_calendar_set_show_heading: (attributes org.gtk.Method.set_property=show-heading)
 * @self: a `GtkCalendar`
 * @value: Whether to show the heading in the calendar
 *
 * Sets whether the calendar should show a heading.
 *
 * The heading contains the current year and month as well as
 * buttons for changing both.
 */
void
gtk_calendar_set_show_heading (GtkCalendar *self,
                               gboolean     value)
{
  g_return_if_fail (GTK_IS_CALENDAR (self));

  if (self->show_heading == value)
    return;

  self->show_heading = value;

  gtk_widget_set_visible (self->header_box, value);

  g_object_notify (G_OBJECT (self), "show-heading");
}

/**
 * gtk_calendar_get_show_heading: (attributes org.gtk.Method.get_property=show-heading)
 * @self: a `GtkCalendar`
 *
 * Returns whether @self is currently showing the heading.
 *
 * This is the value of the [property@Gtk.Calendar:show-heading]
 * property.
 *
 * Return: Whether the calendar is showing a heading.
 */
gboolean
gtk_calendar_get_show_heading (GtkCalendar *self)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (self), FALSE);

  return self->show_heading;
}

/**
 * gtk_calendar_set_show_day_names: (attributes org.gtk.Method.set_property=show-day-names)
 * @self: a `GtkCalendar`
 * @value: Whether to show day names above the day numbers
 *
 * Sets whether the calendar shows day names.
 */
void
gtk_calendar_set_show_day_names (GtkCalendar *self,
                                 gboolean     value)
{
  int i;

  g_return_if_fail (GTK_IS_CALENDAR (self));

  if (self->show_day_names == value)
    return;

  self->show_day_names = value;

  for (i = 0; i < 7; i ++)
    gtk_widget_set_visible (self->day_name_labels[i], value);

  g_object_notify (G_OBJECT (self), "show-day-names");
}

/**
 * gtk_calendar_get_show_day_names: (attributes org.gtk.Method.get_property=show-day-names)
 * @self: a `GtkCalendar`
 *
 * Returns whether @self is currently showing the names
 * of the week days.
 *
 * This is the value of the [property@Gtk.Calendar:show-day-names]
 * property.
 *
 * Returns: Whether the calendar shows day names.
 */
gboolean
gtk_calendar_get_show_day_names (GtkCalendar *self)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (self), FALSE);

  return self->show_day_names;
}

/**
 * gtk_calendar_set_day:
 * @self: a `GtkCalendar`
 * @day: The desired day for the selected date (as a number between 1 and 31).
 *
 * Sets the day for the selected date.
 *
 * The new date must be valid. For example, setting 31 for the day when the
 * month is February, fails.
 *
 * Since: 4.14
 */
void
gtk_calendar_set_day (GtkCalendar *self,
                      int          day)
{
  GDateTime *date;

  g_return_if_fail (GTK_IS_CALENDAR (self));
  g_return_if_fail (day >= 1 && day <= 31);

  if (day == g_date_time_get_day_of_month (self->date))
    return;

  date = g_date_time_new_local (g_date_time_get_year (self->date),
                                g_date_time_get_month (self->date),
                                day,
                                0, 0, 0.0);
  g_return_if_fail (date != NULL);

  calendar_select_day_internal (self, date, TRUE);
  g_date_time_unref (date);

  g_object_notify (G_OBJECT (self), "day");
}

/**
 * gtk_calendar_get_day:
 * @self: a `GtkCalendar`
 *
 * Gets the day of the selected date.
 *
 * Returns: the day of the selected date.
 *
 * Since: 4.14
 */
int
gtk_calendar_get_day (GtkCalendar *self)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (self), -1);

  return g_date_time_get_day_of_month (self->date);
}

/**
 * gtk_calendar_set_month:
 * @self: a `GtkCalendar`
 * @month: The desired month for the selected date (as a number between 0 and 11).
 *
 * Sets the month for the selected date.
 *
 * The new date must be valid. For example, setting 1 (February) for the month
 * when the day is 31, fails.
 *
 * Since: 4.14
 */
void
gtk_calendar_set_month (GtkCalendar *self,
                        int          month)
{
  GDateTime *date;

  g_return_if_fail (GTK_IS_CALENDAR (self));
  g_return_if_fail (month >= 0 && month <= 11);

  if (month == g_date_time_get_month (self->date) - 1)
    return;

  date = g_date_time_new_local (g_date_time_get_year (self->date),
                                month + 1,
                                g_date_time_get_day_of_month (self->date),
                                0, 0, 0.0);
  g_return_if_fail (date != NULL);

  calendar_select_day_internal (self, date, TRUE);
  g_date_time_unref (date);

  g_object_notify (G_OBJECT (self), "month");
}

/**
 * gtk_calendar_get_month:
 * @self: a `GtkCalendar`
 *
 * Gets the month of the selected date.
 *
 * Returns: The month of the selected date (as a number between 0 and 11).
 *
 * Since: 4.14
 */
int
gtk_calendar_get_month (GtkCalendar *self)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (self), -1);

  return g_date_time_get_month (self->date) - 1;
}

/**
 * gtk_calendar_set_year:
 * @self: a `GtkCalendar`
 * @year: The desired year for the selected date (within [struct@GLib.DateTime]
 *   limits, i.e. from 0001 to 9999).
 *
 * Sets the year for the selected date.
 *
 * The new date must be valid. For example, setting 2023 for the year when then
 * the date is 2024-02-29, fails.
 *
 * Since: 4.14
 */
void
gtk_calendar_set_year (GtkCalendar *self,
                       int          year)
{
  GDateTime *date;

  g_return_if_fail (GTK_IS_CALENDAR (self));
  g_return_if_fail (year >= 1 && year <= 9999);

  if (year == g_date_time_get_year (self->date))
    return;

  date = g_date_time_new_local (year,
                                g_date_time_get_month (self->date),
                                g_date_time_get_day_of_month (self->date),
                                0, 0, 0.0);
  g_return_if_fail (date != NULL);

  calendar_select_day_internal (self, date, TRUE);
  g_date_time_unref (date);

  g_object_notify (G_OBJECT (self), "year");
}

/**
 * gtk_calendar_get_year:
 * @self: a `GtkCalendar`
 *
 * Gets the year of the selected date.
 *
 * Returns: the year of the selected date.
 *
 * Since: 4.14
 */
int
gtk_calendar_get_year (GtkCalendar *self)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (self), -1);

  return g_date_time_get_year (self->date);
}

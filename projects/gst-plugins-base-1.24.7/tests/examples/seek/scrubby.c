/* GStreamer
 *
 * scrubby.c: sample application to change the playback speed dynamically
 *
 * Copyright (C) 2005 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (scrubby_debug);
#define GST_CAT_DEFAULT (scrubby_debug)

static GstElement *pipeline;
static gint64 position;
static gint64 duration;
static GtkAdjustment *adjustment;
static GtkWidget *hscale;
static GtkAdjustment *sadjustment;
static GtkWidget *shscale;
static gboolean verbose = FALSE;

static guint bus_watch = 0;
static guint update_id = 0;
static guint changed_id = 0;
static guint schanged_id = 0;

#define SOURCE "filesrc"
#define ASINK   "autoaudiosink"
//#define ASINK "alsasink"
//#define ASINK "osssink"
#define VSINK   "autovideosink"
//#define VSINK "xvimagesink"
//#define VSINK "ximagesink"
//#define VSINK "aasink"
//#define VSINK "cacasink"

#define RANGE_PREC 10000
#define SEGMENT_LEN 100
#define UPDATE_INTERVAL 500

static gdouble prev_range = -1.0;
static GstClockTime prev_time = GST_CLOCK_TIME_NONE;
static gdouble cur_range;
static GstClockTime cur_time;
static GstClockTimeDiff diff;
static gdouble cur_speed = 1.0;

typedef struct
{
  const gchar *padname;
  GstPad *target;
  GstElement *bin;
}
dyn_link;

static GstElement *
gst_element_factory_make_or_warn (const gchar * type, const gchar * name)
{
  GstElement *element = gst_element_factory_make (type, name);

  if (!element) {
    g_warning ("Failed to create element %s of type %s", name, type);
  }

  return element;
}

static GstElement *
make_wav_pipeline (const gchar * location)
{
  GstElement *pipeline;
  GstElement *src, *decoder, *audiosink;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  decoder = gst_element_factory_make_or_warn ("wavparse", "decoder");
  audiosink = gst_element_factory_make_or_warn (ASINK, "sink");

  g_object_set (G_OBJECT (src), "location", location, NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, decoder, audiosink, NULL);
  gst_element_link_many (src, decoder, audiosink, NULL);

  return pipeline;
}

static GstElement *
make_playerbin_pipeline (const gchar * location)
{
  GstElement *player;
  const gchar *uri = g_filename_to_uri (location, NULL, NULL);

  player = gst_element_factory_make ("playbin", "player");
  g_assert (player);

  g_object_set (G_OBJECT (player), "uri", uri, NULL);

  return player;
}

static gchar *
format_value (GtkScale * scale, gdouble value)
{
  gint64 real;
  gint64 seconds;
  gint64 subseconds;

  real = value * duration / RANGE_PREC;
  seconds = (gint64) real / GST_SECOND;
  subseconds = (gint64) real / (GST_SECOND / RANGE_PREC);

  return g_strdup_printf ("%02" G_GINT64_FORMAT ":%02" G_GINT64_FORMAT ":%02"
      G_GINT64_FORMAT, seconds / 60, seconds % 60, subseconds % 100);
}

static gboolean
update_scale (gpointer data)
{
  position = 0;
  duration = 0;

  gst_element_query_position (pipeline, GST_FORMAT_TIME, &position);
  gst_element_query_duration (pipeline, GST_FORMAT_TIME, &duration);

  if (position >= duration)
    duration = position;

  if (duration > 0) {
    gtk_adjustment_set_value (adjustment,
        position * (gdouble) RANGE_PREC / duration);
    gtk_widget_queue_draw (hscale);
  }

  return TRUE;
}

static void
speed_cb (GtkWidget * widget)
{
  GstEvent *s_event;
  gboolean res;

  GST_DEBUG ("speed change");
  cur_speed = gtk_range_get_value (GTK_RANGE (widget));

  if (cur_speed == 0.0)
    return;

  s_event = gst_event_new_seek (cur_speed,
      GST_FORMAT_TIME, 0, GST_SEEK_TYPE_NONE, -1, GST_SEEK_TYPE_NONE, -1);

  res = gst_element_send_event (pipeline, s_event);
  if (!res)
    g_print ("speed change failed\n");
}

static gboolean do_seek (GtkWidget * widget, gboolean flush, gboolean segment);

static void
seek_cb (GtkWidget * widget)
{
  if (changed_id) {
    GST_DEBUG ("seek because of slider move");

    if (do_seek (widget, TRUE, TRUE)) {
      g_signal_handler_disconnect (hscale, changed_id);
      changed_id = 0;
    }
  }
}

static gboolean
do_seek (GtkWidget * widget, gboolean flush, gboolean segment)
{
  gint64 start, stop;
  gboolean res = FALSE;
  GstEvent *s_event;
  gdouble rate;
  gboolean valid;
  gdouble new_range;

  if (segment)
    new_range = gtk_range_get_value (GTK_RANGE (widget));
  else {
    new_range = (gdouble) RANGE_PREC;
    cur_time = -1;
  }

  valid = prev_time != -1;

  GST_DEBUG ("flush %d, segment %d, valid %d", flush, segment, valid);

  if (new_range == cur_range)
    return FALSE;

  prev_time = cur_time;
  prev_range = cur_range;

  cur_range = new_range;

  cur_time = g_get_real_time () * GST_USECOND;

  if (!valid)
    return FALSE;

  GST_DEBUG ("cur:  %lf, %" GST_TIME_FORMAT, cur_range,
      GST_TIME_ARGS (cur_time));
  GST_DEBUG ("prev: %lf, %" GST_TIME_FORMAT, prev_range,
      GST_TIME_ARGS (prev_time));

  diff = cur_time - prev_time;

  GST_DEBUG ("diff: %" GST_STIME_FORMAT, GST_STIME_ARGS (diff));

  start = prev_range * duration / RANGE_PREC;
  /* play 50 milliseconds */
  stop = segment ? cur_range * duration / RANGE_PREC : duration;

  if (start == stop)
    return FALSE;

  if (segment)
    rate = (stop - start) / (gdouble) diff;
  else
    rate = cur_speed;

  if (start > stop) {
    gint64 tmp;

    tmp = start;
    start = stop;
    stop = tmp;
  }

  if (rate == 0.0)
    return TRUE;

  GST_DEBUG ("seek to %" GST_TIME_FORMAT " -- %" GST_TIME_FORMAT ", rate %lf"
      " on element %s",
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop), rate,
      GST_ELEMENT_NAME (pipeline));

  s_event = gst_event_new_seek (rate,
      GST_FORMAT_TIME,
      (flush ? GST_SEEK_FLAG_FLUSH : 0) |
      (segment ? GST_SEEK_FLAG_SEGMENT : 0),
      GST_SEEK_TYPE_SET, start, GST_SEEK_TYPE_SET, stop);

  res = gst_element_send_event (pipeline, s_event);
  if (!res)
    g_print ("seek failed\n");

  gst_element_get_state (pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

  return TRUE;
}

static gboolean
start_seek (GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
  if (update_id) {
    g_source_remove (update_id);
    update_id = 0;
  }

  if (changed_id == 0) {
    changed_id =
        g_signal_connect (hscale, "value_changed", G_CALLBACK (seek_cb),
        pipeline);
  }

  GST_DEBUG ("start seek");

  return FALSE;
}

static gboolean
stop_seek (GtkWidget * widget, gpointer user_data)
{
  update_id =
      g_timeout_add (UPDATE_INTERVAL, (GSourceFunc) update_scale, pipeline);

  GST_DEBUG ("stop seek");

  if (changed_id) {
    g_signal_handler_disconnect (hscale, changed_id);
    changed_id = 0;
  }

  do_seek (hscale, FALSE, FALSE);

  return FALSE;
}

static void
play_cb (GtkButton * button, gpointer data)
{
  GstState state;

  gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
  if (state != GST_STATE_PLAYING) {
    g_print ("PLAY pipeline\n");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    update_id =
        g_timeout_add (UPDATE_INTERVAL, (GSourceFunc) update_scale, pipeline);
  }
}

static void
pause_cb (GtkButton * button, gpointer data)
{
  GstState state;

  gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
  if (state != GST_STATE_PAUSED) {
    g_print ("PAUSE pipeline\n");
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
    g_source_remove (update_id);
  }
}

static void
stop_cb (GtkButton * button, gpointer data)
{
  GstState state;

  gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
  if (state != GST_STATE_READY) {
    g_print ("READY pipeline\n");
    gst_element_set_state (pipeline, GST_STATE_READY);
    /* position and speed return to their default values */
    gtk_adjustment_set_value (adjustment, 0.0);
    gtk_adjustment_set_value (sadjustment, 1.0);
    g_source_remove (update_id);
  }
}

static void
print_message (GstMessage * message)
{
  const GstStructure *s;

  s = gst_message_get_structure (message);
  g_print ("Got Message from element \"%s\"\n",
      GST_STR_NULL (GST_ELEMENT_NAME (GST_MESSAGE_SRC (message))));

  if (s) {
    gchar *sstr;

    sstr = gst_structure_to_string (s);
    g_print ("%s\n", sstr);
    g_free (sstr);
  }
}

static gboolean
bus_message (GstBus * bus, GstMessage * message, gpointer data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      g_print ("EOS\n");
      break;
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_WARNING:
      print_message (message);
      break;
    case GST_MESSAGE_SEGMENT_START:
      break;
    case GST_MESSAGE_SEGMENT_DONE:
      GST_DEBUG ("segment_done, doing next seek");
      if (!do_seek (hscale, FALSE, update_id == 0)) {
        if (changed_id == 0) {
          changed_id =
              g_signal_connect (hscale, "value_changed", G_CALLBACK (seek_cb),
              pipeline);
        }
      }
      break;
    default:
      break;
  }

  return TRUE;
}

typedef struct
{
  const gchar *name;
  GstElement *(*func) (const gchar * location);
}
Pipeline;

static Pipeline pipelines[] = {
  {"wav", make_wav_pipeline},
  {"playerbin", make_playerbin_pipeline},
  {NULL, NULL},
};

#define NUM_TYPES       ((sizeof (pipelines) / sizeof (Pipeline)) - 1)

static void
print_usage (int argc, char **argv)
{
  gint i;

  g_print ("usage: %s <type> <filename>\n", argv[0]);
  g_print ("   possible types:\n");

  for (i = 0; i < NUM_TYPES; i++) {
    g_print ("     %d = %s\n", i, pipelines[i].name);
  }
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *hbox, *vbox, *play_button, *pause_button, *stop_button;
  GstBus *bus;
  GOptionEntry options[] = {
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
        "Verbose properties", NULL},
    {NULL}
  };
  gint type;
  GOptionContext *ctx;
  GError *err = NULL;

  ctx = g_option_context_new ("seek");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    g_clear_error (&err);
    exit (1);
  }

  GST_DEBUG_CATEGORY_INIT (scrubby_debug, "scrubby", 0, "scrubby example");

  gtk_init (&argc, &argv);

  if (argc != 3) {
    print_usage (argc, argv);
    exit (-1);
  }

  type = atoi (argv[1]);

  if (type < 0 || type >= NUM_TYPES) {
    print_usage (argc, argv);
    exit (-1);
  }

  pipeline = pipelines[type].func (argv[2]);
  g_assert (pipeline);

  /* initialize gui elements ... */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  play_button = gtk_button_new_with_label ("play");
  pause_button = gtk_button_new_with_label ("pause");
  stop_button = gtk_button_new_with_label ("stop");

  adjustment =
      GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, (gdouble) RANGE_PREC, 0.1,
          1.0, 1.0));
  hscale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_scale_set_digits (GTK_SCALE (hscale), 2);

  sadjustment =
      GTK_ADJUSTMENT (gtk_adjustment_new (1.0, 0.0, 5.0, 0.1, 1.0, 0.0));
  shscale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, sadjustment);
  gtk_scale_set_digits (GTK_SCALE (shscale), 2);

  schanged_id =
      g_signal_connect (shscale, "value_changed", G_CALLBACK (speed_cb),
      pipeline);

  g_signal_connect (hscale, "button_press_event", G_CALLBACK (start_seek),
      pipeline);
  g_signal_connect (hscale, "button_release_event", G_CALLBACK (stop_seek),
      pipeline);
  g_signal_connect (hscale, "format_value", G_CALLBACK (format_value),
      pipeline);

  /* do the packing stuff ... */
  gtk_window_set_default_size (GTK_WINDOW (window), 96, 96);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_box_pack_start (GTK_BOX (hbox), play_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), pause_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), stop_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hscale, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), shscale, TRUE, TRUE, 2);

  /* connect things ... */
  g_signal_connect (G_OBJECT (play_button), "clicked", G_CALLBACK (play_cb),
      pipeline);
  g_signal_connect (G_OBJECT (pause_button), "clicked", G_CALLBACK (pause_cb),
      pipeline);
  g_signal_connect (G_OBJECT (stop_button), "clicked", G_CALLBACK (stop_cb),
      pipeline);
  g_signal_connect (G_OBJECT (window), "delete_event", gtk_main_quit, NULL);

  /* show the gui. */
  gtk_widget_show_all (window);

  if (verbose) {
    g_signal_connect (pipeline, "deep_notify",
        G_CALLBACK (gst_object_default_deep_notify), NULL);
  }
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  g_assert (bus);

  bus_watch = gst_bus_add_watch_full (bus,
      G_PRIORITY_HIGH, bus_message, pipeline, NULL);

  gtk_main ();

  g_print ("NULL pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (bus);

  g_print ("free pipeline\n");
  gst_object_unref (pipeline);

  return 0;
}

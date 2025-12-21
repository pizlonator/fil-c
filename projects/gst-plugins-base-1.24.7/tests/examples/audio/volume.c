/* GStreamer
 *
 * volume.c: sample application to change the volume of a pipeline
 *
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
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

#include <math.h>

#include <gst/gst.h>
#include <gtk/gtk.h>

/* global pointer for the scale widget */
static GtkWidget *elapsed;
static GtkWidget *scale;

#ifndef M_LN10
#define M_LN10 (log(10.0))
#endif

static void
value_changed_callback (GtkWidget * widget, GstElement * volume)
{
  gdouble value;
  gdouble level;

  value = gtk_range_get_value (GTK_RANGE (widget));
  level = exp (value / 20.0 * M_LN10);
  g_print ("Value: %f dB, level: %f\n", value, level);
  g_object_set (volume, "volume", level, NULL);
}

static void
setup_gui (GstElement * volume)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label, *hbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* elapsed widget */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new ("Elapsed: ");
  elapsed = gtk_label_new ("0.0");
  gtk_container_add (GTK_CONTAINER (hbox), label);
  gtk_container_add (GTK_CONTAINER (hbox), elapsed);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  /* volume */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new ("volume");
  gtk_container_add (GTK_CONTAINER (hbox), label);
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -90.0, 10.0,
      0.2);
  gtk_range_set_value (GTK_RANGE (scale), 0.0);
  gtk_widget_set_size_request (scale, 100, -1);
  gtk_container_add (GTK_CONTAINER (hbox), scale);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  g_signal_connect (scale, "value-changed",
      G_CALLBACK (value_changed_callback), volume);

  gtk_widget_show_all (GTK_WIDGET (window));
}

static gboolean
progress_update (gpointer data)
{
  GstElement *pipeline = (GstElement *) data;
  gint64 position;
  gchar *position_str;

  if (gst_element_query_position (pipeline, GST_FORMAT_TIME, &position))
    position_str = g_strdup_printf ("%.1f", (gfloat) position / GST_SECOND);
  else
    position_str = g_strdup_printf ("n/a");
  gtk_label_set_text (GTK_LABEL (elapsed), position_str);

  g_free (position_str);

  return TRUE;
}

static void
message_received (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  const GstStructure *s;

  s = gst_message_get_structure (message);
  g_print ("message from \"%s\" (%s): ",
      GST_STR_NULL (GST_ELEMENT_NAME (GST_MESSAGE_SRC (message))),
      gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
  if (s) {
    gchar *sstr;

    sstr = gst_structure_to_string (s);
    g_print ("%s\n", sstr);
    g_free (sstr);
  } else {
    g_print ("no message details\n");
  }
}

static void
eos_message_received (GstBus * bus, GstMessage * message,
    GstPipeline * pipeline)
{
  message_received (bus, message, pipeline);
  gtk_main_quit ();
}

int
main (int argc, char *argv[])
{

  GstElement *pipeline = NULL;

#ifndef GST_DISABLE_PARSE
  GError *error = NULL;
#endif
  GstElement *volume;
  GstBus *bus;

#ifdef GST_DISABLE_PARSE
  g_print ("GStreamer was built without pipeline parsing capabilities.\n");
  g_print
      ("Please rebuild GStreamer with pipeline parsing capabilities activated to use this example.\n");
  return 1;
#else
  gst_init (&argc, &argv);
  gtk_init (&argc, &argv);

  pipeline = gst_parse_launchv ((const gchar **) &argv[1], &error);
  if (error) {
    g_print ("pipeline could not be constructed: %s\n", error->message);
    g_print ("Please give a complete pipeline  with a 'volume' element.\n");
    g_print ("Example: audiotestsrc ! volume ! %s\n", DEFAULT_AUDIOSINK);
    g_error_free (error);
    return 1;
  }
#endif
  volume = gst_bin_get_by_name (GST_BIN (pipeline), "volume0");
  if (volume == NULL) {
    g_print ("Please give a pipeline with a 'volume' element in it\n");
    return 1;
  }

  /* setup message handling */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  g_signal_connect (bus, "message::error", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::warning", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::eos", (GCallback) eos_message_received,
      pipeline);

  /* setup GUI */
  setup_gui (volume);

  /* go to main loop */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_timeout_add (100, progress_update, pipeline);
  gtk_main ();
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (volume);
  gst_object_unref (pipeline);
  gst_object_unref (bus);

  return 0;
}

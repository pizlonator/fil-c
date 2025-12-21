/* GStreamer
 *
 * audiomix.c: sample audio mixing application
 *
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
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

#include <string.h>
#include <gst/gst.h>
#include <gtk/gtk.h>

/* global items for the interaction */
static GtkWidget *scale;
static GObject *volumes[2];
static gint num_vol = 0;


static void
value_changed_callback (GtkWidget * widget, gpointer * user_data)
{
  gdouble value = gtk_range_get_value (GTK_RANGE (widget));
  g_object_set (volumes[0], "volume", 1.0 - value, NULL);
  g_object_set (volumes[1], "volume", value, NULL);
}

static void
setup_gui (GstElement * volume, gchar * file_name1, gchar * file_name2)
{
  GtkWidget *window, *layout, *label;
  gchar *name, *ext;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "audiomix");
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);

  layout = gtk_grid_new ();
  g_object_set (G_OBJECT (layout), "column-spacing", 6, NULL);
  gtk_container_add (GTK_CONTAINER (window), layout);

  /* channel labels */
  name = g_path_get_basename (file_name1);
  if ((ext = strrchr (name, '.')))
    *ext = '\0';
  label = gtk_label_new (name);
  g_free (name);
  gtk_grid_attach (GTK_GRID (layout), label, 0, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (layout), gtk_label_new ("|"), 1, 0, 1, 1);

  name = g_path_get_basename (file_name2);
  if ((ext = strrchr (name, '.')))
    *ext = '\0';
  label = gtk_label_new (name);
  g_free (name);
  gtk_grid_attach (GTK_GRID (layout), label, 2, 0, 1, 1);

  /* mix slider */
  scale =
      gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0,
      1.0 / 200.0);
  gtk_range_set_value (GTK_RANGE (scale), 0.0);
  gtk_widget_set_size_request (scale, 200, -1);
  gtk_widget_set_hexpand (scale, TRUE);
  gtk_grid_attach (GTK_GRID (layout), scale, 0, 1, 3, 1);
  g_signal_connect (scale, "value-changed",
      G_CALLBACK (value_changed_callback), volume);

  gtk_widget_show_all (GTK_WIDGET (window));
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

static void
dynamic_link (GstPadTemplate * templ, GstPad * newpad, gpointer user_data)
{
  GstPad *target = GST_PAD (user_data);

  gst_pad_link (newpad, target);
  gst_object_unref (target);
}

static void
make_mixer_channel (GstElement * pipeline, GstElement * mix, gchar * file_name)
{
  GstElement *filesrc, *decodebin, *volume, *convert, *format;
  GstCaps *caps;

  /* prepare mixer channel */
  filesrc = gst_element_factory_make ("filesrc", NULL);
  decodebin = gst_element_factory_make ("decodebin", NULL);
  volume = gst_element_factory_make ("volume", NULL);
  convert = gst_element_factory_make ("audioconvert", NULL);
  format = gst_element_factory_make ("capsfilter", NULL);
  gst_bin_add_many (GST_BIN (pipeline), filesrc, decodebin, volume, convert,
      format, NULL);
  gst_element_link (filesrc, decodebin);
  gst_element_link_many (volume, convert, format, mix, NULL);

  /* configure elements */
  g_object_set (filesrc, "location", file_name, NULL);
  g_object_set (volume, "volume", (num_vol == 0) ? 1.0 : 0.0, NULL);

  caps = gst_caps_from_string ("audio/x-raw, "
      "format = (string) S16LE, " "channels = (int) 2");
  g_object_set (format, "caps", caps, NULL);
  gst_caps_unref (caps);

  /* remember volume element */
  volumes[num_vol++] = (GObject *) volume;

  /* handle dynamic pads */
  g_signal_connect (G_OBJECT (decodebin), "pad-added",
      G_CALLBACK (dynamic_link), gst_element_get_static_pad (volume, "sink"));
}

int
main (int argc, char *argv[])
{
  GstElement *pipeline = NULL;
  GstElement *mix, *convert, *sink;
  GstBus *bus;

  if (argc < 3) {
    g_print ("Usage: audiomix <file1> <file2>\n");
    return 1;
  }

  gst_init (&argc, &argv);
  gtk_init (&argc, &argv);

  /* prepare tail of pipeline */
  pipeline = gst_pipeline_new ("audiomix");
  mix = gst_element_factory_make ("adder", NULL);
  convert = gst_element_factory_make ("audioconvert", NULL);
  sink = gst_element_factory_make ("autoaudiosink", NULL);
  gst_bin_add_many (GST_BIN (pipeline), mix, convert, sink, NULL);
  gst_element_link_many (mix, convert, sink, NULL);

  /* prepare mixer channel strips */
  make_mixer_channel (pipeline, mix, argv[1]);
  make_mixer_channel (pipeline, mix, argv[2]);

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
  setup_gui (pipeline, argv[1], argv[2]);

  /* go to main loop */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  gtk_main ();
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  gst_object_unref (bus);

  return 0;
}

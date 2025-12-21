/* GStreamer
 *
 * giosrc-mounting: example application that shows how to handle the
 * "not-mounted" message
 *
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <gst/gst.h>
#include <gtk/gtk.h>

#include <string.h>

static GstElement *pipeline = NULL;

static void
mount_cb (GObject * obj, GAsyncResult * res, gpointer user_data)
{
  gboolean ret;
  GError *err = NULL;

  ret = g_file_mount_enclosing_volume_finish (G_FILE (obj), res, &err);

  if (ret) {
    g_print ("mounted successfully\n");
    gst_bus_set_flushing ((GstBus *) user_data, FALSE);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);
  } else {
    g_print ("mounting failed: %s\n", err->message);
    g_clear_error (&err);
    gtk_main_quit ();
  }
}

static gboolean
message_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ELEMENT:{
      const GstStructure *s = gst_message_get_structure (message);
      const gchar *name = gst_structure_get_name (s);

      if (strcmp (name, "not-mounted") == 0) {
        GMountOperation *mop = gtk_mount_operation_new (NULL);
        GFile *file =
            G_FILE (g_value_get_object (gst_structure_get_value (s, "file")));

        g_print ("not-mounted\n");
        gst_element_set_state (pipeline, GST_STATE_NULL);
        gst_bus_set_flushing (bus, TRUE);

        g_file_mount_enclosing_volume (file, G_MOUNT_MOUNT_NONE,
            mop, NULL, mount_cb, bus);

        g_object_unref (mop);
      }
      break;
    }

    case GST_MESSAGE_EOS:
      g_print ("EOS\n");
      gtk_main_quit ();
      break;
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;

      gst_message_parse_error (message, &err, NULL);
      g_print ("error: %s\n", err->message);
      g_clear_error (&err);

      gtk_main_quit ();
      break;
    }
    default:
      break;
  }

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GstBus *bus;
  gint watch_id;

  if (argc != 2) {
    g_print ("usage: giosrc-mounting URI\n");
    return -1;
  }

  gst_init (NULL, NULL);
  gtk_init (NULL, NULL);

  pipeline = gst_element_factory_make ("playbin", NULL);
  g_assert (pipeline);
  g_object_set (G_OBJECT (pipeline), "uri", argv[1], NULL);

  bus = gst_element_get_bus (pipeline);
  watch_id = gst_bus_add_watch (bus, message_handler, NULL);
  gst_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  gtk_main ();

  g_source_remove (watch_id);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  return 0;
}

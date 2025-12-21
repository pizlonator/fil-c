/*
 * position-formats.c
 *
 * we mostly use GST_FORMAT_TIME in queries and seeks. Test the other ones to
 * know what works and what not.
 */

#include <gst/gst.h>

#include <stdio.h>

static gboolean
bus_message (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GMainLoop *loop = (GMainLoop *) user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    {
      GError *gerror;
      gchar *debug;

      gst_message_parse_error (message, &gerror, &debug);
      gst_object_default_error (GST_MESSAGE_SRC (message), gerror, debug);
      g_error_free (gerror);
      g_free (debug);

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_WARNING:
    {
      GError *gerror;
      gchar *debug;

      gst_message_parse_warning (message, &gerror, &debug);
      gst_object_default_error (GST_MESSAGE_SRC (message), gerror, debug);
      g_error_free (gerror);
      g_free (debug);

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }
  return TRUE;
}

static void
print_value (gboolean res, GstFormat fmt, gint64 val)
{
  if (res) {
    switch (fmt) {
      case GST_FORMAT_TIME:
        printf ("%" GST_TIME_FORMAT, GST_TIME_ARGS (val));
        break;
      case GST_FORMAT_PERCENT:
        printf ("%8.4lf%%", (gdouble) val / GST_FORMAT_PERCENT_SCALE);
        break;
      case GST_FORMAT_DEFAULT:
      case GST_FORMAT_BYTES:
      case GST_FORMAT_BUFFERS:
      default:
        printf ("%" G_GINT64_FORMAT, val);
        break;
    }
  } else {
    printf ("-");
  }
}

static gboolean
run_queries (gpointer user_data)
{
  GstElement *bin = (GstElement *) user_data;
  GstFormat i, fmt;
  gint64 pos, dur;
  gboolean pres, dres;

  for (i = GST_FORMAT_DEFAULT; i <= GST_FORMAT_PERCENT; i++) {
    fmt = i;
    pres = gst_element_query_position (bin, fmt, &pos);
    fmt = i;
    dres = gst_element_query_duration (bin, fmt, &dur);
    printf ("%-8s : ", gst_format_get_name (i));
    print_value (pres, fmt, pos);
    printf (" / ");
    print_value (dres, fmt, dur);
    printf ("\n");
  }
  printf ("\n");

  return TRUE;
}

gint
main (gint argc, gchar ** argv)
{
  gint res = 1;
  GstElement *bin;
  GstBus *bus;
  GMainLoop *loop;
  const gchar *uri;

  gst_init (&argc, &argv);

  if (argc < 2) {
    printf ("Usage: %s <uri>\n", argv[0]);
    goto Error;
  }
  uri = argv[1];

  /* build pipeline */
  bin = gst_element_factory_make ("playbin", NULL);
  if (!bin) {
    GST_WARNING ("need playbin from gst-plugins-base");
    goto Error;
  }

  g_object_set (bin, "uri", uri, NULL);

  loop = g_main_loop_new (NULL, TRUE);

  /* add watch for messages */
  bus = gst_pipeline_get_bus (GST_PIPELINE (bin));
  gst_bus_add_watch (bus, (GstBusFunc) bus_message, (gpointer) loop);
  gst_object_unref (bus);

  /* add timeout for queries */
  g_timeout_add_seconds (1, (GSourceFunc) run_queries, (gpointer) bin);

  /* run the show */
  if (gst_element_set_state (bin,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE) {
    g_main_loop_run (loop);
    gst_element_set_state (bin, GST_STATE_NULL);
  }

  /* cleanup */
  g_main_loop_unref (loop);
  gst_object_unref (G_OBJECT (bin));
  res = 0;
Error:
  return (res);
}

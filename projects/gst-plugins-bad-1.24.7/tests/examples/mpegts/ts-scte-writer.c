#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

/* 45s stream
 * Send scte-35 NULL packets every 5s
 * Use PID 123 for SCTE-35 */
#define PIPELINE_STR "videotestsrc is-live=True num-buffers=1350 ! video/x-raw,framerate=30/1 ! x264enc tune=zerolatency ! queue ! mpegtsmux name=mux scte-35-pid=123 scte-35-null-interval=450000 ! filesink location=test-scte.ts"

static void
_on_bus_message (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_EOS:
      g_main_loop_quit (mainloop);
      break;
    default:
      break;
  }
}

static void
send_splice (GstElement * mux, gboolean out)
{
  GstMpegtsSCTESIT *sit;
  GstMpegtsSection *section;

  g_print ("Sending Splice %s event\n", out ? "Out" : "In");

  /* Splice is at 5s for 30s */
  if (out)
    sit = gst_mpegts_scte_splice_out_new (1, 5 * GST_SECOND, 30 * GST_SECOND);
  else
    sit = gst_mpegts_scte_splice_in_new (2, 35 * GST_SECOND);

  section = gst_mpegts_section_from_scte_sit (sit, 123);
  gst_mpegts_section_send_event (section, mux);
  gst_mpegts_section_unref (section);
}

static gboolean
send_splice_in (GstElement * mux)
{
  send_splice (mux, FALSE);

  return G_SOURCE_REMOVE;
}

static gboolean
send_splice_out (GstElement * mux)
{
  send_splice (mux, TRUE);

  /* In 30s send the splice-in one */
  g_timeout_add_seconds (30, (GSourceFunc) send_splice_in, mux);

  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;
  GstBus *bus;
  GMainLoop *mainloop;
  GstElement *mux;

  gst_init (&argc, &argv);
  gst_mpegts_initialize ();

  pipeline = gst_parse_launch (PIPELINE_STR, &error);
  if (error) {
    g_print ("pipeline could not be constructed: %s\n", error->message);
    g_clear_error (&error);
    return 1;
  }

  mainloop = g_main_loop_new (NULL, FALSE);

  mux = gst_bin_get_by_name (GST_BIN (pipeline), "mux");
  /* Send splice-out 1s in */
  g_timeout_add_seconds (1, (GSourceFunc) send_splice_out, mux);
  gst_object_unref (mux);

  /* Put a bus handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", (GCallback) _on_bus_message, mainloop);

  /* Start pipeline */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (mainloop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);
  gst_object_unref (bus);

  return 0;
}

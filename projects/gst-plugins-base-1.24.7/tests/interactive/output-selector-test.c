#include <gst/gst.h>

#define SWITCH_TIMEOUT 1
#define NUM_VIDEO_BUFFERS 500

static GMainLoop *loop;

/* Output selector src pads */
static GstPad *osel_src1 = NULL;
static GstPad *osel_src2 = NULL;

static gboolean
my_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_main_loop_quit (loop);
      break;
    default:
      /* unhandled message */
      break;
  }
  /* we want to be notified again the next time there is a message
   * on the bus, so returning TRUE (FALSE means we want to stop watching
   * for messages on the bus and our callback should not be called again)
   */
  return TRUE;
}

static gboolean
switch_cb (gpointer user_data)
{
  GstElement *sel = GST_ELEMENT (user_data);
  GstPad *old_pad, *new_pad = NULL;

  g_object_get (G_OBJECT (sel), "active-pad", &old_pad, NULL);

  if (old_pad == osel_src1)
    new_pad = osel_src2;
  else
    new_pad = osel_src1;

  g_object_set (G_OBJECT (sel), "active-pad", new_pad, NULL);

  g_print ("switched from %s:%s to %s:%s\n", GST_DEBUG_PAD_NAME (old_pad),
      GST_DEBUG_PAD_NAME (new_pad));

  gst_object_unref (old_pad);

  return TRUE;

}

static void
on_bin_element_added (GstBin * bin, GstElement * element, gpointer user_data)
{
  g_object_set (G_OBJECT (element), "sync", FALSE, "async", FALSE, NULL);
}

gint
main (gint argc, gchar * argv[])
{
  GstElement *pipeline, *src, *toverlay, *osel, *sink1, *sink2, *c1, *c2, *c0;
  GstPad *sinkpad;
  GstBus *bus;

  /* init GStreamer */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* create elements */
  pipeline = gst_element_factory_make ("pipeline", "pipeline");
  src = gst_element_factory_make ("videotestsrc", "src");
  c0 = gst_element_factory_make ("videoconvert", NULL);
  toverlay = gst_element_factory_make ("timeoverlay", "timeoverlay");
  osel = gst_element_factory_make ("output-selector", "osel");
  c1 = gst_element_factory_make ("videoconvert", NULL);
  c2 = gst_element_factory_make ("videoconvert", NULL);
  sink1 = gst_element_factory_make ("autovideosink", "sink1");
  sink2 = gst_element_factory_make ("autovideosink", "sink2");

  if (!pipeline || !src || !c0 || !toverlay || !osel || !c1 || !c2 || !sink1 ||
      !sink2) {
    g_print ("missing element\n");
    return -1;
  }

  /* add them to bin */
  gst_bin_add_many (GST_BIN (pipeline), src, c0, toverlay, osel, c1, sink1, c2,
      sink2, NULL);

  /* set properties */
  g_object_set (G_OBJECT (src), "is-live", TRUE, NULL);
  g_object_set (G_OBJECT (src), "do-timestamp", TRUE, NULL);
  g_object_set (G_OBJECT (src), "num-buffers", NUM_VIDEO_BUFFERS, NULL);
  g_object_set (G_OBJECT (osel), "resend-latest", TRUE, NULL);

  /* handle deferred properties */
  g_signal_connect (G_OBJECT (sink1), "element-added",
      G_CALLBACK (on_bin_element_added), NULL);
  g_signal_connect (G_OBJECT (sink2), "element-added",
      G_CALLBACK (on_bin_element_added), NULL);

  /* link src ! timeoverlay ! osel */
  if (!gst_element_link_many (src, c0, toverlay, osel, NULL)) {
    g_print ("linking failed\n");
    return -1;
  }

  /* link output 1 */
  sinkpad = gst_element_get_static_pad (c1, "sink");
  osel_src1 = gst_element_request_pad_simple (osel, "src_%u");
  if (gst_pad_link (osel_src1, sinkpad) != GST_PAD_LINK_OK) {
    g_print ("linking output 1 converter failed\n");
    return -1;
  }
  gst_object_unref (sinkpad);

  if (!gst_element_link (c1, sink1)) {
    g_print ("linking output 1 failed\n");
    return -1;
  }

  /* link output 2 */
  sinkpad = gst_element_get_static_pad (c2, "sink");
  osel_src2 = gst_element_request_pad_simple (osel, "src_%u");
  if (gst_pad_link (osel_src2, sinkpad) != GST_PAD_LINK_OK) {
    g_print ("linking output 2 converter failed\n");
    return -1;
  }
  gst_object_unref (sinkpad);

  if (!gst_element_link (c2, sink2)) {
    g_print ("linking output 2 failed\n");
    return -1;
  }

  /* add switch callback */
  g_timeout_add_seconds (SWITCH_TIMEOUT, switch_cb, osel);

  /* change to playing */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, my_bus_callback, loop);
  gst_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* now run */
  g_main_loop_run (loop);

  /* also clean up */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_element_release_request_pad (osel, osel_src1);
  gst_element_release_request_pad (osel, osel_src2);
  gst_object_unref (GST_OBJECT (pipeline));

  return 0;
}

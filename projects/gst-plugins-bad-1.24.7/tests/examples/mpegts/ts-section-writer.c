#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

#define PIPELINE_STR "videotestsrc num-buffers=100 ! x264enc ! queue ! mpegtsmux name=mux ! fakesink"

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
advertise_service (GstElement * mux)
{
  GstMpegtsSDTService *service;
  GstMpegtsSDT *sdt;
  GstMpegtsDescriptor *desc;
  GstMpegtsSection *section;

  sdt = gst_mpegts_sdt_new ();

  sdt->actual_ts = TRUE;
  sdt->transport_stream_id = 42;

  service = gst_mpegts_sdt_service_new ();
  service->service_id = 42;
  service->running_status =
      GST_MPEGTS_RUNNING_STATUS_RUNNING + service->service_id;
  service->EIT_schedule_flag = FALSE;
  service->EIT_present_following_flag = FALSE;
  service->free_CA_mode = FALSE;

  desc = gst_mpegts_descriptor_from_dvb_service
      (GST_DVB_SERVICE_DIGITAL_TELEVISION, "some-service", NULL);

  g_ptr_array_add (service->descriptors, desc);
  g_ptr_array_add (sdt->services, service);

  section = gst_mpegts_section_from_sdt (sdt);
  gst_mpegts_section_send_event (section, mux);
  gst_mpegts_section_unref (section);
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

  pipeline = gst_parse_launch (PIPELINE_STR, &error);
  if (error) {
    g_print ("pipeline could not be constructed: %s\n", error->message);
    g_clear_error (&error);
    return 1;
  }

  mainloop = g_main_loop_new (NULL, FALSE);

  mux = gst_bin_get_by_name (GST_BIN (pipeline), "mux");
  advertise_service (mux);
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

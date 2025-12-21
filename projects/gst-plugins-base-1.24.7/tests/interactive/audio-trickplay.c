/*
 * audio-trickplay.c
 *
 * Builds a pipeline with two audiotestsources mixed with adder. Assigns
 * controller patterns to the audio generators and test various trick modes.
 *
 * There are currently several issues:
 * - adder only work with flushing seeks
 * - there is a gap of almost 4 seconds before backwards playback
 *   - it is "waiting for free space"
 *   - using sync=false on the sink does not help (but has some other weird effects)
 *   - using fakesink shows same behaviour
 *
 * GST_DEBUG_NO_COLOR=1 GST_DEBUG="*:2,default:3,*sink*:4,*ring*:4,*pulse*:5" ./audio-trickplay 2>log.txt
 * GST_DEBUG_NO_COLOR=1 GST_DEBUG="*:2,default:3,*sink*:4,*ring*:4,*pulse*:5" ./audio-trickplay -a -f 2>log-af.txt
 */

#include <string.h>
#include <gst/gst.h>
#include <gst/controller/gstinterpolationcontrolsource.h>
#include <gst/controller/gstdirectcontrolbinding.h>

static void
check_position (GstElement * elem, GstQuery * pos, const gchar * info)
{
  if (gst_element_query (elem, pos)) {
    gint64 play_pos;
    gst_query_parse_position (pos, NULL, &play_pos);
    GST_INFO ("pos : %" GST_TIME_FORMAT " %s", GST_TIME_ARGS (play_pos), info);
  } else {
    GST_WARNING ("position query failed");
  }
}

static GstPadProbeReturn
print_buffer_ts (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);

  GST_DEBUG_OBJECT (pad, "  ts: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  return GST_PAD_PROBE_OK;
}

gint
main (gint argc, gchar ** argv)
{
  gint res = 1;
  GstElement *src, *mix = NULL, *sink;
  GstElement *bin;
  GstControlSource *cs1, *cs2;
  GstTimedValueControlSource *tvcs;
  GstClock *clock;
  GstClockID clock_id;
  GstClockReturn wait_ret;
  GstEvent *pos_seek, *rate_seek1, *rate_seek2;
  GstQuery *pos;
  GstSeekFlags flags;
  GstPad *src_pad;
  /* options */
  gboolean use_adder = FALSE;
  gboolean use_flush = FALSE;
  gboolean be_quiet = FALSE;

  gst_init (&argc, &argv);

  if (argc) {
    gint arg;
    for (arg = 0; arg < argc; arg++) {
      if (!strcmp (argv[arg], "-a"))
        use_adder = TRUE;
      else if (!strcmp (argv[arg], "-f"))
        use_flush = TRUE;
      else if (!strcmp (argv[arg], "-q"))
        be_quiet = TRUE;
    }
  }

  /* build pipeline */
  bin = gst_pipeline_new ("pipeline");
  clock = gst_pipeline_get_clock (GST_PIPELINE (bin));
  src = gst_element_factory_make ("audiotestsrc", NULL);
  if (!src) {
    GST_WARNING ("need audiotestsrc from gst-plugins-base");
    goto Error;
  }
  if (use_adder) {
    mix = gst_element_factory_make ("adder", NULL);
    if (!mix) {
      GST_WARNING ("need adder from gst-plugins-base");
      goto Error;
    }
  }
  sink = gst_element_factory_make ((be_quiet ? "fakesink" : "autoaudiosink"),
      NULL);
  if (!sink) {
    GST_WARNING ("need autoaudiosink from gst-plugins-base");
    goto Error;
  }

  if (use_adder) {
    gst_bin_add_many (GST_BIN (bin), src, mix, sink, NULL);
    if (!gst_element_link_many (src, mix, sink, NULL)) {
      GST_WARNING ("can't link elements");
      goto Error;
    }
  } else {
    gst_bin_add_many (GST_BIN (bin), src, sink, NULL);
    if (!gst_element_link_many (src, sink, NULL)) {
      GST_WARNING ("can't link elements");
      goto Error;
    }
  }

  /* use 10 buffers per second */
  g_object_set (src, "samplesperbuffer", 44100 / 10, NULL);

  if (be_quiet) {
    g_object_set (sink, "sync", TRUE, NULL);
  }

  src_pad = gst_element_get_static_pad (src, "src");
  gst_pad_add_probe (src_pad, GST_PAD_PROBE_TYPE_BUFFER, print_buffer_ts, NULL,
      NULL);
  gst_object_unref (src_pad);

  cs1 = gst_interpolation_control_source_new ();
  cs2 = gst_interpolation_control_source_new ();

  gst_object_add_control_binding (GST_OBJECT_CAST (src),
      gst_direct_control_binding_new (GST_OBJECT_CAST (src), "volume", cs1));
  gst_object_add_control_binding (GST_OBJECT_CAST (src),
      gst_direct_control_binding_new (GST_OBJECT_CAST (src), "freq", cs2));

  /* Set interpolation mode */

  g_object_set (cs1, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);
  g_object_set (cs2, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control values */
  tvcs = (GstTimedValueControlSource *) cs1;
  gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0);
  gst_timed_value_control_source_set (tvcs, 5 * GST_SECOND, 1.0);

  gst_object_unref (cs1);

  tvcs = (GstTimedValueControlSource *) cs2;
  gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 20000.0 / 220.0);
  gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 20000.0 / 3520.0);
  gst_timed_value_control_source_set (tvcs, 6 * GST_SECOND, 20000.0 / 440.0);

  gst_object_unref (cs2);

  /* prepare events */
  flags = use_flush ? GST_SEEK_FLAG_FLUSH : GST_SEEK_FLAG_NONE;
  pos_seek = gst_event_new_seek (1.0, GST_FORMAT_TIME, flags,
      GST_SEEK_TYPE_SET, 3 * GST_SECOND,
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
  rate_seek1 = gst_event_new_seek (0.5, GST_FORMAT_TIME, flags,
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
  rate_seek2 = gst_event_new_seek (-1.0, GST_FORMAT_TIME, flags,
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  /* prepare queries */
  pos = gst_query_new_position (GST_FORMAT_TIME);


  /* run the show */
  if (gst_element_set_state (bin, GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE) {

    /* run for 5 seconds */
    clock_id =
        gst_clock_new_single_shot_id (clock,
        gst_clock_get_time (clock) + (5 * GST_SECOND));

    if (gst_element_set_state (bin,
            GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE) {
      check_position (bin, pos, "start");
      if ((wait_ret = gst_clock_id_wait (clock_id, NULL)) != GST_CLOCK_OK) {
        GST_WARNING ("clock_id_wait returned: %d", wait_ret);
      }
    }
    gst_clock_id_unref (clock_id);

    check_position (bin, pos, "before seek to new pos");

    /* seek to 3:00 sec (back 2 sec) */
    if (!gst_element_send_event (sink, pos_seek)) {
      GST_WARNING ("element failed to seek to new position");
    }

    check_position (bin, pos, "after seek to new pos");

    /* run for 2 seconds */
    clock_id =
        gst_clock_new_single_shot_id (clock,
        gst_clock_get_time (clock) + (2 * GST_SECOND));
    if ((wait_ret = gst_clock_id_wait (clock_id, NULL)) != GST_CLOCK_OK) {
      GST_WARNING ("clock_id_wait returned: %d", wait_ret);
    }
    gst_clock_id_unref (clock_id);

    check_position (bin, pos, "before slow down rate change");

    /* change playback rate to 0.5 */
    if (!gst_element_send_event (sink, rate_seek1)) {
      GST_WARNING ("element failed to change playback rate");
    }

    check_position (bin, pos, "after slow down rate change");

    /* run for 4 seconds */
    clock_id =
        gst_clock_new_single_shot_id (clock,
        gst_clock_get_time (clock) + (4 * GST_SECOND));
    if ((wait_ret = gst_clock_id_wait (clock_id, NULL)) != GST_CLOCK_OK) {
      GST_WARNING ("clock_id_wait returned: %d", wait_ret);
    }
    gst_clock_id_unref (clock_id);

    check_position (bin, pos, "before reverse rate change");

    /* change playback rate to -1.0  */
    if (!gst_element_send_event (sink, rate_seek2)) {
      GST_WARNING ("element failed to change playback rate");
    }

    check_position (bin, pos, "after reverse rate change");

    /* run for 7 seconds */
    clock_id =
        gst_clock_new_single_shot_id (clock,
        gst_clock_get_time (clock) + (7 * GST_SECOND));
    if ((wait_ret = gst_clock_id_wait (clock_id, NULL)) != GST_CLOCK_OK) {
      GST_WARNING ("clock_id_wait returned: %d", wait_ret);
    }
    gst_clock_id_unref (clock_id);

    check_position (bin, pos, "done");

    gst_element_set_state (bin, GST_STATE_NULL);
  }

  /* cleanup */
  gst_query_unref (pos);
  gst_object_unref (clock);
  gst_object_unref (bin);
  res = 0;
Error:
  return (res);
}

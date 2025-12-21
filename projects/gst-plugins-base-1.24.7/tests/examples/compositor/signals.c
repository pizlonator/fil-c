#include <gst/gst.h>
#include <gst/base/gstaggregator.h>

#define MAKE_AND_ADD(var, pipe, name, label)                                   \
  G_STMT_START                                                                 \
  {                                                                            \
    GError* make_and_add_err = NULL;                                           \
    if (G_UNLIKELY(!(var = (gst_parse_bin_from_description_full(               \
                       name,                                                   \
                       TRUE,                                                   \
                       NULL,                                                   \
                       GST_PARSE_FLAG_NO_SINGLE_ELEMENT_BINS,                  \
                       &make_and_add_err))))) {                                \
      GST_ERROR(                                                               \
        "Could not create element %s (%s)", name, make_and_add_err->message);  \
      g_clear_error(&make_and_add_err);                                        \
      goto label;                                                              \
    }                                                                          \
    if (G_UNLIKELY(!gst_bin_add(GST_BIN_CAST(pipe), var))) {                   \
      GST_ERROR("Could not add element %s", name);                             \
      goto label;                                                              \
    }                                                                          \
  }                                                                            \
  G_STMT_END

static gboolean
check_aggregated_buffer (GstElement * agg, GstPad * pad,
    GHashTable * consumed_buffers)
{
  GstSample *sample;
  GList *pad_consumed_buffers;
  GList *tmp;

  sample =
      gst_aggregator_peek_next_sample (GST_AGGREGATOR (agg),
      GST_AGGREGATOR_PAD (pad));

  pad_consumed_buffers = g_hash_table_lookup (consumed_buffers, pad);

  for (tmp = pad_consumed_buffers; tmp; tmp = tmp->next) {
    GstBuffer *consumed_buffer = (GstBuffer *) tmp->data;
    gboolean aggregated = FALSE;

    if (sample) {
      aggregated =
          consumed_buffer == gst_sample_get_buffer (sample) ? TRUE : FALSE;
    }

    gst_printerr ("One consumed buffer: %" GST_PTR_FORMAT
        ", it was%s aggregated\n", consumed_buffer, aggregated ? "" : " not");
  }

  if (sample) {
    gst_sample_unref (sample);
  }

  g_list_free_full (pad_consumed_buffers, (GDestroyNotify) gst_buffer_unref);
  g_hash_table_steal (consumed_buffers, pad);

  return TRUE;
}

static void
samples_selected_cb (GstElement * agg, GstSegment * segment, GstClockTime pts,
    GstClockTime dts, GstClockTime duration, GstStructure * info,
    GHashTable * consumed_buffers)
{
  gst_printerr
      ("Compositor has selected the samples it will aggregate for output buffer with PTS %"
      GST_TIME_FORMAT " and duration %" GST_TIME_FORMAT "\n",
      GST_TIME_ARGS (pts), GST_TIME_ARGS (duration));
  gst_element_foreach_sink_pad (agg,
      (GstElementForeachPadFunc) check_aggregated_buffer, consumed_buffers);
}

static void
pad_buffer_consumed_cb (GstAggregatorPad * pad, GstBuffer * buffer,
    GHashTable * consumed_buffers)
{
  GList *pad_consumed_buffers;
  gboolean was_empty;

  pad_consumed_buffers = g_hash_table_lookup (consumed_buffers, pad);

  was_empty = (pad_consumed_buffers == NULL);

  pad_consumed_buffers =
      g_list_append (pad_consumed_buffers, gst_buffer_ref (buffer));

  /* we know the list's head pointer doesn't change when items get appended */
  if (was_empty)
    g_hash_table_insert (consumed_buffers, pad, pad_consumed_buffers);
}

static gboolean
unref_consumed_buffers (gpointer key, GList * pad_consumed_buffers)
{
  g_list_free_full (pad_consumed_buffers, (GDestroyNotify) gst_buffer_unref);

  return TRUE;
}

int
main (int ac, char **av)
{
  int ret = 0;
  GstElement *pipe;
  GstBus *bus;
  GstElement *vsrc, *vcfltr1, *compositor, *vcfltr2, *vsink;
  GstCaps *caps;
  GstPad *pad;
  GHashTable *consumed_buffers =
      g_hash_table_new (g_direct_hash, g_direct_equal);

  gst_init (NULL, NULL);

  pipe = gst_pipeline_new (NULL);

  MAKE_AND_ADD (vsrc, pipe, "videotestsrc", err);
  MAKE_AND_ADD (vcfltr1, pipe, "capsfilter", err);
  MAKE_AND_ADD (compositor, pipe, "compositor", err);
  MAKE_AND_ADD (vcfltr2, pipe, "capsfilter", err);
  MAKE_AND_ADD (vsink, pipe, "autovideosink", err);

  if (!gst_element_link_many (vsrc, vcfltr1, compositor, vcfltr2, vsink, NULL)) {
    GST_ERROR ("Failed to link pipeline");
    goto err;
  }

  caps =
      gst_caps_new_simple ("video/x-raw", "framerate", GST_TYPE_FRACTION, 30, 1,
      NULL);
  g_object_set (vcfltr1, "caps", caps, NULL);
  gst_caps_unref (caps);

  caps =
      gst_caps_new_simple ("video/x-raw", "framerate", GST_TYPE_FRACTION, 6, 1,
      NULL);
  g_object_set (vcfltr2, "caps", caps, NULL);
  gst_caps_unref (caps);

  g_object_set (vsrc, "num-buffers", 300, NULL);

  g_object_set (compositor, "emit-signals", TRUE, NULL);
  g_signal_connect (compositor, "samples-selected",
      G_CALLBACK (samples_selected_cb), consumed_buffers);

  pad = gst_element_get_static_pad (compositor, "sink_0");
  g_object_set (pad, "emit-signals", TRUE, NULL);
  g_signal_connect (pad, "buffer-consumed", G_CALLBACK (pad_buffer_consumed_cb),
      consumed_buffers);
  gst_object_unref (pad);

  gst_element_set_state (pipe, GST_STATE_PLAYING);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe));

  gst_message_unref (gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
          GST_MESSAGE_EOS));

  gst_object_unref (bus);

done:
  g_hash_table_foreach_remove (consumed_buffers,
      (GHRFunc) unref_consumed_buffers, NULL);
  g_hash_table_unref (consumed_buffers);
  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
  return ret;

err:
  ret = 1;
  goto done;
}

/* GStreamer Smart Video Encoder element
 * Copyright (C) <2010> Edward Hervey <bilboed@gmail.com>
 * Copyright (C) <2020> Thibault Saunier <tsaunier@igalia.com>
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
#include <gst/video/video.h>
#include "gstsmartencoder.h"

GST_DEBUG_CATEGORY_STATIC (smart_encoder_debug);
#define GST_CAT_DEFAULT smart_encoder_debug

/* FIXME : Update this with new caps */
/* WARNING : We can only allow formats with closed-GOP */
#define ALLOWED_CAPS "video/x-h263;video/x-intel-h263;"\
  "video/x-vp8;"\
  "video/x-vp9;"\
  "video/x-h264;"\
  "video/x-h265;"\
  "video/mpeg,mpegversion=(int)1,systemstream=(boolean)false;"\
  "video/mpeg,mpegversion=(int)2,systemstream=(boolean)false;"

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALLOWED_CAPS)
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALLOWED_CAPS)
    );

G_DEFINE_TYPE (GstSmartEncoder, gst_smart_encoder, GST_TYPE_BIN);

static void
smart_encoder_reset (GstSmartEncoder * self)
{
  gst_segment_init (&self->internal_segment, GST_FORMAT_UNDEFINED);
  gst_segment_init (&self->input_segment, GST_FORMAT_UNDEFINED);
  gst_segment_init (&self->output_segment, GST_FORMAT_UNDEFINED);

  if (self->decoder) {
    /* Clean up/remove internal encoding elements */
    gst_element_set_state (self->encoder, GST_STATE_NULL);
    gst_element_set_state (self->decoder, GST_STATE_NULL);
    gst_clear_object (&self->internal_srcpad);
    gst_element_remove_pad (GST_ELEMENT (self), self->internal_sinkpad);
    gst_bin_remove (GST_BIN (self), gst_object_ref (self->encoder));
    gst_bin_remove (GST_BIN (self), self->decoder);

    self->decoder = NULL;
    self->internal_sinkpad = NULL;
  }
  gst_clear_event (&self->segment_event);
}

static void
translate_timestamp_from_internal_to_src (GstSmartEncoder * self,
    GstClockTime * ts)
{
  GstClockTime running_time;

  if (gst_segment_to_running_time_full (&self->internal_segment,
          GST_FORMAT_TIME, *ts, &running_time) > 0)
    *ts = running_time + self->output_segment.start;
  else                          /* Negative timestamp */
    *ts = self->output_segment.start - running_time;
}

static GstFlowReturn
gst_smart_encoder_finish_buffer (GstSmartEncoder * self, GstBuffer * buf)
{
  if (!GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DTS (buf)))
    GST_BUFFER_DTS (buf) = GST_BUFFER_PTS (buf);

  translate_timestamp_from_internal_to_src (self, &GST_BUFFER_PTS (buf));
  translate_timestamp_from_internal_to_src (self, &GST_BUFFER_DTS (buf));

  if (self->last_dts > GST_BUFFER_DTS (buf)) {
    /* Hack to always produces dts increasing DTS-s that are close to what the
     * encoder produced. */
    GST_BUFFER_DTS (buf) = self->last_dts + 1;
  }
  self->last_dts = GST_BUFFER_DTS (buf);

  return gst_pad_push (self->srcpad, buf);
}

/*****************************************
 *    Internal encoder/decoder pipeline  *
 ******************************************/
static gboolean
internal_event_func (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSmartEncoder *self = GST_SMART_ENCODER (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      g_mutex_lock (&self->internal_flow_lock);
      if (self->internal_flow == GST_FLOW_CUSTOM_SUCCESS)
        self->internal_flow = GST_FLOW_OK;
      g_cond_signal (&self->internal_flow_cond);
      g_mutex_unlock (&self->internal_flow_lock);
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &self->internal_segment);

      if (self->output_segment.format == GST_FORMAT_UNDEFINED) {
        gst_segment_init (&self->output_segment, GST_FORMAT_TIME);

        /* Ensure that we can represent negative DTS in our 'single' segment */
        self->output_segment.start = 60 * 60 * GST_SECOND * 1000;
        if (!gst_pad_push_event (self->srcpad,
                gst_event_new_segment (&self->output_segment))) {
          GST_ERROR_OBJECT (self, "Could not push segment!");

          GST_ELEMENT_FLOW_ERROR (self, GST_FLOW_ERROR);

          return FALSE;
        }
      }

      break;
    case GST_EVENT_CAPS:
    {
      return gst_pad_push_event (self->srcpad, event);
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static GstFlowReturn
internal_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  return gst_smart_encoder_finish_buffer (GST_SMART_ENCODER (parent), buf);
}

static void
decodebin_src_pad_added_cb (GstElement * decodebin, GstPad * srcpad,
    GstSmartEncoder * self)
{
  GstPadLinkReturn ret = gst_pad_link (srcpad, self->encoder->sinkpads->data);

  if (ret != GST_PAD_LINK_OK) {
    GST_ERROR_OBJECT (self, "Could not link decoder with encoder! %s",
        gst_pad_link_get_name (ret));
    g_mutex_lock (&self->internal_flow_lock);
    self->internal_flow = GST_FLOW_NOT_LINKED;
    g_mutex_unlock (&self->internal_flow_lock);
  }
}

static gboolean
setup_recoder_pipeline (GstSmartEncoder * self)
{
  GstPad *tmppad;
  GstElement *capsfilter;
  GstPadLinkReturn lret;

  /* Fast path */
  if (G_UNLIKELY (self->decoder))
    return TRUE;

  g_assert (self->encoder);
  GST_DEBUG ("Creating internal decoder and encoder");

  /* Create decoder/encoder */
  self->decoder = gst_element_factory_make ("decodebin", NULL);
  if (G_UNLIKELY (self->decoder == NULL))
    goto no_decoder;
  g_signal_connect (self->decoder, "pad-added",
      G_CALLBACK (decodebin_src_pad_added_cb), self);
  gst_element_set_locked_state (self->decoder, TRUE);
  gst_bin_add (GST_BIN (self), self->decoder);
  gst_bin_add (GST_BIN (self), gst_object_ref (self->encoder));

  GST_DEBUG_OBJECT (self, "Creating internal pads");

  /* Create internal pads */

  /* Source pad which we'll use to feed data to decoders */
  self->internal_srcpad = gst_pad_new ("internal_src", GST_PAD_SRC);
  self->internal_sinkpad = gst_pad_new ("internal_sink", GST_PAD_SINK);
  gst_pad_set_iterate_internal_links_function (self->internal_sinkpad, NULL);
  if (!gst_element_add_pad (GST_ELEMENT (self), self->internal_sinkpad)) {
    GST_ERROR_OBJECT (self, "Could not add internal sinkpad %" GST_PTR_FORMAT,
        self->internal_sinkpad);
    return FALSE;
  }

  gst_pad_set_chain_function (self->internal_sinkpad,
      GST_DEBUG_FUNCPTR (internal_chain));
  gst_pad_set_event_function (self->internal_sinkpad,
      GST_DEBUG_FUNCPTR (internal_event_func));
  gst_pad_set_active (self->internal_sinkpad, TRUE);
  gst_pad_set_active (self->internal_srcpad, TRUE);

  GST_DEBUG_OBJECT (self, "Linking pads to elements");

  /* Link everything */
  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  if (!gst_bin_add (GST_BIN (self), capsfilter)) {
    GST_ERROR_OBJECT (self, "Could not add capsfilter!");
    return FALSE;
  }

  gst_element_sync_state_with_parent (capsfilter);
  if (!gst_element_link (self->encoder, capsfilter))
    goto encoder_capsfilter_link_fail;
  tmppad = gst_element_get_static_pad (capsfilter, "src");
  if ((lret =
          gst_pad_link_full (tmppad, self->internal_sinkpad,
              GST_PAD_LINK_CHECK_NOTHING)) < GST_PAD_LINK_OK)
    goto sinkpad_link_fail;
  gst_object_unref (tmppad);

  tmppad = gst_element_get_static_pad (self->decoder, "sink");
  if (GST_PAD_LINK_FAILED (gst_pad_link_full (self->internal_srcpad,
              tmppad, GST_PAD_LINK_CHECK_NOTHING)))
    goto srcpad_link_fail;
  gst_object_unref (tmppad);

  GST_DEBUG ("Done creating internal elements/pads");

  return TRUE;

no_decoder:
  {
    GST_WARNING ("Couldn't find a decodebin?!");
    return FALSE;
  }

srcpad_link_fail:
  {
    gst_object_unref (tmppad);
    GST_WARNING ("Couldn't link internal srcpad to decoder");
    return FALSE;
  }

sinkpad_link_fail:
  {
    gst_object_unref (tmppad);
    GST_WARNING ("Couldn't link encoder to internal sinkpad: %s",
        gst_pad_link_get_name (lret));
    return FALSE;
  }

encoder_capsfilter_link_fail:
  {
    GST_WARNING ("Couldn't link encoder to capsfilter");
    return FALSE;
  }
}

static GstFlowReturn
gst_smart_encoder_reencode_gop (GstSmartEncoder * self)
{
  GstFlowReturn res = GST_FLOW_OK;
  GstCaps *caps = NULL;

  GST_DEBUG_OBJECT (self, "Reencoding GOP!");
  if (self->decoder == NULL) {
    if (!setup_recoder_pipeline (self)) {
      GST_ERROR_OBJECT (self, "Could not setup reencoder pipeline");
      return GST_FLOW_ERROR;
    }
  }

  /* Activate elements */
  /* Set elements to PAUSED */
  gst_element_set_state (self->encoder, GST_STATE_PLAYING);
  gst_element_set_state (self->decoder, GST_STATE_PLAYING);

  GST_INFO ("Pushing Flush start/stop to clean decoder/encoder");
  gst_pad_push_event (self->internal_srcpad, gst_event_new_flush_start ());
  gst_pad_push_event (self->internal_srcpad, gst_event_new_flush_stop (TRUE));

  /* push segment_event */
  GST_INFO ("Pushing segment_event %" GST_PTR_FORMAT, self->segment_event);
  gst_pad_push_event (self->internal_srcpad,
      gst_event_ref (self->stream_start_event));
  caps = gst_pad_get_current_caps (self->sinkpad);
  gst_pad_push_event (self->internal_srcpad, gst_event_new_caps (caps));
  gst_caps_unref (caps);

  gst_pad_push_event (self->internal_srcpad,
      gst_event_ref (self->segment_event));

  /* Push buffers through our pads */
  GST_DEBUG ("Pushing %d pending buffers", g_list_length (self->pending_gop));

  g_mutex_lock (&self->internal_flow_lock);
  self->internal_flow = GST_FLOW_CUSTOM_SUCCESS;
  g_mutex_unlock (&self->internal_flow_lock);
  while (self->pending_gop) {
    GstBuffer *buf = (GstBuffer *) self->pending_gop->data;

    self->pending_gop =
        g_list_remove_link (self->pending_gop, self->pending_gop);
    res = gst_pad_push (self->internal_srcpad, buf);
    if (res == GST_FLOW_EOS) {
      GST_INFO_OBJECT (self, "Got eos... waiting for the event"
          " waiting for encoding to be done");
      break;
    }

    if (res != GST_FLOW_OK) {
      GST_WARNING ("Error pushing pending buffers : %s",
          gst_flow_get_name (res));
      goto done;
    }
  }

  GST_DEBUG_OBJECT (self, "-> Drain encoder.");
  gst_pad_push_event (self->internal_srcpad, gst_event_new_eos ());

  g_mutex_lock (&self->internal_flow_lock);
  while (self->internal_flow == GST_FLOW_CUSTOM_SUCCESS) {
    g_cond_wait (&self->internal_flow_cond, &self->internal_flow_lock);
  }
  g_mutex_unlock (&self->internal_flow_lock);

  res = self->internal_flow;

  GST_DEBUG_OBJECT (self, "Done reencoding GOP.");
  gst_element_set_state (self->encoder, GST_STATE_NULL);
  gst_element_set_state (self->decoder, GST_STATE_NULL);
  GST_OBJECT_FLAG_UNSET (self->internal_sinkpad, GST_PAD_FLAG_EOS);
  GST_OBJECT_FLAG_UNSET (self->internal_srcpad, GST_PAD_FLAG_EOS);

done:
  g_list_free_full (self->pending_gop, (GDestroyNotify) gst_buffer_unref);
  self->pending_gop = NULL;

  return res;
}

static gboolean
gst_smart_encoder_force_reencoding_for_caps (GstSmartEncoder * self)
{
  const gchar *profile;
  GstStructure *structure = gst_caps_get_structure (self->original_caps, 0);

  if (!gst_structure_has_name (structure, "video/x-vp9"))
    return FALSE;

  if (!(profile = gst_structure_get_string (structure, "profile"))) {
    GST_WARNING_OBJECT (self,
        "No profile set on `vp9` stream, force reencoding");

    return TRUE;
  }

  if (g_strcmp0 (profile, "0") && g_strcmp0 (profile, "2")) {
    GST_INFO_OBJECT (self, "vp9 profile %s not supported for smart reencoding"
        " as it might be using RGB stream which we can't handle properly"
        " force reencoding", profile);
    return TRUE;
  }

  return FALSE;
}

static GstFlowReturn
gst_smart_encoder_push_pending_gop (GstSmartEncoder * self)
{
  guint64 cstart, cstop;
  GList *tmp;
  gboolean force_reencoding = FALSE;
  GstFlowReturn res = GST_FLOW_OK;

  GST_DEBUG ("Pushing pending GOP (%" GST_TIME_FORMAT " -- %" GST_TIME_FORMAT
      ")", GST_TIME_ARGS (self->gop_start), GST_TIME_ARGS (self->gop_stop));

  if (!self->pending_gop) {
    /* This might happen on EOS */
    GST_INFO_OBJECT (self, "Empty gop!");
    goto done;
  }

  if (!gst_segment_clip (&self->input_segment, GST_FORMAT_TIME, self->gop_start,
          self->gop_stop, &cstart, &cstop)) {
    /* The whole GOP is outside the segment, there's most likely
     * a bug somewhere. */
    GST_DEBUG_OBJECT (self,
        "GOP is entirely outside of the segment, upstream gave us too much data: (%"
        GST_TIME_FORMAT " -- %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (self->gop_start), GST_TIME_ARGS (self->gop_stop));
    for (tmp = self->pending_gop; tmp; tmp = tmp->next)
      gst_buffer_unref ((GstBuffer *) tmp->data);

    goto done;
  }

  force_reencoding = gst_smart_encoder_force_reencoding_for_caps (self);
  if ((cstart != self->gop_start)
      || (cstop != self->gop_stop)
      || force_reencoding) {
    GST_INFO_OBJECT (self,
        "GOP needs to be re-encoded from %" GST_TIME_FORMAT " to %"
        GST_TIME_FORMAT " - %" GST_SEGMENT_FORMAT, GST_TIME_ARGS (cstart),
        GST_TIME_ARGS (cstop), &self->input_segment);
    res = gst_smart_encoder_reencode_gop (self);

    /* Make sure we push the original caps when resuming the original stream */
    if (!force_reencoding)
      self->push_original_caps = TRUE;
  } else {
    if (self->push_original_caps) {
      gst_pad_push_event (self->srcpad,
          gst_event_new_caps (self->original_caps));
      self->push_original_caps = FALSE;
    }

    if (self->output_segment.format == GST_FORMAT_UNDEFINED) {
      gst_segment_init (&self->output_segment, GST_FORMAT_TIME);

      /* Ensure that we can represent negative DTS in our 'single' segment */
      self->output_segment.start = 60 * 60 * GST_SECOND * 1000;
      if (!gst_pad_push_event (self->srcpad,
              gst_event_new_segment (&self->output_segment))) {
        GST_ERROR_OBJECT (self, "Could not push segment!");

        GST_ELEMENT_FLOW_ERROR (self, GST_FLOW_ERROR);

        return GST_FLOW_ERROR;
      }
    }

    /* The whole GOP is within the segment, push all pending buffers downstream */
    GST_INFO_OBJECT (self,
        "GOP doesn't need to be modified, pushing downstream: %" GST_TIME_FORMAT
        " to %" GST_TIME_FORMAT, GST_TIME_ARGS (cstart), GST_TIME_ARGS (cstop));

    self->internal_segment = self->input_segment;
    for (tmp = self->pending_gop; tmp; tmp = tmp->next) {
      GstBuffer *buf = (GstBuffer *) tmp->data;

      res = gst_smart_encoder_finish_buffer (self, buf);
      if (G_UNLIKELY (res != GST_FLOW_OK))
        break;
    }
  }

done:
  g_list_free (self->pending_gop);
  self->pending_gop = NULL;
  self->gop_start = GST_CLOCK_TIME_NONE;
  self->gop_stop = 0;

  return res;
}

static GstFlowReturn
gst_smart_encoder_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstSmartEncoder *self;
  GstFlowReturn res = GST_FLOW_OK;
  gboolean discont, keyframe;
  GstClockTime end_time;

  self = GST_SMART_ENCODER (parent->parent);

  discont = GST_BUFFER_IS_DISCONT (buf);
  keyframe = !GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
  end_time = GST_BUFFER_PTS (buf);
  if (GST_CLOCK_TIME_IS_VALID (end_time))
    end_time += (GST_BUFFER_DURATION_IS_VALID (buf) ? buf->duration : 0);

  GST_DEBUG_OBJECT (pad,
      "New buffer %s %s %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
      discont ? "discont" : "", keyframe ? "keyframe" : "",
      GST_TIME_ARGS (GST_BUFFER_PTS (buf)), GST_TIME_ARGS (end_time));

  if (keyframe) {
    /* If there's a pending GOP, flush it out */
    if (self->pending_gop) {
      /* Mark stop of previous gop */
      if (GST_BUFFER_PTS_IS_VALID (buf)) {
        if (self->gop_stop > buf->pts)
          GST_WARNING_OBJECT (self, "Next gop start < current gop" " end");
        self->gop_stop = buf->pts;
      }

      /* flush pending */
      res = gst_smart_encoder_push_pending_gop (self);
      if (G_UNLIKELY (res != GST_FLOW_OK))
        goto beach;
    }

    /* Mark gop_start for new gop */
    self->gop_start = GST_BUFFER_TIMESTAMP (buf);
  }

  /* Store buffer */
  self->pending_gop = g_list_append (self->pending_gop, buf);

  /* Update GOP stop position */
  if (GST_CLOCK_TIME_IS_VALID (end_time))
    self->gop_stop = MAX (self->gop_stop, end_time);

  GST_DEBUG_OBJECT (self, "Buffer stored , Current GOP : %"
      GST_TIME_FORMAT " -- %" GST_TIME_FORMAT,
      GST_TIME_ARGS (self->gop_start), GST_TIME_ARGS (self->gop_stop));

beach:
  return res;
}

static GstCaps *
smart_encoder_get_caps (GstSmartEncoder * self, GstCaps * original_caps)
{
  gint i;
  GstCaps *caps, *outcaps;
  GstStructure *original_struct = gst_caps_get_structure (original_caps, 0);
  GstStructure *out_struct, *_struct;
  GstVideoInfo info;
  static const gchar *default_fields[] = {
    "pixel-aspect-ratio",
    "framerate",
    "interlace-mode",
    "colorimetry",
    "chroma-site",
    "multiview-mode",
    "multiview-flags",
  };

  if (!gst_structure_has_name (original_struct, "video/x-vp8")) {

    return gst_caps_ref (original_caps);
  }

  /* VP8 is always decoded into YUV colorspaces and we support VP9 profiles
   * where only YUV is supported (0 and 2) so we ensure that all the
   * default fields for video/x-raw are set on the caps if none provided by
   * upstream. This allows us to allow renegotiating new caps downstream when
   * switching from no reencoding to reencoding making sure all the fields are
   * defined all the time
   */
  caps = gst_caps_copy (original_caps);
  _struct = gst_caps_get_structure (caps, 0);
  gst_structure_set_name (_struct, "video/x-raw");
  gst_structure_set (_struct,
      "format", G_TYPE_STRING, "I420",
      "multiview-mode", G_TYPE_STRING, "mono",
      "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
      GST_VIDEO_MULTIVIEW_FLAGS_NONE, GST_FLAG_SET_MASK_EXACT, NULL);

  gst_video_info_from_caps (&info, caps);
  gst_caps_unref (caps);
  caps = gst_video_info_to_caps (&info);
  _struct = gst_caps_get_structure (caps, 0);

  outcaps = gst_caps_copy (original_caps);
  out_struct = gst_caps_get_structure (outcaps, 0);
  for (i = 0; i < G_N_ELEMENTS (default_fields); i++) {
    const gchar *field = default_fields[i];

    if (!gst_structure_has_field (original_struct, field)) {
      const GValue *v = gst_structure_get_value (_struct, field);
      g_assert (v);
      gst_structure_set_value (out_struct, field, v);
    }
  }
  gst_caps_unref (caps);

  return outcaps;
}

static gboolean
smart_encoder_sink_event (GstPad * pad, GstObject * ghostpad, GstEvent * event)
{
  gboolean res = TRUE;
  GstSmartEncoder *self = GST_SMART_ENCODER (ghostpad->parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      smart_encoder_reset (self);
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      if (self->original_caps)
        gst_caps_unref (self->original_caps);


      self->original_caps = smart_encoder_get_caps (self, caps);
      self->push_original_caps = TRUE;
      gst_clear_event (&event);
      break;
    }
    case GST_EVENT_STREAM_START:
      gst_event_replace (&self->stream_start_event, gst_event_ref (event));
      break;
    case GST_EVENT_SEGMENT:
    {
      GST_INFO_OBJECT (self, "Pushing pending GOP on new segment");
      gst_smart_encoder_push_pending_gop (self);

      gst_event_copy_segment (event, &self->input_segment);

      GST_DEBUG_OBJECT (self, "input_segment: %" GST_SEGMENT_FORMAT,
          &self->input_segment);
      if (self->input_segment.format != GST_FORMAT_TIME) {
        GST_ERROR_OBJECT (self, "Can't handle streams %s format",
            gst_format_get_name (self->input_segment.format));
        gst_event_unref (event);

        return FALSE;
      }
      self->segment_event = event;
      event = NULL;
      GST_INFO_OBJECT (self, "Eating segment");
      break;
    }
    case GST_EVENT_EOS:
      if (self->input_segment.format == GST_FORMAT_TIME)
        gst_smart_encoder_push_pending_gop (self);
      break;
    default:
      break;
  }

  if (event)
    res = gst_pad_push_event (self->srcpad, event);

  return res;
}

static GstCaps *
smart_encoder_sink_getcaps (GstSmartEncoder * self, GstPad * pad,
    GstCaps * filter)
{
  GstCaps *peer, *tmpl, *res;

  tmpl = gst_static_pad_template_get_caps (&src_template);

  /* Try getting it from downstream */
  peer = gst_pad_peer_query_caps (self->srcpad, tmpl);
  if (peer == NULL) {
    res = tmpl;
  } else {
    res = peer;
    gst_caps_unref (tmpl);
  }

  if (filter) {
    GstCaps *filtered_res = gst_caps_intersect (res, filter);

    gst_caps_unref (res);
    if (!filtered_res || gst_caps_is_empty (filtered_res)) {
      res = NULL;
    } else {
      res = filtered_res;
    }
  }

  return res;
}

static gboolean
_pad_sink_acceptcaps (GstPad * pad, GstSmartEncoder * self, GstCaps * caps)
{
  gboolean ret;
  GstCaps *modified_caps;
  GstCaps *accepted_caps;
  gint i, n;
  GstStructure *s;

  GST_DEBUG_OBJECT (pad, "%" GST_PTR_FORMAT, caps);

  accepted_caps = gst_pad_get_current_caps (GST_PAD (self->srcpad));
  if (accepted_caps == NULL)
    accepted_caps = gst_pad_get_pad_template_caps (GST_PAD (self->srcpad));
  accepted_caps = gst_caps_make_writable (accepted_caps);

  GST_LOG_OBJECT (pad, "src caps %" GST_PTR_FORMAT, accepted_caps);

  n = gst_caps_get_size (accepted_caps);
  for (i = 0; i < n; i++) {
    s = gst_caps_get_structure (accepted_caps, i);

    if (gst_structure_has_name (s, "video/x-h264") ||
        gst_structure_has_name (s, "video/x-h265")) {
      gst_structure_remove_fields (s, "codec_data", "tier", "profile", "level",
          NULL);
    } else if (gst_structure_has_name (s, "video/x-vp8")
        || gst_structure_has_name (s, "video/x-vp9")) {
      gst_structure_remove_field (s, "streamheader");
    }
  }

  modified_caps = gst_caps_copy (caps);
  n = gst_caps_get_size (modified_caps);
  for (i = 0; i < n; i++) {
    s = gst_caps_get_structure (modified_caps, i);

    if (gst_structure_has_name (s, "video/x-h264") ||
        gst_structure_has_name (s, "video/x-h265")) {
      gst_structure_remove_fields (s, "codec_data", "tier", "profile", "level",
          NULL);
    } else if (gst_structure_has_name (s, "video/x-vp8")
        || gst_structure_has_name (s, "video/x-vp9")) {
      gst_structure_remove_field (s, "streamheader");
    }
  }

  ret = gst_caps_can_intersect (modified_caps, accepted_caps);
  GST_DEBUG_OBJECT (pad, "%saccepted caps %" GST_PTR_FORMAT,
      (ret ? "" : "Doesn't "), caps);
  return ret;
}

static gboolean
smart_encoder_sink_query (GstPad * pad, GstObject * ghostpad, GstQuery * query)
{
  gboolean res;
  GstSmartEncoder *self = GST_SMART_ENCODER (ghostpad->parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = smart_encoder_sink_getcaps (self, pad, filter);
      GST_DEBUG_OBJECT (self, "Got caps: %" GST_PTR_FORMAT, caps);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      res = TRUE;
      break;
    }
    case GST_QUERY_ACCEPT_CAPS:
    {
      GstCaps *caps;

      gst_query_parse_accept_caps (query, &caps);
      res = _pad_sink_acceptcaps (GST_PAD (pad), self, caps);
      gst_query_set_accept_caps_result (query, res);
      res = TRUE;
      break;
    }
    default:
      res = gst_pad_query_default (pad, ghostpad, query);
      break;
  }
  return res;
}

static gboolean
gst_smart_encoder_add_parser (GstSmartEncoder * self, GstCaps * format)
{
  const gchar *stream_format;
  GstPad *chainpad, *internal_chainpad, *sinkpad = NULL;
  GstStructure *structure = gst_caps_get_structure (format, 0);
  GstElement *capsfilter = gst_element_factory_make ("capsfilter", NULL);
  GstElement *parser = NULL;

  gst_bin_add (GST_BIN (self), capsfilter);
  g_object_set (capsfilter, "caps", format, NULL);
  if (gst_structure_has_name (structure, "video/x-h264")) {
    parser = gst_element_factory_make ("h264parse", NULL);
    if (!parser) {
      GST_ERROR_OBJECT (self, "`h264parse` is missing, can't encode smartly");

      goto failed;
    }

    stream_format = gst_structure_get_string (structure, "stream-format");
    if (g_strcmp0 (stream_format, "avc"))
      g_object_set (parser, "config-interval", -1, NULL);

  } else if (gst_structure_has_name (gst_caps_get_structure (format, 0),
          "video/x-h265")) {
    parser = gst_element_factory_make ("h265parse", NULL);
    if (!parser) {
      GST_ERROR_OBJECT (self, "`h265parse` is missing, can't encode smartly");

      goto failed;
    }

    stream_format = gst_structure_get_string (structure, "stream-format");
    if (g_strcmp0 (stream_format, "hvc1"))
      g_object_set (parser, "config-interval", -1, NULL);
  } else if (gst_structure_has_name (structure, "video/x-vp9")) {
    parser = gst_element_factory_make ("vp9parse", NULL);
    if (!parser) {
      GST_ERROR_OBJECT (self, "`vp9parse` is missing, can't encode smartly");

      goto failed;
    }
  } else {
    sinkpad = gst_element_get_static_pad (capsfilter, "sink");
  }

  if (parser) {
    if (!gst_bin_add (GST_BIN (self), parser)) {
      GST_ERROR_OBJECT (self, "Could not add parser.");

      goto failed;
    }

    if (!gst_element_link (parser, capsfilter)) {
      GST_ERROR_OBJECT (self, "Could not link capfilter and parser.");

      goto failed;
    }

    sinkpad = gst_element_get_static_pad (parser, "sink");
  }

  g_assert (sinkpad);

  /* The chainpad is the pad that is linked to the srcpad of the chain
   * of element that is linked to our public sinkpad, this is the pad where
   * we chain the buffers either directly to our srcpad or through the
   * reencoding sub chain. */
  chainpad =
      GST_PAD (gst_ghost_pad_new ("chainpad", capsfilter->srcpads->data));
  gst_element_add_pad (GST_ELEMENT (self), chainpad);
  internal_chainpad =
      GST_PAD (gst_proxy_pad_get_internal (GST_PROXY_PAD (chainpad)));
  gst_pad_set_chain_function (internal_chainpad, gst_smart_encoder_chain);
  gst_pad_set_event_function (internal_chainpad, smart_encoder_sink_event);
  gst_pad_set_query_function (internal_chainpad, smart_encoder_sink_query);

  gst_ghost_pad_set_target (GST_GHOST_PAD (self->sinkpad), sinkpad);
  gst_object_unref (sinkpad);

  return TRUE;

failed:
  gst_clear_object (&parser);

  return FALSE;
}

gboolean
gst_smart_encoder_set_encoder (GstSmartEncoder * self, GstCaps * format,
    GstElement * encoder)
{
  self->encoder = g_object_ref_sink (encoder);
  gst_element_set_locked_state (self->encoder, TRUE);

  return gst_smart_encoder_add_parser (self, format);
}

/******************************************
 *    GstElement vmethod implementations  *
 ******************************************/

static GstStateChangeReturn
gst_smart_encoder_change_state (GstElement * element, GstStateChange transition)
{
  GstSmartEncoder *self;
  GstStateChangeReturn ret;

  g_return_val_if_fail (GST_IS_SMART_ENCODER (element),
      GST_STATE_CHANGE_FAILURE);

  self = GST_SMART_ENCODER (element);

  ret =
      GST_ELEMENT_CLASS (gst_smart_encoder_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      smart_encoder_reset (self);
      break;
    default:
      break;
  }

  return ret;
}

/******************************************
 *          GObject vmethods              *
 ******************************************/
static void
gst_smart_encoder_finalize (GObject * object)
{
  GstSmartEncoder *self = (GstSmartEncoder *) object;
  g_mutex_clear (&self->internal_flow_lock);
  g_cond_clear (&self->internal_flow_cond);

  G_OBJECT_CLASS (gst_smart_encoder_parent_class)->finalize (object);
}

static void
gst_smart_encoder_dispose (GObject * object)
{
  GstSmartEncoder *self = (GstSmartEncoder *) object;

  gst_clear_object (&self->encoder);

  if (self->original_caps) {
    gst_caps_unref (self->original_caps);
    self->original_caps = NULL;
  }

  G_OBJECT_CLASS (gst_smart_encoder_parent_class)->dispose (object);
}


static void
gst_smart_encoder_class_init (GstSmartEncoderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  element_class = (GstElementClass *) klass;
  gobject_class = G_OBJECT_CLASS (klass);

  gst_smart_encoder_parent_class = g_type_class_peek_parent (klass);

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class, "Smart Video Encoder",
      "Codec/Recoder/Video",
      "Re-encodes portions of Video that lay on segment boundaries",
      "Edward Hervey <bilboed@gmail.com>");

  gobject_class->dispose = (GObjectFinalizeFunc) gst_smart_encoder_dispose;
  gobject_class->finalize = (GObjectFinalizeFunc) gst_smart_encoder_finalize;
  element_class->change_state = gst_smart_encoder_change_state;

  GST_DEBUG_CATEGORY_INIT (smart_encoder_debug, "smartencoder", 0,
      "Smart Encoder");
}

static void
gst_smart_encoder_init (GstSmartEncoder * self)
{
  GstPadTemplate *template = gst_static_pad_template_get (&sink_template);

  self->sinkpad = gst_ghost_pad_new_no_target_from_template ("sink", template);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_object_unref (template);

  self->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_use_fixed_caps (self->srcpad);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  g_mutex_init (&self->internal_flow_lock);
  g_cond_init (&self->internal_flow_cond);
  smart_encoder_reset (self);
}

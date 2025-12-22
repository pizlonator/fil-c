/*
 * GStreamer
 * Copyright (C) 2018 Sebastian Dröge <sebastian@centricular.com>
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
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include <string.h>

#include "ccutils.h"
#include "gstccconverter.h"

GST_DEBUG_CATEGORY_STATIC (gst_cc_converter_debug);
#define GST_CAT_DEFAULT gst_cc_converter_debug

/**
 * GstCCConverterCDPMode:
 * @GST_CC_CONVERTER_CDP_MODE_TIME_CODE: Store time code information in CDP packets
 * @GST_CC_CONVERTER_CDP_MODE_CC_DATA: Store CC data in CDP packets
 * @GST_CC_CONVERTER_CDP_MODE_CC_SVC_INFO: Store CC service information in CDP packets
 *
 * Since: 1.20
 */

enum
{
  PROP_0,
  PROP_CDP_MODE,
};

#define DEFAULT_CDP_MODE (GST_CC_CONVERTER_CDP_MODE_TIME_CODE | GST_CC_CONVERTER_CDP_MODE_CC_DATA | GST_CC_CONVERTER_CDP_MODE_CC_SVC_INFO)
#define DEFAULT_FIELD 0

/* Ordered by the amount of information they can contain */
#define CC_CAPS \
        "closedcaption/x-cea-708,format=(string) cdp; " \
        "closedcaption/x-cea-708,format=(string) cc_data; " \
        "closedcaption/x-cea-608,format=(string) s334-1a; " \
        "closedcaption/x-cea-608,format=(string) raw, field=(int) {0, 1}"

#define VAL_OR_0(v) ((v) ? (*(v)) : 0)

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CC_CAPS));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CC_CAPS));

#define parent_class gst_cc_converter_parent_class
G_DEFINE_TYPE (GstCCConverter, gst_cc_converter, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (ccconverter, "ccconverter",
    GST_RANK_NONE, GST_TYPE_CCCONVERTER);

#define GST_TYPE_CC_CONVERTER_CDP_MODE (gst_cc_converter_cdp_mode_get_type())
static GType
gst_cc_converter_cdp_mode_get_type (void)
{
  static const GFlagsValue values[] = {
    {GST_CC_CDP_MODE_TIME_CODE,
        "Store time code information in CDP packets", "time-code"},
    {GST_CC_CDP_MODE_CC_DATA, "Store CC data in CDP packets",
        "cc-data"},
    {GST_CC_CDP_MODE_CC_SVC_INFO,
        "Store CC service information in CDP packets", "cc-svc-info"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_flags_register_static ("GstCCConverterCDPMode", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

static gboolean
gst_cc_converter_transform_size (GstBaseTransform * base,
    GstPadDirection direction,
    GstCaps * caps, gsize size, GstCaps * othercaps, gsize * othersize)
{
  /* We can't really convert from an output size to an input size */
  if (direction != GST_PAD_SINK)
    return FALSE;

  /* Assume worst-case here and over-allocate, and in ::transform() we then
   * downsize the buffer as needed. The worst-case is one CDP packet, which
   * can be up to MAX_CDP_PACKET_LEN bytes large */

  *othersize = MAX_CDP_PACKET_LEN;

  return TRUE;
}

static GstCaps *
gst_cc_converter_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  static GstStaticCaps non_cdp_caps =
      GST_STATIC_CAPS ("closedcaption/x-cea-708, format=(string)cc_data; "
      "closedcaption/x-cea-608,format=(string) s334-1a; "
      "closedcaption/x-cea-608,format=(string) raw, field=(int) {0, 1}");
  static GstStaticCaps cdp_caps =
      GST_STATIC_CAPS ("closedcaption/x-cea-708, format=(string)cdp");
  static GstStaticCaps cdp_caps_framerate =
      GST_STATIC_CAPS ("closedcaption/x-cea-708, format=(string)cdp, "
      "framerate=(fraction){60/1, 60000/1001, 50/1, 30/1, 30000/1001, 25/1, 24/1, 24000/1001}");
  static GstStaticCaps raw_608_caps =
      GST_STATIC_CAPS ("closedcaption/x-cea-608,format=(string) raw");

  GstCCConverter *self = GST_CCCONVERTER (base);
  guint i, n;
  GstCaps *res, *templ;

  templ = gst_pad_get_pad_template_caps (base->srcpad);

  GST_DEBUG_OBJECT (self, "direction %s from caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "src" : "sink", caps);

  res = gst_caps_new_empty ();
  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    const GstStructure *s = gst_caps_get_structure (caps, i);
    const GValue *framerate = gst_structure_get_value (s, "framerate");

    if (gst_structure_has_name (s, "closedcaption/x-cea-608")) {
      const GValue *field = gst_structure_get_value (s, "field");

      if (direction == GST_PAD_SRC) {
        GstCaps *tmp;

        tmp = gst_caps_make_writable (gst_static_caps_get (&non_cdp_caps));

        if (!field) {
          tmp = gst_caps_merge (tmp, gst_static_caps_get (&raw_608_caps));
        }

        /* SRC direction: We produce upstream caps
         *
         * Downstream wanted CEA608 caps. If it had a framerate, we
         * also need upstream to provide exactly that same framerate
         * and otherwise we don't care.
         *
         * We can convert everything to CEA608.
         */
        res = gst_caps_merge (res, gst_static_caps_get (&cdp_caps_framerate));
        if (framerate) {
          /* we can only keep the same framerate for non-cdp */
          gst_caps_set_value (tmp, "framerate", framerate);
        }
        res = gst_caps_merge (res, tmp);
      } else {
        /* SINK: We produce downstream caps
         *
         * Upstream provided CEA608 caps. We can convert that to CDP if
         * also a CDP compatible framerate was provided, and we can convert
         * it to anything else regardless.
         *
         * If upstream provided a framerate we can pass that through, possibly
         * filtered for the CDP case.
         */
        if (framerate) {
          GstCaps *tmp;
          GstStructure *t;

          /* Create caps that contain the intersection of all framerates with
           * the CDP allowed framerates */
          tmp =
              gst_caps_make_writable (gst_static_caps_get
              (&cdp_caps_framerate));
          t = gst_caps_get_structure (tmp, 0);
          gst_structure_set_name (t, "closedcaption/x-cea-608");
          gst_structure_remove_field (t, "format");
          if (gst_structure_can_intersect (s, t)) {
            gst_caps_unref (tmp);

            tmp =
                gst_caps_make_writable (gst_static_caps_get
                (&cdp_caps_framerate));

            res = gst_caps_merge (res, tmp);
          } else {
            gst_caps_unref (tmp);
          }
          /* And we can convert to everything else with the given framerate */
          tmp = gst_caps_make_writable (gst_static_caps_get (&non_cdp_caps));

          if (!field) {
            tmp = gst_caps_merge (tmp, gst_static_caps_get (&raw_608_caps));
          }

          gst_caps_set_value (tmp, "framerate", framerate);
          res = gst_caps_merge (res, tmp);
        } else {
          res = gst_caps_merge (res, gst_static_caps_get (&non_cdp_caps));

          if (!field) {
            res = gst_caps_merge (res, gst_static_caps_get (&raw_608_caps));
          }
        }
      }
    } else if (gst_structure_has_name (s, "closedcaption/x-cea-708")) {
      if (direction == GST_PAD_SRC) {
        /* SRC direction: We produce upstream caps
         *
         * Downstream wanted CEA708 caps. If downstream wants *only* CDP we
         * either need CDP from upstream, or anything else with a CDP
         * framerate.
         * If downstream also wants non-CDP we can accept anything.
         *
         * We pass through any framerate as-is, except for filtering
         * for CDP framerates if downstream wants only CDP.
         */

        if (g_strcmp0 (gst_structure_get_string (s, "format"), "cdp") == 0) {
          /* Downstream wants only CDP */

          /* We need CDP from upstream in that case */
          res = gst_caps_merge (res, gst_static_caps_get (&cdp_caps_framerate));

          /* Or anything else with a CDP framerate */
          {
            GstCaps *tmp, *cdp_caps;
            const GValue *cdp_framerate;

            /* Get all CDP framerates, we can accept anything that has those
             * framerates */
            cdp_caps = gst_static_caps_get (&cdp_caps_framerate);
            cdp_framerate =
                gst_structure_get_value (gst_caps_get_structure (cdp_caps, 0),
                "framerate");

            tmp = gst_caps_make_writable (gst_static_caps_get (&non_cdp_caps));
            tmp = gst_caps_merge (tmp, gst_static_caps_get (&raw_608_caps));
            gst_caps_set_value (tmp, "framerate", cdp_framerate);
            gst_caps_unref (cdp_caps);

            res = gst_caps_merge (res, tmp);
          }
        } else {
          /* Downstream wants not only CDP, we can do everything */
          res = gst_caps_merge (res, gst_static_caps_get (&cdp_caps_framerate));
          if (framerate) {
            /* we can only keep the same framerate for non-cdp */
            GstCaps *tmp;

            tmp = gst_caps_make_writable (gst_static_caps_get (&non_cdp_caps));
            tmp = gst_caps_merge (tmp, gst_static_caps_get (&raw_608_caps));
            gst_caps_set_value (tmp, "framerate", framerate);
            res = gst_caps_merge (res, tmp);
          } else {
            res = gst_caps_merge (res, gst_static_caps_get (&non_cdp_caps));
            res = gst_caps_merge (res, gst_static_caps_get (&raw_608_caps));
          }
        }
      } else {
        GstCaps *tmp;

        /* SINK: We produce downstream caps
         *
         * Upstream provided CEA708 caps. If upstream provided CDP we can
         * output CDP, no matter what (-> passthrough). If upstream did not
         * provide CDP, we can output CDP only if the framerate fits.
         * We can always produce everything else apart from CDP.
         *
         * If upstream provided a framerate we pass that through for non-CDP
         * output, and pass it through filtered for CDP output.
         */

        if (gst_structure_can_intersect (s,
                gst_caps_get_structure (gst_static_caps_get (&cdp_caps), 0))) {
          /* Upstream provided CDP caps, we can do everything independent of
           * framerate */
          res = gst_caps_merge (res, gst_static_caps_get (&cdp_caps_framerate));
        } else if (framerate) {
          const GValue *cdp_framerate;
          GstStructure *t;

          /* Upstream did not provide CDP. We can only do CDP if upstream
           * happened to have a CDP framerate */

          /* Create caps that contain the intersection of all framerates with
           * the CDP allowed framerates */
          tmp =
              gst_caps_make_writable (gst_static_caps_get
              (&cdp_caps_framerate));
          t = gst_caps_get_structure (tmp, 0);

          /* There's an intersection between the framerates so we can convert
           * into CDP with exactly those framerates */
          cdp_framerate = gst_structure_get_value (t, "framerate");
          if (gst_value_intersect (NULL, cdp_framerate, framerate)) {
            gst_caps_set_value (tmp, "framerate", cdp_framerate);

            res = gst_caps_merge (res, tmp);
          } else {
            gst_clear_caps (&tmp);
          }
        }
        /* We can always convert CEA708 to all non-CDP formats */
        if (framerate) {
          /* we can only keep the same framerate for non-cdp */
          GstCaps *tmp;

          tmp = gst_caps_make_writable (gst_static_caps_get (&non_cdp_caps));
          tmp = gst_caps_merge (tmp, gst_static_caps_get (&raw_608_caps));
          gst_caps_set_value (tmp, "framerate", framerate);
          res = gst_caps_merge (res, tmp);
        } else {
          res = gst_caps_merge (res, gst_static_caps_get (&non_cdp_caps));
          res = gst_caps_merge (res, gst_static_caps_get (&raw_608_caps));
        }
      }
    } else {
      g_assert_not_reached ();
    }
  }

  GST_DEBUG_OBJECT (self, "pre filter caps %" GST_PTR_FORMAT, res);

  /* We can convert anything into anything but it might involve loss of
   * information so always filter according to the order in our template caps
   * in the end */
  if (filter) {
    GstCaps *tmp;
    filter = gst_caps_intersect_full (templ, filter, GST_CAPS_INTERSECT_FIRST);

    tmp = gst_caps_intersect_full (filter, res, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (res);
    res = tmp;
  }

  gst_caps_unref (templ);

  GST_DEBUG_OBJECT (self, "Transformed in direction %s caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "src" : "sink", caps);
  GST_DEBUG_OBJECT (self, "filter %" GST_PTR_FORMAT, filter);
  GST_DEBUG_OBJECT (self, "to %" GST_PTR_FORMAT, res);

  gst_clear_caps (&filter);

  return res;
}

static GstCaps *
gst_cc_converter_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * incaps, GstCaps * outcaps)
{
  GstCCConverter *self = GST_CCCONVERTER (base);
  const GstStructure *s;
  GstStructure *t;
  const GValue *framerate;
  GstCaps *intersection, *templ;

  GST_DEBUG_OBJECT (self, "Fixating in direction %s incaps %" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "src" : "sink", incaps);
  GST_DEBUG_OBJECT (self, "and outcaps %" GST_PTR_FORMAT, outcaps);

  /* Prefer passthrough if we can */
  if (gst_caps_is_subset (incaps, outcaps)) {
    gst_caps_unref (outcaps);
    return GST_BASE_TRANSFORM_CLASS (parent_class)->fixate_caps (base,
        direction, incaps, gst_caps_ref (incaps));
  }

  /* Otherwise prefer caps in the order of our template caps */
  templ = gst_pad_get_pad_template_caps (base->srcpad);
  intersection =
      gst_caps_intersect_full (templ, outcaps, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (outcaps);
  outcaps = intersection;

  outcaps =
      GST_BASE_TRANSFORM_CLASS (parent_class)->fixate_caps (base, direction,
      incaps, outcaps);

  s = gst_caps_get_structure (incaps, 0);
  framerate = gst_structure_get_value (s, "framerate");
  outcaps = gst_caps_make_writable (outcaps);
  t = gst_caps_get_structure (outcaps, 0);
  if (!framerate) {
    /* remove any output framerate that might've been added by basetransform
     * due to intersecting with downstream */
    gst_structure_remove_field (t, "framerate");
  } else {
    /* or passthrough the input framerate if possible */
    guint n, d;

    n = gst_value_get_fraction_numerator (framerate);
    d = gst_value_get_fraction_denominator (framerate);

    if (gst_structure_has_field (t, "framerate"))
      gst_structure_fixate_field_nearest_fraction (t, "framerate", n, d);
    else
      gst_structure_set (t, "framerate", GST_TYPE_FRACTION, n, d, NULL);
  }

  GST_DEBUG_OBJECT (self,
      "Fixated caps %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, incaps, outcaps);

  return outcaps;
}

static gboolean
gst_cc_converter_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstCCConverter *self = GST_CCCONVERTER (base);
  const GstStructure *s;
  gboolean passthrough;
  static GstStaticCaps raw_608_caps =
      GST_STATIC_CAPS ("closedcaption/x-cea-608,format=(string) raw");

  self->input_caption_type = gst_video_caption_type_from_caps (incaps);
  self->output_caption_type = gst_video_caption_type_from_caps (outcaps);

  if (self->input_caption_type == GST_VIDEO_CAPTION_TYPE_UNKNOWN ||
      self->output_caption_type == GST_VIDEO_CAPTION_TYPE_UNKNOWN)
    goto invalid_caps;

  s = gst_caps_get_structure (incaps, 0);
  if (!gst_structure_get_fraction (s, "framerate", &self->in_fps_n,
          &self->in_fps_d))
    self->in_fps_n = self->in_fps_d = 0;

  if (!gst_structure_get_int (s, "field", &self->in_field))
    self->in_field = 0;

  s = gst_caps_get_structure (outcaps, 0);
  if (!gst_structure_get_fraction (s, "framerate", &self->out_fps_n,
          &self->out_fps_d))
    self->out_fps_n = self->out_fps_d = 0;

  if (!gst_structure_get_int (s, "field", &self->out_field))
    self->out_field = 0;

  gst_video_time_code_clear (&self->current_output_timecode);

  if (gst_caps_is_subset (incaps, gst_static_caps_get (&raw_608_caps)) &&
      gst_caps_is_subset (outcaps, gst_static_caps_get (&raw_608_caps))) {
    passthrough = self->in_field == self->out_field;
  } else {
    /* Caps can be different but we can passthrough as long as they can
     * intersect, i.e. have same caps name and format */
    passthrough = gst_caps_can_intersect (incaps, outcaps);
  }

  gst_base_transform_set_passthrough (base, passthrough);

  GST_DEBUG_OBJECT (self,
      "Got caps %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT " (passthrough %d)",
      incaps, outcaps, passthrough);

  return TRUE;

invalid_caps:
  {
    GST_ERROR_OBJECT (self,
        "Invalid caps: in %" GST_PTR_FORMAT " out: %" GST_PTR_FORMAT, incaps,
        outcaps);
    return FALSE;
  }
}

static void
get_framerate_output_scale (GstCCConverter * self,
    const struct cdp_fps_entry *in_fps_entry, gint * scale_n, gint * scale_d)
{
  if (self->in_fps_n == 0 || self->out_fps_d == 0) {
    *scale_n = 1;
    *scale_d = 1;
    return;
  }

  /* compute the relative rates of the two framerates */
  if (!gst_util_fraction_multiply (in_fps_entry->fps_d, in_fps_entry->fps_n,
          self->out_fps_n, self->out_fps_d, scale_n, scale_d))
    /* we should never overflow */
    g_assert_not_reached ();
}

static gboolean
interpolate_time_code_with_framerate (GstCCConverter * self,
    const GstVideoTimeCode * tc, gint out_fps_n, gint out_fps_d,
    gint scale_n, gint scale_d, GstVideoTimeCode * out)
{
  gchar *tc_str;
  gint output_n, output_d;
  guint output_frame;
  GstVideoTimeCodeFlags flags;

  g_return_val_if_fail (out != NULL, FALSE);
  /* out_n/d can only be 0 if scale_n/d are 1/1 */
  g_return_val_if_fail ((scale_n == 1 && scale_d == 1) || (out_fps_n != 0
          && out_fps_d != 0), FALSE);

  if (!tc || tc->config.fps_n == 0)
    return FALSE;

  if (!gst_util_fraction_multiply (tc->frames, 1, scale_n, scale_d, &output_n,
          &output_d))
    /* we should never overflow */
    g_assert_not_reached ();

  tc_str = gst_video_time_code_to_string (tc);
  GST_TRACE_OBJECT (self, "interpolating time code %s with scale %d/%d "
      "to frame %d/%d", tc_str, scale_n, scale_d, output_n, output_d);
  g_free (tc_str);

  if (out_fps_n == 0 || out_fps_d == 0) {
    out_fps_n = tc->config.fps_n;
    out_fps_d = tc->config.fps_d;
  }

  flags = tc->config.flags;
  if ((flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) != 0 && out_fps_d != 1001
      && out_fps_n != 60000 && out_fps_n != 30000) {
    flags &= ~GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
  } else if ((flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) == 0
      && out_fps_d == 1001 && (out_fps_n == 60000 || out_fps_n == 30000)) {
    /* XXX: theoretically, not quite correct however this is an assumption
     * we have elsewhere that these framerates are always drop-framed */
    flags |= GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME;
  }

  output_frame = output_n / output_d;

  *out = (GstVideoTimeCode) GST_VIDEO_TIME_CODE_INIT;
  do {
    /* here we try to find the next available valid timecode.  The dropped
     * (when they exist) frames in time codes are that the beginning of each
     * minute */
    gst_video_time_code_clear (out);
    gst_video_time_code_init (out, out_fps_n, out_fps_d,
        tc->config.latest_daily_jam, flags, tc->hours, tc->minutes,
        tc->seconds, output_frame, tc->field_count);
    output_frame++;
  } while ((flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) != 0
      && output_frame < 10 && !gst_video_time_code_is_valid (out));

  tc_str = gst_video_time_code_to_string (out);
  GST_TRACE_OBJECT (self, "interpolated to %s", tc_str);
  g_free (tc_str);

  return TRUE;
}

static gboolean
can_take_buffer (GstCCConverter * self,
    const struct cdp_fps_entry *in_fps_entry,
    const struct cdp_fps_entry *out_fps_entry,
    const GstVideoTimeCode * in_tc, GstVideoTimeCode * out_tc)
{
  int input_frame_n, input_frame_d, output_frame_n, output_frame_d;
  int output_time_cmp, scale_n, scale_d;

  /* TODO: handle input discont */

  if (self->in_fps_n == 0) {
    input_frame_n = self->input_frames;
    input_frame_d = 1;
  } else {
    /* compute the relative frame count for each */
    if (!gst_util_fraction_multiply (self->in_fps_d, self->in_fps_n,
            self->input_frames, 1, &input_frame_n, &input_frame_d))
      /* we should never overflow */
      g_assert_not_reached ();
  }

  if (self->in_fps_n == 0) {
    output_frame_n = self->output_frames;
    output_frame_d = 1;
  } else {
    if (!gst_util_fraction_multiply (self->out_fps_d, self->out_fps_n,
            self->output_frames, 1, &output_frame_n, &output_frame_d))
      /* we should never overflow */
      g_assert_not_reached ();
  }

  output_time_cmp = gst_util_fraction_compare (input_frame_n, input_frame_d,
      output_frame_n, output_frame_d);

  if (output_time_cmp == 0) {
    self->output_frames = 0;
    self->input_frames = 0;
  }

  in_fps_entry = cdp_fps_entry_from_fps (self->in_fps_n, self->in_fps_d);
  if (!in_fps_entry || in_fps_entry->fps_n == 0)
    g_assert_not_reached ();

  /* compute the relative rates of the two framerates */
  get_framerate_output_scale (self, in_fps_entry, &scale_n, &scale_d);

  GST_TRACE_OBJECT (self, "performing conversion at scale %d/%d, "
      "time comparison %i", scale_n, scale_d, output_time_cmp);

  if (output_time_cmp < 0) {
    /* we can't generate an output yet */
    return FALSE;
  } else {
    interpolate_time_code_with_framerate (self, in_tc, out_fps_entry->fps_n,
        out_fps_entry->fps_d, scale_n, scale_d, out_tc);
    return TRUE;
  }
}

static guint
convert_cea708_cc_data_cea708_cdp_internal (GstCCConverter * self,
    const guint8 * cc_data, guint cc_data_len, guint8 * cdp, guint cdp_len,
    const GstVideoTimeCode * tc, const struct cdp_fps_entry *fps_entry)
{
  guint ret;

  ret = convert_cea708_cc_data_to_cdp (GST_OBJECT (self),
      (GstCCCDPMode) self->cdp_mode, self->cdp_hdr_sequence_cntr, cc_data,
      cc_data_len, cdp, cdp_len, tc, fps_entry);
  self->cdp_hdr_sequence_cntr++;

  return ret;
}

static gboolean
push_cdp_buffer (GstCCConverter * self, GstBuffer * inbuf,
    GstVideoTimeCode * out_tc, const struct cdp_fps_entry **in_fps_entry)
{
  guint8 cc_data[MAX_CDP_PACKET_LEN];
  guint cc_data_len = 0;
  GstMapInfo in;

  if (inbuf) {
    gst_buffer_map (inbuf, &in, GST_MAP_READ);

    cc_data_len =
        convert_cea708_cdp_to_cc_data (GST_OBJECT (self), in.data, in.size,
        cc_data, out_tc, in_fps_entry);

    cc_buffer_push_cc_data (self->cc_buffer, cc_data, cc_data_len);

    gst_buffer_unmap (inbuf, &in);
    self->input_frames++;
  }

  return TRUE;
}

static GstFlowReturn
convert_cea608_raw_cea608_s334_1a (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo in, out;
  guint i, n;

  n = gst_buffer_get_size (inbuf);
  if (n & 1) {
    GST_WARNING_OBJECT (self, "Invalid raw CEA608 buffer size");
    gst_buffer_set_size (outbuf, 0);
    return GST_FLOW_OK;
  }

  n /= 2;

  if (n > 3) {
    GST_WARNING_OBJECT (self, "Too many CEA608 pairs %u.  Truncating to %u", n,
        3);
    n = 3;
  }

  gst_buffer_set_size (outbuf, 3 * n);

  gst_buffer_map (inbuf, &in, GST_MAP_READ);
  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);

  /* We don't know from which line offset it originally is */
  for (i = 0; i < n; i++) {
    out.data[i * 3] = self->in_field == 0 ? 0x80 : 0x00;
    out.data[i * 3 + 1] = in.data[i * 2];
    out.data[i * 3 + 2] = in.data[i * 2 + 1];
  }

  gst_buffer_unmap (inbuf, &in);
  gst_buffer_unmap (outbuf, &out);

  return GST_FLOW_OK;
}

static inline guint8
eia608_parity_strip (guint8 cc_data)
{
  return cc_data & 0x7F;
}

static GstFlowReturn
convert_cea608_raw_cea608_raw (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo in, out;
  guint i, n;

  g_assert (self->in_field != self->out_field);

  n = gst_buffer_get_size (inbuf);
  if (n & 1) {
    GST_WARNING_OBJECT (self, "Invalid raw CEA608 buffer size");
    gst_buffer_set_size (outbuf, 0);
    return GST_FLOW_OK;
  }

  n /= 2;

  if (n > 3) {
    GST_WARNING_OBJECT (self, "Too many CEA608 pairs %u.  Truncating to %u", n,
        3);
    n = 3;
  }

  gst_buffer_set_size (outbuf, 2 * n);

  gst_buffer_map (inbuf, &in, GST_MAP_READ);
  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);

  /* EIA/CEA-608-B 8.4 Closed Caption Mode:
   *
   * When closed captioning is used on line 21, field 2,
   * it shall conform to all of the applicable specifications and
   * recommended practices as defined for field 1 services with the
   * following differences:
   *
   * a) The non-printing character of the miscellaneous control-character pairs
   * that fall in the range of 14h, 20h to 14h, 2Fh in field 1, shall be
   * replaced with 15h, 20h to 15h, 2Fh when used in field 2.
   *
   * b) The non-printing character of the miscellaneous control-character pairs
   * that fall in the range of 1Ch, 20h to 1Ch, 2Fh in field 1, shall be replaced
   * with 1Dh, 20h to 1Dh, 2Fh when used in field 2.
   */

  for (i = 0; i < n; i++) {
    guint8 cc_data_1 = eia608_parity_strip (in.data[i * 2]);
    guint8 cc_data_2 = eia608_parity_strip (in.data[i * 2 + 1]);

    out.data[i * 2] = in.data[i * 2];
    out.data[i * 2 + 1] = in.data[i * 2 + 1];

    if (self->in_field == 0 && self->out_field == 1) {
      if (cc_data_1 == 0x14 && cc_data_2 >= 0x20 && cc_data_2 <= 0x2f) {
        out.data[i * 2] = 0x15;
      } else if (cc_data_1 == 0x1c && cc_data_2 >= 0x20 && cc_data_2 <= 0x2f) {
        out.data[i * 2] = 0x9d;
      }
    } else if (self->in_field == 1 && self->out_field == 0) {
      if (cc_data_1 == 0x15 && cc_data_2 >= 0x20 && cc_data_2 <= 0x2f) {
        out.data[i * 2] = 0x94;
      } else if (cc_data_1 == 0x1d && cc_data_2 >= 0x20 && cc_data_2 <= 0x2f) {
        out.data[i * 2] = 0x1c;
      }
    }
  }

  gst_buffer_unmap (inbuf, &in);
  gst_buffer_unmap (outbuf, &out);

  return GST_FLOW_OK;
}

static GstFlowReturn
convert_cea608_raw_cea708_cc_data (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo in, out;
  guint i, n;

  n = gst_buffer_get_size (inbuf);
  if (n & 1) {
    GST_WARNING_OBJECT (self, "Invalid raw CEA608 buffer size");
    gst_buffer_set_size (outbuf, 0);
    return GST_FLOW_OK;
  }

  n /= 2;

  if (n > 3) {
    GST_WARNING_OBJECT (self, "Too many CEA608 pairs %u. Truncating to %u", n,
        3);
    n = 3;
  }

  gst_buffer_set_size (outbuf, 3 * n);

  gst_buffer_map (inbuf, &in, GST_MAP_READ);
  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);

  /* We don't know from which line offset it originally is */
  for (i = 0; i < n; i++) {
    out.data[i * 3] = self->in_field == 0 ? 0xfc : 0xfd;
    out.data[i * 3 + 1] = in.data[i * 2];
    out.data[i * 3 + 2] = in.data[i * 2 + 1];
  }

  gst_buffer_unmap (inbuf, &in);
  gst_buffer_unmap (outbuf, &out);

  return GST_FLOW_OK;
}

static GstFlowReturn
convert_cea608_raw_cea708_cdp (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf, const GstVideoTimeCodeMeta * tc_meta)
{
  GstMapInfo in, out;
  const struct cdp_fps_entry *in_fps_entry, *out_fps_entry;
  guint cc_data_len = MAX_CDP_PACKET_LEN;
  guint8 cc_data[MAX_CDP_PACKET_LEN];

  in_fps_entry = cdp_fps_entry_from_fps (self->in_fps_n, self->in_fps_d);
  if (!in_fps_entry || in_fps_entry->fps_n == 0)
    g_assert_not_reached ();

  if (inbuf) {
    guint n = 0;

    n = gst_buffer_get_size (inbuf);
    if (n & 1) {
      GST_WARNING_OBJECT (self, "Invalid raw CEA608 buffer size");
      gst_buffer_set_size (outbuf, 0);
      return GST_FLOW_OK;
    }

    n /= 2;

    if (n > in_fps_entry->max_cea608_count) {
      GST_WARNING_OBJECT (self, "Too many CEA608 pairs %u. Truncating to %u",
          n, in_fps_entry->max_cea608_count);
      n = in_fps_entry->max_cea608_count;
    }

    gst_buffer_map (inbuf, &in, GST_MAP_READ);
    if (self->in_field == 0) {
      cc_buffer_push_separated (self->cc_buffer, in.data, in.size, NULL, 0,
          NULL, 0);
    } else {
      cc_buffer_push_separated (self->cc_buffer, NULL, 0, in.data, in.size,
          NULL, 0);
    }
    gst_buffer_unmap (inbuf, &in);
    self->input_frames++;
  }

  out_fps_entry = cdp_fps_entry_from_fps (self->out_fps_n, self->out_fps_d);
  if (!out_fps_entry || out_fps_entry->fps_n == 0)
    g_assert_not_reached ();

  if (!can_take_buffer (self, in_fps_entry, out_fps_entry,
          tc_meta ? &tc_meta->tc : NULL, &self->current_output_timecode))
    goto drop;

  cc_buffer_take_cc_data (self->cc_buffer, out_fps_entry, TRUE, cc_data,
      &cc_data_len);

  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);
  cc_data_len =
      convert_cea708_cc_data_cea708_cdp_internal (self, cc_data, cc_data_len,
      out.data, out.size, &self->current_output_timecode, out_fps_entry);
  self->output_frames++;
  gst_buffer_unmap (outbuf, &out);

out:
  gst_buffer_set_size (outbuf, cc_data_len);

  return GST_FLOW_OK;

drop:
  cc_data_len = 0;
  goto out;
}

static GstFlowReturn
convert_cea608_s334_1a_cea608_raw (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo in, out;
  guint i, n;
  guint cea608 = 0;

  n = gst_buffer_get_size (inbuf);
  if (n % 3 != 0) {
    GST_WARNING_OBJECT (self, "Invalid S334-1A CEA608 buffer size");
    n = n - (n % 3);
  }

  n /= 3;

  if (n > 3) {
    GST_WARNING_OBJECT (self, "Too many S334-1A CEA608 triplets %u", n);
    n = 3;
  }

  gst_buffer_map (inbuf, &in, GST_MAP_READ);
  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);

  for (i = 0; i < n; i++) {
    if ((in.data[i * 3] & 0x80 && self->out_field == 0) ||
        (!(in.data[i * 3] & 0x80) && self->out_field == 1)) {
      out.data[cea608 * 2] = in.data[i * 3 + 1];
      out.data[cea608 * 2 + 1] = in.data[i * 3 + 2];
      cea608++;
    }
  }

  gst_buffer_unmap (inbuf, &in);
  gst_buffer_unmap (outbuf, &out);

  gst_buffer_set_size (outbuf, 2 * cea608);

  return GST_FLOW_OK;
}

static GstFlowReturn
convert_cea608_s334_1a_cea708_cc_data (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo in, out;
  guint i, n;

  n = gst_buffer_get_size (inbuf);
  if (n % 3 != 0) {
    GST_WARNING_OBJECT (self, "Invalid S334-1A CEA608 buffer size");
    n = n - (n % 3);
  }

  n /= 3;

  if (n > 3) {
    GST_WARNING_OBJECT (self, "Too many S334-1A CEA608 triplets %u", n);
    n = 3;
  }

  gst_buffer_set_size (outbuf, 3 * n);

  gst_buffer_map (inbuf, &in, GST_MAP_READ);
  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);

  for (i = 0; i < n; i++) {
    out.data[i * 3] = (in.data[i * 3] & 0x80) ? 0xfc : 0xfd;
    out.data[i * 3 + 1] = in.data[i * 3 + 1];
    out.data[i * 3 + 2] = in.data[i * 3 + 2];
  }

  gst_buffer_unmap (inbuf, &in);
  gst_buffer_unmap (outbuf, &out);

  return GST_FLOW_OK;
}

static GstFlowReturn
convert_cea608_s334_1a_cea708_cdp (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf, const GstVideoTimeCodeMeta * tc_meta)
{
  GstMapInfo in, out;
  const struct cdp_fps_entry *in_fps_entry, *out_fps_entry;
  guint cc_data_len = MAX_CDP_PACKET_LEN;
  guint cea608_1_len = 0, cea608_2_len = 0;
  guint8 cc_data[MAX_CDP_PACKET_LEN];
  guint8 cea608_1[MAX_CEA608_LEN], cea608_2[MAX_CEA608_LEN];
  guint i, n;

  in_fps_entry = cdp_fps_entry_from_fps (self->in_fps_n, self->in_fps_d);
  if (!in_fps_entry || in_fps_entry->fps_n == 0)
    g_assert_not_reached ();

  if (inbuf) {
    n = gst_buffer_get_size (inbuf);
    if (n % 3 != 0) {
      GST_WARNING_OBJECT (self, "Invalid S334-1A CEA608 buffer size");
      n = n - (n % 3);
    }

    n /= 3;

    if (n > in_fps_entry->max_cea608_count) {
      GST_WARNING_OBJECT (self, "Too many S334-1A CEA608 triplets %u", n);
      n = in_fps_entry->max_cea608_count;
    }

    gst_buffer_map (inbuf, &in, GST_MAP_READ);

    for (i = 0; i < n; i++) {
      guint byte1 = in.data[i * 3 + 1];
      guint byte2 = in.data[i * 3 + 2];

      if (in.data[i * 3] & 0x80) {
        if (byte1 != 0x80 || byte2 != 0x80) {
          cea608_1[cea608_1_len++] = byte1;
          cea608_1[cea608_1_len++] = byte2;
        }
      } else {
        if (byte1 != 0x80 || byte2 != 0x80) {
          cea608_2[cea608_2_len++] = byte1;
          cea608_2[cea608_2_len++] = byte2;
        }
      }
    }
    gst_buffer_unmap (inbuf, &in);

    cc_buffer_push_separated (self->cc_buffer, cea608_1, cea608_1_len,
        cea608_2, cea608_2_len, NULL, 0);
    self->input_frames++;
  }

  out_fps_entry = cdp_fps_entry_from_fps (self->out_fps_n, self->out_fps_d);
  if (!out_fps_entry || out_fps_entry->fps_n == 0)
    g_assert_not_reached ();

  if (!can_take_buffer (self, in_fps_entry, out_fps_entry,
          tc_meta ? &tc_meta->tc : NULL, &self->current_output_timecode))
    goto drop;

  cc_buffer_take_cc_data (self->cc_buffer, out_fps_entry, TRUE, cc_data,
      &cc_data_len);

  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);
  cc_data_len =
      convert_cea708_cc_data_cea708_cdp_internal (self, cc_data, cc_data_len,
      out.data, out.size, &self->current_output_timecode, out_fps_entry);
  self->output_frames++;
  gst_buffer_unmap (outbuf, &out);

out:
  gst_buffer_set_size (outbuf, cc_data_len);

  return GST_FLOW_OK;

drop:
  cc_data_len = 0;
  goto out;
}

static GstFlowReturn
convert_cea708_cc_data_cea608_raw (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo in, out;
  guint i, n;
  guint cea608 = 0;

  n = gst_buffer_get_size (inbuf);
  if (n % 3 != 0) {
    GST_WARNING_OBJECT (self, "Invalid raw CEA708 buffer size");
    n = n - (n % 3);
  }

  n /= 3;

  if (n > 25) {
    GST_WARNING_OBJECT (self, "Too many CEA708 triplets %u", n);
    n = 25;
  }

  gst_buffer_map (inbuf, &in, GST_MAP_READ);
  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);

  for (i = 0; i < n; i++) {
    if ((in.data[i * 3] == 0xfc && self->out_field == 0) ||
        (in.data[i * 3] == 0xfd && self->out_field == 1)) {
      out.data[cea608 * 2] = in.data[i * 3 + 1];
      out.data[cea608 * 2 + 1] = in.data[i * 3 + 2];
      cea608++;
    }
  }

  gst_buffer_unmap (inbuf, &in);
  gst_buffer_unmap (outbuf, &out);

  gst_buffer_set_size (outbuf, 2 * cea608);

  return GST_FLOW_OK;
}

static GstFlowReturn
convert_cea708_cc_data_cea608_s334_1a (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo in, out;
  guint i, n;
  guint cea608 = 0;

  n = gst_buffer_get_size (inbuf);
  if (n % 3 != 0) {
    GST_WARNING_OBJECT (self, "Invalid raw CEA708 buffer size");
    n = n - (n % 3);
  }

  n /= 3;

  if (n > 25) {
    GST_WARNING_OBJECT (self, "Too many CEA708 triplets %u", n);
    n = 25;
  }

  gst_buffer_map (inbuf, &in, GST_MAP_READ);
  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);

  for (i = 0; i < n; i++) {
    if (in.data[i * 3] == 0xfc || in.data[i * 3] == 0xfd) {
      /* We have to assume a line offset of 0 */
      out.data[cea608 * 3] = in.data[i * 3] == 0xfc ? 0x80 : 0x00;
      out.data[cea608 * 3 + 1] = in.data[i * 3 + 1];
      out.data[cea608 * 3 + 2] = in.data[i * 3 + 2];
      cea608++;
    }
  }

  gst_buffer_unmap (inbuf, &in);
  gst_buffer_unmap (outbuf, &out);

  gst_buffer_set_size (outbuf, 3 * cea608);

  return GST_FLOW_OK;
}

static GstFlowReturn
convert_cea708_cc_data_cea708_cdp (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf, const GstVideoTimeCodeMeta * tc_meta)
{
  GstMapInfo in, out;
  const struct cdp_fps_entry *in_fps_entry, *out_fps_entry;
  guint in_cc_data_len;
  guint cc_data_len = MAX_CDP_PACKET_LEN;
  guint8 cc_data[MAX_CDP_PACKET_LEN];
  guint8 *in_cc_data;

  if (inbuf) {
    gst_buffer_map (inbuf, &in, GST_MAP_READ);
    in_cc_data = in.data;
    in_cc_data_len = in.size;
    self->input_frames++;
  } else {
    in_cc_data = NULL;
    in_cc_data_len = 0;
  }

  in_fps_entry = cdp_fps_entry_from_fps (self->in_fps_n, self->in_fps_d);
  if (!in_fps_entry || in_fps_entry->fps_n == 0)
    g_assert_not_reached ();

  out_fps_entry = cdp_fps_entry_from_fps (self->out_fps_n, self->out_fps_d);
  if (!out_fps_entry || out_fps_entry->fps_n == 0)
    g_assert_not_reached ();

  cc_buffer_push_cc_data (self->cc_buffer, in_cc_data, in_cc_data_len);
  if (inbuf)
    gst_buffer_unmap (inbuf, &in);

  if (!can_take_buffer (self, in_fps_entry, out_fps_entry,
          tc_meta ? &tc_meta->tc : NULL, &self->current_output_timecode))
    goto drop;

  cc_buffer_take_cc_data (self->cc_buffer, out_fps_entry, TRUE, cc_data,
      &cc_data_len);

  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);
  cc_data_len =
      convert_cea708_cc_data_cea708_cdp_internal (self, cc_data, cc_data_len,
      out.data, out.size, &self->current_output_timecode, out_fps_entry);
  self->output_frames++;
  gst_buffer_unmap (outbuf, &out);

out:
  gst_buffer_set_size (outbuf, cc_data_len);

  return GST_FLOW_OK;

drop:
  cc_data_len = 0;
  goto out;
}

static GstFlowReturn
convert_cea708_cdp_cea608_raw (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf, const GstVideoTimeCodeMeta * tc_meta)
{
  GstMapInfo out;
  GstVideoTimeCode tc = GST_VIDEO_TIME_CODE_INIT;
  guint cea608_len;
  const struct cdp_fps_entry *in_fps_entry = NULL, *out_fps_entry;

  if (!push_cdp_buffer (self, inbuf, &tc, &in_fps_entry)) {
    gst_buffer_set_size (outbuf, 0);
    return GST_FLOW_OK;
  }

  out_fps_entry = cdp_fps_entry_from_fps (self->out_fps_n, self->out_fps_d);
  if (!out_fps_entry || out_fps_entry->fps_n == 0)
    out_fps_entry = in_fps_entry;

  if (!can_take_buffer (self, in_fps_entry, out_fps_entry, &tc,
          &self->current_output_timecode))
    goto drop;

  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);
  cea608_len = out.size;
  if (self->out_field == 0) {
    cc_buffer_take_separated (self->cc_buffer, out_fps_entry, out.data,
        &cea608_len, NULL, 0, NULL, 0);
  } else {
    cc_buffer_take_separated (self->cc_buffer, out_fps_entry, NULL, 0, out.data,
        &cea608_len, NULL, 0);
  }
  gst_buffer_unmap (outbuf, &out);
  self->output_frames++;

  if (self->current_output_timecode.config.fps_n != 0 && !tc_meta) {
    gst_buffer_add_video_time_code_meta (outbuf,
        &self->current_output_timecode);
    gst_video_time_code_increment_frame (&self->current_output_timecode);
  }

out:
  gst_buffer_set_size (outbuf, cea608_len);
  return GST_FLOW_OK;

drop:
  cea608_len = 0;
  goto out;
}

static GstFlowReturn
convert_cea708_cdp_cea608_s334_1a (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf, const GstVideoTimeCodeMeta * tc_meta)
{
  GstMapInfo out;
  GstVideoTimeCode tc = GST_VIDEO_TIME_CODE_INIT;
  const struct cdp_fps_entry *in_fps_entry = NULL, *out_fps_entry;
  guint cc_data_len;
  int s334_len;
  guint i;

  if (!push_cdp_buffer (self, inbuf, &tc, &in_fps_entry))
    goto drop;

  out_fps_entry = cdp_fps_entry_from_fps (self->out_fps_n, self->out_fps_d);
  if (!out_fps_entry || out_fps_entry->fps_n == 0)
    out_fps_entry = in_fps_entry;

  if (!can_take_buffer (self, in_fps_entry, out_fps_entry, &tc,
          &self->current_output_timecode))
    goto drop;

  gst_buffer_map (outbuf, &out, GST_MAP_READWRITE);

  cc_data_len = out.size;
  cc_buffer_take_cc_data (self->cc_buffer, out_fps_entry, FALSE,
      out.data, &cc_data_len);
  s334_len = drop_ccp_from_cc_data (out.data, cc_data_len);
  if (s334_len < 0)
    goto drop;

  for (i = 0; i < s334_len / 3; i++) {
    guint byte = out.data[i * 3];
    /* We have to assume a line offset of 0 */
    out.data[i * 3] = (byte == 0xfc || byte == 0xf8) ? 0x80 : 0x00;
  }

  gst_buffer_unmap (outbuf, &out);
  self->output_frames++;

  gst_buffer_set_size (outbuf, s334_len);

  if (self->current_output_timecode.config.fps_n != 0 && !tc_meta) {
    gst_buffer_add_video_time_code_meta (outbuf,
        &self->current_output_timecode);
    gst_video_time_code_increment_frame (&self->current_output_timecode);
  }

  return GST_FLOW_OK;

drop:
  gst_buffer_set_size (outbuf, 0);
  return GST_FLOW_OK;
}

static GstFlowReturn
convert_cea708_cdp_cea708_cc_data (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf, const GstVideoTimeCodeMeta * tc_meta)
{
  GstMapInfo out;
  GstVideoTimeCode tc = GST_VIDEO_TIME_CODE_INIT;
  const struct cdp_fps_entry *in_fps_entry = NULL, *out_fps_entry;
  guint out_len = 0;

  if (!push_cdp_buffer (self, inbuf, &tc, &in_fps_entry))
    goto out;

  out_fps_entry = cdp_fps_entry_from_fps (self->out_fps_n, self->out_fps_d);
  if (!out_fps_entry || out_fps_entry->fps_n == 0)
    out_fps_entry = in_fps_entry;

  if (!can_take_buffer (self, in_fps_entry, out_fps_entry, &tc,
          &self->current_output_timecode))
    goto out;

  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);
  out_len = (guint) out.size;
  cc_buffer_take_cc_data (self->cc_buffer, out_fps_entry, TRUE, out.data,
      &out_len);

  gst_buffer_unmap (outbuf, &out);
  self->output_frames++;

  if (self->current_output_timecode.config.fps_n != 0 && !tc_meta) {
    gst_buffer_add_video_time_code_meta (outbuf,
        &self->current_output_timecode);
    gst_video_time_code_increment_frame (&self->current_output_timecode);
  }

out:
  gst_buffer_set_size (outbuf, out_len);

  return GST_FLOW_OK;
}

static GstFlowReturn
convert_cea708_cdp_cea708_cdp (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo out;
  GstVideoTimeCode tc = GST_VIDEO_TIME_CODE_INIT;
  const struct cdp_fps_entry *in_fps_entry = NULL, *out_fps_entry;
  guint8 cc_data[MAX_CDP_PACKET_LEN];
  guint cc_data_len = MAX_CDP_PACKET_LEN;
  guint out_len = 0;

  if (!push_cdp_buffer (self, inbuf, &tc, &in_fps_entry))
    goto out;

  out_fps_entry = cdp_fps_entry_from_fps (self->out_fps_n, self->out_fps_d);
  if (!out_fps_entry || out_fps_entry->fps_n == 0)
    out_fps_entry = in_fps_entry;

  if (!can_take_buffer (self, in_fps_entry, out_fps_entry, &tc,
          &self->current_output_timecode))
    goto out;

  cc_buffer_take_cc_data (self->cc_buffer, out_fps_entry, TRUE, cc_data,
      &cc_data_len);

  gst_buffer_map (outbuf, &out, GST_MAP_WRITE);
  out_len =
      convert_cea708_cc_data_cea708_cdp_internal (self, cc_data, cc_data_len,
      out.data, out.size, &self->current_output_timecode, out_fps_entry);

  gst_buffer_unmap (outbuf, &out);
  self->output_frames++;

out:
  gst_buffer_set_size (outbuf, out_len);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_cc_converter_transform (GstCCConverter * self, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVideoTimeCodeMeta *tc_meta = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (self, "Converting %" GST_PTR_FORMAT " from %u to %u", inbuf,
      self->input_caption_type, self->output_caption_type);

  if (inbuf)
    tc_meta = gst_buffer_get_video_time_code_meta (inbuf);

  if (tc_meta) {
    if (self->current_output_timecode.config.fps_n <= 0) {
      /* XXX: this assumes the input time codes are well-formed and increase
       * at the rate of one frame for each input buffer */
      const struct cdp_fps_entry *in_fps_entry;
      gint scale_n, scale_d;

      in_fps_entry = cdp_fps_entry_from_fps (self->in_fps_n, self->in_fps_d);
      if (!in_fps_entry || in_fps_entry->fps_n == 0)
        scale_n = scale_d = 1;
      else
        get_framerate_output_scale (self, in_fps_entry, &scale_n, &scale_d);

      interpolate_time_code_with_framerate (self, &tc_meta->tc,
          self->out_fps_n, self->out_fps_d, scale_n, scale_d,
          &self->current_output_timecode);
    }
  }

  switch (self->input_caption_type) {
    case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:

      switch (self->output_caption_type) {
        case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
          ret = convert_cea608_raw_cea608_s334_1a (self, inbuf, outbuf);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
          ret = convert_cea608_raw_cea708_cc_data (self, inbuf, outbuf);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
          ret = convert_cea608_raw_cea708_cdp (self, inbuf, outbuf, tc_meta);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
          ret = convert_cea608_raw_cea608_raw (self, inbuf, outbuf);
          break;
        default:
          g_assert_not_reached ();
          break;
      }

      break;
    case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:

      switch (self->output_caption_type) {
        case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
          ret = convert_cea608_s334_1a_cea608_raw (self, inbuf, outbuf);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
          ret = convert_cea608_s334_1a_cea708_cc_data (self, inbuf, outbuf);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
          ret =
              convert_cea608_s334_1a_cea708_cdp (self, inbuf, outbuf, tc_meta);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
        default:
          g_assert_not_reached ();
          break;
      }

      break;
    case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:

      switch (self->output_caption_type) {
        case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
          ret = convert_cea708_cc_data_cea608_raw (self, inbuf, outbuf);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
          ret = convert_cea708_cc_data_cea608_s334_1a (self, inbuf, outbuf);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
          ret =
              convert_cea708_cc_data_cea708_cdp (self, inbuf, outbuf, tc_meta);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
        default:
          g_assert_not_reached ();
          break;
      }

      break;
    case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:

      switch (self->output_caption_type) {
        case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
          ret = convert_cea708_cdp_cea608_raw (self, inbuf, outbuf, tc_meta);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
          ret =
              convert_cea708_cdp_cea608_s334_1a (self, inbuf, outbuf, tc_meta);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
          ret =
              convert_cea708_cdp_cea708_cc_data (self, inbuf, outbuf, tc_meta);
          break;
        case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
          ret = convert_cea708_cdp_cea708_cdp (self, inbuf, outbuf);
          break;
        default:
          g_assert_not_reached ();
          break;
      }

      break;
    default:
      g_assert_not_reached ();
      break;
  }

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (self, "returning %s", gst_flow_get_name (ret));
    return ret;
  }

  GST_DEBUG_OBJECT (self, "Converted to %" GST_PTR_FORMAT, outbuf);

  if (gst_buffer_get_size (outbuf) > 0) {
    if (self->current_output_timecode.config.fps_n > 0) {
      gst_buffer_add_video_time_code_meta (outbuf,
          &self->current_output_timecode);
      gst_video_time_code_increment_frame (&self->current_output_timecode);
    }

    return GST_FLOW_OK;
  } else {
    return GST_FLOW_OK;
  }
}

static gboolean
gst_cc_converter_transform_meta (GstBaseTransform * base, GstBuffer * outbuf,
    GstMeta * meta, GstBuffer * inbuf)
{
  const GstMetaInfo *info = meta->info;

  /* we do this manually for framerate scaling */
  if (info->api == GST_VIDEO_TIME_CODE_META_API_TYPE)
    return FALSE;

  return GST_BASE_TRANSFORM_CLASS (parent_class)->transform_meta (base, outbuf,
      meta, inbuf);
}

static gboolean
can_generate_output (GstCCConverter * self)
{
  int input_frame_n, input_frame_d, output_frame_n, output_frame_d;
  int output_time_cmp;

  if (self->in_fps_n == 0 || self->out_fps_n == 0)
    return FALSE;

  /* compute the relative frame count for each */
  if (!gst_util_fraction_multiply (self->in_fps_d, self->in_fps_n,
          self->input_frames, 1, &input_frame_n, &input_frame_d))
    /* we should never overflow */
    g_assert_not_reached ();

  if (!gst_util_fraction_multiply (self->out_fps_d, self->out_fps_n,
          self->output_frames, 1, &output_frame_n, &output_frame_d))
    /* we should never overflow */
    g_assert_not_reached ();

  output_time_cmp = gst_util_fraction_compare (input_frame_n, input_frame_d,
      output_frame_n, output_frame_d);

  if (output_time_cmp == 0) {
    self->output_frames = 0;
    self->input_frames = 0;
  }

  /* if the next output frame is at or before the current input frame */
  if (output_time_cmp >= 0)
    return TRUE;

  return FALSE;
}

static void
reset_counters (GstCCConverter * self)
{
  self->input_frames = 0;
  self->output_frames = 1;
  gst_video_time_code_clear (&self->current_output_timecode);
  gst_clear_buffer (&self->previous_buffer);
  cc_buffer_discard (self->cc_buffer);
}

static GstFlowReturn
drain_input (GstCCConverter * self)
{
  GstBaseTransformClass *bclass = GST_BASE_TRANSFORM_GET_CLASS (self);
  GstBaseTransform *trans = GST_BASE_TRANSFORM (self);
  GstFlowReturn ret = GST_FLOW_OK;
  guint cea608_1_len, cea608_2_len, ccp_len;

  cc_buffer_get_stored_size (self->cc_buffer, &cea608_1_len, &cea608_2_len,
      &ccp_len);

  while (ccp_len > 0 || cea608_1_len > 0 || cea608_2_len > 0
      || can_generate_output (self)) {
    GstBuffer *outbuf;

    if (!self->previous_buffer) {
      GST_WARNING_OBJECT (self, "Attempt to draining without a previous "
          "buffer.  Aborting");
      return GST_FLOW_OK;
    }

    outbuf = gst_buffer_new_allocate (NULL, MAX_CDP_PACKET_LEN, NULL);

    if (bclass->copy_metadata) {
      if (!bclass->copy_metadata (trans, self->previous_buffer, outbuf)) {
        /* something failed, post a warning */
        GST_ELEMENT_WARNING (self, STREAM, NOT_IMPLEMENTED,
            ("could not copy metadata"), (NULL));
      }
    }

    ret = gst_cc_converter_transform (self, NULL, outbuf);
    cc_buffer_get_stored_size (self->cc_buffer, &cea608_1_len, &cea608_2_len,
        &ccp_len);
    if (gst_buffer_get_size (outbuf) <= 0) {
      /* try to move the output along */
      self->input_frames++;
      gst_buffer_unref (outbuf);
      continue;
    } else if (ret != GST_FLOW_OK) {
      gst_buffer_unref (outbuf);
      return ret;
    }

    ret = gst_pad_push (GST_BASE_TRANSFORM_SRC_PAD (trans), outbuf);
    if (ret != GST_FLOW_OK) {
      return ret;
    }
  }

  return ret;
}

static GstFlowReturn
gst_cc_converter_generate_output (GstBaseTransform * base, GstBuffer ** outbuf)
{
  GstBaseTransformClass *bclass = GST_BASE_TRANSFORM_GET_CLASS (base);
  GstCCConverter *self = GST_CCCONVERTER (base);
  GstBuffer *inbuf = base->queued_buf;
  GstFlowReturn ret;

  *outbuf = NULL;
  base->queued_buf = NULL;
  if (!inbuf && !can_generate_output (self)) {
    return GST_FLOW_OK;
  }

  if (gst_base_transform_is_passthrough (base)) {
    *outbuf = inbuf;
    ret = GST_FLOW_OK;
  } else {
    if (inbuf && GST_BUFFER_IS_DISCONT (inbuf)) {
      ret = drain_input (self);
      reset_counters (self);
      if (ret != GST_FLOW_OK)
        return ret;
    }

    *outbuf = gst_buffer_new_allocate (NULL, MAX_CDP_PACKET_LEN, NULL);
    if (*outbuf == NULL)
      goto no_buffer;

    if (inbuf)
      gst_buffer_replace (&self->previous_buffer, inbuf);

    if (bclass->copy_metadata) {
      if (!bclass->copy_metadata (base, self->previous_buffer, *outbuf)) {
        /* something failed, post a warning */
        GST_ELEMENT_WARNING (self, STREAM, NOT_IMPLEMENTED,
            ("could not copy metadata"), (NULL));
      }
    }

    ret = gst_cc_converter_transform (self, inbuf, *outbuf);
    if (gst_buffer_get_size (*outbuf) <= 0) {
      gst_buffer_unref (*outbuf);
      *outbuf = NULL;

      if (inbuf && GST_BUFFER_PTS_IS_VALID (inbuf)) {
        GstClockTime dur = 0;
        GstEvent *gap;

        GST_TRACE_OBJECT (self, "Empty generated output for input %"
            GST_PTR_FORMAT, inbuf);

        if (GST_BUFFER_DURATION_IS_VALID (inbuf)) {
          dur = GST_BUFFER_DURATION (inbuf);
        } else if (self->in_fps_n > 0 && self->in_fps_d > 0) {
          dur = gst_util_uint64_scale (GST_SECOND,
              self->in_fps_d, self->in_fps_n);
        }

        gap = gst_event_new_gap (GST_BUFFER_PTS (inbuf), dur);
        gst_pad_push_event (GST_BASE_TRANSFORM_SRC_PAD (self), gap);
      }

      ret = GST_FLOW_OK;
    }

    if (inbuf)
      gst_buffer_unref (inbuf);
  }

  return ret;

no_buffer:
  {
    if (inbuf)
      gst_buffer_unref (inbuf);
    *outbuf = NULL;
    GST_WARNING_OBJECT (self, "could not allocate buffer");
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_cc_converter_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstCCConverter *self = GST_CCCONVERTER (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      GST_DEBUG_OBJECT (self, "received EOS");

      drain_input (self);

      /* fallthrough */
    case GST_EVENT_FLUSH_START:
      reset_counters (self);
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static gboolean
gst_cc_converter_start (GstBaseTransform * base)
{
  GstCCConverter *self = GST_CCCONVERTER (base);

  /* Resetting this is not really needed but makes debugging easier */
  self->cdp_hdr_sequence_cntr = 0;
  self->current_output_timecode = (GstVideoTimeCode) GST_VIDEO_TIME_CODE_INIT;
  reset_counters (self);

  return TRUE;
}

static gboolean
gst_cc_converter_stop (GstBaseTransform * base)
{
  GstCCConverter *self = GST_CCCONVERTER (base);

  gst_video_time_code_clear (&self->current_output_timecode);
  gst_clear_buffer (&self->previous_buffer);

  return TRUE;
}

static void
gst_cc_converter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCCConverter *filter = GST_CCCONVERTER (object);

  switch (prop_id) {
    case PROP_CDP_MODE:
      filter->cdp_mode = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cc_converter_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstCCConverter *filter = GST_CCCONVERTER (object);

  switch (prop_id) {
    case PROP_CDP_MODE:
      g_value_set_flags (value, filter->cdp_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cc_converter_finalize (GObject * object)
{
  GstCCConverter *self = GST_CCCONVERTER (object);

  gst_clear_object (&self->cc_buffer);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_cc_converter_class_init (GstCCConverterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *basetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  basetransform_class = (GstBaseTransformClass *) klass;

  gobject_class->set_property = gst_cc_converter_set_property;
  gobject_class->get_property = gst_cc_converter_get_property;
  gobject_class->finalize = gst_cc_converter_finalize;

  /**
   * GstCCConverter:cdp-mode
   *
   * Only insert the selection sections into CEA 708 CDP packets.
   *
   * Various software does not handle any other information than CC data
   * contained in CDP packets and might fail parsing the packets otherwise.
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_CDP_MODE, g_param_spec_flags ("cdp-mode",
          "CDP Mode",
          "Select which CDP sections to store in CDP packets",
          GST_TYPE_CC_CONVERTER_CDP_MODE, DEFAULT_CDP_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "Closed Caption Converter",
      "Filter/ClosedCaption",
      "Converts Closed Captions between different formats",
      "Sebastian Dröge <sebastian@centricular.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);
  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  basetransform_class->start = GST_DEBUG_FUNCPTR (gst_cc_converter_start);
  basetransform_class->stop = GST_DEBUG_FUNCPTR (gst_cc_converter_stop);
  basetransform_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_cc_converter_sink_event);
  basetransform_class->transform_size =
      GST_DEBUG_FUNCPTR (gst_cc_converter_transform_size);
  basetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_cc_converter_transform_caps);
  basetransform_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_cc_converter_fixate_caps);
  basetransform_class->set_caps = GST_DEBUG_FUNCPTR (gst_cc_converter_set_caps);
  basetransform_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_cc_converter_transform_meta);
  basetransform_class->generate_output =
      GST_DEBUG_FUNCPTR (gst_cc_converter_generate_output);
  basetransform_class->passthrough_on_same_caps = TRUE;

  GST_DEBUG_CATEGORY_INIT (gst_cc_converter_debug, "ccconverter",
      0, "Closed Caption converter");

  gst_type_mark_as_plugin_api (GST_TYPE_CC_CONVERTER_CDP_MODE, 0);
}

static void
gst_cc_converter_init (GstCCConverter * self)
{
  self->cdp_mode = DEFAULT_CDP_MODE;
  self->in_field = 0;
  self->out_field = 0;
  self->cc_buffer = cc_buffer_new ();
  cc_buffer_set_output_padding (self->cc_buffer, TRUE, FALSE);
}

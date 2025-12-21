/* GStreamer
 * Copyright (C) 2023 Carlos Rafael Giani <crg7475@mailbox.org>
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

#include "gstdsdconvert.h"


/**
 * SECTION:element-dsdconvert
 *
 * Dsdconvert converts between DSD grouping formats and byte reversals.
 * See #GstDsdInfo and @gst_dsd_convert for details about the conversion.
 * Neither the DSD rate nor the channel count can be changed; this only
 * converts the grouping format.
 *
 * Since: 1.24
 */


GST_DEBUG_CATEGORY_STATIC (dsd_convert_debug);
#define GST_CAT_DEFAULT dsd_convert_debug

#define STATIC_CAPS \
    GST_STATIC_CAPS (GST_DSD_CAPS_MAKE (GST_DSD_FORMATS_ALL))

static GstStaticPadTemplate static_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    STATIC_CAPS);

static GstStaticPadTemplate static_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    STATIC_CAPS);

struct _GstDsdConvert
{
  GstBaseTransform parent;

  GstDsdInfo in_info;
  GstDsdInfo out_info;

  GstAdapter *input_adapter;
};

#define gst_dsd_convert_parent_class parent_class
G_DEFINE_TYPE (GstDsdConvert, gst_dsd_convert, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (dsdconvert, "dsdconvert",
    GST_RANK_SECONDARY, GST_TYPE_DSD_CONVERT);

static gboolean gst_dsd_convert_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_dsd_convert_prepare_output_buffer (GstBaseTransform *
    trans, GstBuffer * input, GstBuffer ** outbuf);
static GstCaps *gst_dsd_convert_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_dsd_convert_transform_size (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize);
static GstFlowReturn gst_dsd_convert_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);

static void
gst_dsd_convert_class_init (GstDsdConvertClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *basetransform_class = GST_BASE_TRANSFORM_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (dsd_convert_debug, "dsdconvert", 0,
      "DSD grouping format converter");

  gst_element_class_add_static_pad_template (element_class,
      &static_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &static_src_template);

  basetransform_class->set_caps = GST_DEBUG_FUNCPTR (gst_dsd_convert_set_caps);
  basetransform_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_dsd_convert_prepare_output_buffer);
  basetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_dsd_convert_transform_caps);
  basetransform_class->transform_size =
      GST_DEBUG_FUNCPTR (gst_dsd_convert_transform_size);
  basetransform_class->transform =
      GST_DEBUG_FUNCPTR (gst_dsd_convert_transform);

  gst_element_class_set_static_metadata (element_class, "DSD converter",
      "Filter/Converter/Audio",
      "Convert between different DSD grouping formats",
      "Carlos Rafael Giani <crg7475@mailbox.org>");
}

static void
gst_dsd_convert_init (G_GNUC_UNUSED GstDsdConvert * self)
{
}

static gboolean
gst_dsd_convert_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps)
{
  GstDsdConvert *self = GST_DSD_CONVERT (base);
  gboolean can_passthrough;

  if (!gst_dsd_info_from_caps (&self->in_info, incaps))
    goto invalid_in;
  if (!gst_dsd_info_from_caps (&self->out_info, outcaps))
    goto invalid_out;

  can_passthrough = gst_dsd_info_is_equal (&self->in_info, &self->out_info);
  gst_base_transform_set_passthrough (base, can_passthrough);

  return TRUE;

invalid_in:
  {
    GST_ERROR_OBJECT (base, "invalid input caps");
    return FALSE;
  }
invalid_out:
  {
    GST_ERROR_OBJECT (base, "invalid output caps");
    return FALSE;
  }
}

static GstFlowReturn
gst_dsd_convert_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * input, GstBuffer ** outbuf)
{
  GstFlowReturn flow_ret;
  GstDsdConvert *self = GST_DSD_CONVERT_CAST (trans);

  /* The point of this prepare_buffer override is to add the plane
   * offset meta if the outgoing data uses a non-interleaved layout. */

  flow_ret =
      GST_BASE_TRANSFORM_CLASS (parent_class)->prepare_output_buffer (trans,
      input, outbuf);
  if (flow_ret != GST_FLOW_OK)
    return flow_ret;

  if (GST_DSD_INFO_LAYOUT (&self->out_info) == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    g_assert (*outbuf != NULL);

    GST_LOG_OBJECT (trans, "adding dsd plane offset meta to output buffer");

    /* Add the meta, with num_channels set to 0 and offsets to NULL. That's
     * because we do not yet know these quantities - they need to instead
     * be set in gst_dsd_convert_transform(). */
    gst_buffer_add_dsd_plane_offset_meta (*outbuf,
        GST_DSD_INFO_CHANNELS (&self->out_info), 0, NULL);
  }

  return GST_FLOW_OK;
}

static gboolean
remove_format_from_structure (GstCapsFeatures * features,
    GstStructure * structure, gpointer user_data G_GNUC_UNUSED)
{
  gst_structure_remove_fields (structure, "format", "layout",
      "reversed-bytes", NULL);
  return TRUE;
}

static GstCaps *
gst_dsd_convert_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *tmp, *tmp2, *template_caps;
  GstCaps *result;

  tmp = gst_caps_copy (caps);

  /* Remove any existing format, layout, reversed-bytes fields. */
  gst_caps_map_in_place (tmp, remove_format_from_structure, NULL);

  /* Then fill in the removed fields with those from the template caps. */
  template_caps = gst_static_pad_template_get_caps (&static_sink_template);
  tmp2 = gst_caps_intersect_full (tmp, template_caps, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (tmp);
  gst_caps_unref (template_caps);
  tmp = tmp2;

  if (filter) {
    tmp2 = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG_OBJECT (base, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, result);

  return result;
}

static gboolean
gst_dsd_convert_transform_size (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, gsize size,
    GstCaps * othercaps, gsize * othersize)
{
  GstDsdConvert *self = (GstDsdConvert *) (base);
  GstDsdInfo info;
  GstDsdInfo otherinfo;
  guint width, otherwidth, maxwidth;

  g_return_val_if_fail (caps != NULL, FALSE);
  g_return_val_if_fail (othercaps != NULL, FALSE);
  g_return_val_if_fail (othersize != NULL, FALSE);

  if (!gst_dsd_info_from_caps (&info, caps))
    goto invalid_caps;

  if (!gst_dsd_info_from_caps (&otherinfo, othercaps))
    goto invalid_othercaps;

  width = gst_dsd_format_get_width (GST_DSD_INFO_FORMAT (&info));
  otherwidth = gst_dsd_format_get_width (GST_DSD_INFO_FORMAT (&otherinfo));
  maxwidth = MAX (width, otherwidth);

  *othersize = (size / maxwidth) * maxwidth;

  GST_LOG_OBJECT (self, "transformed size %" G_GSIZE_FORMAT " to othersize %"
      G_GSIZE_FORMAT "; width: %u otherwidth: %u", size, *othersize, width,
      otherwidth);

  return TRUE;

invalid_caps:
  {
    GST_INFO_OBJECT (base, "failed to parse caps to transform size");
    return FALSE;
  }

invalid_othercaps:
  {
    GST_INFO_OBJECT (base, "failed to parse othercaps to transform size");
    return FALSE;
  }
}

static GstFlowReturn
gst_dsd_convert_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstDsdConvert *self = GST_DSD_CONVERT (base);
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstMapInfo in_map_info, out_map_info;
  gboolean inbuf_mapped = FALSE;
  gboolean outbuf_mapped = FALSE;
  gboolean need_to_reverse_bytes;
  const gsize *input_plane_offsets = NULL;
  const gsize *output_plane_offsets = NULL;
  GstDsdPlaneOffsetMeta *in_dsd_plane_ofs_meta = NULL;
  GstDsdPlaneOffsetMeta *out_dsd_plane_ofs_meta = NULL;
  gint num_channels;
  gsize num_dsd_bytes = 0;

  g_return_val_if_fail (inbuf != NULL, GST_FLOW_ERROR);
  g_return_val_if_fail (outbuf != NULL, GST_FLOW_ERROR);

  GST_LOG_OBJECT (self, "about to transform input buffer %" GST_PTR_FORMAT
      "; output buffer size: %" G_GSIZE_FORMAT, inbuf,
      gst_buffer_get_size (outbuf));

  num_channels = GST_DSD_INFO_CHANNELS (&self->in_info);

  /* Get the plane offset metas if the audio layouts are non-interleaved.
   * Some of the quantities necessary for converting from/to non-interleaved
   * data are known only later, which is why not all of them are set here. */

  if (GST_DSD_INFO_LAYOUT (&self->in_info) == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    in_dsd_plane_ofs_meta = gst_buffer_get_dsd_plane_offset_meta (inbuf);
    if (G_UNLIKELY (in_dsd_plane_ofs_meta == NULL))
      goto in_dsd_plane_ofs_meta_missing;

    input_plane_offsets = in_dsd_plane_ofs_meta->offsets;
    num_dsd_bytes = in_dsd_plane_ofs_meta->num_bytes_per_channel * num_channels;
  }

  if (GST_DSD_INFO_LAYOUT (&self->out_info) == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    out_dsd_plane_ofs_meta = gst_buffer_get_dsd_plane_offset_meta (outbuf);
    if (G_UNLIKELY (out_dsd_plane_ofs_meta == NULL))
      goto out_dsd_plane_ofs_meta_missing;
  }

  /* Map the input and output buffers now. */

  if (!gst_buffer_map (inbuf, &in_map_info, GST_MAP_READ))
    goto in_map_failed;
  inbuf_mapped = TRUE;

  if (!gst_buffer_map (outbuf, &out_map_info, GST_MAP_WRITE))
    goto out_map_failed;
  outbuf_mapped = TRUE;

  /* In case of interleaved input audio, we only know now how many DSD bytes
   * there are, because then, the amount equals the number of bytes in the
   * buffer divided by the number of channels. This does not apply to
   * non-interleaved (= planar) data, since in such data, there can be
   * a space between the planes. */
  if (GST_DSD_INFO_LAYOUT (&self->in_info) == GST_AUDIO_LAYOUT_INTERLEAVED)
    num_dsd_bytes = in_map_info.size;

  need_to_reverse_bytes = GST_DSD_INFO_REVERSED_BYTES (&self->in_info) !=
      GST_DSD_INFO_REVERSED_BYTES (&self->out_info);

  /* We now have the necessary info to complete the output plane offset meta
   * (which is present if we are producing non-interleaved data). */
  if (GST_DSD_INFO_LAYOUT (&self->out_info) == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    gint channel_idx;

    out_dsd_plane_ofs_meta->num_bytes_per_channel =
        num_dsd_bytes / num_channels;

    for (channel_idx = 0; channel_idx < num_channels; ++channel_idx) {
      out_dsd_plane_ofs_meta->offsets[channel_idx] =
          out_dsd_plane_ofs_meta->num_bytes_per_channel * channel_idx;
    }

    output_plane_offsets = out_dsd_plane_ofs_meta->offsets;
  }

  /* Do the actual conversion. */
  gst_dsd_convert (in_map_info.data, out_map_info.data,
      GST_DSD_INFO_FORMAT (&self->in_info),
      GST_DSD_INFO_FORMAT (&self->out_info),
      GST_DSD_INFO_LAYOUT (&self->in_info),
      GST_DSD_INFO_LAYOUT (&self->out_info), input_plane_offsets,
      output_plane_offsets, num_dsd_bytes, num_channels, need_to_reverse_bytes);

finish:
  if (inbuf_mapped)
    gst_buffer_unmap (inbuf, &in_map_info);
  if (outbuf_mapped)
    gst_buffer_unmap (outbuf, &out_map_info);

  return flow_ret;

in_dsd_plane_ofs_meta_missing:
  {
    GST_ERROR_OBJECT (base,
        "input buffer has no DSD plane offset meta; buffer details: %"
        GST_PTR_FORMAT, inbuf);
    flow_ret = GST_FLOW_ERROR;
    goto finish;
  }

out_dsd_plane_ofs_meta_missing:
  {
    GST_ERROR_OBJECT (base,
        "output buffer has no DSD plane offset meta; buffer details: %"
        GST_PTR_FORMAT, outbuf);
    flow_ret = GST_FLOW_ERROR;
    goto finish;
  }

in_map_failed:
  {
    GST_ERROR_OBJECT (base, "could not map input buffer; buffer details: %"
        GST_PTR_FORMAT, inbuf);
    flow_ret = GST_FLOW_ERROR;
    goto finish;
  }

out_map_failed:
  {
    GST_ERROR_OBJECT (base, "could not map output buffer; buffer details: %"
        GST_PTR_FORMAT, outbuf);
    flow_ret = GST_FLOW_ERROR;
    goto finish;
  }
}

/* GStreamer
 * Copyright (C) 2008-2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

/* Implementation of SMPTE 384M - Mapping of Uncompressed Pictures into the MXF
 * Generic Container
 */

/* TODO: 
 *   - Handle CDCI essence
 *   - Handle more formats with RGBA descriptor (4:4:4 / 4:4:4:4 YUV, RGB656, ...)
 *   - Handle all the dimensions and other properties in the picture
 *     essence descriptors correctly according to S377M Annex E
 *   - Handle interlaced correctly, i.e. weave until we support one-field-per-buffer
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>

#include <gst/video/video.h>

#include "mxfup.h"
#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

static const struct
{
  const gchar *format;
  guint32 n_pixel_layout;
  guint8 pixel_layout[10];
  const gchar *caps_string;
} _rgba_mapping_table[] = {
  {
        "RGB", 3, {
          'R', 8, 'G', 8, 'B', 8}, GST_VIDEO_CAPS_MAKE ("RGB")}, {
        "BGR", 3, {
          'B', 8, 'G', 8, 'R', 8}, GST_VIDEO_CAPS_MAKE ("BGR")}, {
        "v308", 3, {
          'Y', 8, 'U', 8, 'V', 8}, GST_VIDEO_CAPS_MAKE ("v308")}, {
        "xRGB", 4, {
          'F', 8, 'R', 8, 'G', 8, 'B', 8}, GST_VIDEO_CAPS_MAKE ("xRGB")}, {
        "RGBx", 4, {
          'R', 8, 'G', 8, 'B', 8, 'F', 8}, GST_VIDEO_CAPS_MAKE ("RGBx")}, {
        "xBGR", 4, {
          'F', 8, 'B', 8, 'G', 8, 'R', 8}, GST_VIDEO_CAPS_MAKE ("xBGR")}, {
        "BGRx", 4, {
          'B', 8, 'G', 8, 'R', 8, 'F', 8}, GST_VIDEO_CAPS_MAKE ("BGRx")}, {
        "RGBA", 4, {
          'R', 8, 'G', 8, 'B', 8, 'A', 8}, GST_VIDEO_CAPS_MAKE ("RGBA")}, {
        "ARGB", 4, {
          'A', 8, 'R', 8, 'G', 8, 'B', 8}, GST_VIDEO_CAPS_MAKE ("RGBA")}, {
        "BGRA", 4, {
          'B', 8, 'G', 8, 'R', 8, 'A', 8}, GST_VIDEO_CAPS_MAKE ("BGRA")}, {
        "ABGR", 4, {
          'A', 8, 'B', 8, 'G', 8, 'R', 8}, GST_VIDEO_CAPS_MAKE ("ABGR")}, {
        "AYUV", 4, {
          'A', 8, 'Y', 8, 'U', 8, 'V', 8}, GST_VIDEO_CAPS_MAKE ("AYUV")}
};

static const struct
{
  const gchar *format;
  guint bpp;
  guint horizontal_subsampling;
  guint vertical_subsampling;
  gboolean reversed_byte_order;
  const gchar *caps_string;
} _cdci_mapping_table[] = {
  {
      "YUY2", 2, 1, 0, TRUE, GST_VIDEO_CAPS_MAKE ("YUY2")}, {
      "UYVY", 2, 1, 0, FALSE, GST_VIDEO_CAPS_MAKE ("UYVY")},
};

typedef struct
{
  const gchar *format;          /* video format string */
  gint width, height;
  guint bpp;
  guint32 image_start_offset;
  guint32 image_end_offset;
} MXFUPMappingData;

static gboolean
mxf_is_up_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;

  /* SMPTE 384M 8 */
  return (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 && key->u[13] == 0x05 && key->u[15] <= 0x03);
}

static GstFlowReturn
mxf_up_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  MXFUPMappingData *data = mapping_data;
  gsize expected_in_stride = 0, out_stride = 0;
  gsize expected_in_size = 0, out_size = 0;

  /* SMPTE 384M 7.1 */
  if (key->u[12] != 0x15 || (key->u[14] != 0x01 && key->u[14] != 0x02
          && key->u[14] != 0x03 && key->u[14] != 0x04)) {
    GST_ERROR ("Invalid uncompressed picture essence element");
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  if (!data) {
    GST_ERROR ("Invalid mapping data");
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  if (data->image_start_offset == 0 && data->image_end_offset == 0) {
  } else {
    if (data->image_start_offset + data->image_end_offset
        > gst_buffer_get_size (buffer)) {
      gst_buffer_unref (buffer);
      GST_ERROR ("Invalid buffer size");
      return GST_FLOW_ERROR;
    } else {
      gst_buffer_resize (buffer, data->image_start_offset,
          data->image_end_offset - data->image_start_offset);
    }
  }

  // Checked for overflows when parsing the descriptor
  expected_in_stride = data->bpp * data->width;
  out_stride = GST_ROUND_UP_4 (expected_in_stride);
  expected_in_size = expected_in_stride * data->height;
  out_size = out_stride * data->height;

  if (gst_buffer_get_size (buffer) != expected_in_size) {
    GST_ERROR ("Invalid buffer size");
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  if (data->bpp != 4 || out_stride != expected_in_stride) {
    guint y;
    GstBuffer *ret;
    GstMapInfo inmap, outmap;
    guint8 *indata, *outdata;

    ret = gst_buffer_new_and_alloc (out_size);
    gst_buffer_map (buffer, &inmap, GST_MAP_READ);
    gst_buffer_map (ret, &outmap, GST_MAP_WRITE);
    indata = inmap.data;
    outdata = outmap.data;

    for (y = 0; y < data->height; y++) {
      memcpy (outdata, indata, data->width * data->bpp);
      outdata += out_stride;
      indata += expected_in_stride;
    }

    gst_buffer_unmap (buffer, &inmap);
    gst_buffer_unmap (ret, &outmap);

    gst_buffer_unref (buffer);
    *outbuf = ret;
  } else {
    *outbuf = buffer;
  }

  return GST_FLOW_OK;
}

static MXFEssenceWrapping
mxf_up_get_track_wrapping (const MXFMetadataTimelineTrack * track)
{
  guint i;

  g_return_val_if_fail (track != NULL, MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING);

  if (track->parent.descriptor == NULL) {
    GST_ERROR ("No descriptor found for this track");
    return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
  }

  for (i = 0; i < track->parent.n_descriptor; i++) {
    if (!track->parent.descriptor[i])
      continue;

    if (!MXF_IS_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (track->
            parent.descriptor[i]))
      continue;

    switch (track->parent.descriptor[i]->essence_container.u[15]) {
      case 0x01:
        return MXF_ESSENCE_WRAPPING_FRAME_WRAPPING;
        break;
      case 0x02:
        return MXF_ESSENCE_WRAPPING_CLIP_WRAPPING;
        break;
      default:
        return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
        break;
    }
  }

  return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
}

static GstCaps *
mxf_up_rgba_create_caps (MXFMetadataTimelineTrack * track,
    MXFMetadataRGBAPictureEssenceDescriptor * d, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  GstCaps *caps = NULL;
  guint i;
  const gchar *format = NULL;
  guint bpp;

  if (!d->pixel_layout) {
    GST_ERROR ("No pixel layout");
    return NULL;
  }

  for (i = 0; i < G_N_ELEMENTS (_rgba_mapping_table); i++) {
    if (d->n_pixel_layout != _rgba_mapping_table[i].n_pixel_layout)
      continue;

    if (memcmp (d->pixel_layout, &_rgba_mapping_table[i].pixel_layout,
            _rgba_mapping_table[i].n_pixel_layout * 2) == 0) {
      caps = gst_caps_from_string (_rgba_mapping_table[i].caps_string);
      format = _rgba_mapping_table[i].format;
      bpp = _rgba_mapping_table[i].n_pixel_layout;
      break;
    }
  }

  if (caps) {
    MXFUPMappingData *data = g_new0 (MXFUPMappingData, 1);

    mxf_metadata_generic_picture_essence_descriptor_set_caps (&d->parent, caps);

    data->width = d->parent.stored_width;
    data->height = d->parent.stored_height;
    data->format = format;
    data->bpp = bpp;
    data->image_start_offset =
        ((MXFMetadataGenericPictureEssenceDescriptor *) d)->image_start_offset;
    data->image_end_offset =
        ((MXFMetadataGenericPictureEssenceDescriptor *) d)->image_end_offset;

    *mapping_data = data;
    *intra_only = TRUE;
  } else {
    GST_WARNING ("Unsupported pixel layout");
  }

  return caps;
}

static GstCaps *
mxf_up_cdci_create_caps (MXFMetadataTimelineTrack * track,
    MXFMetadataCDCIPictureEssenceDescriptor * d, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  GstCaps *caps = NULL;
  guint i;
  const gchar *format;
  guint bpp;

  for (i = 0; i < G_N_ELEMENTS (_cdci_mapping_table); i++) {
    if (_cdci_mapping_table[i].horizontal_subsampling ==
        d->horizontal_subsampling
        && _cdci_mapping_table[i].vertical_subsampling ==
        d->vertical_subsampling
        && _cdci_mapping_table[i].reversed_byte_order ==
        d->reversed_byte_order) {
      caps = gst_caps_from_string (_cdci_mapping_table[i].caps_string);
      format = _cdci_mapping_table[i].format;
      bpp = _cdci_mapping_table[i].bpp;
      break;
    }
  }

  if (caps) {
    MXFUPMappingData *data = g_new0 (MXFUPMappingData, 1);

    mxf_metadata_generic_picture_essence_descriptor_set_caps (&d->parent, caps);

    data->width = d->parent.stored_width;
    data->height = d->parent.stored_height;
    data->format = format;
    data->bpp = bpp;
    data->image_start_offset =
        ((MXFMetadataGenericPictureEssenceDescriptor *) d)->image_start_offset;
    data->image_end_offset =
        ((MXFMetadataGenericPictureEssenceDescriptor *) d)->image_end_offset;

    *mapping_data = data;
    *intra_only = TRUE;
  } else {
    GST_WARNING ("Unsupported CDCI format");
  }

  return caps;
}

static GstCaps *
mxf_up_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  MXFMetadataGenericPictureEssenceDescriptor *p = NULL;
  MXFMetadataCDCIPictureEssenceDescriptor *c = NULL;
  MXFMetadataRGBAPictureEssenceDescriptor *r = NULL;
  guint i;
  GstCaps *caps = NULL;

  g_return_val_if_fail (track != NULL, NULL);

  if (track->parent.descriptor == NULL) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  }

  for (i = 0; i < track->parent.n_descriptor; i++) {
    if (!track->parent.descriptor[i])
      continue;

    if (MXF_IS_METADATA_RGBA_PICTURE_ESSENCE_DESCRIPTOR (track->parent.
            descriptor[i])) {
      p = (MXFMetadataGenericPictureEssenceDescriptor *) track->parent.
          descriptor[i];
      r = (MXFMetadataRGBAPictureEssenceDescriptor *) track->parent.
          descriptor[i];
      break;
    } else if (MXF_IS_METADATA_CDCI_PICTURE_ESSENCE_DESCRIPTOR (track->
            parent.descriptor[i])) {
      p = (MXFMetadataGenericPictureEssenceDescriptor *) track->parent.
          descriptor[i];
      c = (MXFMetadataCDCIPictureEssenceDescriptor *) track->parent.
          descriptor[i];
    }
  }

  if (!p) {
    GST_ERROR ("No picture essence descriptor found for this track");
    return NULL;
  }

  *handler = mxf_up_handle_essence_element;

  if (r) {
    caps =
        mxf_up_rgba_create_caps (track, r, tags, intra_only, handler,
        mapping_data);
  } else if (c) {
    caps =
        mxf_up_cdci_create_caps (track, c, tags, intra_only, handler,
        mapping_data);
  } else {
    return NULL;
  }

  if (caps) {
    MXFUPMappingData *data = *mapping_data;
    gsize expected_in_stride = 0, out_stride = 0;
    gsize expected_in_size = 0, out_size = 0;

    // Do some checking of the parameters to see if they're valid and
    // we can actually work with them.
    if (data->image_start_offset > data->image_end_offset) {
      GST_WARNING ("Invalid image start/end offset");
      g_free (data);
      *mapping_data = NULL;
      gst_clear_caps (&caps);

      return NULL;
    }

    if (!g_size_checked_mul (&expected_in_stride, data->bpp, data->width) ||
        (out_stride = GST_ROUND_UP_4 (expected_in_stride)) < expected_in_stride
        || !g_size_checked_mul (&expected_in_size, expected_in_stride,
            data->height)
        || !g_size_checked_mul (&out_size, out_stride, data->height)) {
      GST_ERROR ("Invalid resolution or bit depth");
      g_free (data);
      *mapping_data = NULL;
      gst_clear_caps (&caps);

      return NULL;
    }
  }

  return caps;
}

static const MXFEssenceElementHandler mxf_up_essence_element_handler = {
  mxf_is_up_essence_track,
  mxf_up_get_track_wrapping,
  mxf_up_create_caps
};

static GstFlowReturn
mxf_up_write_func (GstBuffer * buffer, gpointer mapping_data,
    GstAdapter * adapter, GstBuffer ** outbuf, gboolean flush)
{
  MXFUPMappingData *data = mapping_data;

  if (!buffer)
    return GST_FLOW_OK;

  if (gst_buffer_get_size (buffer) !=
      GST_ROUND_UP_4 (data->bpp * data->width) * data->height) {
    GST_ERROR ("Invalid buffer size");
    return GST_FLOW_ERROR;
  }

  if (data->bpp != 4
      || GST_ROUND_UP_4 (data->width * data->bpp) != data->width * data->bpp) {
    guint y;
    GstBuffer *ret;
    GstMapInfo inmap, outmap;
    guint8 *indata, *outdata;

    ret = gst_buffer_new_and_alloc (data->width * data->bpp * data->height);
    gst_buffer_map (buffer, &inmap, GST_MAP_READ);
    gst_buffer_map (ret, &outmap, GST_MAP_WRITE);
    indata = inmap.data;
    outdata = outmap.data;

    for (y = 0; y < data->height; y++) {
      memcpy (outdata, indata, data->width * data->bpp);
      indata += GST_ROUND_UP_4 (data->width * data->bpp);
      outdata += data->width * data->bpp;
    }

    gst_buffer_unmap (buffer, &inmap);
    gst_buffer_unmap (ret, &outmap);
    gst_buffer_unref (buffer);

    *outbuf = ret;
  } else {
    *outbuf = buffer;
  }

  return GST_FLOW_OK;
}

static const guint8 up_essence_container_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01,
  0x0D, 0x01, 0x03, 0x01, 0x02, 0x05, 0x7F, 0x01
};

static MXFMetadataFileDescriptor *
mxf_up_get_rgba_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataRGBAPictureEssenceDescriptor *ret;
  guint i;
  GstCaps *tmp, *intersection;
  MXFUPMappingData *md = g_new0 (MXFUPMappingData, 1);

  *mapping_data = md;

  ret = (MXFMetadataRGBAPictureEssenceDescriptor *)
      g_object_new (MXF_TYPE_METADATA_RGBA_PICTURE_ESSENCE_DESCRIPTOR, NULL);

  for (i = 0; i < G_N_ELEMENTS (_rgba_mapping_table); i++) {
    tmp = gst_caps_from_string (_rgba_mapping_table[i].caps_string);
    intersection = gst_caps_intersect (caps, tmp);
    gst_caps_unref (tmp);

    if (!gst_caps_is_empty (intersection)) {
      gst_caps_unref (intersection);
      ret->n_pixel_layout = _rgba_mapping_table[i].n_pixel_layout;
      ret->pixel_layout = g_new0 (guint8, ret->n_pixel_layout * 2);
      md->format = _rgba_mapping_table[i].format;
      md->bpp = _rgba_mapping_table[i].n_pixel_layout;
      memcpy (ret->pixel_layout, _rgba_mapping_table[i].pixel_layout,
          ret->n_pixel_layout * 2);
      break;
    }
    gst_caps_unref (intersection);
  }

  if (md->format == NULL) {
    GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, caps);
    g_object_unref (ret);
    return NULL;
  }


  memcpy (&ret->parent.parent.essence_container, &up_essence_container_ul, 16);

  if (!mxf_metadata_generic_picture_essence_descriptor_from_caps (&ret->parent,
          caps)) {
    g_object_unref (ret);
    return NULL;
  }

  md->width = ret->parent.stored_width;
  md->height = ret->parent.stored_height;

  *handler = mxf_up_write_func;

  return (MXFMetadataFileDescriptor *) ret;
}

static MXFMetadataFileDescriptor *
mxf_up_get_cdci_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataCDCIPictureEssenceDescriptor *ret;
  guint i;
  GstCaps *tmp, *intersection;
  MXFUPMappingData *md = g_new0 (MXFUPMappingData, 1);

  *mapping_data = md;

  ret = (MXFMetadataCDCIPictureEssenceDescriptor *)
      g_object_new (MXF_TYPE_METADATA_CDCI_PICTURE_ESSENCE_DESCRIPTOR, NULL);

  for (i = 0; i < G_N_ELEMENTS (_cdci_mapping_table); i++) {
    tmp = gst_caps_from_string (_cdci_mapping_table[i].caps_string);
    intersection = gst_caps_intersect (caps, tmp);
    gst_caps_unref (tmp);

    if (!gst_caps_is_empty (intersection)) {
      gst_caps_unref (intersection);
      ret->horizontal_subsampling =
          _cdci_mapping_table[i].horizontal_subsampling;
      ret->vertical_subsampling = _cdci_mapping_table[i].vertical_subsampling;
      ret->reversed_byte_order = _cdci_mapping_table[i].reversed_byte_order;
      md->format = _cdci_mapping_table[i].format;
      md->bpp = _cdci_mapping_table[i].bpp;
      break;
    }
    gst_caps_unref (intersection);
  }

  if (md->format == NULL) {
    GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, caps);
    g_object_unref (ret);
    return NULL;
  }

  memcpy (&ret->parent.parent.essence_container, &up_essence_container_ul, 16);

  if (!mxf_metadata_generic_picture_essence_descriptor_from_caps (&ret->parent,
          caps)) {
    g_object_unref (ret);
    return NULL;
  }

  md->width = ret->parent.stored_width;
  md->height = ret->parent.stored_height;

  *handler = mxf_up_write_func;

  return (MXFMetadataFileDescriptor *) ret;
}

static MXFMetadataFileDescriptor *
mxf_up_get_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  GstStructure *s;

  s = gst_caps_get_structure (caps, 0);
  if (strcmp (gst_structure_get_name (s), "video/x-raw") == 0) {
    const gchar *format;

    format = gst_structure_get_string (s, "format");
    if (format == NULL)
      return NULL;

    if (g_str_equal (format, "YUY2") || g_str_equal (format, "UYVY"))
      return mxf_up_get_cdci_descriptor (tmpl, caps, handler, mapping_data);
    else
      return mxf_up_get_rgba_descriptor (tmpl, caps, handler, mapping_data);
  }

  g_assert_not_reached ();
  return NULL;
}

static void
mxf_up_update_descriptor (MXFMetadataFileDescriptor * d, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf)
{
  return;
}

static void
mxf_up_get_edit_rate (MXFMetadataFileDescriptor * a, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf, MXFMetadataSourcePackage * package,
    MXFMetadataTimelineTrack * track, MXFFraction * edit_rate)
{
  edit_rate->n = a->sample_rate.n;
  edit_rate->d = a->sample_rate.d;
}

static guint32
mxf_up_get_track_number_template (MXFMetadataFileDescriptor * a,
    GstCaps * caps, gpointer mapping_data)
{
  return (0x15 << 24) | (0x02 << 8);
}

static MXFEssenceElementWriter mxf_up_essence_element_writer = {
  mxf_up_get_descriptor,
  mxf_up_update_descriptor,
  mxf_up_get_edit_rate,
  mxf_up_get_track_number_template,
  NULL,
  {{0,}}
};

void
mxf_up_init (void)
{
  mxf_essence_element_handler_register (&mxf_up_essence_element_handler);
  mxf_up_essence_element_writer.pad_template =
      gst_pad_template_new ("up_video_sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("RGB") "; "
          GST_VIDEO_CAPS_MAKE ("BGR") "; "
          GST_VIDEO_CAPS_MAKE ("RGBx") "; "
          GST_VIDEO_CAPS_MAKE ("xRGB") "; "
          GST_VIDEO_CAPS_MAKE ("BGRx") "; "
          GST_VIDEO_CAPS_MAKE ("xBGR") "; "
          GST_VIDEO_CAPS_MAKE ("ARGB") "; "
          GST_VIDEO_CAPS_MAKE ("RGBA") "; "
          GST_VIDEO_CAPS_MAKE ("ABGR") "; "
          GST_VIDEO_CAPS_MAKE ("BGRA") "; "
          GST_VIDEO_CAPS_MAKE ("AYUV") "; "
          GST_VIDEO_CAPS_MAKE ("v308") "; "
          GST_VIDEO_CAPS_MAKE ("UYVY") "; " GST_VIDEO_CAPS_MAKE ("YUY2")));

  memcpy (&mxf_up_essence_element_writer.data_definition,
      mxf_metadata_track_identifier_get (MXF_METADATA_TRACK_PICTURE_ESSENCE),
      16);
  mxf_essence_element_writer_register (&mxf_up_essence_element_writer);
}

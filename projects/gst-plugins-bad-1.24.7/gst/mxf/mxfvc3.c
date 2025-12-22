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

/* Implementation of SMPTE S2019-4 - Mapping VC-3 coding units into the MXF
 * Generic Container
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "mxfvc3.h"
#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

static const MXFUL picture_essence_coding_vc3_avid = { {
        0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0e, 0x04, 0x02, 0x01,
        0x02,
    0x04, 0x01, 0x00}
};

static gboolean
mxf_is_vc3_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;

  /* SMPTE S2019-4 7 */
  if (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 && key->u[13] == 0x11 &&
      (key->u[14] == 0x01 || key->u[14] == 0x02)) {
    return TRUE;
  }

  if (mxf_is_avid_essence_container_label (key)) {
    MXFMetadataGenericPictureEssenceDescriptor *p;

    if (!MXF_IS_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (d))
      return FALSE;
    p = MXF_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (d);

    key = &p->picture_essence_coding;
    if (mxf_ul_is_subclass (&picture_essence_coding_vc3_avid, key))
      return TRUE;
  }

  return FALSE;
}

static GstFlowReturn
mxf_vc3_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  /* SMPTE 2019-4 6.1 */
  if (key->u[12] != 0x15 || (key->u[14] != 0x05 && key->u[14] != 0x06
          && key->u[14] != 0x0C && key->u[14] != 0x0D)) {
    GST_ERROR ("Invalid VC-3 essence element");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static MXFEssenceWrapping
mxf_vc3_get_track_wrapping (const MXFMetadataTimelineTrack * track)
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
            parent.descriptor[i])
        && !(MXF_IS_METADATA_FILE_DESCRIPTOR (track->parent.descriptor[i])
            && !MXF_IS_METADATA_MULTIPLE_DESCRIPTOR (track->
                parent.descriptor[i])))
      continue;

    switch (track->parent.descriptor[i]->essence_container.u[14]) {
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
mxf_vc3_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  MXFMetadataFileDescriptor *f = NULL;
  MXFMetadataGenericPictureEssenceDescriptor *p = NULL;
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

    if (MXF_IS_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (track->
            parent.descriptor[i])) {
      p = (MXFMetadataGenericPictureEssenceDescriptor *) track->parent.
          descriptor[i];
      f = track->parent.descriptor[i];
      break;
    } else if (MXF_IS_METADATA_FILE_DESCRIPTOR (track->parent.descriptor[i]) &&
        !MXF_IS_METADATA_MULTIPLE_DESCRIPTOR (track->parent.descriptor[i])) {
      f = track->parent.descriptor[i];
    }
  }

  if (!f) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  }

  *handler = mxf_vc3_handle_essence_element;

  caps = gst_caps_new_empty_simple ("video/x-dnxhd");
  if (p) {
    mxf_metadata_generic_picture_essence_descriptor_set_caps (p, caps);
  } else {
    GST_WARNING ("Only a generic file descriptor found");
  }

  if (!*tags)
    *tags = gst_tag_list_new_empty ();
  gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
      "VC-3 Video", NULL);
  *intra_only = TRUE;

  return caps;
}

static const MXFEssenceElementHandler mxf_vc3_essence_element_handler = {
  mxf_is_vc3_essence_track,
  mxf_vc3_get_track_wrapping,
  mxf_vc3_create_caps
};

static GstFlowReturn
mxf_vc3_write_func (GstBuffer * buffer, gpointer mapping_data,
    GstAdapter * adapter, GstBuffer ** outbuf, gboolean flush)
{
  *outbuf = buffer;
  return GST_FLOW_OK;
}

/* FIXME: In which version was this added? Byte 7, assuming version 10 */
static const guint8 vc3_essence_container_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x0A,
  0x0d, 0x01, 0x03, 0x01, 0x02, 0x11, 0x01, 0x00
};

static MXFMetadataFileDescriptor *
mxf_vc3_get_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataCDCIPictureEssenceDescriptor *ret;
  GstStructure *s;

  s = gst_caps_get_structure (caps, 0);
  if (strcmp (gst_structure_get_name (s), "video/x-dnxhd") != 0) {
    GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, caps);
    return NULL;
  }

  ret = (MXFMetadataCDCIPictureEssenceDescriptor *)
      g_object_new (MXF_TYPE_METADATA_CDCI_PICTURE_ESSENCE_DESCRIPTOR, NULL);

  memcpy (&ret->parent.parent.essence_container, &vc3_essence_container_ul, 16);

  if (!mxf_metadata_generic_picture_essence_descriptor_from_caps (&ret->parent,
          caps)) {
    g_object_unref (ret);
    return NULL;
  }

  *handler = mxf_vc3_write_func;

  return (MXFMetadataFileDescriptor *) ret;
}

static void
mxf_vc3_update_descriptor (MXFMetadataFileDescriptor * d, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf)
{
  return;
}

static void
mxf_vc3_get_edit_rate (MXFMetadataFileDescriptor * a, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf, MXFMetadataSourcePackage * package,
    MXFMetadataTimelineTrack * track, MXFFraction * edit_rate)
{
  edit_rate->n = a->sample_rate.n;
  edit_rate->d = a->sample_rate.d;
}

static guint32
mxf_vc3_get_track_number_template (MXFMetadataFileDescriptor * a,
    GstCaps * caps, gpointer mapping_data)
{
  return (0x15 << 24) | (0x0C << 8);
}

static MXFEssenceElementWriter mxf_vc3_essence_element_writer = {
  mxf_vc3_get_descriptor,
  mxf_vc3_update_descriptor,
  mxf_vc3_get_edit_rate,
  mxf_vc3_get_track_number_template,
  NULL,
  {{0,}}
};

void
mxf_vc3_init (void)
{
  mxf_essence_element_handler_register (&mxf_vc3_essence_element_handler);

  mxf_vc3_essence_element_writer.pad_template =
      gst_pad_template_new ("vc3_video_sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      gst_caps_from_string ("video/x-dnxhd, width = " GST_VIDEO_SIZE_RANGE
          ", height = " GST_VIDEO_SIZE_RANGE ", framerate = "
          GST_VIDEO_FPS_RANGE));
  memcpy (&mxf_vc3_essence_element_writer.data_definition,
      mxf_metadata_track_identifier_get (MXF_METADATA_TRACK_PICTURE_ESSENCE),
      16);
  mxf_essence_element_writer_register (&mxf_vc3_essence_element_writer);
}

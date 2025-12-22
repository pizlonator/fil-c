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

/* Implementation of SMPTE 383M - Mapping DV-DIF data into the MXF
 * Generic Container
 */

/* TODO:
 *  - playbin hangs on a lot of MXF/DV-DIF files (bug #563827)
 *  - decodebin creates loops inside the linking graph (bug #563828)
 *  - track descriptor might be multiple descriptor, one for sound, one for video
 *  - there might be 2 tracks for one essence, i.e. one audio/one video track
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "mxfdv-dif.h"
#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

static const MXFUL picture_essence_coding_dv = { {
        0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x04, 0x01, 0x02, 0x02,
    0x02}
};

static gboolean
mxf_is_dv_dif_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;
  /* SMPTE 383M 8 */
  if (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 && key->u[13] == 0x02) {
    return TRUE;
  }
  if (mxf_is_avid_essence_container_label (key)) {
    MXFMetadataGenericPictureEssenceDescriptor *p;

    if (!MXF_IS_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (d))
      return FALSE;
    p = MXF_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (d);

    key = &p->picture_essence_coding;
    if (mxf_ul_is_subclass (&picture_essence_coding_dv, key))
      return TRUE;
  }

  return FALSE;
}

static GstFlowReturn
mxf_dv_dif_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  /* SMPTE 383 6.1.1 */
  if (key->u[12] != 0x18 || (key->u[14] != 0x01 && key->u[14] != 0x02)) {
    GST_ERROR ("Invalid DV-DIF essence element");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static MXFEssenceWrapping
mxf_dv_dif_get_track_wrapping (const MXFMetadataTimelineTrack * track)
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
mxf_dv_dif_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  GstCaps *caps = NULL;
  guint i;
  MXFMetadataGenericPictureEssenceDescriptor *d = NULL;

  g_return_val_if_fail (track != NULL, NULL);

  if (track->parent.descriptor == NULL) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  }

  for (i = 0; i < track->parent.n_descriptor; i++) {
    if (MXF_IS_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (track->
            parent.descriptor[i])) {
      d = MXF_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (track->
          parent.descriptor[i]);
      break;
    }
  }

  *handler = mxf_dv_dif_handle_essence_element;
  /* SMPTE 383M 8 */

  /* TODO: might be video or audio only, use values of the generic sound/picture
   * descriptor in the caps in that case
   */
  GST_DEBUG ("Found DV-DIF stream");
  caps =
      gst_caps_new_simple ("video/x-dv", "systemstream", G_TYPE_BOOLEAN, TRUE,
      NULL);

  if (d)
    mxf_metadata_generic_picture_essence_descriptor_set_caps (d, caps);

  if (!*tags)
    *tags = gst_tag_list_new_empty ();

  gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_CODEC, "DV-DIF", NULL);

  *intra_only = TRUE;

  return caps;
}

static const MXFEssenceElementHandler mxf_dv_dif_essence_element_handler = {
  mxf_is_dv_dif_essence_track,
  mxf_dv_dif_get_track_wrapping,
  mxf_dv_dif_create_caps
};

static GstFlowReturn
mxf_dv_dif_write_func (GstBuffer * buffer,
    gpointer mapping_data, GstAdapter * adapter, GstBuffer ** outbuf,
    gboolean flush)
{
  *outbuf = buffer;
  return GST_FLOW_OK;
}

static const guint8 dv_dif_essence_container_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01,
  0x0d, 0x01, 0x03, 0x01, 0x02, 0x02, 0x7f, 0x01
};

static MXFMetadataFileDescriptor *
mxf_dv_dif_get_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataCDCIPictureEssenceDescriptor *ret;

  ret = (MXFMetadataCDCIPictureEssenceDescriptor *)
      g_object_new (MXF_TYPE_METADATA_CDCI_PICTURE_ESSENCE_DESCRIPTOR, NULL);

  memcpy (&ret->parent.parent.essence_container, &dv_dif_essence_container_ul,
      16);

  if (!mxf_metadata_generic_picture_essence_descriptor_from_caps (&ret->parent,
          caps)) {
    g_object_unref (ret);
    return NULL;
  }
  *handler = mxf_dv_dif_write_func;

  return (MXFMetadataFileDescriptor *) ret;
}

static void
mxf_dv_dif_update_descriptor (MXFMetadataFileDescriptor * d, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf)
{
  return;
}

static void
mxf_dv_dif_get_edit_rate (MXFMetadataFileDescriptor * a, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf, MXFMetadataSourcePackage * package,
    MXFMetadataTimelineTrack * track, MXFFraction * edit_rate)
{
  edit_rate->n = a->sample_rate.n;
  edit_rate->d = a->sample_rate.d;
}

static guint32
mxf_dv_dif_get_track_number_template (MXFMetadataFileDescriptor * a,
    GstCaps * caps, gpointer mapping_data)
{
  return (0x18 << 24) | (0x01 << 8);
}

static MXFEssenceElementWriter mxf_dv_dif_essence_element_writer = {
  mxf_dv_dif_get_descriptor,
  mxf_dv_dif_update_descriptor,
  mxf_dv_dif_get_edit_rate,
  mxf_dv_dif_get_track_number_template,
  NULL,
  {{0,}}
};

void
mxf_dv_dif_init (void)
{
  mxf_essence_element_handler_register (&mxf_dv_dif_essence_element_handler);

  mxf_dv_dif_essence_element_writer.pad_template =
      gst_pad_template_new ("dv_dif_video_sink_%u", GST_PAD_SINK,
      GST_PAD_REQUEST,
      gst_caps_from_string ("video/x-dv, width = "
          GST_VIDEO_SIZE_RANGE ", height = " GST_VIDEO_SIZE_RANGE
          ", framerate = " GST_VIDEO_FPS_RANGE ", systemstream = true"));
  memcpy (&mxf_dv_dif_essence_element_writer.data_definition,
      mxf_metadata_track_identifier_get (MXF_METADATA_TRACK_PICTURE_ESSENCE),
      16);
  mxf_essence_element_writer_register (&mxf_dv_dif_essence_element_writer);
}

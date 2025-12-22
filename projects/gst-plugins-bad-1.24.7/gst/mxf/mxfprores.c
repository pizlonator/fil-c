/* GStreamer MXF Apple ProRes Mapping
 * Copyright (C) 2019 Tim-Philipp Müller <tim@centricular.com>
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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "mxfprores.h"
#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

/* Implementation of SMPTE RDD 44:2017-11 "Material Exchange Format - Mapping
 * and Application of Apple ProRes"
 */
static gboolean
mxf_is_prores_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;

  return (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 && key->u[13] == 0x1C);
}

static GstFlowReturn
mxf_prores_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  /* SMPTE RDD 41:2017-11 6.3 */
  if (key->u[12] != 0x15 || key->u[14] != 0x17) {
    GST_MEMDUMP ("Essence element", &key->u[0], 16);
    GST_ERROR ("Invalid ProRes essence element");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static MXFEssenceWrapping
mxf_prores_get_track_wrapping (const MXFMetadataTimelineTrack * track)
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

    /* sanity check */
    if (track->parent.descriptor[i]->essence_container.u[13] != 0x1C)
      return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;

    switch (track->parent.descriptor[i]->essence_container.u[14]) {
      case 0x01:
        return MXF_ESSENCE_WRAPPING_FRAME_WRAPPING;
      case 0x02:
        return MXF_ESSENCE_WRAPPING_CLIP_WRAPPING;
      default:
        return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
    }
  }

  return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
}

static GstCaps *
mxf_prores_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  MXFMetadataGenericPictureEssenceDescriptor *d = NULL;
  const gchar *variant;
  GstCaps *caps;
  guint i;

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

  if (d == NULL) {
    GST_ERROR ("No picture essence coding descriptor found for this track");
    return NULL;
  }

  if (d->picture_essence_coding.u[13] != 0x06) {
    GST_MEMDUMP ("Picture essence", &d->picture_essence_coding.u[0], 16);
    GST_ERROR ("Picture essence coding descriptor not for ProRes?!");
    return NULL;
  }

  GST_INFO ("Found Apple ProRes video stream");

  switch (d->picture_essence_coding.u[14]) {
    case 0x01:
      variant = "proxy";
      break;
    case 0x02:
      variant = "lt";
      break;
    case 0x03:
      variant = "standard";
      break;
    case 0x04:
      variant = "hq";
      break;
    case 0x05:
      variant = "4444";
      break;
    case 0x06:
      variant = "4444xq";
      break;
    default:
      GST_ERROR ("Unknown ProRes profile %2d", d->picture_essence_coding.u[14]);
      return NULL;
  }

  *handler = mxf_prores_handle_essence_element;

  caps = gst_caps_new_simple ("video/x-prores",
      "variant", G_TYPE_STRING, variant, NULL);

  if (d)
    mxf_metadata_generic_picture_essence_descriptor_set_caps (d, caps);

  if (!*tags)
    *tags = gst_tag_list_new_empty ();

  gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_CODEC, "Apple ProRes",
      NULL);

  *intra_only = TRUE;

  return caps;
}

static const MXFEssenceElementHandler mxf_prores_essence_element_handler = {
  mxf_is_prores_essence_track,
  mxf_prores_get_track_wrapping,
  mxf_prores_create_caps
};

void
mxf_prores_init (void)
{
  mxf_essence_element_handler_register (&mxf_prores_essence_element_handler);
}

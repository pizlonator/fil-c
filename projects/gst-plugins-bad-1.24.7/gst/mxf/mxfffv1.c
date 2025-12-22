/* GStreamer
 * Copyright (C) 2023 Edward Hervey <edward@centricular.com>
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

/* Implementation of RDD48 Amd1 - mapping of RFC 9043 FFV1 Video Coding Format
 * Versions 0, 1, and 3 to RDD 48 and the MXF Generic Container
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "mxfffv1.h"
#include "mxfessence.h"
#include "mxfquark.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

/* RRD 48 Amd 1 */

static const MXFUL _ffv1_picture_descriptor_ul = { {
        0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x0e, 0x04, 0x01, 0x06, 0x0c,
    }
};



#define MXF_TYPE_METADATA_FFV1_PICTURE_DESCRIPTOR \
  (mxf_metadata_ffv1_picture_descriptor_get_type())
#define MXF_METADATA_FFV1_PICTURE_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_METADATA_FFV1_PICTURE_DESCRIPTOR, MXFMetadataFFV1PictureDescriptor))
#define MXF_IS_METADATA_FFV1_PICTURE_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_METADATA_FFV1_PICTURE_DESCRIPTOR))
typedef struct _MXFMetadataFFV1PictureDescriptor
    MXFMetadataFFV1PictureDescriptor;
typedef MXFMetadataClass MXFMetadataFFV1PictureDescriptorClass;
GType mxf_metadata_ffv1_picture_descriptor_get_type (void);

struct _MXFMetadataFFV1PictureDescriptor
{
  MXFMetadataCDCIPictureEssenceDescriptor parent;

  gchar *initialization_data;
  gsize id_size;
};

G_DEFINE_TYPE (MXFMetadataFFV1PictureDescriptor,
    mxf_metadata_ffv1_picture_descriptor,
    MXF_TYPE_METADATA_CDCI_PICTURE_ESSENCE_DESCRIPTOR);

static gboolean
mxf_is_ffv1_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;

  return (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 && key->u[13] == 0x23);
}

static GstFlowReturn
mxf_ffv1_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  if (key->u[12] != 0x15 || (key->u[14] != 0x1d && key->u[14] != 0x1e)) {
    GST_ERROR ("Invalid FFV1 essence element");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static MXFEssenceWrapping
mxf_ffv1_get_track_wrapping (const MXFMetadataTimelineTrack * track)
{
  MXFMetadataTrack *p = (MXFMetadataTrack *) track;
  guint i;

  g_return_val_if_fail (track != NULL, MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING);

  if (p->descriptor == NULL) {
    GST_ERROR ("No descriptor found for this track");
    return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
  }

  for (i = 0; i < p->n_descriptor; i++) {
    MXFMetadataFileDescriptor *desc = p->descriptor[i];
    if (!desc)
      continue;

    switch (desc->essence_container.u[14]) {
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
mxf_ffv1_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  MXFMetadataTrack *p = (MXFMetadataTrack *) track;
  MXFMetadataGenericDescriptor *generic = NULL;
  guint i;
  GstCaps *caps = NULL;

  g_return_val_if_fail (track != NULL, NULL);

  if (p->descriptor == NULL) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  }

  for (i = 0; i < p->n_descriptor; i++) {
    MXFMetadataFileDescriptor *desc = p->descriptor[i];
    if (!p)
      continue;

    if (MXF_IS_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (desc) &&
        desc->essence_container.u[13] == 0x23) {
      generic = (MXFMetadataGenericDescriptor *) desc;
      break;
    }
  }

  if (generic) {
    GST_DEBUG ("Found FFV1 byte-stream stream");

    *handler = mxf_ffv1_handle_essence_element;
    caps =
        gst_caps_new_simple ("video/x-ffv", "ffvversion", G_TYPE_INT, 1, NULL);

    for (i = 0; i < generic->n_sub_descriptors; i++) {
      if (generic->sub_descriptors[i]
          &&
          MXF_IS_METADATA_FFV1_PICTURE_DESCRIPTOR (generic->sub_descriptors[i]))
      {
        MXFMetadataFFV1PictureDescriptor *ffv1 =
            (MXFMetadataFFV1PictureDescriptor *) generic->sub_descriptors[i];
        GstBuffer *codec_data;
        g_assert (ffv1->initialization_data);
        codec_data =
            gst_buffer_new_wrapped (g_memdup2 (ffv1->initialization_data,
                ffv1->id_size), ffv1->id_size);
        gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, codec_data,
            NULL);
        gst_buffer_unref (codec_data);
        break;
      }
    }

    if (!*tags)
      *tags = gst_tag_list_new_empty ();
    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
        "FFV1 Video", NULL);
    *intra_only = TRUE;
    mxf_metadata_generic_picture_essence_descriptor_set_caps (
        (MXFMetadataGenericPictureEssenceDescriptor *) generic, caps);
  }

  return caps;
}

static const MXFEssenceElementHandler mxf_ffv1_essence_element_handler = {
  mxf_is_ffv1_essence_track,
  mxf_ffv1_get_track_wrapping,
  mxf_ffv1_create_caps
};

static gboolean
mxf_metadata_ffv1_picture_descriptor_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFMetadataFFV1PictureDescriptor *self =
      MXF_METADATA_FFV1_PICTURE_DESCRIPTOR (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif

  tag_ul = mxf_primer_tag_to_ul (primer, tag);
  if (!tag_ul)
    return FALSE;

  GST_DEBUG ("%s", mxf_ul_to_string (tag_ul, str));

  if (mxf_ul_is_subclass (&_ffv1_picture_descriptor_ul, tag_ul)) {
    switch (tag_ul->u[12]) {
      case 0x01:
      {
        GST_MEMDUMP ("Initialization data", tag_data, tag_size);
        self->initialization_data = g_memdup2 (tag_data, tag_size);
        self->id_size = tag_size;
        break;
      }
      default:
        GST_DEBUG ("Tag 0x%02x", tag_ul->u[12]);
        break;
    }
    ret = TRUE;
  } else {

    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_metadata_ffv1_picture_descriptor_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }
  return ret;
}

static void
mxf_metadata_ffv1_picture_descriptor_init (MXFMetadataFFV1PictureDescriptor *
    self)
{

}

static void
    mxf_metadata_ffv1_picture_descriptor_class_init
    (MXFMetadataFFV1PictureDescriptorClass * klass)
{
  MXFMetadataBaseClass *metadata_base_class = (MXFMetadataBaseClass *) klass;
  MXFMetadataClass *metadata_class = (MXFMetadataClass *) klass;

  metadata_base_class->handle_tag =
      mxf_metadata_ffv1_picture_descriptor_handle_tag;
  metadata_base_class->name_quark = MXF_QUARK (FFV1_PICTURE_DESCRIPTOR);

  metadata_class->type = 0x0181;
}

void
mxf_ffv1_init (void)
{
  mxf_essence_element_handler_register (&mxf_ffv1_essence_element_handler);

  mxf_metadata_register (MXF_TYPE_METADATA_FFV1_PICTURE_DESCRIPTOR);
}

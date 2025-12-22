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

/* Implementation of SMPTE 381M - Mapping MPEG streams into the MXF
 * Generic Container
 *
 * RP 2008 - Mapping AVC Streams into the MXF Generic Container
 *
 */

/* TODO:
 * - Handle PES streams
 * - Fix TS/PS demuxers to forward timestamps
 * - AAC support
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "mxfmpeg.h"
#include "mxfquark.h"
#include "mxfessence.h"

#include <gst/base/gstbytereader.h>

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

/* SMPTE 381M 8.1 - ULs of local tags */
static const guint8 _single_sequence_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x02, 0x00, 0x00
};

static const guint8 _constant_b_frames_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x03, 0x00, 0x00
};

static const guint8 _coded_content_type_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x04, 0x00, 0x00
};

static const guint8 _low_delay_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x05, 0x00, 0x00
};

static const guint8 _closed_gop_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x06, 0x00, 0x00
};

static const guint8 _identical_gop_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x07, 0x00, 0x00
};

static const guint8 _max_gop_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x08, 0x00, 0x00
};

static const guint8 _b_picture_count_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x09, 0x00, 0x00
};

static const guint8 _bitrate_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x0b, 0x00, 0x00
};

static const guint8 _profile_and_level_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x04, 0x01, 0x06, 0x02, 0x01,
  0x0a, 0x00, 0x00
};

/* SMPTE 381M 8.1 */
#define MXF_TYPE_METADATA_MPEG_VIDEO_DESCRIPTOR \
  (mxf_metadata_mpeg_video_descriptor_get_type())
#define MXF_METADATA_MPEG_VIDEO_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_METADATA_MPEG_VIDEO_DESCRIPTOR, MXFMetadataMPEGVideoDescriptor))
#define MXF_IS_METADATA_MPEG_VIDEO_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_METADATA_MPEG_VIDEO_DESCRIPTOR))
typedef struct _MXFMetadataMPEGVideoDescriptor MXFMetadataMPEGVideoDescriptor;
typedef MXFMetadataClass MXFMetadataMPEGVideoDescriptorClass;
GType mxf_metadata_mpeg_video_descriptor_get_type (void);

struct _MXFMetadataMPEGVideoDescriptor
{
  MXFMetadataCDCIPictureEssenceDescriptor parent;

  gboolean single_sequence;
  gboolean const_b_frames;
  guint8 coded_content_type;
  gboolean low_delay;

  gboolean closed_gop;
  gboolean identical_gop;
  guint16 max_gop;

  guint16 b_picture_count;
  guint32 bitrate;
  guint8 profile_and_level;
};

G_DEFINE_TYPE (MXFMetadataMPEGVideoDescriptor,
    mxf_metadata_mpeg_video_descriptor,
    MXF_TYPE_METADATA_CDCI_PICTURE_ESSENCE_DESCRIPTOR);

static gboolean
mxf_metadata_mpeg_video_descriptor_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFMetadataMPEGVideoDescriptor *self =
      MXF_METADATA_MPEG_VIDEO_DESCRIPTOR (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;

  tag_ul = mxf_primer_tag_to_ul (primer, tag);
  if (!tag_ul)
    return FALSE;

  if (memcmp (tag_ul, &_single_sequence_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;
    self->single_sequence = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  single sequence = %s",
        (self->single_sequence) ? "yes" : "no");
  } else if (memcmp (tag_ul, &_constant_b_frames_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;
    self->const_b_frames = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  constant b frames = %s",
        (self->single_sequence) ? "yes" : "no");
  } else if (memcmp (tag_ul, &_coded_content_type_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;
    self->coded_content_type = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  coded content type = %u", self->coded_content_type);
  } else if (memcmp (tag_ul, &_low_delay_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;
    self->low_delay = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  low delay = %s", (self->low_delay) ? "yes" : "no");
  } else if (memcmp (tag_ul, &_closed_gop_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;
    self->closed_gop = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  closed gop = %s", (self->closed_gop) ? "yes" : "no");
  } else if (memcmp (tag_ul, &_identical_gop_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;
    self->identical_gop = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  identical gop = %s", (self->identical_gop) ? "yes" : "no");
  } else if (memcmp (tag_ul, &_max_gop_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;
    self->max_gop = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  max gop = %u", self->max_gop);
  } else if (memcmp (tag_ul, &_b_picture_count_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;
    self->b_picture_count = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  b picture count = %u", self->b_picture_count);
  } else if (memcmp (tag_ul, &_bitrate_ul, 16) == 0) {
    if (tag_size != 4)
      goto error;
    self->bitrate = GST_READ_UINT32_BE (tag_data);
    GST_DEBUG ("  bitrate = %u", self->bitrate);
  } else if (memcmp (tag_ul, &_profile_and_level_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;
    self->profile_and_level = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  profile & level = %u", self->profile_and_level);
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_metadata_mpeg_video_descriptor_parent_class)->handle_tag (metadata,
        primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid MPEG video descriptor local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static GstStructure *
mxf_metadata_mpeg_video_descriptor_to_structure (MXFMetadataBase * m)
{
  GstStructure *ret =
      MXF_METADATA_BASE_CLASS
      (mxf_metadata_mpeg_video_descriptor_parent_class)->to_structure (m);
  MXFMetadataMPEGVideoDescriptor *self = MXF_METADATA_MPEG_VIDEO_DESCRIPTOR (m);

  gst_structure_id_set (ret, MXF_QUARK (SINGLE_SEQUENCE), G_TYPE_BOOLEAN,
      self->single_sequence, MXF_QUARK (CONST_B_FRAMES), G_TYPE_BOOLEAN,
      self->const_b_frames, MXF_QUARK (CODED_CONTENT_TYPE), G_TYPE_UCHAR,
      self->coded_content_type, MXF_QUARK (LOW_DELAY), G_TYPE_BOOLEAN,
      self->low_delay, MXF_QUARK (CLOSED_GOP), G_TYPE_BOOLEAN, self->closed_gop,
      MXF_QUARK (IDENTICAL_GOP), G_TYPE_BOOLEAN, self->identical_gop,
      MXF_QUARK (PROFILE_AND_LEVEL), G_TYPE_UCHAR, self->profile_and_level,
      NULL);

  if (self->max_gop)
    gst_structure_id_set (ret, MXF_QUARK (MAX_GOP), G_TYPE_UINT, self->max_gop,
        NULL);

  if (self->b_picture_count)
    gst_structure_id_set (ret, MXF_QUARK (B_PICTURE_COUNT), G_TYPE_UINT,
        self->b_picture_count, NULL);

  if (self->bitrate)
    gst_structure_id_set (ret, MXF_QUARK (BITRATE), G_TYPE_UINT, self->bitrate,
        NULL);

  return ret;
}

static GList *
mxf_metadata_mpeg_video_descriptor_write_tags (MXFMetadataBase * m,
    MXFPrimerPack * primer)
{
  MXFMetadataMPEGVideoDescriptor *self = MXF_METADATA_MPEG_VIDEO_DESCRIPTOR (m);
  GList *ret =
      MXF_METADATA_BASE_CLASS
      (mxf_metadata_mpeg_video_descriptor_parent_class)->write_tags (m, primer);
  MXFLocalTag *t;

  if (self->single_sequence != -1) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_single_sequence_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, (self->single_sequence) ? 1 : 0);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->const_b_frames) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_constant_b_frames_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, (self->const_b_frames) ? 1 : 0);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->coded_content_type) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_coded_content_type_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, self->coded_content_type);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->low_delay) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_low_delay_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, (self->low_delay) ? 1 : 0);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->closed_gop) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_closed_gop_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, (self->closed_gop) ? 1 : 0);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->identical_gop) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_identical_gop_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, (self->identical_gop) ? 1 : 0);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->max_gop) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_identical_gop_ul, 16);
    t->size = 2;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT16_BE (t->data, self->max_gop);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->b_picture_count) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_b_picture_count_ul, 16);
    t->size = 2;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT16_BE (t->data, self->b_picture_count);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->bitrate) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_bitrate_ul, 16);
    t->size = 4;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->bitrate);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->profile_and_level) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &_profile_and_level_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, self->profile_and_level);
    mxf_primer_pack_add_mapping (primer, 0, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  return ret;
}

static void
mxf_metadata_mpeg_video_descriptor_init (MXFMetadataMPEGVideoDescriptor * self)
{
  self->single_sequence = -1;
}

static void
    mxf_metadata_mpeg_video_descriptor_class_init
    (MXFMetadataMPEGVideoDescriptorClass * klass)
{
  MXFMetadataBaseClass *metadata_base_class = (MXFMetadataBaseClass *) klass;
  MXFMetadataClass *metadata_class = (MXFMetadataClass *) klass;

  metadata_base_class->handle_tag =
      mxf_metadata_mpeg_video_descriptor_handle_tag;
  metadata_base_class->name_quark = MXF_QUARK (MPEG_VIDEO_DESCRIPTOR);
  metadata_base_class->to_structure =
      mxf_metadata_mpeg_video_descriptor_to_structure;
  metadata_base_class->write_tags =
      mxf_metadata_mpeg_video_descriptor_write_tags;

  metadata_class->type = 0x0151;
}

typedef enum
{
  MXF_MPEG_ESSENCE_TYPE_OTHER = 0,
  MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG2,
  MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG4,
  MXF_MPEG_ESSENCE_TYPE_VIDEO_AVC
} MXFMPEGEssenceType;

static gboolean
mxf_is_mpeg_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;
  /* SMPTE 381M 7 */
  /* SMPTE RP2008 8.1 */
  return (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 &&
      (key->u[13] == 0x04 ||
          key->u[13] == 0x07 || key->u[13] == 0x08 || key->u[13] == 0x09 ||
          key->u[13] == 0x0f || key->u[13] == 0x10));
}

/* See ISO/IEC 13818-2 for MPEG ES format */
gboolean
mxf_mpeg_is_mpeg2_keyframe (GstBuffer * buffer)
{
  GstMapInfo map;
  GstByteReader reader;
  guint32 tmp;
  gboolean ret = FALSE;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  gst_byte_reader_init (&reader, map.data, map.size);

  while (gst_byte_reader_get_remaining (&reader) > 3) {
    if (gst_byte_reader_peek_uint24_be (&reader, &tmp) && tmp == 0x000001) {
      guint8 type = 0;

      /* Found sync code */
      gst_byte_reader_skip_unchecked (&reader, 3);

      if (!gst_byte_reader_get_uint8 (&reader, &type))
        break;

      /* GOP packets are meant as random access markers */
      if (type == 0xb8) {
        ret = TRUE;
        goto done;
      } else if (type == 0x00) {
        guint8 pic_type = 0;

        if (!gst_byte_reader_skip (&reader, 5))
          break;

        if (!gst_byte_reader_get_uint8 (&reader, &pic_type))
          break;

        pic_type = (pic_type >> 3) & 0x07;
        if (pic_type == 0x01) {
          ret = TRUE;
        }
        goto done;
      }
    } else if (gst_byte_reader_skip (&reader, 1) == FALSE)
      break;
  }

done:
  gst_buffer_unmap (buffer, &map);

  return ret;
}

static gboolean
mxf_mpeg_is_mpeg4_keyframe (GstBuffer * buffer)
{
  GstMapInfo map;
  GstByteReader reader;
  guint32 tmp;
  gboolean ret = FALSE;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  gst_byte_reader_init (&reader, map.data, map.size);

  while (gst_byte_reader_get_remaining (&reader) > 3) {
    if (gst_byte_reader_peek_uint24_be (&reader, &tmp) && tmp == 0x000001) {
      guint8 type = 0;

      /* Found sync code */
      gst_byte_reader_skip_unchecked (&reader, 3);

      if (!gst_byte_reader_get_uint8 (&reader, &type))
        break;

      if (type == 0xb6) {
        guint8 pic_type = 0;

        if (!gst_byte_reader_get_uint8 (&reader, &pic_type))
          break;

        pic_type = (pic_type >> 6) & 0x03;
        if (pic_type == 0) {
          ret = TRUE;
        }
        goto done;
      }
    } else if (gst_byte_reader_skip (&reader, 1) == FALSE)
      break;
  }

done:
  gst_buffer_unmap (buffer, &map);

  return ret;
}

static GstFlowReturn
mxf_mpeg_video_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps, MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  MXFMPEGEssenceType type = *((MXFMPEGEssenceType *) mapping_data);

  *outbuf = buffer;

  /* SMPTE 381M 6.1 */
  if (key->u[12] != 0x15 || (key->u[14] != 0x05 && key->u[14] != 0x06
          && key->u[14] != 0x07)) {
    GST_ERROR ("Invalid MPEG video essence element");
    return GST_FLOW_ERROR;
  }

  switch (type) {
    case MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG2:
      if (mxf_mpeg_is_mpeg2_keyframe (buffer))
        GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      else
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      break;
    case MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG4:
      if (mxf_mpeg_is_mpeg4_keyframe (buffer))
        GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      else
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      break;

    default:
      break;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
mxf_mpeg_audio_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps, MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  /* SMPTE 381M 6.2 */
  if (key->u[12] != 0x16 || (key->u[14] != 0x05 && key->u[14] != 0x06
          && key->u[14] != 0x07)) {
    GST_ERROR ("Invalid MPEG audio essence element");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

/* Private uid used by SONY C0023S01.mxf,
 * taken from the ffmpeg mxf demuxer */
static const guint8 sony_mpeg4_extradata[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0e, 0x06, 0x06, 0x02, 0x02,
  0x01, 0x00, 0x00
};

/* RP224 */

static const MXFUL video_mpeg_compression = { {
        0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x01, 0x04, 0x01, 0x02, 0x02,
    0x01,}
};

static const MXFUL sound_essence_compression_ac3 = { {
        0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x01, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x02, 0x01, 0x00}
};

static const MXFUL sound_essence_compression_mpeg1_layer1 = { {
        0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x01, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x02, 0x04, 0x00}
};

static const MXFUL sound_essence_compression_mpeg1_layer23 = { {
        0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x01, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x02, 0x05, 0x00}
};

static const MXFUL sound_essence_compression_mpeg1_layer2 = { {
        0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x08, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x02, 0x05, 0x01}
};

static const MXFUL sound_essence_compression_mpeg2_layer1 = { {
        0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x01, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x02, 0x06, 0x00}
};

static const MXFUL sound_essence_compression_dts = { {
        0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x01, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x02, 0x1c, 0x00}
};

static const MXFUL sound_essence_compression_aac = { {
        0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x03, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x01, 0x00}
};

static GstCaps *
mxf_mpeg_es_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data, MXFMetadataGenericPictureEssenceDescriptor * p,
    MXFMetadataGenericSoundEssenceDescriptor * s)
{
  GstCaps *caps = NULL;
  const gchar *codec_name = NULL;
  MXFMPEGEssenceType t, *mdata;
  gchar str[48];

  *mapping_data = g_malloc (sizeof (MXFMPEGEssenceType));
  mdata = (MXFMPEGEssenceType *) * mapping_data;

  /* SMPTE RP224 */
  if (p) {
    if (mxf_ul_is_zero (&p->picture_essence_coding) ||
        mxf_ul_is_equal (&p->picture_essence_coding,
            &p->parent.essence_container)) {
      GST_WARNING ("No picture essence coding defined, assuming MPEG2");
      caps =
          gst_caps_new_simple ("video/mpeg", "mpegversion", G_TYPE_INT, 2,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      codec_name = "MPEG-2 Video";
      t = MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG2;
      memcpy (mdata, &t, sizeof (MXFMPEGEssenceType));
      *intra_only = FALSE;
    } else if (!mxf_ul_is_subclass (&video_mpeg_compression,
            &p->picture_essence_coding)) {
      GST_ERROR ("Not MPEG picture essence coding %s",
          mxf_ul_to_string (&p->picture_essence_coding, str));
      caps = NULL;
    } else if (p->picture_essence_coding.u[13] >= 0x01 &&
        p->picture_essence_coding.u[13] <= 0x08) {
      caps = gst_caps_new_simple ("video/mpeg", "mpegversion", G_TYPE_INT, 2,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      codec_name = "MPEG-2 Video";
      t = MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG2;
      memcpy (mdata, &t, sizeof (MXFMPEGEssenceType));
      *intra_only = FALSE;
    } else if (p->picture_essence_coding.u[13] == 0x10) {
      caps = gst_caps_new_simple ("video/mpeg", "mpegversion", G_TYPE_INT, 1,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      codec_name = "MPEG-1 Video";
      t = MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG2;
      memcpy (mdata, &t, sizeof (MXFMPEGEssenceType));
      *intra_only = TRUE;
    } else if (p->picture_essence_coding.u[13] == 0x20) {
      MXFLocalTag *local_tag =
          (((MXFMetadataBase *) p)->other_tags) ?
          g_hash_table_lookup (((MXFMetadataBase *)
              p)->other_tags, &sony_mpeg4_extradata) : NULL;

      caps = gst_caps_new_simple ("video/mpeg", "mpegversion", G_TYPE_INT, 4,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);

      if (local_tag) {
        GstMapInfo map;
        GstBuffer *codec_data = NULL;
        codec_data = gst_buffer_new_and_alloc (local_tag->size);
        gst_buffer_map (codec_data, &map, GST_MAP_WRITE);
        memcpy (map.data, local_tag->data, local_tag->size);
        gst_buffer_unmap (codec_data, &map);
        gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, codec_data,
            NULL);
        gst_buffer_unref (codec_data);
      }
      codec_name = "MPEG-4 Video";
      t = MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG4;
      memcpy (mdata, &t, sizeof (MXFMPEGEssenceType));
      *intra_only = FALSE;
    } else if ((p->picture_essence_coding.u[13] >> 4) == 0x03) {
      /* RP 2008 */

      caps =
          gst_caps_new_simple ("video/x-h264", "stream-format", G_TYPE_STRING,
          "byte-stream", NULL);
      codec_name = "h.264 Video";
      t = MXF_MPEG_ESSENCE_TYPE_VIDEO_AVC;
      memcpy (mdata, &t, sizeof (MXFMPEGEssenceType));
      *intra_only = FALSE;
    } else {
      GST_ERROR ("Unsupported MPEG picture essence coding 0x%02x",
          p->picture_essence_coding.u[13]);
      caps = NULL;
    }
    if (caps)
      *handler = mxf_mpeg_video_handle_essence_element;
  } else if (s) {
    if (mxf_ul_is_zero (&s->sound_essence_compression)) {
      GST_WARNING ("Zero sound essence compression, assuming MPEG1 audio");
      caps =
          gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
          NULL);
      codec_name = "MPEG-1 Audio";
    } else if (mxf_ul_is_equal (&s->sound_essence_compression,
            &sound_essence_compression_ac3)) {
      caps = gst_caps_new_empty_simple ("audio/x-ac3");
      codec_name = "AC3 Audio";
    } else if (mxf_ul_is_equal (&s->sound_essence_compression,
            &sound_essence_compression_mpeg1_layer1)) {
      caps =
          gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
          "layer", G_TYPE_INT, 1, NULL);
      codec_name = "MPEG-1 Layer 1 Audio";
    } else if (mxf_ul_is_equal (&s->sound_essence_compression,
            &sound_essence_compression_mpeg1_layer23)) {
      caps =
          gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
          NULL);
      codec_name = "MPEG-1 Audio";
    } else if (mxf_ul_is_equal (&s->sound_essence_compression,
            &sound_essence_compression_mpeg1_layer2)) {
      caps =
          gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
          "layer", G_TYPE_INT, 2, NULL);
      codec_name = "MPEG-1 Layer 2 Audio";
    } else if (mxf_ul_is_equal (&s->sound_essence_compression,
            &sound_essence_compression_mpeg2_layer1)) {
      caps =
          gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
          "layer", G_TYPE_INT, 1, "mpegaudioversion", G_TYPE_INT, 2, NULL);
      codec_name = "MPEG-2 Layer 1 Audio";
    } else if (mxf_ul_is_equal (&s->sound_essence_compression,
            &sound_essence_compression_dts)) {
      caps = gst_caps_new_empty_simple ("audio/x-dts");
      codec_name = "Dolby DTS Audio";
    } else if (mxf_ul_is_equal (&s->sound_essence_compression,
            &sound_essence_compression_aac)) {
      caps = gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT,
          2, NULL);
      codec_name = "MPEG-2 AAC Audio";
    }

    if (caps) {
      mxf_metadata_generic_sound_essence_descriptor_set_caps (s, caps);
      *handler = mxf_mpeg_audio_handle_essence_element;
    }
    *intra_only = TRUE;
  }

  if (caps) {
    if (!*tags)
      *tags = gst_tag_list_new_empty ();
    if (codec_name)
      gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
          codec_name, NULL);

    if (p && MXF_IS_METADATA_MPEG_VIDEO_DESCRIPTOR (p)
        && MXF_METADATA_MPEG_VIDEO_DESCRIPTOR (p)->bitrate) {
      gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
          MXF_METADATA_MPEG_VIDEO_DESCRIPTOR (p)->bitrate, NULL);
    }
  }

  return caps;
}

static MXFEssenceWrapping
mxf_mpeg_get_track_wrapping (const MXFMetadataTimelineTrack * track)
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
        && !MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->
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
mxf_mpeg_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  MXFMetadataFileDescriptor *f = NULL;
  MXFMetadataGenericPictureEssenceDescriptor *p = NULL;
  MXFMetadataGenericSoundEssenceDescriptor *s = NULL;
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
      f = track->parent.descriptor[i];
      p = (MXFMetadataGenericPictureEssenceDescriptor *) track->parent.
          descriptor[i];
      break;
    } else if (MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->
            parent.descriptor[i])) {
      f = track->parent.descriptor[i];
      s = (MXFMetadataGenericSoundEssenceDescriptor *) track->parent.
          descriptor[i];
      break;
    }
  }

  if (!f) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  }

  /* SMPTE 381M 7 */
  if (f->essence_container.u[13] == 0x04) {
    GST_DEBUG ("Found MPEG ES stream");

    caps =
        mxf_mpeg_es_create_caps (track, tags, intra_only, handler, mapping_data,
        p, s);
  } else if (f->essence_container.u[13] == 0x07) {
    GST_ERROR ("MPEG PES streams not supported yet");
    return NULL;
  } else if (f->essence_container.u[13] == 0x08) {
    /* FIXME: get mpeg version somehow */
    GST_DEBUG ("Found MPEG PS stream");
    caps = gst_caps_new_simple ("video/mpeg", "mpegversion", G_TYPE_INT, 1,
        "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);

    if (!*tags)
      *tags = gst_tag_list_new_empty ();
    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
        "MPEG PS", NULL);
    *intra_only = FALSE;
  } else if (f->essence_container.u[13] == 0x09) {
    GST_DEBUG ("Found MPEG TS stream");
    caps = gst_caps_new_empty_simple ("video/mpegts");

    if (!*tags)
      *tags = gst_tag_list_new_empty ();
    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
        "MPEG TS", NULL);
    *intra_only = FALSE;
  } else if (f->essence_container.u[13] == 0x0f) {
    GST_DEBUG ("Found h264 NAL unit stream");
    /* RP 2008 */
    caps =
        gst_caps_new_simple ("video/x-h264", "stream-format", G_TYPE_STRING,
        "byte-stream", NULL);

    if (!*tags)
      *tags = gst_tag_list_new_empty ();
    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
        "h.264 Video", NULL);
    *intra_only = FALSE;
  } else if (f->essence_container.u[13] == 0x10) {
    GST_DEBUG ("Found h264 byte-stream stream");
    /* RP 2008 */
    caps =
        gst_caps_new_simple ("video/x-h264", "stream-format", G_TYPE_STRING,
        "byte-stream", NULL);

    if (!*tags)
      *tags = gst_tag_list_new_empty ();
    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
        "h.264 Video", NULL);
    *intra_only = FALSE;
  }

  if (p && caps)
    mxf_metadata_generic_picture_essence_descriptor_set_caps (p, caps);

  return caps;
}

static const MXFEssenceElementHandler mxf_mpeg_essence_element_handler = {
  mxf_is_mpeg_essence_track,
  mxf_mpeg_get_track_wrapping,
  mxf_mpeg_create_caps
};

typedef struct
{
  guint spf;
  guint rate;
} MPEGAudioMappingData;

static GstFlowReturn
mxf_mpeg_audio_write_func (GstBuffer * buffer,
    gpointer mapping_data, GstAdapter * adapter, GstBuffer ** outbuf,
    gboolean flush)
{
  *outbuf = buffer;
  return GST_FLOW_OK;
}

static const guint8 mpeg_essence_container_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02,
  0x0d, 0x01, 0x03, 0x01, 0x02, 0x00, 0x00, 0x01
};

static MXFMetadataFileDescriptor *
mxf_mpeg_audio_get_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataGenericSoundEssenceDescriptor *ret;
  GstStructure *s;
  MPEGAudioMappingData *md = g_new0 (MPEGAudioMappingData, 1);
  gint rate;

  md->spf = -1;
  *mapping_data = md;

  ret = (MXFMetadataGenericSoundEssenceDescriptor *)
      g_object_new (MXF_TYPE_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR, NULL);

  s = gst_caps_get_structure (caps, 0);
  if (strcmp (gst_structure_get_name (s), "audio/mpeg") == 0) {
    gint mpegversion;

    if (!gst_structure_get_int (s, "mpegversion", &mpegversion)) {
      GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, caps);
      g_object_unref (ret);
      return NULL;
    }

    if (mpegversion == 1) {
      gint layer = 0;
      gint mpegaudioversion = 0;

      gst_structure_get_int (s, "layer", &layer);
      gst_structure_get_int (s, "mpegaudioversion", &mpegaudioversion);

      if (mpegaudioversion == 1 && layer == 1)
        memcpy (&ret->sound_essence_compression,
            &sound_essence_compression_mpeg1_layer1, 16);
      else if (mpegaudioversion == 1 && (layer == 2 || layer == 3))
        memcpy (&ret->sound_essence_compression,
            &sound_essence_compression_mpeg1_layer23, 16);
      else if (mpegaudioversion == 2 && layer == 1)
        memcpy (&ret->sound_essence_compression,
            &sound_essence_compression_mpeg2_layer1, 16);

      if (layer == 1)
        md->spf = 384;
      else if (layer == 2 || mpegaudioversion == 1)
        md->spf = 1152;
      else
        md->spf = 576;          /* MPEG-2 or 2.5 */

      /* Otherwise all 0x00, must be some kind of mpeg1 audio */
    } else if (mpegversion == 2) {
      memcpy (&ret->sound_essence_compression, &sound_essence_compression_aac,
          16);
      md->spf = 1024;           /* FIXME: is this correct? */
    }
  } else if (strcmp (gst_structure_get_name (s), "audio/x-ac3") == 0) {
    memcpy (&ret->sound_essence_compression, &sound_essence_compression_ac3,
        16);
    md->spf = 256;              /* FIXME: is this correct? */
  } else {
    g_assert_not_reached ();
  }

  if (!gst_structure_get_int (s, "rate", &rate)) {
    GST_ERROR ("Invalid rate");
    g_object_unref (ret);
    return NULL;
  }
  md->rate = rate;

  memcpy (&ret->parent.essence_container, &mpeg_essence_container_ul, 16);

  ret->parent.essence_container.u[13] = 0x04;
  ret->parent.essence_container.u[14] = 0x40;

  if (!mxf_metadata_generic_sound_essence_descriptor_from_caps (ret, caps)) {
    g_object_unref (ret);
    return NULL;
  }

  *handler = mxf_mpeg_audio_write_func;

  return (MXFMetadataFileDescriptor *) ret;
}

static void
mxf_mpeg_audio_update_descriptor (MXFMetadataFileDescriptor * d, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf)
{
  return;
}

static void
mxf_mpeg_audio_get_edit_rate (MXFMetadataFileDescriptor * a, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf, MXFMetadataSourcePackage * package,
    MXFMetadataTimelineTrack * track, MXFFraction * edit_rate)
{
  MPEGAudioMappingData *md = mapping_data;

  edit_rate->n = md->rate;
  edit_rate->d = md->spf;
}

static guint32
mxf_mpeg_audio_get_track_number_template (MXFMetadataFileDescriptor * a,
    GstCaps * caps, gpointer mapping_data)
{
  return (0x16 << 24) | (0x05 << 8);
}

static MXFEssenceElementWriter mxf_mpeg_audio_essence_element_writer = {
  mxf_mpeg_audio_get_descriptor,
  mxf_mpeg_audio_update_descriptor,
  mxf_mpeg_audio_get_edit_rate,
  mxf_mpeg_audio_get_track_number_template,
  NULL,
  {{0,}}
};

#define MPEG_AUDIO_CAPS \
      "audio/mpeg, " \
      "mpegversion = (int) 1, " \
      "layer = (int) [ 1, 3 ], " \
      "rate = (int) [ 8000, 48000 ], " \
      "channels = (int) [ 1, 2 ], " \
      "parsed = (boolean) TRUE; " \
      "audio/x-ac3, " \
      "rate = (int) [ 4000, 96000 ], " \
      "channels = (int) [ 1, 6 ]; " \
      "audio/mpeg, " \
      "mpegversion = (int) 2, " \
      "rate = (int) [ 8000, 96000 ], " \
      "channels = (int) [ 1, 8 ]"

/* See ISO/IEC 13818-2 for MPEG ES format */
static gboolean
mxf_mpeg_is_mpeg2_frame (GstBuffer * buffer)
{
  GstByteReader reader;
  guint32 tmp;
  GstMapInfo map;
  gboolean ret = FALSE;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  gst_byte_reader_init (&reader, map.data, map.size);

  while (gst_byte_reader_get_remaining (&reader) > 3) {
    if (gst_byte_reader_peek_uint24_be (&reader, &tmp) && tmp == 0x000001) {
      guint8 type = 0;

      /* Found sync code */
      gst_byte_reader_skip_unchecked (&reader, 3);

      if (!gst_byte_reader_get_uint8 (&reader, &type))
        break;

      /* PICTURE */
      if (type == 0x00) {
        ret = TRUE;
        goto done;
      }
    } else {
      if (gst_byte_reader_skip (&reader, 1) == FALSE)
        break;
    }
  }

done:
  gst_buffer_unmap (buffer, &map);

  return ret;
}

static gboolean
mxf_mpeg_is_mpeg4_frame (GstBuffer * buffer)
{
  GstByteReader reader;
  guint32 tmp;
  GstMapInfo map;
  gboolean ret = FALSE;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  gst_byte_reader_init (&reader, map.data, map.size);

  while (gst_byte_reader_get_remaining (&reader) > 3) {
    if (gst_byte_reader_peek_uint24_be (&reader, &tmp) && tmp == 0x000001) {
      guint8 type = 0;

      /* Found sync code */
      gst_byte_reader_skip_unchecked (&reader, 3);

      if (!gst_byte_reader_get_uint8 (&reader, &type))
        break;

      /* PICTURE */
      if (type == 0xb6) {
        ret = TRUE;
        goto done;
      }
    } else {
      if (gst_byte_reader_skip (&reader, 1) == FALSE)
        break;
    }
  }

done:
  gst_buffer_unmap (buffer, &map);

  return ret;
}

static GstFlowReturn
mxf_mpeg_video_write_func (GstBuffer * buffer,
    gpointer mapping_data, GstAdapter * adapter, GstBuffer ** outbuf,
    gboolean flush)
{
  MXFMPEGEssenceType type = MXF_MPEG_ESSENCE_TYPE_OTHER;

  if (mapping_data)
    type = *((MXFMPEGEssenceType *) mapping_data);

  if (type == MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG2) {
    if (buffer && !mxf_mpeg_is_mpeg2_frame (buffer)) {
      gst_adapter_push (adapter, buffer);
      *outbuf = NULL;
      return GST_FLOW_OK;
    } else if (buffer || gst_adapter_available (adapter)) {
      guint av = gst_adapter_available (adapter);
      GstBuffer *ret;
      GstMapInfo map;

      if (buffer)
        ret = gst_buffer_new_and_alloc (gst_buffer_get_size (buffer) + av);
      else
        ret = gst_buffer_new_and_alloc (av);

      gst_buffer_map (ret, &map, GST_MAP_WRITE);
      if (av) {
        gconstpointer data = gst_adapter_map (adapter, av);
        memcpy (map.data, data, av);
        gst_adapter_unmap (adapter);
      }

      if (buffer) {
        GstMapInfo buffermap;
        gst_buffer_map (buffer, &buffermap, GST_MAP_READ);
        memcpy (map.data + av, buffermap.data, buffermap.size);
        gst_buffer_unmap (buffer, &buffermap);
        gst_buffer_unref (buffer);
      }

      gst_buffer_unmap (ret, &map);

      *outbuf = ret;
      return GST_FLOW_OK;
    }
  } else if (type == MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG4) {
    if (buffer && !mxf_mpeg_is_mpeg4_frame (buffer)) {
      gst_adapter_push (adapter, buffer);
      *outbuf = NULL;
      return GST_FLOW_OK;
    } else if (buffer || gst_adapter_available (adapter)) {
      guint av = gst_adapter_available (adapter);
      GstBuffer *ret;
      GstMapInfo map;

      if (buffer)
        ret = gst_buffer_new_and_alloc (gst_buffer_get_size (buffer) + av);
      else
        ret = gst_buffer_new_and_alloc (av);

      gst_buffer_map (ret, &map, GST_MAP_WRITE);
      if (av) {
        gconstpointer data = gst_adapter_map (adapter, av);
        memcpy (map.data, data, av);
        gst_adapter_unmap (adapter);
      }

      if (buffer) {
        GstMapInfo buffermap;
        gst_buffer_map (buffer, &buffermap, GST_MAP_READ);
        memcpy (map.data + av, buffermap.data, buffermap.size);
        gst_buffer_unmap (buffer, &buffermap);
        gst_buffer_unref (buffer);
      }

      gst_buffer_unmap (ret, &map);

      *outbuf = ret;
      return GST_FLOW_OK;
    }
  }

  *outbuf = buffer;
  return GST_FLOW_OK;
}

static const guint8 mpeg_video_picture_essence_compression_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x00,
  0x04, 0x01, 0x02, 0x02, 0x01, 0x00, 0x00, 0x00
};

static MXFMetadataFileDescriptor *
mxf_mpeg_video_get_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataMPEGVideoDescriptor *ret;
  GstStructure *s;

  ret = (MXFMetadataMPEGVideoDescriptor *)
      g_object_new (MXF_TYPE_METADATA_MPEG_VIDEO_DESCRIPTOR, NULL);

  s = gst_caps_get_structure (caps, 0);

  memcpy (&ret->parent.parent.parent.essence_container,
      &mpeg_essence_container_ul, 16);

  memcpy (&ret->parent.parent.picture_essence_coding,
      &mpeg_video_picture_essence_compression_ul, 16);
  if (strcmp (gst_structure_get_name (s), "video/mpeg") == 0) {
    gint mpegversion;

    if (!gst_structure_get_int (s, "mpegversion", &mpegversion)) {
      GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, caps);
      g_object_unref (ret);
      return NULL;
    }

    if (mpegversion == 1) {
      MXFMPEGEssenceType type = MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG2;

      *mapping_data = g_new0 (MXFMPEGEssenceType, 1);
      memcpy (*mapping_data, &type, sizeof (MXFMPEGEssenceType));
      ret->parent.parent.picture_essence_coding.u[7] = 0x03;
      ret->parent.parent.picture_essence_coding.u[13] = 0x10;
      ret->parent.parent.parent.essence_container.u[13] = 0x04;
      ret->parent.parent.parent.essence_container.u[14] = 0x60;
    } else if (mpegversion == 2) {
      MXFMPEGEssenceType type = MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG2;

      *mapping_data = g_new0 (MXFMPEGEssenceType, 1);
      memcpy (*mapping_data, &type, sizeof (MXFMPEGEssenceType));
      ret->parent.parent.picture_essence_coding.u[7] = 0x01;
      ret->parent.parent.picture_essence_coding.u[13] = 0x01;
      ret->parent.parent.parent.essence_container.u[13] = 0x04;
      ret->parent.parent.parent.essence_container.u[14] = 0x60;
    } else {
      const GValue *v;
      const GstBuffer *codec_data;
      MXFMPEGEssenceType type = MXF_MPEG_ESSENCE_TYPE_VIDEO_MPEG4;

      *mapping_data = g_new0 (MXFMPEGEssenceType, 1);
      memcpy (*mapping_data, &type, sizeof (MXFMPEGEssenceType));

      ret->parent.parent.picture_essence_coding.u[7] = 0x03;
      ret->parent.parent.picture_essence_coding.u[13] = 0x20;
      ret->parent.parent.parent.essence_container.u[13] = 0x04;
      ret->parent.parent.parent.essence_container.u[14] = 0x60;
      if ((v = gst_structure_get_value (s, "codec_data"))) {
        MXFLocalTag *t = g_new0 (MXFLocalTag, 1);
        GstMapInfo map;

        codec_data = gst_value_get_buffer (v);
        gst_buffer_map ((GstBuffer *) codec_data, &map, GST_MAP_READ);
        t->size = map.size;
        t->data = g_memdup2 (map.data, map.size);
        gst_buffer_unmap ((GstBuffer *) codec_data, &map);
        memcpy (&t->ul, &sony_mpeg4_extradata, 16);
        mxf_local_tag_insert (t, &MXF_METADATA_BASE (ret)->other_tags);
      }
    }
  } else if (strcmp (gst_structure_get_name (s), "video/x-h264") == 0) {
    MXFMPEGEssenceType type = MXF_MPEG_ESSENCE_TYPE_VIDEO_AVC;

    *mapping_data = g_new0 (MXFMPEGEssenceType, 1);
    memcpy (*mapping_data, &type, sizeof (MXFMPEGEssenceType));
    ret->parent.parent.picture_essence_coding.u[7] = 0x0a;
    ret->parent.parent.picture_essence_coding.u[13] = 0x30;
    ret->parent.parent.parent.essence_container.u[7] = 0x0a;
    ret->parent.parent.parent.essence_container.u[13] = 0x10;
    ret->parent.parent.parent.essence_container.u[14] = 0x60;
  } else {
    g_assert_not_reached ();
  }


  if (!mxf_metadata_generic_picture_essence_descriptor_from_caps (&ret->parent.
          parent, caps)) {
    g_object_unref (ret);
    return NULL;
  }

  *handler = mxf_mpeg_video_write_func;

  return (MXFMetadataFileDescriptor *) ret;
}

static void
mxf_mpeg_video_update_descriptor (MXFMetadataFileDescriptor * d, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf)
{
  return;
}

static void
mxf_mpeg_video_get_edit_rate (MXFMetadataFileDescriptor * a, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf, MXFMetadataSourcePackage * package,
    MXFMetadataTimelineTrack * track, MXFFraction * edit_rate)
{
  (*edit_rate).n = a->sample_rate.n;
  (*edit_rate).d = a->sample_rate.d;
}

static guint32
mxf_mpeg_video_get_track_number_template (MXFMetadataFileDescriptor * a,
    GstCaps * caps, gpointer mapping_data)
{
  return (0x15 << 24) | (0x05 << 8);
}

static MXFEssenceElementWriter mxf_mpeg_video_essence_element_writer = {
  mxf_mpeg_video_get_descriptor,
  mxf_mpeg_video_update_descriptor,
  mxf_mpeg_video_get_edit_rate,
  mxf_mpeg_video_get_track_number_template,
  NULL,
  {{0,}}
};

#define MPEG_VIDEO_CAPS \
"video/mpeg, " \
"mpegversion = (int) { 1, 2, 4 }, " \
"systemstream = (boolean) FALSE, " \
"width = " GST_VIDEO_SIZE_RANGE ", " \
"height = " GST_VIDEO_SIZE_RANGE ", " \
"framerate = " GST_VIDEO_FPS_RANGE "; " \
"video/x-h264, " \
"stream-format = (string) byte-stream, " \
"width = " GST_VIDEO_SIZE_RANGE ", " \
"height = " GST_VIDEO_SIZE_RANGE ", " \
"framerate = " GST_VIDEO_FPS_RANGE

void
mxf_mpeg_init (void)
{
  mxf_metadata_register (MXF_TYPE_METADATA_MPEG_VIDEO_DESCRIPTOR);
  mxf_essence_element_handler_register (&mxf_mpeg_essence_element_handler);

  mxf_mpeg_audio_essence_element_writer.pad_template =
      gst_pad_template_new ("mpeg_audio_sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      gst_caps_from_string (MPEG_AUDIO_CAPS));
  memcpy (&mxf_mpeg_audio_essence_element_writer.data_definition,
      mxf_metadata_track_identifier_get (MXF_METADATA_TRACK_SOUND_ESSENCE), 16);
  mxf_essence_element_writer_register (&mxf_mpeg_audio_essence_element_writer);

  mxf_mpeg_video_essence_element_writer.pad_template =
      gst_pad_template_new ("mpeg_video_sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      gst_caps_from_string (MPEG_VIDEO_CAPS));
  memcpy (&mxf_mpeg_video_essence_element_writer.data_definition,
      mxf_metadata_track_identifier_get (MXF_METADATA_TRACK_PICTURE_ESSENCE),
      16);
  mxf_essence_element_writer_register (&mxf_mpeg_video_essence_element_writer);

}

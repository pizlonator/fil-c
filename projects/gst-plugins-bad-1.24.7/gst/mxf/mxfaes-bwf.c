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

/* Implementation of SMPTE 382M - Mapping AES3 and Broadcast Wave
 * Audio into the MXF Generic Container
 */

/* TODO:
 * - Handle the case were a track only references specific channels
 *   of the essence (ChannelID property)
 * - Add support for more codecs 
 * - Handle more of the metadata inside the descriptors
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>

#include <string.h>

#include "mxfaes-bwf.h"
#include "mxfessence.h"
#include "mxfquark.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

/* SMPTE 382M Annex 1 */
#define MXF_TYPE_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR \
  (mxf_metadata_wave_audio_essence_descriptor_get_type())
#define MXF_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR, MXFMetadataWaveAudioEssenceDescriptor))
#define MXF_IS_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR))
typedef struct _MXFMetadataWaveAudioEssenceDescriptor
    MXFMetadataWaveAudioEssenceDescriptor;
typedef MXFMetadataClass MXFMetadataWaveAudioEssenceDescriptorClass;
GType mxf_metadata_wave_audio_essence_descriptor_get_type (void);

struct _MXFMetadataWaveAudioEssenceDescriptor
{
  MXFMetadataGenericSoundEssenceDescriptor parent;

  guint16 block_align;
  guint8 sequence_offset;

  guint32 avg_bps;

  MXFUL channel_assignment;

  guint32 peak_envelope_version;
  guint32 peak_envelope_format;
  guint32 points_per_peak_value;
  guint32 peak_envelope_block_size;
  guint32 peak_channels;
  guint32 peak_frames;
  gint64 peak_of_peaks_position;
  MXFTimestamp peak_envelope_timestamp;

  guint8 *peak_envelope_data;
  guint16 peak_envelope_data_length;
};

/* SMPTE 382M Annex 2 */
#define MXF_TYPE_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR \
  (mxf_metadata_aes3_audio_essence_descriptor_get_type())
#define MXF_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR, MXFMetadataAES3AudioEssenceDescriptor))
#define MXF_IS_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR))
typedef struct _MXFMetadataAES3AudioEssenceDescriptor
    MXFMetadataAES3AudioEssenceDescriptor;
typedef MXFMetadataClass MXFMetadataAES3AudioEssenceDescriptorClass;
GType mxf_metadata_aes3_audio_essence_descriptor_get_type (void);

struct _MXFMetadataAES3AudioEssenceDescriptor
{
  MXFMetadataWaveAudioEssenceDescriptor parent;

  guint8 emphasis;
  guint16 block_start_offset;
  guint8 auxiliary_bits_mode;

  guint32 n_channel_status_mode;
  guint8 *channel_status_mode;

  guint32 n_fixed_channel_status_data;
  guint8 **fixed_channel_status_data;

  guint32 n_user_data_mode;
  guint8 *user_data_mode;

  guint32 n_fixed_user_data;
  guint8 **fixed_user_data;

  guint32 linked_timecode_track_id;
  guint8 stream_number;
};

/* SMPTE 382M Annex 1 */
G_DEFINE_TYPE (MXFMetadataWaveAudioEssenceDescriptor,
    mxf_metadata_wave_audio_essence_descriptor,
    MXF_TYPE_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR);

static gboolean
mxf_metadata_wave_audio_essence_descriptor_handle_tag (MXFMetadataBase *
    metadata, MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFMetadataWaveAudioEssenceDescriptor *self =
      MXF_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif

  switch (tag) {
    case 0x3d0a:
      if (tag_size != 2)
        goto error;
      self->block_align = GST_READ_UINT16_BE (tag_data);
      GST_DEBUG ("  block align = %u", self->block_align);
      break;
    case 0x3d0b:
      if (tag_size != 1)
        goto error;
      self->sequence_offset = GST_READ_UINT8 (tag_data);
      GST_DEBUG ("  sequence offset = %u", self->sequence_offset);
      break;
    case 0x3d09:
      if (tag_size != 4)
        goto error;
      self->avg_bps = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  average bps = %u", self->avg_bps);
      break;
    case 0x3d32:
      if (tag_size != 16)
        goto error;
      memcpy (&self->channel_assignment, tag_data, 16);
      GST_DEBUG ("  channel assignment = %s",
          mxf_ul_to_string (&self->channel_assignment, str));
      break;
    case 0x3d29:
      if (tag_size != 4)
        goto error;
      self->peak_envelope_version = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  peak envelope version = %u", self->peak_envelope_version);
      break;
    case 0x3d2a:
      if (tag_size != 4)
        goto error;
      self->peak_envelope_format = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  peak envelope format = %u", self->peak_envelope_format);
      break;
    case 0x3d2b:
      if (tag_size != 4)
        goto error;
      self->points_per_peak_value = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  points per peak value = %u", self->points_per_peak_value);
      break;
    case 0x3d2c:
      if (tag_size != 4)
        goto error;
      self->peak_envelope_block_size = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  peak envelope block size = %u",
          self->peak_envelope_block_size);
      break;
    case 0x3d2d:
      if (tag_size != 4)
        goto error;
      self->peak_channels = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  peak channels = %u", self->peak_channels);
      break;
    case 0x3d2e:
      if (tag_size != 4)
        goto error;
      self->peak_frames = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  peak frames = %u", self->peak_frames);
      break;
    case 0x3d2f:
      if (tag_size != 8)
        goto error;
      self->peak_of_peaks_position = GST_READ_UINT64_BE (tag_data);
      GST_DEBUG ("  peak of peaks position = %" G_GINT64_FORMAT,
          self->peak_of_peaks_position);
      break;
    case 0x3d30:
      if (!mxf_timestamp_parse (&self->peak_envelope_timestamp,
              tag_data, tag_size))
        goto error;
      GST_DEBUG ("  peak envelope timestamp = %s",
          mxf_timestamp_to_string (&self->peak_envelope_timestamp, str));
      break;
    case 0x3d31:
      self->peak_envelope_data = g_memdup2 (tag_data, tag_size);
      self->peak_envelope_data_length = tag_size;
      GST_DEBUG ("  peak evelope data size = %u",
          self->peak_envelope_data_length);
      break;
    default:
      ret =
          MXF_METADATA_BASE_CLASS
          (mxf_metadata_wave_audio_essence_descriptor_parent_class)->handle_tag
          (metadata, primer, tag, tag_data, tag_size);
      break;
  }

  return ret;

error:

  GST_ERROR
      ("Invalid wave audio essence descriptor local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static GstStructure *
mxf_metadata_wave_audio_essence_descriptor_to_structure (MXFMetadataBase * m)
{
  GstStructure *ret =
      MXF_METADATA_BASE_CLASS
      (mxf_metadata_wave_audio_essence_descriptor_parent_class)->to_structure
      (m);
  MXFMetadataWaveAudioEssenceDescriptor *self =
      MXF_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR (m);
  gchar str[48];

  gst_structure_id_set (ret, MXF_QUARK (BLOCK_ALIGN), G_TYPE_UINT,
      self->block_align, NULL);

  if (self->sequence_offset)
    gst_structure_id_set (ret, MXF_QUARK (SEQUENCE_OFFSET), G_TYPE_UCHAR,
        self->sequence_offset, NULL);

  if (self->avg_bps)
    gst_structure_id_set (ret, MXF_QUARK (AVG_BPS), G_TYPE_UINT, self->avg_bps,
        NULL);

  if (!mxf_ul_is_zero (&self->channel_assignment)) {
    gst_structure_id_set (ret, MXF_QUARK (CHANNEL_ASSIGNMENT), G_TYPE_STRING,
        mxf_ul_to_string (&self->channel_assignment, str), NULL);
  }

  if (self->peak_envelope_version)
    gst_structure_id_set (ret, MXF_QUARK (PEAK_ENVELOPE_VERSION), G_TYPE_UINT,
        self->peak_envelope_version, NULL);

  if (self->peak_envelope_format)
    gst_structure_id_set (ret, MXF_QUARK (PEAK_ENVELOPE_FORMAT), G_TYPE_UINT,
        self->peak_envelope_format, NULL);

  if (self->points_per_peak_value)
    gst_structure_id_set (ret, MXF_QUARK (POINTS_PER_PEAK_VALUE), G_TYPE_UINT,
        self->points_per_peak_value, NULL);

  if (self->peak_envelope_block_size)
    gst_structure_id_set (ret, MXF_QUARK (PEAK_ENVELOPE_BLOCK_SIZE),
        G_TYPE_UINT, self->peak_envelope_block_size, NULL);

  if (self->peak_channels)
    gst_structure_id_set (ret, MXF_QUARK (PEAK_CHANNELS), G_TYPE_UINT,
        self->peak_channels, NULL);

  if (self->peak_frames)
    gst_structure_id_set (ret, MXF_QUARK (PEAK_FRAMES), G_TYPE_UINT,
        self->peak_frames, NULL);

  if (self->peak_of_peaks_position)
    gst_structure_id_set (ret, MXF_QUARK (PEAK_OF_PEAKS_POSITION), G_TYPE_INT64,
        self->peak_of_peaks_position, NULL);

  if (!mxf_timestamp_is_unknown (&self->peak_envelope_timestamp))
    gst_structure_id_set (ret, MXF_QUARK (PEAK_ENVELOPE_TIMESTAMP),
        G_TYPE_STRING, mxf_timestamp_to_string (&self->peak_envelope_timestamp,
            str), NULL);

  if (self->peak_envelope_data) {
    GstBuffer *buf = gst_buffer_new_and_alloc (self->peak_envelope_data_length);
    GstMapInfo map;

    gst_buffer_map (buf, &map, GST_MAP_WRITE);
    memcpy (map.data, self->peak_envelope_data,
        self->peak_envelope_data_length);
    gst_buffer_unmap (buf, &map);
    gst_structure_id_set (ret, MXF_QUARK (PEAK_ENVELOPE_DATA), GST_TYPE_BUFFER,
        buf, NULL);
    gst_buffer_unref (buf);
  }

  return ret;
}

static GList *
mxf_metadata_wave_audio_essence_descriptor_write_tags (MXFMetadataBase * m,
    MXFPrimerPack * primer)
{
  MXFMetadataWaveAudioEssenceDescriptor *self =
      MXF_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR (m);
  GList *ret =
      MXF_METADATA_BASE_CLASS
      (mxf_metadata_wave_audio_essence_descriptor_parent_class)->write_tags (m,
      primer);
  MXFLocalTag *t;
  static const guint8 block_align_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 sequence_offset_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x03, 0x02, 0x02, 0x00, 0x00, 0x00
  };
  static const guint8 avg_bps_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x03, 0x03, 0x05, 0x00, 0x00, 0x00
  };
  static const guint8 channel_assignment_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x07,
    0x04, 0x02, 0x01, 0x01, 0x05, 0x00, 0x00, 0x00
  };
  static const guint8 peak_envelope_version_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x06, 0x00, 0x00, 0x00
  };
  static const guint8 peak_envelope_format_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x07, 0x00, 0x00, 0x00
  };
  static const guint8 points_per_peak_value_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x08, 0x00, 0x00, 0x00
  };
  static const guint8 peak_envelope_block_size_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x09, 0x00, 0x00, 0x00
  };
  static const guint8 peak_channels_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x0A, 0x00, 0x00, 0x00
  };
  static const guint8 peak_frames_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x0B, 0x00, 0x00, 0x00
  };
  static const guint8 peak_of_peaks_position_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x0C, 0x00, 0x00, 0x00
  };
  static const guint8 peak_envelope_timestamp_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x0D, 0x00, 0x00, 0x00
  };
  static const guint8 peak_envelope_data_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x08,
    0x04, 0x02, 0x03, 0x01, 0x0E, 0x00, 0x00, 0x00
  };

  t = g_new0 (MXFLocalTag, 1);
  memcpy (&t->ul, &block_align_ul, 16);
  t->size = 2;
  t->data = g_malloc (t->size);
  GST_WRITE_UINT16_BE (t->data, self->block_align);
  mxf_primer_pack_add_mapping (primer, 0x3d0a, &t->ul);
  ret = g_list_prepend (ret, t);

  if (self->sequence_offset) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &sequence_offset_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, self->sequence_offset);
    mxf_primer_pack_add_mapping (primer, 0x3d0b, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  t = g_new0 (MXFLocalTag, 1);
  memcpy (&t->ul, &avg_bps_ul, 16);
  t->size = 4;
  t->data = g_malloc (t->size);
  GST_WRITE_UINT32_BE (t->data, self->avg_bps);
  mxf_primer_pack_add_mapping (primer, 0x3d09, &t->ul);
  ret = g_list_prepend (ret, t);

  if (!mxf_ul_is_zero (&self->channel_assignment)) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &channel_assignment_ul, 16);
    t->size = 16;
    t->data = g_malloc (t->size);
    memcpy (t->data, &self->channel_assignment, 16);
    mxf_primer_pack_add_mapping (primer, 0x3d32, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->peak_envelope_version) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &peak_envelope_version_ul, 16);
    t->size = 4;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->peak_envelope_version);
    mxf_primer_pack_add_mapping (primer, 0x3d29, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->peak_envelope_format) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &peak_envelope_format_ul, 16);
    t->size = 4;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->peak_envelope_format);
    mxf_primer_pack_add_mapping (primer, 0x3d2a, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->points_per_peak_value) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &points_per_peak_value_ul, 16);
    t->size = 4;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->points_per_peak_value);
    mxf_primer_pack_add_mapping (primer, 0x3d2b, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->peak_envelope_block_size) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &peak_envelope_block_size_ul, 16);
    t->size = 4;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->peak_envelope_block_size);
    mxf_primer_pack_add_mapping (primer, 0x3d2c, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->peak_channels) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &peak_channels_ul, 16);
    t->size = 4;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->peak_channels);
    mxf_primer_pack_add_mapping (primer, 0x3d2d, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->peak_frames) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &peak_frames_ul, 16);
    t->size = 4;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->peak_frames);
    mxf_primer_pack_add_mapping (primer, 0x3d2e, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->peak_of_peaks_position) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &peak_of_peaks_position_ul, 16);
    t->size = 8;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT64_BE (t->data, self->peak_of_peaks_position);
    mxf_primer_pack_add_mapping (primer, 0x3d2f, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (!mxf_timestamp_is_unknown (&self->peak_envelope_timestamp)) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &peak_envelope_timestamp_ul, 16);
    t->size = 8;
    t->data = g_malloc (t->size);
    mxf_timestamp_write (&self->peak_envelope_timestamp, t->data);
    mxf_primer_pack_add_mapping (primer, 0x3d30, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->peak_envelope_data) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &peak_envelope_data_ul, 16);
    t->size = self->peak_envelope_data_length;
    t->data = g_memdup2 (self->peak_envelope_data, t->size);
    mxf_primer_pack_add_mapping (primer, 0x3d31, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  return ret;
}

static void
    mxf_metadata_wave_audio_essence_descriptor_init
    (MXFMetadataWaveAudioEssenceDescriptor * self)
{

}

static void
    mxf_metadata_wave_audio_essence_descriptor_class_init
    (MXFMetadataWaveAudioEssenceDescriptorClass * klass)
{
  MXFMetadataBaseClass *metadata_base_class = (MXFMetadataBaseClass *) klass;
  MXFMetadataClass *metadata_class = (MXFMetadataClass *) klass;

  metadata_base_class->handle_tag =
      mxf_metadata_wave_audio_essence_descriptor_handle_tag;
  metadata_base_class->name_quark = MXF_QUARK (WAVE_AUDIO_ESSENCE_DESCRIPTOR);
  metadata_base_class->to_structure =
      mxf_metadata_wave_audio_essence_descriptor_to_structure;
  metadata_base_class->write_tags =
      mxf_metadata_wave_audio_essence_descriptor_write_tags;
  metadata_class->type = 0x0148;
}

/* SMPTE 382M Annex 2 */
G_DEFINE_TYPE (MXFMetadataAES3AudioEssenceDescriptor,
    mxf_metadata_aes3_audio_essence_descriptor,
    MXF_TYPE_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR);

static void
mxf_metadata_aes3_audio_essence_descriptor_finalize (GObject * object)
{
  MXFMetadataAES3AudioEssenceDescriptor *self =
      MXF_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR (object);

  g_free (self->channel_status_mode);
  self->channel_status_mode = NULL;
  g_free (self->fixed_channel_status_data);
  self->fixed_channel_status_data = NULL;
  g_free (self->user_data_mode);
  self->user_data_mode = NULL;
  g_free (self->fixed_user_data);
  self->fixed_user_data = NULL;

  G_OBJECT_CLASS
      (mxf_metadata_aes3_audio_essence_descriptor_parent_class)->finalize
      (object);
}

static gboolean
mxf_metadata_aes3_audio_essence_descriptor_handle_tag (MXFMetadataBase *
    metadata, MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFMetadataAES3AudioEssenceDescriptor *self =
      MXF_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR (metadata);
  gboolean ret = TRUE;

  switch (tag) {
    case 0x3d0d:
      if (tag_size != 1)
        goto error;
      self->emphasis = GST_READ_UINT8 (tag_data);
      GST_DEBUG ("  emphasis = %u", self->emphasis);
      break;
    case 0x3d0f:
      if (tag_size != 2)
        goto error;
      self->block_start_offset = GST_READ_UINT16_BE (tag_data);
      GST_DEBUG ("  block start offset = %u", self->block_start_offset);
      break;
    case 0x3d08:
      if (tag_size != 1)
        goto error;
      self->auxiliary_bits_mode = GST_READ_UINT8 (tag_data);
      GST_DEBUG ("  auxiliary bits mode = %u", self->auxiliary_bits_mode);
      break;
    case 0x3d10:{
      guint32 len;
      guint i;

      if (tag_size < 8)
        goto error;
      len = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  number of channel status mode = %u", len);
      self->n_channel_status_mode = len;
      if (len == 0)
        return TRUE;

      if (GST_READ_UINT32_BE (tag_data + 4) != 1)
        goto error;

      tag_data += 8;
      tag_size -= 8;

      if (tag_size != len)
        goto error;

      self->channel_status_mode = g_new0 (guint8, len);

      for (i = 0; i < len; i++) {
        self->channel_status_mode[i] = GST_READ_UINT8 (tag_data);
        GST_DEBUG ("    channel status mode %u = %u", i,
            self->channel_status_mode[i]);
        tag_data++;
        tag_size--;
      }

      break;
    }
    case 0x3d11:{
      guint32 len;
      guint i;

      if (tag_size < 8)
        goto error;
      len = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  number of fixed channel status data = %u", len);
      self->n_fixed_channel_status_data = len;
      if (len == 0)
        return TRUE;

      if (GST_READ_UINT32_BE (tag_data + 4) != 24)
        goto error;

      tag_data += 8;
      tag_size -= 8;

      if (tag_size / 24 != len)
        goto error;

      if (G_MAXINT / (24 + sizeof (guint8 *)) < len)
        goto error;

      self->fixed_channel_status_data =
          g_malloc0 (len * (sizeof (guint8 *) + 24));

      for (i = 0; i < len; i++) {
        self->fixed_channel_status_data[i] =
            ((guint8 *) self->fixed_channel_status_data) +
            len * sizeof (guint8 *) + i * 24;

        memcpy (self->fixed_channel_status_data[i], tag_data, 24);
        GST_DEBUG
            ("    fixed channel status data %u = 0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x",
            i, self->fixed_channel_status_data[i][0],
            self->fixed_channel_status_data[i][1],
            self->fixed_channel_status_data[i][2],
            self->fixed_channel_status_data[i][3],
            self->fixed_channel_status_data[i][4],
            self->fixed_channel_status_data[i][5],
            self->fixed_channel_status_data[i][6],
            self->fixed_channel_status_data[i][7],
            self->fixed_channel_status_data[i][8],
            self->fixed_channel_status_data[i][9],
            self->fixed_channel_status_data[i][10],
            self->fixed_channel_status_data[i][11],
            self->fixed_channel_status_data[i][12],
            self->fixed_channel_status_data[i][13],
            self->fixed_channel_status_data[i][14],
            self->fixed_channel_status_data[i][15],
            self->fixed_channel_status_data[i][16],
            self->fixed_channel_status_data[i][17],
            self->fixed_channel_status_data[i][18],
            self->fixed_channel_status_data[i][19],
            self->fixed_channel_status_data[i][20],
            self->fixed_channel_status_data[i][21],
            self->fixed_channel_status_data[i][22],
            self->fixed_channel_status_data[i][23]
            );
        tag_data += 24;
        tag_size -= 24;
      }

      break;
    }
    case 0x3d12:{
      guint32 len;
      guint i;

      if (tag_size < 8)
        goto error;
      len = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  number of user data mode = %u", len);
      self->n_user_data_mode = len;
      if (len == 0)
        return TRUE;

      if (GST_READ_UINT32_BE (tag_data + 4) != 1)
        goto error;

      tag_data += 8;
      tag_size -= 8;

      if (tag_size != len)
        goto error;

      self->user_data_mode = g_new0 (guint8, len);

      for (i = 0; i < len; i++) {
        self->user_data_mode[i] = GST_READ_UINT8 (tag_data);
        GST_DEBUG ("    user data mode %u = %u", i, self->user_data_mode[i]);
        tag_data++;
        tag_size--;
      }

      break;
    }
    case 0x3d13:{
      guint32 len;
      guint i;

      if (tag_size < 8)
        goto error;
      len = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("  number of fixed user data = %u", len);
      self->n_fixed_user_data = len;
      if (len == 0)
        return TRUE;

      if (GST_READ_UINT32_BE (tag_data + 4) != 24)
        goto error;

      tag_data += 8;
      tag_size -= 8;

      if (tag_size / 24 != len)
        goto error;

      if (G_MAXINT / (24 + sizeof (guint8 *)) < len)
        goto error;

      self->fixed_user_data = g_malloc0 (len * (sizeof (guint8 *) + 24));

      for (i = 0; i < len; i++) {
        self->fixed_user_data[i] =
            ((guint8 *) self->fixed_user_data) + len * sizeof (guint8 *) +
            i * 24;

        memcpy (self->fixed_user_data[i], tag_data, 24);
        GST_DEBUG
            ("    fixed user data %u = 0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x.0x%02x",
            i, self->fixed_user_data[i][0],
            self->fixed_user_data[i][1],
            self->fixed_user_data[i][2],
            self->fixed_user_data[i][3],
            self->fixed_user_data[i][4],
            self->fixed_user_data[i][5],
            self->fixed_user_data[i][6],
            self->fixed_user_data[i][7],
            self->fixed_user_data[i][8],
            self->fixed_user_data[i][9],
            self->fixed_user_data[i][10],
            self->fixed_user_data[i][11],
            self->fixed_user_data[i][12],
            self->fixed_user_data[i][13],
            self->fixed_user_data[i][14],
            self->fixed_user_data[i][15],
            self->fixed_user_data[i][16],
            self->fixed_user_data[i][17],
            self->fixed_user_data[i][18],
            self->fixed_user_data[i][19],
            self->fixed_user_data[i][20],
            self->fixed_user_data[i][21],
            self->fixed_user_data[i][22], self->fixed_user_data[i][23]
            );
        tag_data += 24;
        tag_size -= 24;
      }

      break;
    }
      /* TODO: linked timecode track / data_stream_number parsing, see
       * SMPTE 382M Annex 2 */
    default:
      ret =
          MXF_METADATA_BASE_CLASS
          (mxf_metadata_aes3_audio_essence_descriptor_parent_class)->handle_tag
          (metadata, primer, tag, tag_data, tag_size);
      break;
  }

  return ret;

error:

  GST_ERROR
      ("Invalid AES3 audio essence descriptor local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static GstStructure *
mxf_metadata_aes3_audio_essence_descriptor_to_structure (MXFMetadataBase * m)
{
  GstStructure *ret =
      MXF_METADATA_BASE_CLASS
      (mxf_metadata_aes3_audio_essence_descriptor_parent_class)->to_structure
      (m);
  MXFMetadataAES3AudioEssenceDescriptor *self =
      MXF_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR (m);

  if (self->emphasis)
    gst_structure_id_set (ret, MXF_QUARK (EMPHASIS), G_TYPE_UCHAR,
        self->emphasis, NULL);

  if (self->block_start_offset)
    gst_structure_id_set (ret, MXF_QUARK (BLOCK_START_OFFSET), G_TYPE_UINT,
        self->block_start_offset, NULL);

  if (self->auxiliary_bits_mode)
    gst_structure_id_set (ret, MXF_QUARK (AUXILIARY_BITS_MODE), G_TYPE_UCHAR,
        self->auxiliary_bits_mode, NULL);

  if (self->channel_status_mode) {
    GstBuffer *buf = gst_buffer_new_and_alloc (self->n_channel_status_mode);
    GstMapInfo map;

    gst_buffer_map (buf, &map, GST_MAP_WRITE);
    memcpy (map.data, self->channel_status_mode, self->n_channel_status_mode);
    gst_buffer_unmap (buf, &map);
    gst_structure_id_set (ret, MXF_QUARK (CHANNEL_STATUS_MODE), GST_TYPE_BUFFER,
        buf, NULL);
    gst_buffer_unref (buf);
  }

  if (self->channel_status_mode) {
    GstBuffer *buf = gst_buffer_new_and_alloc (self->n_channel_status_mode);
    GstMapInfo map;

    gst_buffer_map (buf, &map, GST_MAP_WRITE);
    memcpy (map.data, self->channel_status_mode, self->n_channel_status_mode);
    gst_buffer_unmap (buf, &map);
    gst_structure_id_set (ret, MXF_QUARK (CHANNEL_STATUS_MODE), GST_TYPE_BUFFER,
        buf, NULL);
    gst_buffer_unref (buf);
  }

  if (self->fixed_channel_status_data) {
    guint i;
    GValue va = { 0, }
    , v = {
      0,
    };
    GstBuffer *buf;
    GstMapInfo map;

    g_value_init (&va, GST_TYPE_ARRAY);

    for (i = 0; i < self->n_fixed_channel_status_data; i++) {
      buf = gst_buffer_new_and_alloc (24);
      g_value_init (&v, GST_TYPE_BUFFER);

      gst_buffer_map (buf, &map, GST_MAP_WRITE);
      memcpy (map.data, self->fixed_channel_status_data[i], 24);
      gst_buffer_unmap (buf, &map);
      gst_value_set_buffer (&v, buf);
      gst_value_array_append_value (&va, &v);
      gst_buffer_unref (buf);
      g_value_unset (&v);
    }

    if (gst_value_array_get_size (&va) > 0)
      gst_structure_id_set_value (ret, MXF_QUARK (FIXED_CHANNEL_STATUS_DATA),
          &va);
    g_value_unset (&va);
  }


  if (self->user_data_mode) {
    GstBuffer *buf = gst_buffer_new_and_alloc (self->n_user_data_mode);
    GstMapInfo map;

    gst_buffer_map (buf, &map, GST_MAP_WRITE);
    memcpy (map.data, self->user_data_mode, self->n_user_data_mode);
    gst_buffer_unmap (buf, &map);
    gst_structure_id_set (ret, MXF_QUARK (USER_DATA_MODE), GST_TYPE_BUFFER, buf,
        NULL);
    gst_buffer_unref (buf);
  }

  if (self->fixed_user_data) {
    guint i;
    GValue va = { 0, }
    , v = {
      0,
    };
    GstBuffer *buf;
    GstMapInfo map;

    g_value_init (&va, GST_TYPE_ARRAY);

    for (i = 0; i < self->n_fixed_user_data; i++) {
      buf = gst_buffer_new_and_alloc (24);
      g_value_init (&v, GST_TYPE_BUFFER);

      gst_buffer_map (buf, &map, GST_MAP_WRITE);
      memcpy (map.data, self->fixed_user_data[i], 24);
      gst_buffer_unmap (buf, &map);
      gst_value_set_buffer (&v, buf);
      gst_value_array_append_value (&va, &v);
      gst_buffer_unref (buf);
      g_value_unset (&v);
    }

    if (gst_value_array_get_size (&va) > 0)
      gst_structure_id_set_value (ret, MXF_QUARK (FIXED_USER_DATA), &va);
    g_value_unset (&va);
  }

  if (self->linked_timecode_track_id)
    gst_structure_id_set (ret, MXF_QUARK (LINKED_TIMECODE_TRACK_ID),
        G_TYPE_UINT, self->linked_timecode_track_id, NULL);

  if (self->stream_number)
    gst_structure_id_set (ret, MXF_QUARK (STREAM_NUMBER), G_TYPE_UCHAR,
        self->stream_number, NULL);

  return ret;
}

static GList *
mxf_metadata_aes3_audio_essence_descriptor_write_tags (MXFMetadataBase * m,
    MXFPrimerPack * primer)
{
  MXFMetadataAES3AudioEssenceDescriptor *self =
      MXF_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR (m);
  GList *ret =
      MXF_METADATA_BASE_CLASS
      (mxf_metadata_aes3_audio_essence_descriptor_parent_class)->write_tags (m,
      primer);
  MXFLocalTag *t;
  static const guint8 emphasis_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x05, 0x01, 0x06, 0x00, 0x00, 0x00
  };
  static const guint8 block_start_offset_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x03, 0x02, 0x03, 0x00, 0x00, 0x00
  };
  static const guint8 auxiliary_bits_mode_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x05, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 channel_status_mode_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x05, 0x01, 0x02, 0x00, 0x00, 0x00
  };
  static const guint8 fixed_channel_status_data_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x05, 0x01, 0x03, 0x00, 0x00, 0x00
  };
  static const guint8 user_data_mode_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x05, 0x01, 0x04, 0x00, 0x00, 0x00
  };
  static const guint8 fixed_user_data_ul[] = {
    0x06, 0x0E, 0x2B, 0x34, 0x01, 0x01, 0x01, 0x05,
    0x04, 0x02, 0x05, 0x01, 0x05, 0x00, 0x00, 0x00
  };

  if (self->emphasis) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &emphasis_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, self->emphasis);
    mxf_primer_pack_add_mapping (primer, 0x3d0d, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->block_start_offset) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &block_start_offset_ul, 16);
    t->size = 2;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT16_BE (t->data, self->block_start_offset);
    mxf_primer_pack_add_mapping (primer, 0x3d0f, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->auxiliary_bits_mode) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &auxiliary_bits_mode_ul, 16);
    t->size = 1;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT8 (t->data, self->auxiliary_bits_mode);
    mxf_primer_pack_add_mapping (primer, 0x3d08, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->channel_status_mode) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &channel_status_mode_ul, 16);
    t->size = 8 + self->n_channel_status_mode;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->n_channel_status_mode);
    GST_WRITE_UINT32_BE (t->data + 4, 1);
    memcpy (t->data + 8, self->channel_status_mode, t->size);
    mxf_primer_pack_add_mapping (primer, 0x3d10, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->fixed_channel_status_data) {
    guint i;

    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &fixed_channel_status_data_ul, 16);
    t->size = 8 + 24 * self->n_fixed_channel_status_data;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->n_fixed_channel_status_data);
    GST_WRITE_UINT32_BE (t->data + 4, 24);
    for (i = 0; i < self->n_fixed_channel_status_data; i++)
      memcpy (t->data + 8 + 24 * i, self->fixed_channel_status_data[i], 24);
    mxf_primer_pack_add_mapping (primer, 0x3d11, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->user_data_mode) {
    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &user_data_mode_ul, 16);
    t->size = 8 + self->n_user_data_mode;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->n_user_data_mode);
    GST_WRITE_UINT32_BE (t->data + 4, 1);
    memcpy (t->data + 8, self->user_data_mode, t->size);
    mxf_primer_pack_add_mapping (primer, 0x3d12, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  if (self->fixed_user_data) {
    guint i;

    t = g_new0 (MXFLocalTag, 1);
    memcpy (&t->ul, &fixed_user_data_ul, 16);
    t->size = 8 + 24 * self->n_fixed_user_data;
    t->data = g_malloc (t->size);
    GST_WRITE_UINT32_BE (t->data, self->n_fixed_user_data);
    GST_WRITE_UINT32_BE (t->data + 4, 24);
    for (i = 0; i < self->n_fixed_user_data; i++)
      memcpy (t->data + 8 + 24 * i, self->fixed_user_data[i], 24);
    mxf_primer_pack_add_mapping (primer, 0x3d11, &t->ul);
    ret = g_list_prepend (ret, t);
  }

  return ret;
}

static void
    mxf_metadata_aes3_audio_essence_descriptor_init
    (MXFMetadataAES3AudioEssenceDescriptor * self)
{

}

static void
    mxf_metadata_aes3_audio_essence_descriptor_class_init
    (MXFMetadataAES3AudioEssenceDescriptorClass * klass)
{
  MXFMetadataBaseClass *metadata_base_class = (MXFMetadataBaseClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataClass *metadata_class = (MXFMetadataClass *) klass;

  object_class->finalize = mxf_metadata_aes3_audio_essence_descriptor_finalize;
  metadata_base_class->handle_tag =
      mxf_metadata_aes3_audio_essence_descriptor_handle_tag;
  metadata_base_class->name_quark = MXF_QUARK (AES3_AUDIO_ESSENCE_DESCRIPTOR);
  metadata_base_class->to_structure =
      mxf_metadata_aes3_audio_essence_descriptor_to_structure;
  metadata_base_class->write_tags =
      mxf_metadata_aes3_audio_essence_descriptor_write_tags;
  metadata_class->type = 0x0147;
}

static gboolean
mxf_is_aes_bwf_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;

  /* SMPTE 382M 9 */
  return (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 &&
      key->u[13] == 0x06 &&
      (key->u[14] == 0x01 ||
          key->u[14] == 0x02 ||
          key->u[14] == 0x03 ||
          key->u[14] == 0x04 || key->u[14] == 0x08 || key->u[14] == 0x09 ||
          key->u[14] == 0x0a || key->u[14] == 0x0b));
}

static MXFEssenceWrapping
mxf_aes_bwf_get_track_wrapping (const MXFMetadataTimelineTrack * track)
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
    if (!MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->
            parent.descriptor[i]))
      continue;

    switch (track->parent.descriptor[i]->essence_container.u[14]) {
      case 0x01:
      case 0x03:
        return MXF_ESSENCE_WRAPPING_FRAME_WRAPPING;
        break;
      case 0x02:
      case 0x04:
        return MXF_ESSENCE_WRAPPING_CLIP_WRAPPING;
        break;
      case 0x08:
      case 0x09:
      case 0x0a:
      case 0x0b:
        return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
      default:
        GST_WARNING ("Unknown frame wrapping");
        return MXF_ESSENCE_WRAPPING_UNKNOWN_WRAPPING;
        break;
    }
  }

  return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
}

static GstFlowReturn
mxf_bwf_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  /* SMPTE 382M Table 1: Check if this is some kind of Wave element */
  if (key->u[12] != 0x16 || (key->u[14] != 0x01 && key->u[14] != 0x02
          && key->u[14] != 0x0b)) {
    GST_ERROR ("Invalid BWF essence element");
    return GST_FLOW_ERROR;
  }

  /* FIXME: check if the size is a multiply of the unit size, ... */
  return GST_FLOW_OK;
}

static GstFlowReturn
mxf_aes3_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps, MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  /* SMPTE 382M Table 1: Check if this is some kind of Wave element */
  if (key->u[12] != 0x16 || (key->u[14] != 0x03 && key->u[14] != 0x04
          && key->u[14] != 0x0c)) {
    GST_ERROR ("Invalid AES3 essence element");
    return GST_FLOW_ERROR;
  }

  /* FIXME: check if the size is a multiply of the unit size, ... */
  return GST_FLOW_OK;
}



/* SMPTE RP224 */
static const MXFUL mxf_sound_essence_compression_uncompressed =
    { {0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x01, 0x04, 0x02, 0x02, 0x01,
    0x7F, 0x00, 0x00, 0x00}
};

/* Also seems to be uncompressed */
static const MXFUL mxf_sound_essence_compression_s24le =
    { {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x0a, 0x04, 0x02, 0x02, 0x01,
    0x01, 0x00, 0x00, 0x00}
};

static const MXFUL mxf_sound_essence_compression_aiff =
    { {0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x07, 0x04, 0x02, 0x02, 0x01,
    0x7E, 0x00, 0x00, 0x00}
};

static const MXFUL mxf_sound_essence_compression_alaw =
    { {0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x03, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x01, 0x01, 0x00}
};

static GstCaps *
mxf_bwf_create_caps (MXFMetadataTimelineTrack * track,
    MXFMetadataGenericSoundEssenceDescriptor * descriptor, GstTagList ** tags,
    gboolean * intra_only,
    MXFEssenceElementHandleFunc * handler, gpointer * mapping_data)
{
  GstCaps *ret = NULL;
  MXFMetadataWaveAudioEssenceDescriptor *wa_descriptor = NULL;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  gchar *codec_name = NULL;

  if (MXF_IS_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR (descriptor))
    wa_descriptor = (MXFMetadataWaveAudioEssenceDescriptor *) descriptor;

  /* TODO: Handle width=!depth, needs shifting of samples */

  /* FIXME: set a channel layout */

  if (mxf_ul_is_zero (&descriptor->sound_essence_compression) ||
      mxf_ul_is_subclass (&mxf_sound_essence_compression_uncompressed,
          &descriptor->sound_essence_compression) ||
      mxf_ul_is_subclass (&mxf_sound_essence_compression_s24le,
          &descriptor->sound_essence_compression)) {
    guint block_align;
    GstAudioFormat audio_format;

    if (descriptor->channel_count == 0 ||
        descriptor->quantization_bits == 0 ||
        descriptor->audio_sampling_rate.n == 0 ||
        descriptor->audio_sampling_rate.d == 0) {
      GST_ERROR ("Invalid descriptor");
      return NULL;
    }

    /* XXX: block align value can be carried via audio essential descriptor but
     * there are some files with broken block align value.
     * Calculates the value always */
    block_align =
        (GST_ROUND_UP_8 (descriptor->quantization_bits) *
        descriptor->channel_count) / 8;

    audio_format =
        gst_audio_format_build_integer (block_align !=
        descriptor->channel_count, G_LITTLE_ENDIAN,
        (block_align / descriptor->channel_count) * 8,
        (block_align / descriptor->channel_count) * 8);
    ret =
        mxf_metadata_generic_sound_essence_descriptor_create_caps (descriptor,
        &audio_format);

    codec_name =
        g_strdup_printf ("Uncompressed %u-bit little endian integer PCM audio",
        (block_align / descriptor->channel_count) * 8);
  } else if (mxf_ul_is_subclass (&mxf_sound_essence_compression_aiff,
          &descriptor->sound_essence_compression)) {
    guint block_align;
    GstAudioFormat audio_format;

    if (descriptor->channel_count == 0 ||
        descriptor->quantization_bits == 0 ||
        descriptor->audio_sampling_rate.n == 0 ||
        descriptor->audio_sampling_rate.d == 0) {
      GST_ERROR ("Invalid descriptor");
      return NULL;
    }

    /* XXX: block align value can be carried via audio essential descriptor but
     * there are some files with broken block align value.
     * Calculates the value always */
    block_align =
        (GST_ROUND_UP_8 (descriptor->quantization_bits) *
        descriptor->channel_count) / 8;

    audio_format =
        gst_audio_format_build_integer (block_align !=
        descriptor->channel_count, G_BIG_ENDIAN,
        (block_align / descriptor->channel_count) * 8,
        (block_align / descriptor->channel_count) * 8);
    ret =
        mxf_metadata_generic_sound_essence_descriptor_create_caps (descriptor,
        &audio_format);

    codec_name =
        g_strdup_printf ("Uncompressed %u-bit big endian integer PCM audio",
        (block_align / descriptor->channel_count) * 8);
  } else if (mxf_ul_is_subclass (&mxf_sound_essence_compression_alaw,
          &descriptor->sound_essence_compression)) {

    if (descriptor->audio_sampling_rate.n != 0 ||
        descriptor->audio_sampling_rate.d != 0 ||
        descriptor->channel_count != 0) {
      GST_ERROR ("Invalid descriptor");
      return NULL;
    }
    ret = gst_caps_new_empty_simple ("audio/x-alaw");
    mxf_metadata_generic_sound_essence_descriptor_set_caps (descriptor, ret);

    codec_name = g_strdup ("A-law encoded audio");
  } else {
    GST_ERROR ("Unsupported sound essence compression: %s",
        mxf_ul_to_string (&descriptor->sound_essence_compression, str));
  }

  *handler = mxf_bwf_handle_essence_element;

  if (!*tags)
    *tags = gst_tag_list_new_empty ();

  if (codec_name) {
    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
        codec_name, NULL);
    g_free (codec_name);
  }

  if (wa_descriptor && wa_descriptor->avg_bps)
    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_BITRATE,
        wa_descriptor->avg_bps * 8, NULL);

  *intra_only = TRUE;

  return ret;
}

static GstCaps *
mxf_aes3_create_caps (MXFMetadataTimelineTrack * track,
    MXFMetadataGenericSoundEssenceDescriptor * descriptor, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  GstCaps *ret = NULL;
  MXFMetadataWaveAudioEssenceDescriptor *wa_descriptor = NULL;
  gchar *codec_name = NULL;
  GstAudioFormat audio_format;
  guint block_align;

  if (MXF_IS_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR (descriptor))
    wa_descriptor = (MXFMetadataWaveAudioEssenceDescriptor *) descriptor;

  /* FIXME: set a channel layout */

  if (descriptor->channel_count == 0 ||
      descriptor->quantization_bits == 0 ||
      descriptor->audio_sampling_rate.n == 0 ||
      descriptor->audio_sampling_rate.d == 0) {
    GST_ERROR ("Invalid descriptor");
    return NULL;
  }
  if (wa_descriptor && wa_descriptor->block_align != 0)
    block_align = wa_descriptor->block_align;
  else
    block_align =
        (GST_ROUND_UP_8 (descriptor->quantization_bits) *
        descriptor->channel_count) / 8;

  audio_format =
      gst_audio_format_build_integer (block_align != descriptor->channel_count,
      G_LITTLE_ENDIAN, (block_align / descriptor->channel_count) * 8,
      (block_align / descriptor->channel_count) * 8);
  ret =
      mxf_metadata_generic_sound_essence_descriptor_create_caps (descriptor,
      &audio_format);

  codec_name =
      g_strdup_printf ("Uncompressed %u-bit AES3 audio",
      (block_align / descriptor->channel_count) * 8);

  if (!*tags)
    *tags = gst_tag_list_new_empty ();

  gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
      codec_name, GST_TAG_BITRATE,
      (gint) (block_align * 8 *
          mxf_fraction_to_double (&descriptor->audio_sampling_rate)) /
      (descriptor->channel_count), NULL);
  g_free (codec_name);

  *handler = mxf_aes3_handle_essence_element;
  *intra_only = TRUE;

  return ret;
}

static GstCaps *
mxf_aes_bwf_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  MXFMetadataGenericSoundEssenceDescriptor *s = NULL;
  gboolean bwf = FALSE;
  guint i;

  g_return_val_if_fail (track != NULL, NULL);

  if (track->parent.descriptor == NULL) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  }

  for (i = 0; i < track->parent.n_descriptor; i++) {
    if (!track->parent.descriptor[i])
      continue;

    if (MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->parent.
            descriptor[i])
        && (track->parent.descriptor[i]->essence_container.u[14] == 0x01
            || track->parent.descriptor[i]->essence_container.u[14] == 0x02
            || track->parent.descriptor[i]->essence_container.u[14] == 0x08
            || track->parent.descriptor[i]->essence_container.u[14] == 0x0a)) {
      s = (MXFMetadataGenericSoundEssenceDescriptor *) track->parent.
          descriptor[i];
      bwf = TRUE;
      break;
    } else
        if (MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->parent.
            descriptor[i])
        && (track->parent.descriptor[i]->essence_container.u[14] == 0x03
            || track->parent.descriptor[i]->essence_container.u[14] == 0x04
            || track->parent.descriptor[i]->essence_container.u[14] == 0x09
            || track->parent.descriptor[i]->essence_container.u[14] == 0x0b)) {

      s = (MXFMetadataGenericSoundEssenceDescriptor *) track->parent.
          descriptor[i];
      bwf = FALSE;
      break;
    }
  }

  if (!s) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  } else if (bwf) {
    return mxf_bwf_create_caps (track, s, tags, intra_only, handler,
        mapping_data);
  } else {
    return mxf_aes3_create_caps (track, s, tags, intra_only, handler,
        mapping_data);
  }

  return NULL;
}

static const MXFEssenceElementHandler mxf_aes_bwf_essence_handler = {
  mxf_is_aes_bwf_essence_track,
  mxf_aes_bwf_get_track_wrapping,
  mxf_aes_bwf_create_caps
};

typedef struct
{
  guint64 error;
  gint width, rate, channels;
  MXFFraction edit_rate;
} BWFMappingData;

static GstFlowReturn
mxf_bwf_write_func (GstBuffer * buffer, gpointer mapping_data,
    GstAdapter * adapter, GstBuffer ** outbuf, gboolean flush)
{
  BWFMappingData *md = mapping_data;
  guint bytes;
  guint64 speu =
      gst_util_uint64_scale (md->rate, md->edit_rate.d, md->edit_rate.n);

  md->error += (md->edit_rate.d * md->rate) % (md->edit_rate.n);
  if (md->error >= md->edit_rate.n) {
    md->error = 0;
    speu += 1;
  }

  bytes = (speu * md->channels * md->width) / 8;

  if (buffer)
    gst_adapter_push (adapter, buffer);

  if (gst_adapter_available (adapter) == 0)
    return GST_FLOW_OK;

  if (flush)
    bytes = MIN (gst_adapter_available (adapter), bytes);

  if (gst_adapter_available (adapter) >= bytes) {
    *outbuf = gst_adapter_take_buffer (adapter, bytes);
  }

  if (gst_adapter_available (adapter) >= bytes)
    return GST_FLOW_CUSTOM_SUCCESS;
  else
    return GST_FLOW_OK;
}

static const guint8 bwf_essence_container_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01,
  0x0d, 0x01, 0x03, 0x01, 0x02, 0x06, 0x01, 0x00
};

static MXFMetadataFileDescriptor *
mxf_bwf_get_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataWaveAudioEssenceDescriptor *ret;
  BWFMappingData *md;
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps)) {
    GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, caps);
    return NULL;
  }

  ret = (MXFMetadataWaveAudioEssenceDescriptor *)
      g_object_new (MXF_TYPE_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR, NULL);

  memcpy (&ret->parent.parent.essence_container, &bwf_essence_container_ul, 16);
  if (info.finfo->endianness == G_LITTLE_ENDIAN)
    memcpy (&ret->parent.sound_essence_compression,
        &mxf_sound_essence_compression_uncompressed, 16);
  else
    memcpy (&ret->parent.sound_essence_compression,
        &mxf_sound_essence_compression_aiff, 16);

  ret->block_align = (info.finfo->width / 8) * info.channels;
  ret->parent.quantization_bits = info.finfo->width;
  ret->avg_bps = ret->block_align * info.rate;

  if (!mxf_metadata_generic_sound_essence_descriptor_from_caps (&ret->parent,
          caps)) {
    g_object_unref (ret);
    return NULL;
  }

  *handler = mxf_bwf_write_func;

  md = g_new0 (BWFMappingData, 1);
  md->width = info.finfo->width;
  md->rate = info.rate;
  md->channels = info.channels;
  *mapping_data = md;

  return (MXFMetadataFileDescriptor *) ret;
}

static void
mxf_bwf_update_descriptor (MXFMetadataFileDescriptor * d, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf)
{
  return;
}

static void
mxf_bwf_get_edit_rate (MXFMetadataFileDescriptor * a, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf, MXFMetadataSourcePackage * package,
    MXFMetadataTimelineTrack * track, MXFFraction * edit_rate)
{
  guint i;
  gdouble min = G_MAXDOUBLE;
  BWFMappingData *md = mapping_data;

  for (i = 0; i < package->parent.n_tracks; i++) {
    MXFMetadataTimelineTrack *tmp;

    if (!MXF_IS_METADATA_TIMELINE_TRACK (package->parent.tracks[i]) ||
        package->parent.tracks[i] == (MXFMetadataTrack *) track)
      continue;

    tmp = MXF_METADATA_TIMELINE_TRACK (package->parent.tracks[i]);
    if (((gdouble) tmp->edit_rate.n) / ((gdouble) tmp->edit_rate.d) < min) {
      min = ((gdouble) tmp->edit_rate.n) / ((gdouble) tmp->edit_rate.d);
      memcpy (edit_rate, &tmp->edit_rate, sizeof (MXFFraction));
    }
  }

  if (min == G_MAXDOUBLE) {
    /* 100ms edit units */
    edit_rate->n = 10;
    edit_rate->d = 1;
  }

  memcpy (&md->edit_rate, edit_rate, sizeof (MXFFraction));
}

static guint32
mxf_bwf_get_track_number_template (MXFMetadataFileDescriptor * a,
    GstCaps * caps, gpointer mapping_data)
{
  return (0x16 << 24) | (0x01 << 8);
}

static MXFEssenceElementWriter mxf_bwf_essence_element_writer = {
  mxf_bwf_get_descriptor,
  mxf_bwf_update_descriptor,
  mxf_bwf_get_edit_rate,
  mxf_bwf_get_track_number_template,
  NULL,
  {{0,}}
};

#define BWF_CAPS \
      GST_AUDIO_CAPS_MAKE ("S32LE") "; " \
      GST_AUDIO_CAPS_MAKE ("S32BE") "; " \
      GST_AUDIO_CAPS_MAKE ("S24LE") "; " \
      GST_AUDIO_CAPS_MAKE ("S24BE") "; " \
      GST_AUDIO_CAPS_MAKE ("S16LE") "; " \
      GST_AUDIO_CAPS_MAKE ("S16BE") "; " \
      GST_AUDIO_CAPS_MAKE ("U8")

void
mxf_aes_bwf_init (void)
{
  mxf_metadata_register (MXF_TYPE_METADATA_WAVE_AUDIO_ESSENCE_DESCRIPTOR);
  mxf_metadata_register (MXF_TYPE_METADATA_AES3_AUDIO_ESSENCE_DESCRIPTOR);

  mxf_essence_element_handler_register (&mxf_aes_bwf_essence_handler);

  mxf_bwf_essence_element_writer.pad_template =
      gst_pad_template_new ("bwf_audio_sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      gst_caps_from_string (BWF_CAPS));
  memcpy (&mxf_bwf_essence_element_writer.data_definition,
      mxf_metadata_track_identifier_get (MXF_METADATA_TRACK_SOUND_ESSENCE), 16);
  mxf_essence_element_writer_register (&mxf_bwf_essence_element_writer);
}

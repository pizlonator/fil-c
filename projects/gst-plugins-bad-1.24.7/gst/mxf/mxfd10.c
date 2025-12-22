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

/* Implementation of SMPTE 386M - Mapping Type-D10 essence data into the MXF
 * Generic Container
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <string.h>

#include "mxfd10.h"

#include "mxfmpeg.h"
#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

typedef struct
{
  guint width, channels;
} MXFD10AudioMappingData;

static gboolean
mxf_is_d10_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;
  /* SMPTE 386M 5.1 */
  return (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 && key->u[13] == 0x01 &&
      (key->u[14] >= 0x01 && key->u[14] <= 0x06) &&
      (key->u[15] == 0x01 || key->u[15] == 0x02 || key->u[15] == 0x7f));
}

static GstFlowReturn
mxf_d10_picture_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  /* SMPTE 386M 5.2.1 */
  if (key->u[12] != 0x05 || key->u[13] != 0x01 || key->u[14] != 0x01) {
    GST_ERROR ("Invalid D10 picture essence element");
    return GST_FLOW_ERROR;
  }

  if (mxf_mpeg_is_mpeg2_keyframe (buffer))
    GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  else
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

  return GST_FLOW_OK;
}

static GstFlowReturn
mxf_d10_sound_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  guint i, j, nsamples;
  const guint8 *indata;
  guint8 *outdata;
  GstMapInfo map;
  GstMapInfo outmap;
  MXFD10AudioMappingData *data = mapping_data;

  g_return_val_if_fail (data != NULL, GST_FLOW_ERROR);
  g_return_val_if_fail (data->channels != 0
      && data->width != 0, GST_FLOW_ERROR);

  /* SMPTE 386M 5.3.1 */
  if (key->u[12] != 0x06 || key->u[13] != 0x01 || key->u[14] != 0x10) {
    GST_ERROR ("Invalid D10 sound essence element");
    return GST_FLOW_ERROR;
  }

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  /* Now transform raw AES3 into raw audio, see SMPTE 331M */
  if (map.size < 4 || (map.size - 4) % 32 != 0) {
    gst_buffer_unmap (buffer, &map);
    GST_ERROR ("Invalid D10 sound essence buffer size");
    return GST_FLOW_ERROR;
  }

  nsamples = ((map.size - 4) / 4) / 8;

  *outbuf = gst_buffer_new_and_alloc (nsamples * data->width * data->channels);
  gst_buffer_copy_into (*outbuf, buffer, GST_BUFFER_COPY_METADATA, 0, -1);
  gst_buffer_map (*outbuf, &outmap, GST_MAP_WRITE);

  indata = map.data;
  outdata = outmap.data;

  /* Skip 32 bit header */
  indata += 4;

  for (i = 0; i < nsamples; i++) {
    for (j = 0; j < data->channels; j++) {
      guint32 in = GST_READ_UINT32_LE (indata);

      /* Remove first 4 and last 4 bits as they only
       * contain status data. Shift the 24 bit samples
       * to the correct width afterwards. */
      if (data->width == 2) {
        in = (in >> 12) & 0xffff;
        GST_WRITE_UINT16_LE (outdata, in);
      } else if (data->width == 3) {
        in = (in >> 4) & 0xffffff;
        GST_WRITE_UINT24_LE (outdata, in);
      }
      indata += 4;
      outdata += data->width;
    }
    /* There are always 8 channels but only the first
     * ones contain valid data, skip the others */
    indata += 4 * (8 - data->channels);
  }

  gst_buffer_unmap (*outbuf, &outmap);
  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);

  return GST_FLOW_OK;
}

static MXFEssenceWrapping
mxf_d10_get_track_wrapping (const MXFMetadataTimelineTrack * track)
{
  return MXF_ESSENCE_WRAPPING_FRAME_WRAPPING;
}

static GstCaps *
mxf_d10_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
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

    if (MXF_IS_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (track->parent.
            descriptor[i])) {
      p = (MXFMetadataGenericPictureEssenceDescriptor *) track->
          parent.descriptor[i];
      break;
    } else if (MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->parent.
            descriptor[i])) {
      s = (MXFMetadataGenericSoundEssenceDescriptor *) track->
          parent.descriptor[i];
      break;
    }
  }

  if (!s && !p) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  }

  if (!*tags)
    *tags = gst_tag_list_new_empty ();

  if (s) {
    MXFD10AudioMappingData *data;
    GstAudioFormat audio_format;

    if (s->channel_count == 0 ||
        s->channel_count > 8 ||
        s->quantization_bits == 0 ||
        s->audio_sampling_rate.n == 0 || s->audio_sampling_rate.d == 0) {
      GST_ERROR ("Invalid descriptor");
      return NULL;
    }

    if (s->quantization_bits != 16 && s->quantization_bits != 24) {
      GST_ERROR ("Invalid width %u", s->quantization_bits);
      return NULL;
    }

    /* FIXME: set channel layout */

    audio_format =
        gst_audio_format_build_integer (s->quantization_bits != 8,
        G_LITTLE_ENDIAN, s->quantization_bits, s->quantization_bits);
    caps =
        mxf_metadata_generic_sound_essence_descriptor_create_caps (s,
        &audio_format);

    *handler = mxf_d10_sound_handle_essence_element;

    data = g_new0 (MXFD10AudioMappingData, 1);
    data->width = s->quantization_bits / 8;
    data->channels = s->channel_count;
    *mapping_data = data;

    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
        "SMPTE D-10 Audio", NULL);

    *intra_only = TRUE;
  } else if (p) {
    caps =
        gst_caps_new_simple ("video/mpeg", "systemstream", G_TYPE_BOOLEAN,
        FALSE, "mpegversion", G_TYPE_INT, 2, NULL);
    mxf_metadata_generic_picture_essence_descriptor_set_caps (p, caps);

    *handler = mxf_d10_picture_handle_essence_element;
    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
        "SMPTE D-10 Video", NULL);

    /* Does not allow temporal reordering */
    *intra_only = TRUE;
  }

  return caps;
}

static const MXFEssenceElementHandler mxf_d10_essence_element_handler = {
  mxf_is_d10_essence_track,
  mxf_d10_get_track_wrapping,
  mxf_d10_create_caps
};

void
mxf_d10_init (void)
{
  mxf_essence_element_handler_register (&mxf_d10_essence_element_handler);
}

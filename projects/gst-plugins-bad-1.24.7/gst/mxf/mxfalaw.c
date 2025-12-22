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

/* Implementation of SMPTE 388M - Mapping A-Law coded audio into the MXF
 * Generic Container
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>

#include "mxfalaw.h"
#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

static gboolean
mxf_is_alaw_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;
  /* SMPTE 388M 6.1 */
  return (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 && key->u[13] == 0x0a &&
      (key->u[14] == 0x01 || key->u[14] == 0x02 || key->u[14] == 0x03));
}

static GstFlowReturn
mxf_alaw_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;

  /* SMPTE 388M 5.1 */
  if (key->u[12] != 0x16 || (key->u[14] != 0x08 && key->u[14] != 0x09
          && key->u[14] != 0x0a)) {
    GST_ERROR ("Invalid A-Law essence element");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static MXFEssenceWrapping
mxf_alaw_get_track_wrapping (const MXFMetadataTimelineTrack * track)
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

    if (!MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->parent.
            descriptor[i]))
      continue;

    switch (track->parent.descriptor[i]->essence_container.u[14]) {
      case 0x01:
        return MXF_ESSENCE_WRAPPING_FRAME_WRAPPING;
        break;
      case 0x02:
        return MXF_ESSENCE_WRAPPING_CLIP_WRAPPING;
        break;
      case 0x03:
      default:
        return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
        break;
    }
  }

  return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
}

static GstCaps *
mxf_alaw_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
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

    if (MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->parent.
            descriptor[i])) {
      s = (MXFMetadataGenericSoundEssenceDescriptor *) track->parent.
          descriptor[i];
      break;
    }
  }

  if (!s) {
    GST_ERROR ("No generic sound essence descriptor found for this track");
    return NULL;
  }

  *handler = mxf_alaw_handle_essence_element;

  if (s && s->audio_sampling_rate.n != 0 && s->audio_sampling_rate.d != 0 &&
      s->channel_count != 0) {

    caps = gst_caps_new_empty_simple ("audio/x-alaw");
    mxf_metadata_generic_sound_essence_descriptor_set_caps (s, caps);

    /* TODO: Handle channel layout somehow?
     * Or is alaw limited to two channels? */
    if (!*tags)
      *tags = gst_tag_list_new_empty ();

    gst_tag_list_add (*tags, GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
        "A-law encoded audio", NULL);

  }

  *intra_only = TRUE;

  return caps;
}

static const MXFEssenceElementHandler mxf_alaw_essence_element_handler = {
  mxf_is_alaw_essence_track,
  mxf_alaw_get_track_wrapping,
  mxf_alaw_create_caps
};

typedef struct
{
  guint64 error;
  gint rate, channels;
  MXFFraction edit_rate;
} ALawMappingData;

static GstFlowReturn
mxf_alaw_write_func (GstBuffer * buffer, gpointer mapping_data,
    GstAdapter * adapter, GstBuffer ** outbuf, gboolean flush)
{
  ALawMappingData *md = mapping_data;
  guint bytes;
  guint64 speu =
      gst_util_uint64_scale (md->rate, md->edit_rate.d, md->edit_rate.n);

  md->error += (md->edit_rate.d * md->rate) % (md->edit_rate.n);
  if (md->error >= md->edit_rate.n) {
    md->error = 0;
    speu += 1;
  }

  bytes = speu * md->channels;

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

static const guint8 alaw_essence_container_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x03,
  0x0d, 0x01, 0x03, 0x01, 0x02, 0x0a, 0x01, 0x00
};

static const MXFUL mxf_sound_essence_compression_alaw =
    { {0x06, 0x0E, 0x2B, 0x34, 0x04, 0x01, 0x01, 0x03, 0x04, 0x02, 0x02, 0x02,
    0x03, 0x01, 0x01, 0x00}
};

static MXFMetadataFileDescriptor *
mxf_alaw_get_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataGenericSoundEssenceDescriptor *ret;
  GstStructure *s;
  ALawMappingData *md;
  gint rate, channels;

  s = gst_caps_get_structure (caps, 0);
  if (strcmp (gst_structure_get_name (s), "audio/x-alaw") != 0 ||
      !gst_structure_get_int (s, "rate", &rate) ||
      !gst_structure_get_int (s, "channels", &channels)) {
    GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, caps);
    return NULL;
  }

  ret = (MXFMetadataGenericSoundEssenceDescriptor *)
      g_object_new (MXF_TYPE_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR, NULL);

  memcpy (&ret->parent.essence_container, &alaw_essence_container_ul, 16);
  memcpy (&ret->sound_essence_compression, &mxf_sound_essence_compression_alaw,
      16);

  if (!mxf_metadata_generic_sound_essence_descriptor_from_caps (ret, caps)) {
    g_object_unref (ret);
    return NULL;
  }

  *handler = mxf_alaw_write_func;

  md = g_new0 (ALawMappingData, 1);
  md->rate = rate;
  md->channels = channels;
  *mapping_data = md;

  return (MXFMetadataFileDescriptor *) ret;
}

static void
mxf_alaw_update_descriptor (MXFMetadataFileDescriptor * d, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf)
{
  return;
}

static void
mxf_alaw_get_edit_rate (MXFMetadataFileDescriptor * a, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf, MXFMetadataSourcePackage * package,
    MXFMetadataTimelineTrack * track, MXFFraction * edit_rate)
{
  guint i;
  gdouble min = G_MAXDOUBLE;
  ALawMappingData *md = mapping_data;

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
mxf_alaw_get_track_number_template (MXFMetadataFileDescriptor * a,
    GstCaps * caps, gpointer mapping_data)
{
  return (0x16 << 24) | (0x08 << 8);
}

static MXFEssenceElementWriter mxf_alaw_essence_element_writer = {
  mxf_alaw_get_descriptor,
  mxf_alaw_update_descriptor,
  mxf_alaw_get_edit_rate,
  mxf_alaw_get_track_number_template,
  NULL,
  {{0,}}
};

#define ALAW_CAPS \
      "audio/x-alaw, " \
      "rate = (int) [ 8000, 192000 ], " \
      "channels = (int) [ 1, 2 ]"

void
mxf_alaw_init (void)
{
  mxf_essence_element_handler_register (&mxf_alaw_essence_element_handler);

  mxf_alaw_essence_element_writer.pad_template =
      gst_pad_template_new ("alaw_audio_sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      gst_caps_from_string (ALAW_CAPS));
  memcpy (&mxf_alaw_essence_element_writer.data_definition,
      mxf_metadata_track_identifier_get (MXF_METADATA_TRACK_SOUND_ESSENCE), 16);
  mxf_essence_element_writer_register (&mxf_alaw_essence_element_writer);
}

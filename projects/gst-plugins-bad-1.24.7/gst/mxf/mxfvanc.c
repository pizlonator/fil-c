/* GStreamer
 * Copyright (C) 2020 Sebastian Dröge <sebastian@centricular.com>
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

/* Implementation of SMPTE 436M - MXF Mappings for VBI Lines
 * and Ancillary Data Packets
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include <string.h>

#include "mxfvanc.h"
#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

/* SMPTE S436M 7 */
#define MXF_TYPE_METADATA_VANC_DESCRIPTOR \
  (mxf_metadata_vanc_descriptor_get_type())
#define MXF_METADATA_VANC_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_METADATA_VANC_DESCRIPTOR, MXFMetadataVANCDescriptor))
#define MXF_IS_METADATA_VANC_DESCRIPTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_METADATA_VANC_DESCRIPTOR))
typedef struct _MXFMetadataVANCDescriptor MXFMetadataVANCDescriptor;
typedef MXFMetadataClass MXFMetadataVANCDescriptorClass;
GType mxf_metadata_vanc_descriptor_get_type (void);

struct _MXFMetadataVANCDescriptor
{
  MXFMetadataGenericDataEssenceDescriptor parent;
};

G_DEFINE_TYPE (MXFMetadataVANCDescriptor,
    mxf_metadata_vanc_descriptor,
    MXF_TYPE_METADATA_GENERIC_DATA_ESSENCE_DESCRIPTOR);

static void
mxf_metadata_vanc_descriptor_init (MXFMetadataVANCDescriptor * self)
{
}

static void
    mxf_metadata_vanc_descriptor_class_init
    (MXFMetadataVANCDescriptorClass * klass)
{
  MXFMetadataClass *metadata_class = (MXFMetadataClass *) klass;

  metadata_class->type = 0x015c;
}

static gboolean
mxf_is_vanc_essence_track (const MXFMetadataFileDescriptor * d)
{
  const MXFUL *key = &d->essence_container;

  /* SMPTE 436M 4.3 */
  return (mxf_is_generic_container_essence_container_label (key) &&
      key->u[12] == 0x02 && key->u[13] == 0x0e &&
      key->u[14] == 0x00 && key->u[15] == 0x00);
}

static GstFlowReturn
mxf_vanc_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  GstMapInfo map;
  GstByteReader reader;
  GstFlowReturn ret = GST_FLOW_ERROR;
  guint16 num_packets, i;

  /* SMPTE 436M 6.1 */
  if (key->u[12] != 0x17 || key->u[14] != 0x02) {
    GST_ERROR ("Invalid VANC essence element");
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  /* Either there is no data or there is at least room for the 16bit length,
   * therefore the only invalid packet length is 1 */
  if (gst_buffer_get_size (buffer) == 1) {
    GST_ERROR ("Invalid VANC essence element size");
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  gst_byte_reader_init (&reader, map.data, map.size);

  /* Some XDCAM recorders store empty vanc packets (without even the
   * length). Treat them as gaps */
  if (map.size == 0)
    goto no_data;

  num_packets = gst_byte_reader_get_uint16_be_unchecked (&reader);
  if (num_packets == 0) {
    /* SMPTE 436-1:2013 5.5 The Number of VI Lines or ANC Packets Property
     *
     * One of the properties in the VI Element is the “Number of Lines” which is
     * the number of the VI lines contained in this VI Element. This number can
     * be zero if the current VI Element does not have any VI lines in the
     * payload space. This capability can be used so every Content Package in a
     * file can have a VI Element even if the video stream does not have VI
     * lines with every frame (or field.)
     *
     * The same scheme can be used for ANC packets.
     */
    goto no_data;
  }

  for (i = 0; i < num_packets; i++) {
    G_GNUC_UNUSED guint16 line_num;
    G_GNUC_UNUSED guint8 wrapping_type;
    guint8 payload_sample_coding;
    guint16 payload_sample_count;
    guint32 array_count;
    guint32 array_item_size;
    guint8 did, sdid;
    guint8 cdp_size;

    if (gst_byte_reader_get_remaining (&reader) < 16)
      goto out;

    line_num = gst_byte_reader_get_uint16_be_unchecked (&reader);
    wrapping_type = gst_byte_reader_get_uint8_unchecked (&reader);
    payload_sample_coding = gst_byte_reader_get_uint8_unchecked (&reader);
    payload_sample_count = gst_byte_reader_get_uint16_be_unchecked (&reader);

    array_count = gst_byte_reader_get_uint32_be_unchecked (&reader);
    array_item_size = gst_byte_reader_get_uint32_be_unchecked (&reader);

    /* Skip over anything that is not 8 bit VANC */
    if (payload_sample_coding != 4 && payload_sample_coding != 5
        && payload_sample_coding != 6) {
      if (!gst_byte_reader_skip (&reader, array_count * array_item_size))
        goto out;
      continue;
    }

    if (gst_byte_reader_get_remaining (&reader) < array_count * array_item_size)
      goto out;

    if (gst_byte_reader_get_remaining (&reader) < payload_sample_count)
      goto out;

    if (payload_sample_count < 2) {
      if (!gst_byte_reader_skip (&reader, array_count * array_item_size))
        goto out;
      continue;
    }

    did = gst_byte_reader_get_uint8_unchecked (&reader);
    sdid = gst_byte_reader_get_uint8_unchecked (&reader);

    /* Not S334 EIA-708 */
    if (did != 0x61 && sdid != 0x01) {
      GST_TRACE ("Skipping VANC data with DID/SDID 0x%02X/0x%02X", did, sdid);
      if (!gst_byte_reader_skip (&reader, array_count * array_item_size - 2))
        goto out;
      continue;
    }

    if (payload_sample_count < 2) {
      if (!gst_byte_reader_skip (&reader, array_count * array_item_size - 2))
        goto out;
      continue;
    }

    cdp_size = gst_byte_reader_get_uint8_unchecked (&reader);
    if (payload_sample_count - 3 < cdp_size) {
      if (!gst_byte_reader_skip (&reader, array_count * array_item_size - 3))
        goto out;
      continue;
    }

    gst_buffer_unmap (buffer, &map);
    *outbuf =
        gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL,
        gst_byte_reader_get_pos (&reader), cdp_size);
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

no_data:

  /* No packets or we skipped over all packets */
  *outbuf = gst_buffer_new ();
  GST_BUFFER_FLAG_SET (*outbuf, GST_BUFFER_FLAG_GAP);
  ret = GST_FLOW_OK;

out:
  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);

  return ret;
}

static MXFEssenceWrapping
mxf_vanc_get_track_wrapping (const MXFMetadataTimelineTrack * track)
{
  g_return_val_if_fail (track != NULL, MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING);

  if (track->parent.descriptor == NULL) {
    GST_ERROR ("No descriptor found for this track");
    return MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING;
  }

  return MXF_ESSENCE_WRAPPING_FRAME_WRAPPING;
}

static GstCaps *
mxf_vanc_create_caps (MXFMetadataTimelineTrack * track, GstTagList ** tags,
    gboolean * intra_only, MXFEssenceElementHandleFunc * handler,
    gpointer * mapping_data)
{
  GstCaps *caps = NULL;
  guint i;
  MXFMetadataFileDescriptor *f = NULL;
  MXFMetadataVANCDescriptor *p = NULL;

  g_return_val_if_fail (track != NULL, NULL);

  if (track->parent.descriptor == NULL) {
    GST_ERROR ("No descriptor found for this track");
    return NULL;
  }

  for (i = 0; i < track->parent.n_descriptor; i++) {
    if (!track->parent.descriptor[i])
      continue;

    if (MXF_IS_METADATA_VANC_DESCRIPTOR (track->parent.descriptor[i])) {
      p = (MXFMetadataVANCDescriptor *) track->parent.descriptor[i];
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

  *handler = mxf_vanc_handle_essence_element;

  caps =
      gst_caps_new_simple ("closedcaption/x-cea-708", "format",
      G_TYPE_STRING, "cdp", NULL);

  if (p && p->parent.parent.sample_rate.d != 0) {
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION,
        p->parent.parent.sample_rate.n, p->parent.parent.sample_rate.d, NULL);
  }

  *intra_only = TRUE;

  return caps;
}

static const MXFEssenceElementHandler mxf_vanc_essence_element_handler = {
  mxf_is_vanc_essence_track,
  mxf_vanc_get_track_wrapping,
  mxf_vanc_create_caps
};

static GstFlowReturn
mxf_vanc_write_func (GstBuffer * buffer,
    gpointer mapping_data, GstAdapter * adapter, GstBuffer ** outbuf,
    gboolean flush)
{
  GstMapInfo map;
  GstByteWriter writer;
  guint8 *data;
  guint size;
  guint i;
  guint8 checksum;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  size = GST_ROUND_UP_4 (map.size) + 20;

  gst_byte_writer_init_with_size (&writer, size, TRUE);

  gst_byte_writer_put_uint16_be_unchecked (&writer, 1);
  gst_byte_writer_put_uint16_be_unchecked (&writer, 9);
  gst_byte_writer_put_uint8_unchecked (&writer, 1);
  gst_byte_writer_put_uint8_unchecked (&writer, 4);
  gst_byte_writer_put_uint16_be_unchecked (&writer, map.size + 4);
  gst_byte_writer_put_uint32_be_unchecked (&writer,
      GST_ROUND_UP_4 (map.size + 4));
  gst_byte_writer_put_uint32_be_unchecked (&writer, 1);

  gst_byte_writer_put_uint16_be_unchecked (&writer, 0x6101);
  gst_byte_writer_put_uint8_unchecked (&writer, map.size);

  gst_byte_writer_put_data_unchecked (&writer, map.data, map.size);

  checksum = 0x61 + 0x01 + map.size;
  for (i = 0; i < map.size; i++)
    checksum += map.data[i];
  gst_byte_writer_put_uint8_unchecked (&writer, checksum & 0xff);

  if (GST_ROUND_UP_4 (map.size) != map.size) {
    gst_byte_writer_fill_unchecked (&writer, 0,
        GST_ROUND_UP_4 (map.size) - map.size);
  }

  data = gst_byte_writer_reset_and_get_data (&writer);

  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);

  *outbuf = gst_buffer_new_wrapped (data, size);

  return GST_FLOW_OK;
}

static const guint8 vanc_essence_container_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x09,
  0x0d, 0x01, 0x03, 0x01, 0x02, 0x0e, 0x00, 0x00
};

static MXFMetadataFileDescriptor *
mxf_vanc_get_descriptor (GstPadTemplate * tmpl, GstCaps * caps,
    MXFEssenceElementWriteFunc * handler, gpointer * mapping_data)
{
  MXFMetadataVANCDescriptor *ret;
  GstStructure *s;
  const gchar *format;
  gint fps_n, fps_d;

  s = gst_caps_get_structure (caps, 0);
  if (strcmp (gst_structure_get_name (s), "closedcaption/x-cea-708") != 0 ||
      !(format = gst_structure_get_string (s, "format")) ||
      strcmp (format, "cdp") != 0 ||
      !gst_structure_get_value (s, "framerate")) {
    GST_ERROR ("Invalid caps %" GST_PTR_FORMAT, caps);
    return NULL;
  }

  gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d);

  ret = (MXFMetadataVANCDescriptor *)
      g_object_new (MXF_TYPE_METADATA_VANC_DESCRIPTOR, NULL);

  memcpy (&ret->parent.parent.essence_container, &vanc_essence_container_ul,
      16);

  *handler = mxf_vanc_write_func;

  return (MXFMetadataFileDescriptor *) ret;
}

static void
mxf_vanc_update_descriptor (MXFMetadataFileDescriptor * d, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf)
{
  return;
}

static void
mxf_vanc_get_edit_rate (MXFMetadataFileDescriptor * a, GstCaps * caps,
    gpointer mapping_data, GstBuffer * buf, MXFMetadataSourcePackage * package,
    MXFMetadataTimelineTrack * track, MXFFraction * edit_rate)
{
  const GstStructure *s;
  gint fps_n, fps_d;

  s = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d))
    return;

  edit_rate->n = fps_n;
  edit_rate->d = fps_d;
}

static guint32
mxf_vanc_get_track_number_template (MXFMetadataFileDescriptor * a,
    GstCaps * caps, gpointer mapping_data)
{
  return (0x17 << 24) | (0x02 << 8);
}

static MXFEssenceElementWriter mxf_vanc_essence_element_writer = {
  mxf_vanc_get_descriptor,
  mxf_vanc_update_descriptor,
  mxf_vanc_get_edit_rate,
  mxf_vanc_get_track_number_template,
  NULL,
  {{0,}}
};

/**
 * GstMXFMux!vanc_sink_%u:
 *
 * Since: 1.18
 */

void
mxf_vanc_init (void)
{
  mxf_metadata_register (MXF_TYPE_METADATA_VANC_DESCRIPTOR);
  mxf_essence_element_handler_register (&mxf_vanc_essence_element_handler);

  mxf_vanc_essence_element_writer.pad_template =
      gst_pad_template_new ("vanc_sink_%u", GST_PAD_SINK,
      GST_PAD_REQUEST,
      gst_caps_from_string
      ("closedcaption/x-cea-708, format = (string) cdp, framerate = "
          GST_VIDEO_FPS_RANGE));
  memcpy (&mxf_vanc_essence_element_writer.data_definition,
      mxf_metadata_track_identifier_get (MXF_METADATA_TRACK_DATA_ESSENCE), 16);
  mxf_essence_element_writer_register (&mxf_vanc_essence_element_writer);
}

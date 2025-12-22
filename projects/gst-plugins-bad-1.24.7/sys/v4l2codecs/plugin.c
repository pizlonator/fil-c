/* GStreamer
 * Copyright (C) 2020 Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#include "gstv4l2codecav1dec.h"
#include "gstv4l2codecdevice.h"
#include "gstv4l2codech264dec.h"
#include "gstv4l2codech265dec.h"
#include "gstv4l2codecmpeg2dec.h"
#include "gstv4l2codecvp8dec.h"
#include "gstv4l2codecvp9dec.h"
#include "gstv4l2decoder.h"
#include "linux/v4l2-controls.h"
#include "linux/media.h"

#define GST_CAT_DEFAULT gstv4l2codecs_debug
GST_DEBUG_CATEGORY (gstv4l2codecs_debug);

static void
register_video_decoder (GstPlugin * plugin, GstV4l2CodecDevice * device)
{
  GstV4l2Decoder *decoder = gst_v4l2_decoder_new (device);
  gint i;
  guint32 fmt;

  if (!gst_v4l2_decoder_open (decoder)) {
    g_object_unref (decoder);
    return;
  }

  for (i = 0; gst_v4l2_decoder_enum_sink_fmt (decoder, i, &fmt); i++) {
    switch (fmt) {
      case V4L2_PIX_FMT_H264_SLICE:
        GST_INFO_OBJECT (decoder, "Registering %s as H264 Decoder",
            device->name);
        gst_v4l2_codec_h264_dec_register (plugin, decoder, device,
            GST_RANK_PRIMARY + 1);
        break;
      case V4L2_PIX_FMT_HEVC_SLICE:
        GST_INFO_OBJECT (decoder, "Registering %s as H265 Decoder",
            device->name);
        gst_v4l2_codec_h265_dec_register (plugin, decoder, device,
            GST_RANK_PRIMARY + 1);
        break;

      case V4L2_PIX_FMT_VP8_FRAME:
        GST_INFO_OBJECT (decoder, "Registering %s as VP8 Decoder",
            device->name);
        gst_v4l2_codec_vp8_dec_register (plugin, decoder, device,
            GST_RANK_PRIMARY + 1);
        break;
      case V4L2_PIX_FMT_MPEG2_SLICE:
        GST_INFO_OBJECT (decoder, "Registering %s as Mpeg2 Decoder",
            device->name);
        gst_v4l2_codec_mpeg2_dec_register (plugin, decoder, device,
            GST_RANK_PRIMARY + 1);
        break;
      case V4L2_PIX_FMT_VP9_FRAME:
        GST_INFO_OBJECT (decoder, "Registering %s as VP9 Decoder",
            device->name);
        gst_v4l2_codec_vp9_dec_register (plugin, decoder, device,
            GST_RANK_PRIMARY + 1);
        break;
      case V4L2_PIX_FMT_AV1_FRAME:
        GST_INFO_OBJECT (decoder, "Registering %s as AV1 Decoder",
            device->name);
        gst_v4l2_codec_av1_dec_register (plugin, decoder, device,
            GST_RANK_PRIMARY + 1);
        break;
      default:
        GST_FIXME_OBJECT (decoder, "%" GST_FOURCC_FORMAT " is not supported.",
            GST_FOURCC_ARGS (fmt));
        break;
    }
  }

  g_object_unref (decoder);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GList *devices, *d;
  const gchar *paths[] = { "/dev", NULL };
  const gchar *names[] = { "media", NULL };

  GST_DEBUG_CATEGORY_INIT (gstv4l2codecs_debug, "v4l2codecs", 0,
      "V4L2 CODECs general debug");

  /* Add some dependency, so the dynamic features get updated upon changes in
   * /dev/media* */
  gst_plugin_add_dependency (plugin,
      NULL, paths, names, GST_PLUGIN_DEPENDENCY_FLAG_FILE_NAME_IS_PREFIX);

  devices = gst_v4l2_codec_find_devices ();
  for (d = devices; d; d = g_list_next (d)) {
    GstV4l2CodecDevice *device = d->data;

    if (device->function == MEDIA_ENT_F_PROC_VIDEO_DECODER)
      register_video_decoder (plugin, device);
  }

  gst_v4l2_codec_device_list_free (devices);
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    v4l2codecs,
    "V4L2 CODEC Accelerators plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)

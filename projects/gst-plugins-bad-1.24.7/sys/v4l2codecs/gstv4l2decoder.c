/* GStreamer
 * Copyright (C) 2020 Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
#include <config.h>
#endif

#include "gstv4l2codecallocator.h"
#include "gstv4l2codecpool.h"
#include "gstv4l2decoder.h"
#include "gstv4l2format.h"
#include "linux/media.h"
#include "linux/videodev2.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gst/base/base.h>

#define IMAGE_MINSZ (256*1024)  /* 256kB */

GST_DEBUG_CATEGORY (v4l2_decoder_debug);
#define GST_CAT_DEFAULT v4l2_decoder_debug

enum
{
  PROP_0,
  PROP_MEDIA_DEVICE,
  PROP_VIDEO_DEVICE,
};

struct _GstV4l2Request
{
  /* non-thread safe */
  gint ref_count;

  GstV4l2Decoder *decoder;
  gint fd;
  guint32 frame_num;
  GstMemory *bitstream;
  GstBuffer *pic_buf;
  GstPoll *poll;
  GstPollFD pollfd;

  /* request state */
  gboolean pending;
  gboolean failed;
  gboolean hold_pic_buf;
  gboolean sub_request;
};

struct _GstV4l2Decoder
{
  GstObject parent;

  gboolean opened;
  gint media_fd;
  gint video_fd;
  GstQueueArray *request_pool;
  GstQueueArray *pending_requests;
  guint version;

  enum v4l2_buf_type src_buf_type;
  enum v4l2_buf_type sink_buf_type;
  gboolean mplane;

  /* properties */
  gchar *media_device;
  gchar *video_device;
  guint render_delay;

  /* detected features */
  gboolean supports_holding_capture;
};

G_DEFINE_TYPE_WITH_CODE (GstV4l2Decoder, gst_v4l2_decoder, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (v4l2_decoder_debug, "v4l2codecs-decoder", 0,
        "V4L2 stateless decoder helper"));

static void gst_v4l2_request_free (GstV4l2Request * request);

static guint32
direction_to_buffer_type (GstV4l2Decoder * self, GstPadDirection direction)
{
  if (direction == GST_PAD_SRC)
    return self->src_buf_type;
  else
    return self->sink_buf_type;
}

static void
gst_v4l2_decoder_finalize (GObject * obj)
{
  GstV4l2Decoder *self = GST_V4L2_DECODER (obj);

  gst_v4l2_decoder_close (self);

  g_free (self->media_device);
  g_free (self->video_device);
  gst_queue_array_free (self->request_pool);
  gst_queue_array_free (self->pending_requests);

  G_OBJECT_CLASS (gst_v4l2_decoder_parent_class)->finalize (obj);
}

static void
gst_v4l2_decoder_init (GstV4l2Decoder * self)
{
  self->request_pool = gst_queue_array_new (16);
  self->pending_requests = gst_queue_array_new (16);
}

static void
gst_v4l2_decoder_class_init (GstV4l2DecoderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_v4l2_decoder_finalize;
  gobject_class->get_property = gst_v4l2_decoder_get_property;
  gobject_class->set_property = gst_v4l2_decoder_set_property;

  gst_v4l2_decoder_install_properties (gobject_class, 0, NULL);
}

GstV4l2Decoder *
gst_v4l2_decoder_new (GstV4l2CodecDevice * device)
{
  GstV4l2Decoder *decoder;

  g_return_val_if_fail (device->function == MEDIA_ENT_F_PROC_VIDEO_DECODER,
      NULL);

  decoder = g_object_new (GST_TYPE_V4L2_DECODER,
      "media-device", device->media_device_path,
      "video-device", device->video_device_path, NULL);

  return gst_object_ref_sink (decoder);
}

guint
gst_v4l2_decoder_get_version (GstV4l2Decoder * self)
{
  return self->version;
}

gboolean
gst_v4l2_decoder_open (GstV4l2Decoder * self)
{
  gint ret;
  struct v4l2_capability querycap;
  guint32 capabilities;

  self->media_fd = open (self->media_device, 0);
  if (self->media_fd < 0) {
    GST_ERROR_OBJECT (self, "Failed to open '%s': %s",
        self->media_device, g_strerror (errno));
    return FALSE;
  }

  self->video_fd = open (self->video_device, O_NONBLOCK);
  if (self->video_fd < 0) {
    GST_ERROR_OBJECT (self, "Failed to open '%s': %s",
        self->video_device, g_strerror (errno));
    return FALSE;
  }

  ret = ioctl (self->video_fd, VIDIOC_QUERYCAP, &querycap);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_QUERYCAP failed: %s", g_strerror (errno));
    gst_v4l2_decoder_close (self);
    return FALSE;
  }

  self->version = querycap.version;

  if (querycap.capabilities & V4L2_CAP_DEVICE_CAPS)
    capabilities = querycap.device_caps;
  else
    capabilities = querycap.capabilities;

  if (capabilities & V4L2_CAP_VIDEO_M2M_MPLANE) {
    self->sink_buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    self->src_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    self->mplane = TRUE;
  } else if (capabilities & V4L2_CAP_VIDEO_M2M) {
    self->sink_buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    self->src_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    self->mplane = FALSE;
  } else {
    GST_ERROR_OBJECT (self, "Unsupported memory-2-memory device.");
    gst_v4l2_decoder_close (self);
    return FALSE;
  }

  self->opened = TRUE;

  return TRUE;
}

gboolean
gst_v4l2_decoder_close (GstV4l2Decoder * self)
{
  GstV4l2Request *request;

  while ((request = gst_queue_array_pop_head (self->pending_requests)))
    gst_v4l2_request_unref (request);

  while ((request = gst_queue_array_pop_head (self->request_pool)))
    gst_v4l2_request_free (request);

  if (self->media_fd)
    close (self->media_fd);
  if (self->video_fd)
    close (self->video_fd);

  self->media_fd = 0;
  self->video_fd = 0;
  self->opened = FALSE;

  return TRUE;
}

gboolean
gst_v4l2_decoder_streamon (GstV4l2Decoder * self, GstPadDirection direction)
{
  gint ret;
  guint32 type = direction_to_buffer_type (self, direction);

  ret = ioctl (self->video_fd, VIDIOC_STREAMON, &type);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_STREAMON failed: %s", g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_v4l2_decoder_streamoff (GstV4l2Decoder * self, GstPadDirection direction)
{
  guint32 type = direction_to_buffer_type (self, direction);
  gint ret;

  if (direction == GST_PAD_SRC) {
    GstV4l2Request *pending_req;

    /* STREAMOFF have the effect of cancelling all requests and unqueuing all
     * buffers, so clear the pending request list */
    while ((pending_req = gst_queue_array_pop_head (self->pending_requests))) {
      g_clear_pointer (&pending_req->bitstream, gst_memory_unref);
      pending_req->pending = FALSE;
      gst_v4l2_request_unref (pending_req);
    }
  }

  ret = ioctl (self->video_fd, VIDIOC_STREAMOFF, &type);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_STREAMOFF failed: %s", g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_v4l2_decoder_flush (GstV4l2Decoder * self)
{
  /* We ignore streamoff failure as it's not relevant, if we manage to
   * streamon again, we are good. */
  gst_v4l2_decoder_streamoff (self, GST_PAD_SINK);
  gst_v4l2_decoder_streamoff (self, GST_PAD_SRC);

  return gst_v4l2_decoder_streamon (self, GST_PAD_SINK) &&
      gst_v4l2_decoder_streamon (self, GST_PAD_SRC);
}

gboolean
gst_v4l2_decoder_enum_sink_fmt (GstV4l2Decoder * self, gint i,
    guint32 * out_fmt)
{
  struct v4l2_fmtdesc fmtdesc = { i, self->sink_buf_type, };
  gint ret;

  g_return_val_if_fail (self->opened, FALSE);

  ret = ioctl (self->video_fd, VIDIOC_ENUM_FMT, &fmtdesc);
  if (ret < 0) {
    if (errno != EINVAL)
      GST_ERROR_OBJECT (self, "VIDIOC_ENUM_FMT failed: %s", g_strerror (errno));
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Found format %" GST_FOURCC_FORMAT " (%s)",
      GST_FOURCC_ARGS (fmtdesc.pixelformat), fmtdesc.description);
  *out_fmt = fmtdesc.pixelformat;

  return TRUE;
}

gboolean
gst_v4l2_decoder_set_sink_fmt (GstV4l2Decoder * self, guint32 pix_fmt,
    gint width, gint height, gint pixel_bitdepth)
{
  struct v4l2_format format = (struct v4l2_format) {
    .type = self->sink_buf_type,
    /* Compatible with .fmt.pix for these field */
    .fmt.pix_mp = (struct v4l2_pix_format_mplane) {
          .pixelformat = pix_fmt,
          .width = width,
          .height = height,
        },
  };
  gint ret;

  /* Using raw image size for now, it is guarantied to be large enough */
  gsize sizeimage = MAX (IMAGE_MINSZ, (width * height * pixel_bitdepth) / 8);

  if (self->mplane)
    format.fmt.pix_mp.plane_fmt[0].sizeimage = sizeimage;
  else
    format.fmt.pix.sizeimage = sizeimage;

  ret = ioctl (self->video_fd, VIDIOC_S_FMT, &format);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_S_FMT failed: %s", g_strerror (errno));
    return FALSE;
  }

  if (format.fmt.pix_mp.pixelformat != pix_fmt
      || format.fmt.pix_mp.width < width || format.fmt.pix_mp.height < height) {
    GST_WARNING_OBJECT (self, "Failed to set sink format to %"
        GST_FOURCC_FORMAT " %ix%i", GST_FOURCC_ARGS (pix_fmt), width, height);
    errno = EINVAL;
    return FALSE;
  }

  return TRUE;
}

static GstCaps *
gst_v4l2_decoder_enum_size_for_format (GstV4l2Decoder * self,
    guint32 pixelformat, gint index, gint unscaled_width, gint unscaled_height)
{
  struct v4l2_frmsizeenum size;
  GstVideoFormat format;
  gint ret;
  gboolean res;

  memset (&size, 0, sizeof (struct v4l2_frmsizeenum));
  size.index = index;
  size.pixel_format = pixelformat;

  GST_DEBUG_OBJECT (self, "enumerate size index %d for %" GST_FOURCC_FORMAT,
      index, GST_FOURCC_ARGS (pixelformat));

  ret = ioctl (self->video_fd, VIDIOC_ENUM_FRAMESIZES, &size);

  if (ret < 0)
    return NULL;

  if (size.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
    GST_WARNING_OBJECT (self, "V4L2_FRMSIZE type not supported");
    return NULL;
  }

  if (gst_util_fraction_compare (unscaled_width, unscaled_height,
          size.discrete.width, size.discrete.height)) {
    GST_DEBUG_OBJECT (self,
        "Pixel ratio modification not supported %dx%d %dx%d (%d)",
        unscaled_width, unscaled_height, size.discrete.width,
        size.discrete.height, ret);
    return NULL;
  }

  res = gst_v4l2_format_to_video_format (pixelformat, &format);
  g_assert (res);

  GST_DEBUG_OBJECT (self, "get size (%d x %d) index %d for %" GST_FOURCC_FORMAT,
      size.discrete.width, size.discrete.height, index,
      GST_FOURCC_ARGS (pixelformat));

  return gst_caps_new_simple ("video/x-raw",
      "width", G_TYPE_INT, size.discrete.width,
      "height", G_TYPE_INT, size.discrete.height, NULL);
}

static GstCaps *
gst_v4l2_decoder_probe_caps_for_format (GstV4l2Decoder * self,
    guint32 pixelformat, gint unscaled_width, gint unscaled_height)
{
  gint index = 0;
  GstCaps *caps, *tmp, *size_caps;
  GstVideoFormat format;
  guint32 drm_fourcc;

  GST_DEBUG_OBJECT (self, "enumerate size for %" GST_FOURCC_FORMAT,
      GST_FOURCC_ARGS (pixelformat));

  if (!gst_v4l2_format_to_video_format (pixelformat, &format))
    return gst_caps_new_empty ();

  caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING,
      gst_video_format_to_string (format), NULL);

  size_caps = gst_caps_new_empty ();
  while ((tmp = gst_v4l2_decoder_enum_size_for_format (self, pixelformat,
              index++, unscaled_width, unscaled_height))) {
    size_caps = gst_caps_merge (size_caps, tmp);
  }

  if (!gst_caps_is_empty (size_caps)) {
    tmp = caps;
    caps = gst_caps_intersect_full (tmp, size_caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  }

  /* TODO: Add a V4L2 to DRM fourcc translator for formats that we don't support
   * in software.
   */
  drm_fourcc = gst_video_dma_drm_fourcc_from_format (format);
  if (drm_fourcc /* != DRM_FORMAT_INVALID */ ) {
    GstCaps *drm_caps;

    drm_caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING,
        "DMA_DRM", "drm-format", G_TYPE_STRING,
        gst_video_dma_drm_fourcc_to_string (drm_fourcc, 0), NULL);
    gst_caps_set_features_simple (drm_caps,
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_DMABUF));

    if (!gst_caps_is_empty (size_caps)) {
      gst_caps_set_features_simple (size_caps,
          gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_DMABUF));
      tmp = drm_caps;
      drm_caps =
          gst_caps_intersect_full (tmp, size_caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (tmp);
    }

    caps = gst_caps_merge (drm_caps, caps);
  }

  gst_caps_unref (size_caps);

  return caps;
}

static gboolean
filter_only_dmabuf_caps (GstCapsFeatures * features,
    GstStructure * structure, gpointer user_data)
{
  return gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_DMABUF);
}

static gboolean
filter_non_dmabuf_caps (GstCapsFeatures * features,
    GstStructure * structure, gpointer user_data)
{
  return !gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_DMABUF);
}

GstCaps *
gst_v4l2_decoder_enum_src_formats (GstV4l2Decoder * self,
    GstStaticCaps * static_filter)
{
  gint ret;
  struct v4l2_format fmt = {
    .type = self->src_buf_type,
  };
  GstCaps *caps, *filter, *tmp;
  gint i;

  g_return_val_if_fail (self->opened, FALSE);

  ret = ioctl (self->video_fd, VIDIOC_G_FMT, &fmt);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_G_FMT failed: %s", g_strerror (errno));
    return FALSE;
  }

  caps =
      gst_v4l2_decoder_probe_caps_for_format (self,
      fmt.fmt.pix_mp.pixelformat, fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);

  /* And then enumerate other possible formats and place that as a second
   * structure in the caps */
  for (i = 0; ret >= 0; i++) {
    struct v4l2_fmtdesc fmtdesc = { i, self->src_buf_type, };

    ret = ioctl (self->video_fd, VIDIOC_ENUM_FMT, &fmtdesc);
    if (ret < 0) {
      if (errno != EINVAL)
        GST_ERROR_OBJECT (self, "VIDIOC_ENUM_FMT failed: %s",
            g_strerror (errno));
      continue;
    }

    tmp = gst_v4l2_decoder_probe_caps_for_format (self, fmtdesc.pixelformat,
        fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);
    caps = gst_caps_merge (caps, tmp);
  }

  filter = gst_static_caps_get (static_filter);
  tmp = caps;
  caps = gst_caps_intersect_full (tmp, filter, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (tmp);
  gst_caps_unref (filter);

  tmp = gst_caps_copy (caps);
  gst_caps_filter_and_map_in_place (caps, filter_only_dmabuf_caps, NULL);
  gst_caps_filter_and_map_in_place (tmp, filter_non_dmabuf_caps, NULL);
  gst_caps_append (caps, tmp);

  GST_DEBUG_OBJECT (self, "Probed caps: %" GST_PTR_FORMAT, caps);

  return caps;
}

gboolean
gst_v4l2_decoder_select_src_format (GstV4l2Decoder * self, GstCaps * caps,
    GstVideoInfo * vinfo, GstVideoInfoDmaDrm * vinfo_drm)
{
  gint ret;
  struct v4l2_format fmt = {
    .type = self->src_buf_type,
  };
  GstVideoFormat format;
  guint32 pix_fmt;
  GstVideoInfo tmp_vinfo;
  GstVideoInfoDmaDrm tmp_vinfo_drm;

  if (gst_caps_is_empty (caps))
    return FALSE;

  ret = ioctl (self->video_fd, VIDIOC_G_FMT, &fmt);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_G_FMT failed: %s", g_strerror (errno));
    return FALSE;
  }

  gst_video_info_init (&tmp_vinfo);
  gst_video_info_dma_drm_init (&tmp_vinfo_drm);

  GST_DEBUG_OBJECT (self, "Original caps: %" GST_PTR_FORMAT, caps);
  caps = gst_caps_fixate (caps);
  GST_DEBUG_OBJECT (self, "Fixated caps: %" GST_PTR_FORMAT, caps);

  if (gst_video_info_dma_drm_from_caps (&tmp_vinfo_drm, caps)) {
    format = tmp_vinfo_drm.vinfo.finfo->format;
  } else if (gst_video_info_from_caps (&tmp_vinfo, caps)) {
    format = tmp_vinfo.finfo->format;
  } else {
    GST_WARNING_OBJECT (self, "Can't transform caps into video info!");
    return FALSE;
  }

  if (!gst_v4l2_format_from_video_format (format, &pix_fmt)) {
    GST_ERROR_OBJECT (self, "Unsupported V4L2 pixelformat %" GST_FOURCC_FORMAT,
        GST_FOURCC_ARGS (fmt.fmt.pix_mp.pixelformat));
    return FALSE;
  }

  if (pix_fmt != fmt.fmt.pix_mp.pixelformat) {
    GST_WARNING_OBJECT (self, "Trying to use peer format: %s",
        gst_video_format_to_string (format));
    fmt.fmt.pix_mp.pixelformat = pix_fmt;

    ret = ioctl (self->video_fd, VIDIOC_S_FMT, &fmt);
    if (ret < 0) {
      GST_ERROR_OBJECT (self, "VIDIOC_S_FMT failed: %s", g_strerror (errno));
      return FALSE;
    }
  }

  if (!gst_v4l2_format_to_video_info (&fmt, vinfo)) {
    GST_ERROR_OBJECT (self, "Unsupported V4L2 pixelformat %" GST_FOURCC_FORMAT,
        GST_FOURCC_ARGS (fmt.fmt.pix_mp.pixelformat));
    return FALSE;
  }

  if (tmp_vinfo_drm.drm_fourcc) {
    if (!gst_video_info_dma_drm_from_video_info (vinfo_drm, vinfo, 0)) {
      GST_ERROR_OBJECT (self,
          "Unsupported V4L2 pixelformat for DRM %" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (fmt.fmt.pix_mp.pixelformat));
      return FALSE;
    }
  } else {
    gst_video_info_dma_drm_init (vinfo_drm);
  }

  GST_INFO_OBJECT (self, "Selected format %s %ix%i",
      gst_video_format_to_string (vinfo->finfo->format),
      vinfo->width, vinfo->height);

  return TRUE;
}

GstVideoCodecState *
gst_v4l2_decoder_set_output_state (GstVideoDecoder * decoder,
    GstVideoInfo * vinfo, GstVideoInfoDmaDrm * vinfo_drm, guint width,
    guint height, GstVideoCodecState * reference)
{
  GstVideoCodecState *state;

  state = gst_video_decoder_set_output_state (decoder, vinfo->finfo->format,
      width, height, reference);

  if (vinfo_drm->drm_fourcc /* != DRM_FORMAT_INVALID */ ) {
    GstVideoInfoDmaDrm tmp_vinfo_drm;

    gst_video_info_dma_drm_from_video_info (&tmp_vinfo_drm, &state->info, 0);
    state->caps = gst_video_info_dma_drm_to_caps (&tmp_vinfo_drm);
  } else {
    state->caps = gst_video_info_to_caps (&state->info);
  }

  GST_DEBUG_OBJECT (decoder, "Setting caps: %" GST_PTR_FORMAT, state->caps);

  return state;
}

gint
gst_v4l2_decoder_request_buffers (GstV4l2Decoder * self,
    GstPadDirection direction, guint num_buffers)
{
  gint ret;
  struct v4l2_requestbuffers reqbufs = {
    .count = num_buffers,
    .memory = V4L2_MEMORY_MMAP,
    .type = direction_to_buffer_type (self, direction),
  };

  GST_DEBUG_OBJECT (self, "Requesting %u buffers", num_buffers);

  ret = ioctl (self->video_fd, VIDIOC_REQBUFS, &reqbufs);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_REQBUFS failed: %s", g_strerror (errno));
    return ret;
  }

  if (direction == GST_PAD_SINK) {
    if (reqbufs.capabilities & V4L2_BUF_CAP_SUPPORTS_M2M_HOLD_CAPTURE_BUF)
      self->supports_holding_capture = TRUE;
    else
      self->supports_holding_capture = FALSE;
  }

  return reqbufs.count;
}

gboolean
gst_v4l2_decoder_export_buffer (GstV4l2Decoder * self,
    GstPadDirection direction, gint index, gint * fds, gsize * sizes,
    gsize * offsets, guint * num_fds)
{
  gint i, ret;
  struct v4l2_plane planes[GST_VIDEO_MAX_PLANES] = { {0} };
  struct v4l2_buffer v4l2_buf = {
    .index = index,
    .type = direction_to_buffer_type (self, direction),
  };

  if (self->mplane) {
    v4l2_buf.length = GST_VIDEO_MAX_PLANES;
    v4l2_buf.m.planes = planes;
  }

  ret = ioctl (self->video_fd, VIDIOC_QUERYBUF, &v4l2_buf);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_QUERYBUF failed: %s", g_strerror (errno));
    return FALSE;
  }

  if (self->mplane) {
    for (i = 0; i < v4l2_buf.length; i++) {
      struct v4l2_plane *plane = v4l2_buf.m.planes + i;
      struct v4l2_exportbuffer expbuf = {
        .type = direction_to_buffer_type (self, direction),
        .index = index,
        .plane = i,
        .flags = O_CLOEXEC | O_RDWR,
      };

      ret = ioctl (self->video_fd, VIDIOC_EXPBUF, &expbuf);
      if (ret < 0) {
        gint j;
        GST_ERROR_OBJECT (self, "VIDIOC_EXPBUF failed: %s", g_strerror (errno));

        for (j = i - 1; j >= 0; j--)
          close (fds[j]);

        return FALSE;
      }

      *num_fds = v4l2_buf.length;
      fds[i] = expbuf.fd;
      sizes[i] = plane->length;
      offsets[i] = plane->data_offset;
    }
  } else {
    struct v4l2_exportbuffer expbuf = {
      .type = direction_to_buffer_type (self, direction),
      .index = index,
      .flags = O_CLOEXEC | O_RDWR,
    };

    ret = ioctl (self->video_fd, VIDIOC_EXPBUF, &expbuf);
    if (ret < 0) {
      GST_ERROR_OBJECT (self, "VIDIOC_EXPBUF failed: %s", g_strerror (errno));
      return FALSE;
    }

    *num_fds = 1;
    fds[0] = expbuf.fd;
    sizes[0] = v4l2_buf.length;
    offsets[0] = 0;
  }

  return TRUE;
}

static gboolean
gst_v4l2_decoder_queue_sink_mem (GstV4l2Decoder * self,
    GstV4l2Request * request, GstMemory * mem, guint32 frame_num, guint flags)
{
  gint ret;
  gsize bytesused = gst_memory_get_sizes (mem, NULL, NULL);
  struct v4l2_plane plane = {
    .bytesused = bytesused,
  };
  struct v4l2_buffer buf = {
    .type = self->sink_buf_type,
    .memory = V4L2_MEMORY_MMAP,
    .index = gst_v4l2_codec_memory_get_index (mem),
    .timestamp.tv_sec = frame_num / 1000000,
    .timestamp.tv_usec = frame_num % 1000000,
    .request_fd = request->fd,
    .flags = V4L2_BUF_FLAG_REQUEST_FD | flags,
  };

  GST_TRACE_OBJECT (self, "Queueing bitstream buffer %i", buf.index);

  if (self->mplane) {
    buf.length = 1;
    buf.m.planes = &plane;
  } else {
    buf.bytesused = bytesused;
  }

  ret = ioctl (self->video_fd, VIDIOC_QBUF, &buf);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_QBUF failed: %s", g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_v4l2_decoder_queue_src_buffer (GstV4l2Decoder * self, GstBuffer * buffer)
{
  gint i, ret;
  struct v4l2_plane planes[GST_VIDEO_MAX_PLANES];
  struct v4l2_buffer buf = {
    .type = self->src_buf_type,
    .memory = V4L2_MEMORY_MMAP,
    .index = gst_v4l2_codec_buffer_get_index (buffer),
  };

  GST_TRACE_OBJECT (self, "Queuing picture buffer %i", buf.index);

  if (self->mplane) {
    buf.length = gst_buffer_n_memory (buffer);
    buf.m.planes = planes;
    for (i = 0; i < buf.length; i++) {
      GstMemory *mem = gst_buffer_peek_memory (buffer, i);
      /* *INDENT-OFF* */
      planes[i] = (struct v4l2_plane) {
        .bytesused = gst_memory_get_sizes (mem, NULL, NULL),
      };
      /* *INDENT-ON* */
    }
  } else {
    buf.bytesused = gst_buffer_get_size (buffer);
  }

  ret = ioctl (self->video_fd, VIDIOC_QBUF, &buf);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_QBUF failed: %s", g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_v4l2_decoder_dequeue_sink (GstV4l2Decoder * self)
{
  gint ret;
  struct v4l2_plane planes[GST_VIDEO_MAX_PLANES] = { {0} };
  struct v4l2_buffer buf = {
    .type = self->sink_buf_type,
    .memory = V4L2_MEMORY_MMAP,
  };

  if (self->mplane) {
    buf.length = GST_VIDEO_MAX_PLANES;
    buf.m.planes = planes;
  }

  ret = ioctl (self->video_fd, VIDIOC_DQBUF, &buf);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_DQBUF failed: %s", g_strerror (errno));
    return FALSE;
  }

  GST_TRACE_OBJECT (self, "Dequeued bitstream buffer %i", buf.index);

  return TRUE;
}

static gboolean
gst_v4l2_decoder_dequeue_src (GstV4l2Decoder * self, guint32 * out_frame_num)
{
  gint ret;
  struct v4l2_plane planes[GST_VIDEO_MAX_PLANES] = { {0} };
  struct v4l2_buffer buf = {
    .type = self->src_buf_type,
    .memory = V4L2_MEMORY_MMAP,
  };

  if (self->mplane) {
    buf.length = GST_VIDEO_MAX_PLANES;
    buf.m.planes = planes;
  }

  ret = ioctl (self->video_fd, VIDIOC_DQBUF, &buf);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_DQBUF failed: %s", g_strerror (errno));
    return FALSE;
  }

  *out_frame_num = buf.timestamp.tv_usec + buf.timestamp.tv_sec * 1000000;

  GST_TRACE_OBJECT (self, "Dequeued picture buffer %i", buf.index);

  return TRUE;
}

gboolean
gst_v4l2_decoder_set_controls (GstV4l2Decoder * self, GstV4l2Request * request,
    struct v4l2_ext_control *control, guint count)
{
  gint ret;
  struct v4l2_ext_controls controls = {
    .controls = control,
    .count = count,
    .request_fd = request ? request->fd : 0,
    .which = request ? V4L2_CTRL_WHICH_REQUEST_VAL : 0,
  };

  ret = ioctl (self->video_fd, VIDIOC_S_EXT_CTRLS, &controls);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_S_EXT_CTRLS failed: %s",
        g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_v4l2_decoder_get_controls (GstV4l2Decoder * self,
    struct v4l2_ext_control *control, guint count)
{
  gint ret;
  struct v4l2_ext_controls controls = {
    .controls = control,
    .count = count,
  };

  ret = ioctl (self->video_fd, VIDIOC_G_EXT_CTRLS, &controls);
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "VIDIOC_G_EXT_CTRLS failed: %s",
        g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_v4l2_decoder_query_control_size (GstV4l2Decoder * self,
    unsigned int control_id, unsigned int *control_size)
{
  gint ret;
  struct v4l2_query_ext_ctrl control = {
    .id = control_id,
  };

  if (control_size)
    *control_size = 0;

  ret = ioctl (self->video_fd, VIDIOC_QUERY_EXT_CTRL, &control);
  if (ret < 0)
    /*
     * It's not an error if a control is not supported by this driver.
     * Return false but don't print any error.
     */
    return FALSE;

  if (control_size)
    *control_size = control.elem_size;
  return TRUE;
}

void
gst_v4l2_decoder_install_properties (GObjectClass * gobject_class,
    gint prop_offset, GstV4l2CodecDevice * device)
{
  const gchar *media_device_path = NULL;
  const gchar *video_device_path = NULL;

  if (device) {
    media_device_path = device->media_device_path;
    video_device_path = device->video_device_path;
  }

  g_object_class_install_property (gobject_class, PROP_MEDIA_DEVICE,
      g_param_spec_string ("media-device", "Media Device Path",
          "Path to the media device node", media_device_path,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VIDEO_DEVICE,
      g_param_spec_string ("video-device", "Video Device Path",
          "Path to the video device node", video_device_path,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

void
gst_v4l2_decoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstV4l2Decoder *self = GST_V4L2_DECODER (object);

  switch (prop_id) {
    case PROP_MEDIA_DEVICE:
      g_free (self->media_device);
      self->media_device = g_value_dup_string (value);
      break;
    case PROP_VIDEO_DEVICE:
      g_free (self->video_device);
      self->video_device = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

void
gst_v4l2_decoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstV4l2Decoder *self = GST_V4L2_DECODER (object);

  switch (prop_id) {
    case PROP_MEDIA_DEVICE:
      g_value_set_string (value, self->media_device);
      break;
    case PROP_VIDEO_DEVICE:
      g_value_set_string (value, self->video_device);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * gst_v4l2_decoder_register:
 * @plugin: a #GstPlugin
 * @dec_type: A #GType for the codec
 * @class_init: The #GClassInitFunc for #dec_type
 * @instance_init: The #GInstanceInitFunc for #dec_type
 * @element_name_tmpl: A string to use for the first codec found and as a template for the next ones.
 * @device: (transfer full) A #GstV4l2CodecDevice
 * @rank: The rank to use for the element
 * @class_data: (nullable) (transfer full) A #gpointer to pass as class_data, set to @device if null
 * @element_name (nullable) (out) Sets the pointer to the new element name
 *
 * Registers a decoder element as a subtype of @dec_type for @plugin.
 * Will create a different sub_types for each subsequent @decoder of the
 * same type.
 */
void
gst_v4l2_decoder_register (GstPlugin * plugin,
    GType dec_type, GClassInitFunc class_init, gconstpointer class_data,
    GInstanceInitFunc instance_init, const gchar * element_name_tmpl,
    GstV4l2CodecDevice * device, guint rank, gchar ** element_name)
{
  GTypeQuery type_query;
  GTypeInfo type_info = { 0, };
  GType subtype;
  gchar *type_name;

  g_type_query (dec_type, &type_query);
  memset (&type_info, 0, sizeof (type_info));
  type_info.class_size = type_query.class_size;
  type_info.instance_size = type_query.instance_size;
  type_info.class_init = class_init;
  type_info.class_data = class_data;
  type_info.instance_init = instance_init;

  if (class_data == device)
    GST_MINI_OBJECT_FLAG_SET (device, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  /* The first decoder to be registered should use a constant name, like
   * v4l2slvp8dec, for any additional decoders, we create unique names. Decoder
   * names may change between boots, so this should help gain stable names for
   * the most common use cases. SL stands for state-less, we differentiate
   * with v4l2vp8dec as this element may not have the same properties */
  type_name = g_strdup_printf (element_name_tmpl, "");

  if (g_type_from_name (type_name) != 0) {
    gchar *basename = g_path_get_basename (device->video_device_path);
    g_free (type_name);
    type_name = g_strdup_printf (element_name_tmpl, basename);
    g_free (basename);
  }

  subtype = g_type_register_static (dec_type, type_name, &type_info, 0);

  if (!gst_element_register (plugin, type_name, rank, subtype)) {
    GST_WARNING ("Failed to register plugin '%s'", type_name);
    g_free (type_name);
    type_name = NULL;
  }

  if (element_name)
    *element_name = type_name;
  else
    g_free (type_name);
}

/*
 * gst_v4l2_decoder_alloc_request:
 * @self a #GstV4l2Decoder pointer
 * @frame_num: Used as a timestamp to identify references
 * @bitstream the #GstMemory that holds the bitstream data
 * @pic_buf the #GstBuffer holding the decoded picture
 *
 * Allocate a Linux media request file descriptor. This request wrapper will
 * hold a reference to the requested bitstream memory to decoded and the
 * picture buffer this request will decode to. This will be used for
 * transparent management of the V4L2 queues.
 *
 * Returns: a new #GstV4l2Request
 */
GstV4l2Request *
gst_v4l2_decoder_alloc_request (GstV4l2Decoder * self, guint32 frame_num,
    GstMemory * bitstream, GstBuffer * pic_buf)
{
  GstV4l2Request *request = gst_queue_array_pop_head (self->request_pool);
  gint ret;

  if (!request) {
    request = g_new0 (GstV4l2Request, 1);

    ret = ioctl (self->media_fd, MEDIA_IOC_REQUEST_ALLOC, &request->fd);
    if (ret < 0) {
      GST_ERROR_OBJECT (self, "MEDIA_IOC_REQUEST_ALLOC failed: %s",
          g_strerror (errno));
      return NULL;
    }

    request->poll = gst_poll_new (FALSE);
    gst_poll_fd_init (&request->pollfd);
    request->pollfd.fd = request->fd;
    gst_poll_add_fd (request->poll, &request->pollfd);
    gst_poll_fd_ctl_pri (request->poll, &request->pollfd, TRUE);
  }

  request->decoder = g_object_ref (self);
  request->bitstream = gst_memory_ref (bitstream);
  request->pic_buf = gst_buffer_ref (pic_buf);
  request->frame_num = frame_num;
  request->ref_count = 1;

  return request;
}

/*
 * gst_v4l2_decoder_alloc_sub_request:
 * @self a #GstV4l2Decoder pointer
 * @prev_request the #GstV4l2Request this request continue
 * @bitstream the #GstMemory that holds the bitstream data
 *
 * Allocate a Linux media request file descriptor. Similar to
 * gst_v4l2_decoder_alloc_request(), but used when a request is the
 * continuation of the decoding of the same picture. This is notably the case
 * for subsequent slices or for second field of a frame.
 *
 * Returns: a new #GstV4l2Request
 */
GstV4l2Request *
gst_v4l2_decoder_alloc_sub_request (GstV4l2Decoder * self,
    GstV4l2Request * prev_request, GstMemory * bitstream)
{
  GstV4l2Request *request = gst_queue_array_pop_head (self->request_pool);
  gint ret;

  if (!request) {
    request = g_new0 (GstV4l2Request, 1);

    ret = ioctl (self->media_fd, MEDIA_IOC_REQUEST_ALLOC, &request->fd);
    if (ret < 0) {
      GST_ERROR_OBJECT (self, "MEDIA_IOC_REQUEST_ALLOC failed: %s",
          g_strerror (errno));
      return NULL;
    }

    request->poll = gst_poll_new (FALSE);
    gst_poll_fd_init (&request->pollfd);
    request->pollfd.fd = request->fd;
    gst_poll_add_fd (request->poll, &request->pollfd);
    gst_poll_fd_ctl_pri (request->poll, &request->pollfd, TRUE);
  }

  request->decoder = g_object_ref (self);
  request->bitstream = gst_memory_ref (bitstream);
  request->pic_buf = gst_buffer_ref (prev_request->pic_buf);
  request->frame_num = prev_request->frame_num;
  request->sub_request = TRUE;
  request->ref_count = 1;

  return request;
}

/**
 * gst_v4l2_decoder_set_render_delay:
 * @self: a #GstV4l2Decoder pointer
 * @delay: The expected render delay
 *
 * The decoder will adjust the number of allowed concurrent request in order
 * to allow this delay. The same number of concurrent bitstream buffer will be
 * used, so make sure to adjust the number of bitstream buffer.
 *
 * For per-slice decoder, this is the maximum number of pending slice, so the
 * render backlog in frame may be less then the render delay.
 */
void
gst_v4l2_decoder_set_render_delay (GstV4l2Decoder * self, guint delay)
{
  self->render_delay = delay;
}

/**
 * gst_v4l2_decoder_get_render_delay:
 * @self: a #GstV4l2Decoder pointer
 *
 * This function is used to avoid storing the render delay in multiple places.
 *
 * Returns: The currently configured render delay.
 */
guint
gst_v4l2_decoder_get_render_delay (GstV4l2Decoder * self)
{
  return self->render_delay;
}

GstV4l2Request *
gst_v4l2_request_ref (GstV4l2Request * request)
{
  request->ref_count++;
  return request;
}

static void
gst_v4l2_request_free (GstV4l2Request * request)
{
  GstV4l2Decoder *decoder = request->decoder;

  request->decoder = NULL;
  close (request->fd);
  gst_poll_free (request->poll);
  g_free (request);

  if (decoder)
    g_object_unref (decoder);
}

void
gst_v4l2_request_unref (GstV4l2Request * request)
{
  GstV4l2Decoder *decoder = request->decoder;
  gint ret;

  g_return_if_fail (request->ref_count > 0);

  if (--request->ref_count > 0)
    return;

  g_clear_pointer (&request->bitstream, gst_memory_unref);
  g_clear_pointer (&request->pic_buf, gst_buffer_unref);
  request->frame_num = G_MAXUINT32;
  request->failed = FALSE;
  request->hold_pic_buf = FALSE;
  request->sub_request = FALSE;

  if (request->pending) {
    gint idx;

    GST_DEBUG_OBJECT (decoder, "Freeing pending request %i.", request->fd);

    idx = gst_queue_array_find (decoder->pending_requests, NULL, request);
    if (idx >= 0)
      gst_queue_array_drop_element (decoder->pending_requests, idx);

    gst_v4l2_request_free (request);
    return;
  }

  GST_TRACE_OBJECT (decoder, "Recycling request %i.", request->fd);

  ret = ioctl (request->fd, MEDIA_REQUEST_IOC_REINIT, NULL);
  if (ret < 0) {
    GST_ERROR_OBJECT (request->decoder, "MEDIA_REQUEST_IOC_REINIT failed: %s",
        g_strerror (errno));
    gst_v4l2_request_free (request);
    return;
  }

  gst_queue_array_push_tail (decoder->request_pool, request);
  g_clear_object (&request->decoder);
}

gboolean
gst_v4l2_request_queue (GstV4l2Request * request, guint flags)
{
  GstV4l2Decoder *decoder = request->decoder;
  gint ret;
  guint max_pending;

  GST_TRACE_OBJECT (decoder, "Queuing request %i.", request->fd);

  /* this would lead to stalls if we tried to use this feature and it wasn't
   * supported. */
  if ((flags & V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF)
      && !decoder->supports_holding_capture) {
    GST_ERROR_OBJECT (decoder,
        "Driver does not support holding capture buffer.");
    return FALSE;
  }

  if (!gst_v4l2_decoder_queue_sink_mem (decoder, request,
          request->bitstream, request->frame_num, flags)) {
    GST_ERROR_OBJECT (decoder, "Driver did not accept the bitstream data.");
    return FALSE;
  }

  if (!request->sub_request &&
      !gst_v4l2_decoder_queue_src_buffer (decoder, request->pic_buf)) {
    GST_ERROR_OBJECT (decoder, "Driver did not accept the picture buffer.");
    return FALSE;
  }

  ret = ioctl (request->fd, MEDIA_REQUEST_IOC_QUEUE, NULL);
  if (ret < 0) {
    GST_ERROR_OBJECT (decoder, "MEDIA_REQUEST_IOC_QUEUE, failed: %s",
        g_strerror (errno));
    return FALSE;
  }

  if (flags & V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF)
    request->hold_pic_buf = TRUE;

  request->pending = TRUE;
  gst_queue_array_push_tail (decoder->pending_requests,
      gst_v4l2_request_ref (request));

  max_pending = MAX (1, decoder->render_delay);

  if (gst_queue_array_get_length (decoder->pending_requests) > max_pending) {
    GstV4l2Request *pending_req;

    pending_req = gst_queue_array_peek_head (decoder->pending_requests);
    gst_v4l2_request_set_done (pending_req);
  }

  return TRUE;
}

gint
gst_v4l2_request_set_done (GstV4l2Request * request)
{
  GstV4l2Decoder *decoder = request->decoder;
  GstV4l2Request *pending_req = NULL;
  gint ret;

  if (!request->pending)
    return 1;

  GST_DEBUG_OBJECT (decoder, "Waiting for request %i to complete.",
      request->fd);

  ret = gst_poll_wait (request->poll, GST_SECOND);
  if (ret == 0) {
    GST_WARNING_OBJECT (decoder, "Request %i took too long.", request->fd);
    return 0;
  }

  if (ret < 0) {
    GST_WARNING_OBJECT (decoder, "Request %i error: %s (%i)",
        request->fd, g_strerror (errno), errno);
    return ret;
  }

  while ((pending_req = gst_queue_array_pop_head (decoder->pending_requests))) {
    gst_v4l2_decoder_dequeue_sink (decoder);
    g_clear_pointer (&pending_req->bitstream, gst_memory_unref);

    if (!pending_req->hold_pic_buf) {
      guint32 frame_num = G_MAXUINT32;

      if (!gst_v4l2_decoder_dequeue_src (decoder, &frame_num)) {
        pending_req->failed = TRUE;
      } else if (frame_num != pending_req->frame_num) {
        GST_WARNING_OBJECT (decoder,
            "Requested frame %u, but driver returned frame %u.",
            pending_req->frame_num, frame_num);
        pending_req->failed = TRUE;
      }
    }

    pending_req->pending = FALSE;
    gst_v4l2_request_unref (pending_req);

    if (pending_req == request)
      break;
  }

  /* Pending request must be in the pending request list */
  g_assert (pending_req == request);

  return ret;
}

gboolean
gst_v4l2_request_failed (GstV4l2Request * request)
{
  return request->failed;
}

GstBuffer *
gst_v4l2_request_dup_pic_buf (GstV4l2Request * request)
{
  return gst_buffer_ref (request->pic_buf);
}

gint
gst_v4l2_request_get_fd (GstV4l2Request * request)
{
  return request->fd;
}

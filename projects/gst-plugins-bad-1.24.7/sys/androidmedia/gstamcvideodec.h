/*
 * Copyright (C) 2012, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_AMC_VIDEO_DEC_H__
#define __GST_AMC_VIDEO_DEC_H__

#include <gst/gst.h>
#include <gst/gl/gl.h>

#include <gst/video/gstvideodecoder.h>

#include "gstamc.h"
#include "gstamcsurfacetexture.h"

G_BEGIN_DECLS

#define GST_TYPE_AMC_VIDEO_DEC \
  (gst_amc_video_dec_get_type())
#define GST_AMC_VIDEO_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMC_VIDEO_DEC,GstAmcVideoDec))
#define GST_AMC_VIDEO_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMC_VIDEO_DEC,GstAmcVideoDecClass))
#define GST_AMC_VIDEO_DEC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_AMC_VIDEO_DEC,GstAmcVideoDecClass))
#define GST_IS_AMC_VIDEO_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMC_VIDEO_DEC))
#define GST_IS_AMC_VIDEO_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMC_VIDEO_DEC))

typedef struct _GstAmcVideoDec GstAmcVideoDec;
typedef struct _GstAmcVideoDecClass GstAmcVideoDecClass;
typedef enum _GstAmcCodecConfig GstAmcCodecConfig;

enum _GstAmcCodecConfig
{
  AMC_CODEC_CONFIG_NONE,
  AMC_CODEC_CONFIG_WITH_SURFACE,
  AMC_CODEC_CONFIG_WITHOUT_SURFACE,
};

struct _GstAmcVideoDec
{
  GstVideoDecoder parent;

  /* < private > */
  GstAmcCodec *codec;
  GstAmcCodecConfig codec_config;

  GstVideoCodecState *input_state;
  gboolean input_state_changed;

  /* Output format of the codec */
  GstVideoFormat format;
  GstAmcColorFormatInfo color_format_info;

  /* Output dimensions */
  guint width;
  guint height;

  guint8 *codec_data;
  gsize codec_data_size;
  /* TRUE if the component is configured and saw
   * the first buffer */
  gboolean started;
  gboolean flushing;

  GstClockTime last_upstream_ts;

  /* Draining state */
  GMutex drain_lock;
  GCond drain_cond;
  /* TRUE if EOS buffers shouldn't be forwarded */
  gboolean draining;
  /* TRUE if the component is drained currently */
  gboolean drained;

  GstAmcSurfaceTexture *surface;

  GstGLDisplay *gl_display;
  GstGLContext *gl_context;
  GstGLContext *other_gl_context;

  gboolean downstream_supports_gl;
  GstFlowReturn downstream_flow_ret;

  gboolean gl_mem_attached;
  GstGLMemory *oes_mem;
  GError *gl_error;
  GMutex gl_lock;
  GCond gl_cond;
  guint gl_last_rendered_frame;
  guint gl_pushed_frame_count; /* n buffers pushed */
  guint gl_ready_frame_count;  /* n buffers ready for GL access */
  guint gl_released_frame_count;  /* n buffers released */
  GQueue *gl_queue;
};

struct _GstAmcVideoDecClass
{
  GstVideoDecoderClass parent_class;

  const GstAmcCodecInfo *codec_info;
};

GType gst_amc_video_dec_get_type (void);

G_END_DECLS

#endif /* __GST_AMC_VIDEO_DEC_H__ */

/* GStreamer
 *
 * Copyright (C) 2016 Igalia
 *
 * Authors:
 *  Víctor Manuel Jáquez Leal <vjaquez@igalia.com>
 *  Javier Martin <javiermartin@by.com.es>
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
 *
 */

#ifndef __GST_KMS_SINK_H__
#define __GST_KMS_SINK_H__

#include <gst/video/gstvideosink.h>
#include <gst/video/video-hdr.h>

G_BEGIN_DECLS

#define GST_TYPE_KMS_SINK \
  (gst_kms_sink_get_type())
#define GST_KMS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_KMS_SINK, GstKMSSink))
#define GST_KMS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_KMS_SINK, GstKMSSinkClass))
#define GST_IS_KMS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_KMS_SINK))
#define GST_IS_KMS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_KMS_SINK))

typedef struct _GstKMSSink GstKMSSink;
typedef struct _GstKMSSinkClass GstKMSSinkClass;

struct _GstKMSSink {
  GstVideoSink videosink;

  /*< private >*/
  gint fd;
  gint conn_id;
  gint crtc_id;
  gint plane_id;
  guint pipe;

  /* crtc data */
  guint16 hdisplay, vdisplay;
  guint32 buffer_id;

  /* capabilities */
  gboolean has_prime_import;
  gboolean has_prime_export;
  gboolean has_async_page_flip;
  gboolean can_scale;

  gboolean modesetting_enabled;
  gboolean restore_crtc;
  GstStructure *connector_props;
  GstStructure *plane_props;

  GstVideoInfo vinfo;
  GstCaps *allowed_caps;
  GstBufferPool *pool;
  GstAllocator *allocator;
  GstVideoInfo last_vinfo;
  guint last_width;
  guint last_height;
  GstBuffer *last_buffer;
  GstMemory *tmp_kmsmem;

  gchar *devname;
  gchar *bus_id;

  guint32 mm_width, mm_height;
  gpointer saved_crtc;
  GstPoll *poll;
  GstPollFD pollfd;

  /* render video rectangle */
  GstVideoRectangle render_rect;

  /* reconfigure info if driver doesn't scale */
  GstVideoRectangle pending_rect;
  gboolean reconfigure;

  gboolean is_internal_fd;
  gboolean skip_vsync;

#ifdef HAVE_DRM_HDR
  /* HDR mastering related structure */
  gboolean no_infoframe;
  gboolean has_hdr_info;
  gboolean has_sent_hdrif;
  guint32 edidPropID;
  guint32 hdrPropID;
  gchar colorimetry;
  GstVideoMasteringDisplayInfo hdr_minfo;
  GstVideoContentLightLevel hdr_cll;
#endif
};

struct _GstKMSSinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_kms_sink_get_type (void) G_GNUC_CONST;
GST_ELEMENT_REGISTER_DECLARE (kmssink);
G_END_DECLS

#endif /* __GST_KMS_SINK_H__ */

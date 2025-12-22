/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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

#ifndef __GST_KS_VIDEO_DEVICE_H__
#define __GST_KS_VIDEO_DEVICE_H__

#include "gstksclock.h"

#include <gst/gst.h>

#include <windows.h>
#include <ks.h>

G_BEGIN_DECLS

#define GST_TYPE_KS_VIDEO_DEVICE \
  (gst_ks_video_device_get_type ())
#define GST_KS_VIDEO_DEVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_KS_VIDEO_DEVICE, GstKsVideoDevice))
#define GST_KS_VIDEO_DEVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_KS_VIDEO_DEVICE, GstKsVideoDeviceClass))
#define GST_IS_KS_VIDEO_DEVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_KS_VIDEO_DEVICE))
#define GST_IS_KS_VIDEO_DEVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_KS_VIDEO_DEVICE))

typedef struct _GstKsVideoDevice        GstKsVideoDevice;
typedef struct _GstKsVideoDeviceClass   GstKsVideoDeviceClass;
typedef struct _GstKsVideoDevicePrivate GstKsVideoDevicePrivate;

typedef GstBuffer * (* GstKsAllocFunction)(guint buf_size, guint alignment, gpointer user_data);

struct _GstKsVideoDevice
{
  GObject parent;

  GstKsAllocFunction allocfunc;
  gpointer allocfunc_data;

  GstKsVideoDevicePrivate *priv;
};

struct _GstKsVideoDeviceClass
{
  GObjectClass parent_class;
};

GType gst_ks_video_device_get_type (void);

GstKsVideoDevice * gst_ks_video_device_new (const gchar * device_path, GstKsClock * clock, GstKsAllocFunction allocfunc, gpointer allocfunc_data);
gboolean gst_ks_video_device_open (GstKsVideoDevice * self);
void gst_ks_video_device_close (GstKsVideoDevice * self);

GstCaps * gst_ks_video_device_get_available_caps (GstKsVideoDevice * self);
gboolean gst_ks_video_device_has_caps (GstKsVideoDevice * self);
gboolean gst_ks_video_device_set_caps (GstKsVideoDevice * self, GstCaps * caps);

gboolean gst_ks_video_device_set_state (GstKsVideoDevice * self, KSSTATE state, gulong * error_code);

GstClockTime gst_ks_video_device_get_duration (GstKsVideoDevice * self);
gboolean gst_ks_video_device_get_latency (GstKsVideoDevice * self, GstClockTime * min_latency, GstClockTime * max_latency);

GstFlowReturn gst_ks_video_device_read_frame (GstKsVideoDevice * self, GstBuffer ** buf, GstClockTime * presentation_time, gulong * error_code, gchar ** error_str);
gboolean gst_ks_video_device_postprocess_frame (GstKsVideoDevice * self, GstBuffer **buf);
void gst_ks_video_device_cancel (GstKsVideoDevice * self);
void gst_ks_video_device_cancel_stop (GstKsVideoDevice * self);

gboolean gst_ks_video_device_stream_is_muxed (GstKsVideoDevice * self);

G_END_DECLS

#endif /* __GST_KS_VIDEO_DEVICE_H__ */

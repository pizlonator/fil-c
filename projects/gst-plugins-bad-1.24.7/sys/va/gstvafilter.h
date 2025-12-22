/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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

#pragma once

#include <gst/va/gstva.h>
#include <gst/video/video.h>

#include <va/va.h>
#include <va/va_vpp.h>

G_BEGIN_DECLS

#define GST_TYPE_VA_FILTER (gst_va_filter_get_type())
G_DECLARE_FINAL_TYPE (GstVaFilter, gst_va_filter, GST, VA_FILTER, GstObject)

typedef enum {
  GST_VA_DEINTERLACE_BOB = VAProcDeinterlacingBob,
  GST_VA_DEINTERLACE_WEAVE = VAProcDeinterlacingWeave,
  GST_VA_DEINTERLACE_ADAPTIVE = VAProcDeinterlacingMotionAdaptive,
  GST_VA_DEINTERLACE_COMPENSATED = VAProcDeinterlacingMotionCompensated,
} GstVaDeinterlaceMethods;

enum {
  GST_VA_FILTER_PROP_DENOISE = 1,
  GST_VA_FILTER_PROP_SHARPEN,
  GST_VA_FILTER_PROP_SKINTONE,
  GST_VA_FILTER_PROP_VIDEO_DIR,
  GST_VA_FILTER_PROP_HUE,
  GST_VA_FILTER_PROP_SATURATION,
  GST_VA_FILTER_PROP_BRIGHTNESS,
  GST_VA_FILTER_PROP_CONTRAST,
  GST_VA_FILTER_PROP_AUTO_SATURATION,
  GST_VA_FILTER_PROP_AUTO_BRIGHTNESS,
  GST_VA_FILTER_PROP_AUTO_CONTRAST,
  GST_VA_FILTER_PROP_DEINTERLACE_METHOD,
  GST_VA_FILTER_PROP_HDR,
  GST_VA_FILTER_PROP_LAST
};

#define GST_TYPE_VA_SCALE_METHOD gst_va_scale_method_get_type()
GType gst_va_scale_method_get_type (void) G_GNUC_CONST;

typedef struct _GstVaSample GstVaSample;
struct _GstVaSample
{
  GstBuffer *buffer;
  guint32 flags;

  /* references for (de)interlacing */
  VASurfaceID *forward_references;
  guint num_forward_references;
  VASurfaceID *backward_references;
  guint num_backward_references;

  /* borders to preserve dar */
  gint borders_h;
  gint borders_w;

  /*< private >*/
  VASurfaceID surface;
  VARectangle rect;
};

typedef struct _GstVaComposeSample GstVaComposeSample;
struct _GstVaComposeSample
{
  /* input buffer (transfer full) */
  GstBuffer *buffer;

  VARectangle input_region;
  VARectangle output_region;

  gdouble alpha;
};

typedef struct _GstVaComposeTransaction GstVaComposeTransaction;
struct _GstVaComposeTransaction
{
  /* input sample iterator function */
  GstVaComposeSample* (*next) (gpointer user_data);

  /* the output buffer to compose onto */
  GstBuffer *output;

  /* user data parameter for "next" function */
  gpointer user_data;
};

GstVaFilter *         gst_va_filter_new                   (GstVaDisplay * display);
gboolean              gst_va_filter_open                  (GstVaFilter * self);
gboolean              gst_va_filter_close                 (GstVaFilter * self);
gboolean              gst_va_filter_is_open               (GstVaFilter * self);
gboolean              gst_va_filter_has_filter            (GstVaFilter * self,
                                                           VAProcFilterType type);
gboolean              gst_va_filter_install_properties    (GstVaFilter * self,
                                                           GObjectClass * klass);
gboolean              gst_va_filter_install_deinterlace_properties
                                                          (GstVaFilter * self,
                                                           GObjectClass * klass);
gboolean              gst_va_filter_set_scale_method      (GstVaFilter * self,
                                                           guint32 method);
gboolean              gst_va_filter_set_orientation       (GstVaFilter * self,
                                                           GstVideoOrientationMethod orientation);
GstVideoOrientationMethod gst_va_filter_get_orientation   (GstVaFilter * self);
void                  gst_va_filter_enable_cropping       (GstVaFilter * self,
                                                           gboolean cropping);
const gpointer        gst_va_filter_get_filter_caps       (GstVaFilter * self,
                                                           VAProcFilterType type,
                                                           guint * num_caps);
guint32               gst_va_filter_get_mem_types         (GstVaFilter * self);
GArray *              gst_va_filter_get_surface_formats   (GstVaFilter * self);
GstCaps *             gst_va_filter_get_caps              (GstVaFilter * self);
gboolean              gst_va_filter_set_video_info        (GstVaFilter * self,
                                                           GstVideoInfo * in_info,
                                                           GstVideoInfo * out_info);
gboolean              gst_va_filter_add_filter_buffer     (GstVaFilter * self,
                                                           gpointer data,
                                                           gsize size,
                                                           guint num);
gboolean              gst_va_filter_add_deinterlace_buffer(GstVaFilter * self,
                                                           VAProcDeinterlacingType method,
                                                           guint32 * forward,
                                                           guint32 * backward);
gboolean              gst_va_filter_drop_filter_buffers   (GstVaFilter * self);
gboolean              gst_va_filter_process               (GstVaFilter * self,
                                                           GstVaSample * src,
                                                           GstVaSample * dest);

guint32               gst_va_buffer_get_surface_flags     (GstBuffer * buffer,
                                                           GstVideoInfo * info);

gboolean              gst_va_filter_has_video_format      (GstVaFilter * self,
                                                           GstVideoFormat format,
                                                           GstCapsFeatures * feature);

gboolean              gst_va_filter_has_compose           (GstVaFilter * self);
gboolean              gst_va_filter_compose               (GstVaFilter * self,
                                                           GstVaComposeTransaction * tx);

G_END_DECLS

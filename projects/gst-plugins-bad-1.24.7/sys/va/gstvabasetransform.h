/* GStreamer
 * Copyright (C) 2021 Igalia, S.L.
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

#include <gst/base/gstbasetransform.h>

#include "gstvafilter.h"

G_BEGIN_DECLS

#define GST_TYPE_VA_BASE_TRANSFORM            (gst_va_base_transform_get_type())
#define GST_VA_BASE_TRANSFORM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VA_BASE_TRANSFORM, GstVaBaseTransform))
#define GST_IS_VA_BASE_TRANSFORM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VA_BASE_TRANSFORM))
#define GST_VA_BASE_TRANSFORM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  GST_TYPE_VA_BASE_TRANSFORM, GstVaBaseTransformClass))
#define GST_IS_VA_BASE_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  GST_TYPE_VA_BASE_TRANSFORM))
#define GST_VA_BASE_TRANSFORM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  GST_TYPE_VA_BASE_TRANSFORM, GstVaBaseTransformClass))

typedef struct _GstVaBaseTransform GstVaBaseTransform;
typedef struct _GstVaBaseTransformClass GstVaBaseTransformClass;
typedef struct _GstVaBaseTransformPrivate GstVaBaseTransformPrivate;

struct _GstVaBaseTransform
{
  GstBaseTransform parent;

  /*< public >*/
  GstVaDisplay *display;
  GstVaFilter *filter;

  GstCaps *in_caps;
  GstCaps *out_caps;
  union {
    GstVideoInfo in_info;
    GstVideoInfoDmaDrm in_drm_info;
  };
  GstVideoInfo out_info;

  gboolean negotiated;

  guint extra_min_buffers;

  /*< private >*/
  GstVaBaseTransformPrivate *priv;

  gpointer _padding[GST_PADDING];
};

struct _GstVaBaseTransformClass
{
  GstBaseTransformClass parent_class;

  /*< public >*/
  gboolean (*set_info) (GstVaBaseTransform *self,
                        GstCaps *incaps, GstVideoInfo *in_info,
                        GstCaps *outcaps, GstVideoInfo *out_info);

  void (*update_properties) (GstVaBaseTransform *self);

  /*< private >*/
  gchar *render_device_path;

  gpointer _padding[GST_PADDING];
};

GType                 gst_va_base_transform_get_type      (void);

GstAllocator *        gst_va_base_transform_allocator_from_caps
                                                          (GstVaBaseTransform * self,
                                                           GstCaps * caps);

GstFlowReturn         gst_va_base_transform_import_buffer (GstVaBaseTransform * self,
                                                           GstBuffer * inbuf,
                                                           GstBuffer ** buf);

GstCaps *             gst_va_base_transform_get_filter_caps
                                                          (GstVaBaseTransform * self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVaBaseTransform, gst_object_unref)

G_END_DECLS

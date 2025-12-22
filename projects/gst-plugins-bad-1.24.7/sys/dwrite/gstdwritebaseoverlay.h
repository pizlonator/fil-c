/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include <gst/d3d11/gstd3d11.h>
#include <gst/video/video.h>
#include <gst/base/base.h>
#include <string>
#include "gstdwrite-utils.h"
#include "gstdwrite-enums.h"
#include <vector>

G_BEGIN_DECLS

#define GST_TYPE_DWRITE_BASE_OVERLAY             (gst_dwrite_base_overlay_get_type())
#define GST_DWRITE_BASE_OVERLAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DWRITE_BASE_OVERLAY,GstDWriteBaseOverlay))
#define GST_DWRITE_BASE_OVERLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DWRITE_BASE_OVERLAY,GstDWriteBaseOverlayClass))
#define GST_DWRITE_BASE_OVERLAY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_DWRITE_BASE_OVERLAY,GstDWriteBaseOverlayClass))
#define GST_IS_DWRITE_BASE_OVERLAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DWRITE_BASE_OVERLAY))
#define GST_IS_DWRITE_BASE_OVERLAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DWRITE_BASE_OVERLAY))

typedef struct _GstDWriteBaseOverlay GstDWriteBaseOverlay;
typedef struct _GstDWriteBaseOverlayClass GstDWriteBaseOverlayClass;
typedef struct _GstDWriteBaseOverlayPrivate GstDWriteBaseOverlayPrivate;

typedef std::wstring WString;

struct _GstDWriteBaseOverlay
{
  GstBaseTransform parent;

  GstVideoInfo info;

  GstDWriteBaseOverlayPrivate *priv;
};

struct _GstDWriteBaseOverlayClass
{
  GstBaseTransformClass parent_class;

  gboolean     (*sink_event) (GstDWriteBaseOverlay * overlay,
                              GstEvent * event);

  WString      (*get_text)   (GstDWriteBaseOverlay * overlay,
                              const std::wstring & default_text,
                              GstBuffer * buffer);

  void         (*after_transform) (GstDWriteBaseOverlay * overlay,
                                   GstBuffer * buffer);
};

GType gst_dwrite_base_overlay_get_type (void);

void gst_dwrite_base_overlay_build_param_specs (std::vector<GParamSpec *> & pspec);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstDWriteBaseOverlay, gst_object_unref)

G_END_DECLS

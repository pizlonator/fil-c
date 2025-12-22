/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

#ifndef __GST_D3D11_BASE_FILTER_H__
#define __GST_D3D11_BASE_FILTER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>
#include <gst/d3d11/gstd3d11.h>
#include "gstd3d11pluginutils.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D11_BASE_FILTER             (gst_d3d11_base_filter_get_type())
#define GST_D3D11_BASE_FILTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_D3D11_BASE_FILTER,GstD3D11BaseFilter))
#define GST_D3D11_BASE_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_D3D11_BASE_FILTER,GstD3D11BaseFilterClass))
#define GST_D3D11_BASE_FILTER_GET_CLASS(obj)   (GST_D3D11_BASE_FILTER_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_IS_D3D11_BASE_FILTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_D3D11_BASE_FILTER))
#define GST_IS_D3D11_BASE_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_D3D11_BASE_FILTER))
#define GST_D3D11_BASE_FILTER_CAST(obj)        ((GstD3D11BaseFilter*)(obj))

typedef struct _GstD3D11BaseFilter GstD3D11BaseFilter;
typedef struct _GstD3D11BaseFilterClass GstD3D11BaseFilterClass;

struct _GstD3D11BaseFilter
{
  GstBaseTransform parent;

  GstD3D11Device *device;

  GstVideoInfo in_info;
  GstVideoInfo out_info;

  /* properties */
  gint adapter;
};

struct _GstD3D11BaseFilterClass
{
  GstBaseTransformClass parent_class;

  gboolean      (*set_info)           (GstD3D11BaseFilter *filter,
                                       GstCaps *incaps, GstVideoInfo *in_info,
                                       GstCaps *outcaps, GstVideoInfo *out_info);
};

GType gst_d3d11_base_filter_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstD3D11BaseFilter, gst_object_unref)

G_END_DECLS

#endif /* __GST_D3D11_BASE_FILTER_H__ */

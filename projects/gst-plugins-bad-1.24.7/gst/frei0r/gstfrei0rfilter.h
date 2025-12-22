/* GStreamer
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_FREI0R_FILTER_H__
#define __GST_FREI0R_FILTER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include "frei0r.h"
#include "gstfrei0r.h"

G_BEGIN_DECLS

#define GST_FREI0R_FILTER(obj) \
  ((GstFrei0rFilter *) obj)
#define GST_FREI0R_FILTER_CLASS(klass) \
  ((GstFrei0rFilterClass *) klass)
#define GST_FREI0R_FILTER_GET_CLASS(obj) \
  ((GstFrei0rFilterClass *) g_type_class_peek (G_TYPE_FROM_INSTANCE (obj)))

typedef struct _GstFrei0rFilter GstFrei0rFilter;
typedef struct _GstFrei0rFilterClass GstFrei0rFilterClass;

struct _GstFrei0rFilter {
  GstVideoFilter parent;

  gint width, height;

  f0r_instance_t *f0r_instance;
  GstFrei0rPropertyValue *property_cache;
};

struct _GstFrei0rFilterClass {
  GstVideoFilterClass parent;

  f0r_plugin_info_t *info;
  GstFrei0rFuncTable *ftable;

  GstFrei0rProperty *properties;
  gint n_properties;
};

GstFrei0rPluginRegisterReturn gst_frei0r_filter_register (GstPlugin *plugin, const gchar * vendor, const f0r_plugin_info_t *info, const GstFrei0rFuncTable *ftable);

G_END_DECLS

#endif /* __GST_FREI0R_FILTER_H__ */

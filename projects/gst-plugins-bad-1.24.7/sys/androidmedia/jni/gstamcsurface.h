/*
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 *
 * Copyright (C) 2015, Collabora Ltd.
 *   Author: Matthieu Bouron <matthieu.bouron@collabora.com>
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

#ifndef __GST_AMC_SURFACE_H__
#define __GST_AMC_SURFACE_H__

#include <glib-object.h>
#include <jni.h>
#include "jni/gstamcsurfacetexture-jni.h"

G_BEGIN_DECLS

#define GST_TYPE_AMC_SURFACE                  (gst_amc_surface_get_type ())
#define GST_AMC_SURFACE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_AMC_SURFACE, GstAmcSurface))
#define GST_IS_AMC_SURFACE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_AMC_SURFACE))
#define GST_AMC_SURFACE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_AMC_SURFACE, GstAmcSurfaceClass))
#define GST_IS_AMC_SURFACE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_AMC_SURFACE))
#define GST_AMC_SURFACE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AMC_SURFACE, GstAmcSurfaceClass))

typedef struct _GstAmcSurface        GstAmcSurface;
typedef struct _GstAmcSurfaceClass   GstAmcSurfaceClass;

struct _GstAmcSurface
{
  GObject parent_instance;

  /* instance members */
  jobject jobject;
  GstAmcSurfaceTextureJNI *texture;
};

struct _GstAmcSurfaceClass
{
  GObjectClass parent_class;

  /* class members */
  jclass jklass;
  jmethodID constructor;
  jmethodID is_valid;
  jmethodID release;
  jmethodID describe_contents;
};

GType gst_amc_surface_get_type (void);

GstAmcSurface * gst_amc_surface_new           (GstAmcSurfaceTextureJNI *texture,
                                              GError ** err);

gboolean gst_amc_surface_is_valid             (GstAmcSurface *surface,
                                              gboolean * result,
                                              GError ** err);

gboolean gst_amc_surface_release              (GstAmcSurface *surface,
                                              GError ** err);

gboolean gst_amc_surface_describe_contents    (GstAmcSurface *surface,
                                              gint * result,
                                              GError ** err);

G_END_DECLS
#endif

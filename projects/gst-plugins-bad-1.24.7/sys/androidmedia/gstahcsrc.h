/* GStreamer android.hardware.Camera Source
 *
 * Copyright (C) 2012, Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * Copyright (C) 2015, Collabora Ltd.
 *   Author: Justin Kim <justin.kim@collabora.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_AHC_SRC_H__
#define __GST_AHC_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/base/gstdataqueue.h>

#include "gst-android-hardware-camera.h"
#include "jni/gstamcsurfacetexture-jni.h"

G_BEGIN_DECLS

#define GST_TYPE_AHC_SRC \
  (gst_ahc_src_get_type())
#define GST_AHC_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AHC_SRC, GstAHCSrc))
#define GST_AHC_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AHC_SRC, GstAHCSrcClass))
#define GST_IS_AHC_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AHC_SRC))
#define GST_IS_AHC_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AHC_SRC))


typedef struct _GstAHCSrc GstAHCSrc;
typedef struct _GstAHCSrcClass GstAHCSrcClass;

/**
 * GstAHCSrc:
 *
 * Opaque data structure.
 */
struct _GstAHCSrc
{
  /*< private >*/
  GstPushSrc parent;

  GstAHCamera *camera;
  GstAmcSurfaceTextureJNI *texture;
  GList *data;
  GstDataQueue *queue;
  gint buffer_size;
  GstClockTime previous_ts;
  gint format;
  gint width;
  gint height;
  gint fps_min;
  gint fps_max;
  gboolean start;
  gboolean smooth_zoom;
  GMutex mutex;

  /* Properties */
  gint device;
};

struct _GstAHCSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_ahc_src_get_type (void);

G_END_DECLS
#endif /* __GST_AHC_SRC_H__ */

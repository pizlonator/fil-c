/* GStreamer
 * Copyright (C) 2010 Oblong Industries, Inc.
 * Copyright (C) 2010 Collabora Multimedia
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

#ifndef _GST_JP2K_DECIMATOR_H_
#define _GST_JP2K_DECIMATOR_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_JP2K_DECIMATOR   (gst_jp2k_decimator_get_type())
#define GST_JP2K_DECIMATOR(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_JP2K_DECIMATOR,GstJP2kDecimator))
#define GST_JP2K_DECIMATOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_JP2K_DECIMATOR,GstJP2kDecimatorClass))
#define GST_IS_JP2K_DECIMATOR(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_JP2K_DECIMATOR))
#define GST_IS_JP2K_DECIMATOR_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_JP2K_DECIMATOR))

typedef struct _GstJP2kDecimator GstJP2kDecimator;
typedef struct _GstJP2kDecimatorClass GstJP2kDecimatorClass;

struct _GstJP2kDecimator
{
  GstElement parent;

  GstPad *sinkpad;
  GstPad *srcpad;

  gint max_layers;
  gint max_decomposition_levels;
};

struct _GstJP2kDecimatorClass
{
  GstElementClass parent;
};

GType gst_jp2k_decimator_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (jp2kdecimator);

G_END_DECLS

#endif

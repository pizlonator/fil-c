/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef __GST_DEBUGSPY_H__
#define __GST_DEBUGSPY_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_DEBUGSPY \
  (gst_debug_spy_get_type())
#define GST_DEBUGSPY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DEBUGSPY,GstDebugSpy))
#define GST_DEBUGSPY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DEBUGSPY,GstDebugSpyClass))
#define GST_IS_DEBUGSPY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DEBUGSPY))
#define GST_IS_DEBUGSPY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DEBUGSPY))

typedef struct _GstDebugSpy      GstDebugSpy;
typedef struct _GstDebugSpyClass GstDebugSpyClass;

struct _GstDebugSpy
{
  GstBaseTransform transform;

  gboolean silent;
  GChecksumType checksum_type;
};

struct _GstDebugSpyClass
{
  GstBaseTransformClass parent_class;
};

GType gst_debug_spy_get_type (void);

G_END_DECLS

#endif /* __GST_DEBUGSPY_H__ */

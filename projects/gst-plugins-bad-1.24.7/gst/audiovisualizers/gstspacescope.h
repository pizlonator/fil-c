/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * gstspacescope.h: simple stereo visualizer
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

#ifndef __GST_SPACE_SCOPE_H__
#define __GST_SPACE_SCOPE_H__

#include "gst/pbutils/gstaudiovisualizer.h"

G_BEGIN_DECLS
#define GST_TYPE_SPACE_SCOPE            (gst_space_scope_get_type())
#define GST_SPACE_SCOPE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SPACE_SCOPE,GstSpaceScope))
#define GST_SPACE_SCOPE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SPACE_SCOPE,GstSpaceScopeClass))
#define GST_IS_SPACE_SCOPE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SPACE_SCOPE))
#define GST_IS_SPACE_SCOPE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SPACE_SCOPE))
typedef struct _GstSpaceScope GstSpaceScope;
typedef struct _GstSpaceScopeClass GstSpaceScopeClass;

typedef void (*GstSpaceScopeProcessFunc) (GstAudioVisualizer *, guint32 *, gint16 *, guint);

struct _GstSpaceScope
{
  GstAudioVisualizer parent;

  /* < private > */
  GstSpaceScopeProcessFunc process;
  gint style;

  /* filter specific data */
  gdouble f1l_l, f1l_m, f1l_h;
  gdouble f1r_l, f1r_m, f1r_h;
  gdouble f2l_l, f2l_m, f2l_h;
  gdouble f2r_l, f2r_m, f2r_h;
};

struct _GstSpaceScopeClass
{
  GstAudioVisualizerClass parent_class;
};

GType gst_space_scope_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (spacescope);

G_END_DECLS
#endif /* __GST_SPACE_SCOPE_H__ */

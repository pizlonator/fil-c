/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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

#ifndef __GST_KS_VIDEO_SRC_H__
#define __GST_KS_VIDEO_SRC_H__

#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_KS_VIDEO_SRC \
  (gst_ks_video_src_get_type ())
#define GST_KS_VIDEO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_KS_VIDEO_SRC, GstKsVideoSrc))
#define GST_KS_VIDEO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_KS_VIDEO_SRC, GstKsVideoSrcClass))
#define GST_IS_KS_VIDEO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_KS_VIDEO_SRC))
#define GST_IS_KS_VIDEO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_KS_VIDEO_SRC))

typedef struct _GstKsVideoSrc         GstKsVideoSrc;
typedef struct _GstKsVideoSrcClass    GstKsVideoSrcClass;
typedef struct _GstKsVideoSrcPrivate  GstKsVideoSrcPrivate;

struct _GstKsVideoSrc
{
  GstPushSrc push_src;

  GstKsVideoSrcPrivate * priv;
};

struct _GstKsVideoSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_ks_video_src_get_type (void);

G_END_DECLS

#endif /* __GST_KS_VIDEO_SRC_H__ */

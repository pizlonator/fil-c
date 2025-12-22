/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2006> Julien Moutte <julien@moutte.net>
 * Copyright (C) <2006> Zeeshan Ali <zeeshan.ali@nokia.com>
 * Copyright (C) <2006-2008> Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
 * Copyright (C) <2015> British Broadcasting Corporation <dash@rd.bbc.co.uk>
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

#ifndef __GST_TTML_RENDER_H__
#define __GST_TTML_RENDER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <pango/pango.h>

G_BEGIN_DECLS

#define GST_TYPE_TTML_RENDER            (gst_ttml_render_get_type())
#define GST_TTML_RENDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                         GST_TYPE_TTML_RENDER, GstTtmlRender))
#define GST_TTML_RENDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                         GST_TYPE_TTML_RENDER, \
                                         GstTtmlRenderClass))
#define GST_TTML_RENDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                         GST_TYPE_TTML_RENDER, \
                                         GstTtmlRenderClass))
#define GST_IS_TTML_RENDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                         GST_TYPE_TTML_RENDER))
#define GST_IS_TTML_RENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                         GST_TYPE_TTML_RENDER))

typedef struct _GstTtmlRender GstTtmlRender;
typedef struct _GstTtmlRenderClass GstTtmlRenderClass;
typedef struct _GstTtmlRenderRenderedImage GstTtmlRenderRenderedImage;

struct _GstTtmlRenderRenderedImage {
  GstBuffer *image;
  gint x;
  gint y;
  guint width;
  guint height;
};


struct _GstTtmlRender {
    GstElement               element;

    GstPad                  *video_sinkpad;
    GstPad                  *text_sinkpad;
    GstPad                  *srcpad;

    GstSegment               segment;
    GstSegment               text_segment;
    GstBuffer               *text_buffer;
    gboolean                text_linked;
    gboolean                video_flushing;
    gboolean                video_eos;
    gboolean                text_flushing;
    gboolean                text_eos;

    GMutex                   lock;
    GCond                    cond;  /* to signal removal of a queued text
                                     * buffer, arrival of a text buffer,
                                     * a text segment update, or a change
                                     * in status (e.g. shutdown, flushing) */

    GstVideoInfo             info;
    GstVideoFormat           format;
    gint                     width;
    gint                     height;

    gboolean                 want_background;
    gboolean                 wait_text;

    gboolean                 need_render;

    PangoLayout             *layout;
    GList * compositions;
};

struct _GstTtmlRenderClass {
    GstElementClass parent_class;

    PangoContext *pango_context;
    GMutex       *pango_lock;
};

GType gst_ttml_render_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_TTML_RENDER_H */

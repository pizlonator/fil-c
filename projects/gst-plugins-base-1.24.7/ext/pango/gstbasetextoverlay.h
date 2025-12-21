/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2006> Julien Moutte <julien@moutte.net>
 * Copyright (C) <2006> Zeeshan Ali <zeeshan.ali@nokia.com>
 * Copyright (C) <2006-2008> Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
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

#ifndef __GST_BASE_TEXT_OVERLAY_H__
#define __GST_BASE_TEXT_OVERLAY_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/video-overlay-composition.h>
#include <pango/pangocairo.h>

G_BEGIN_DECLS

#define GST_TYPE_BASE_TEXT_OVERLAY            (gst_base_text_overlay_get_type())
#define GST_BASE_TEXT_OVERLAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                         GST_TYPE_BASE_TEXT_OVERLAY, GstBaseTextOverlay))
#define GST_BASE_TEXT_OVERLAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                         GST_TYPE_BASE_TEXT_OVERLAY,GstBaseTextOverlayClass))
#define GST_BASE_TEXT_OVERLAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                         GST_TYPE_BASE_TEXT_OVERLAY, GstBaseTextOverlayClass))
#define GST_IS_BASE_TEXT_OVERLAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                         GST_TYPE_BASE_TEXT_OVERLAY))
#define GST_IS_BASE_TEXT_OVERLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                         GST_TYPE_BASE_TEXT_OVERLAY))

typedef struct _GstBaseTextOverlay      GstBaseTextOverlay;
typedef struct _GstBaseTextOverlayClass GstBaseTextOverlayClass;

/**
 * GstBaseTextOverlayVAlign:
 * @GST_BASE_TEXT_OVERLAY_VALIGN_BASELINE: draw text on the baseline
 * @GST_BASE_TEXT_OVERLAY_VALIGN_BOTTOM: draw text on the bottom
 * @GST_BASE_TEXT_OVERLAY_VALIGN_TOP: draw text on top
 * @GST_BASE_TEXT_OVERLAY_VALIGN_POS: draw text according to the #GstBaseTextOverlay:ypos property
 * @GST_BASE_TEXT_OVERLAY_VALIGN_CENTER: draw text vertically centered
 *
 * Vertical alignment of the text.
 */
typedef enum {
    GST_BASE_TEXT_OVERLAY_VALIGN_BASELINE,
    GST_BASE_TEXT_OVERLAY_VALIGN_BOTTOM,
    GST_BASE_TEXT_OVERLAY_VALIGN_TOP,
    GST_BASE_TEXT_OVERLAY_VALIGN_POS,
    GST_BASE_TEXT_OVERLAY_VALIGN_CENTER,
    GST_BASE_TEXT_OVERLAY_VALIGN_ABSOLUTE
} GstBaseTextOverlayVAlign;

/**
 * GstBaseTextOverlayHAlign:
 * @GST_BASE_TEXT_OVERLAY_HALIGN_LEFT: align text left
 * @GST_BASE_TEXT_OVERLAY_HALIGN_CENTER: align text center
 * @GST_BASE_TEXT_OVERLAY_HALIGN_RIGHT: align text right
 * @GST_BASE_TEXT_OVERLAY_HALIGN_POS: position text according to the #GstBaseTextOverlay:xpos property
 *
 * Horizontal alignment of the text.
 */
/* FIXME 0.11: remove GST_BASE_TEXT_OVERLAY_HALIGN_UNUSED */
typedef enum {
    GST_BASE_TEXT_OVERLAY_HALIGN_LEFT,
    GST_BASE_TEXT_OVERLAY_HALIGN_CENTER,
    GST_BASE_TEXT_OVERLAY_HALIGN_RIGHT,
    GST_BASE_TEXT_OVERLAY_HALIGN_UNUSED,
    GST_BASE_TEXT_OVERLAY_HALIGN_POS,
    GST_BASE_TEXT_OVERLAY_HALIGN_ABSOLUTE
} GstBaseTextOverlayHAlign;

/**
 * GstBaseTextOverlayWrapMode:
 * @GST_BASE_TEXT_OVERLAY_WRAP_MODE_NONE: no wrapping
 * @GST_BASE_TEXT_OVERLAY_WRAP_MODE_WORD: do word wrapping
 * @GST_BASE_TEXT_OVERLAY_WRAP_MODE_CHAR: do char wrapping
 * @GST_BASE_TEXT_OVERLAY_WRAP_MODE_WORD_CHAR: do word and char wrapping
 *
 * Whether to wrap the text and if so how.
 */
typedef enum {
    GST_BASE_TEXT_OVERLAY_WRAP_MODE_NONE = -1,
    GST_BASE_TEXT_OVERLAY_WRAP_MODE_WORD = PANGO_WRAP_WORD,
    GST_BASE_TEXT_OVERLAY_WRAP_MODE_CHAR = PANGO_WRAP_CHAR,
    GST_BASE_TEXT_OVERLAY_WRAP_MODE_WORD_CHAR = PANGO_WRAP_WORD_CHAR
} GstBaseTextOverlayWrapMode;

/**
 * GstBaseTextOverlayLineAlign:
 * @GST_BASE_TEXT_OVERLAY_LINE_ALIGN_LEFT: lines are left-aligned
 * @GST_BASE_TEXT_OVERLAY_LINE_ALIGN_CENTER: lines are center-aligned
 * @GST_BASE_TEXT_OVERLAY_LINE_ALIGN_RIGHT: lines are right-aligned
 *
 * Alignment of text lines relative to each other
 */
typedef enum {
    GST_BASE_TEXT_OVERLAY_LINE_ALIGN_LEFT = PANGO_ALIGN_LEFT,
    GST_BASE_TEXT_OVERLAY_LINE_ALIGN_CENTER = PANGO_ALIGN_CENTER,
    GST_BASE_TEXT_OVERLAY_LINE_ALIGN_RIGHT = PANGO_ALIGN_RIGHT
} GstBaseTextOverlayLineAlign;

/**
 * GstBaseTextOverlayScaleMode:
 * @GST_BASE_TEXT_OVERLAY_SCALE_MODE_NONE: no compensation
 * @GST_BASE_TEXT_OVERLAY_SCALE_MODE_PAR: compensate pixel-aspect-ratio scaling
 * @GST_BASE_TEXT_OVERLAY_SCALE_MODE_DISPLAY: compensate for scaling to display (as determined by overlay allocation meta)
 * @GST_BASE_TEXT_OVERLAY_SCALE_MODE_USER: compensate scaling set by #GstBaseTextOverlay:scale-pixel-aspect-ratio property
 *
 * Scale text to compensate for and avoid aspect distortion by subsequent
 * scaling of video
 */
typedef enum {
    GST_BASE_TEXT_OVERLAY_SCALE_MODE_NONE,
    GST_BASE_TEXT_OVERLAY_SCALE_MODE_PAR,
    GST_BASE_TEXT_OVERLAY_SCALE_MODE_DISPLAY,
    GST_BASE_TEXT_OVERLAY_SCALE_MODE_USER
} GstBaseTextOverlayScaleMode;

/**
 * GstBaseTextOverlay:
 *
 * Opaque textoverlay object structure
 */
struct _GstBaseTextOverlay {
    GstElement               element;

    GstPad                  *video_sinkpad;
    GstPad                  *text_sinkpad;
    GstPad                  *srcpad;

    PangoContext            *pango_context;

    GstSegment               segment;
    GstSegment               text_segment;
    GstBuffer               *text_buffer;
    GstClockTime             text_buffer_running_time;
    GstClockTime             text_buffer_running_time_end;
    gboolean                 text_linked;
    gboolean                 video_flushing;
    gboolean                 video_eos;
    gboolean                 text_flushing;
    gboolean                 text_eos;

    GMutex                   lock;
    GCond                    cond;  /* to signal removal of a queued text
                                     * buffer, arrival of a text buffer,
                                     * a text segment update, or a change
                                     * in status (e.g. shutdown, flushing) */

    /* stream metrics */
    GstVideoInfo             info;
    GstVideoFormat           format;
    gint                     width;
    gint                     height;

    /* properties */
    gint                     xpad;
    gint                     ypad;
    gint                     deltax;
    gint                     deltay;
    gdouble                  xpos;
    gdouble                  ypos;
    gchar                   *default_text;
    gboolean                 want_shading;
    gboolean                 silent;
    gboolean                 wait_text;
    guint                    color, outline_color;
    PangoLayout             *layout;
    gboolean                 auto_adjust_size;
    gboolean                 draw_shadow;
    gboolean                 draw_outline;
    gint                     shading_value;  /* for timeoverlay subclass */
    gboolean                 use_vertical_render;
    GstBaseTextOverlayVAlign     valign;
    GstBaseTextOverlayHAlign     halign;
    GstBaseTextOverlayWrapMode   wrap_mode;
    GstBaseTextOverlayLineAlign  line_align;
    GstBaseTextOverlayScaleMode  scale_mode;
    gint                     scale_par_n;
    gint                     scale_par_d;

    /* text pad format */
    gboolean                 have_pango_markup;

    /* rendering state */
    gboolean                 need_render;
    GstBuffer               *text_image;

    /* dimension relative to witch the render is done, this is the stream size
     * or a portion of the window_size (adapted to aspect ratio) */
    gint                     render_width;
    gint                     render_height;
    /* This is (render_width / width) uses to convert to stream scale */
    gdouble                  render_scale;

    /* dimension of text_image, the physical dimension */
    guint                    text_width;
    guint                    text_height;

    /* position of rendering in image coordinates */
    gint                     text_x;
    gint                     text_y;

    /* window dimension, reported in the composition meta params. This is set
     * to stream width, height if missing */
    gint                     window_width;
    gint                     window_height;

    gdouble                  shadow_offset;
    gdouble                  outline_offset;

    PangoRectangle           ink_rect;
    PangoRectangle           logical_rect;

    gboolean                    attach_compo_to_buffer;
    GstVideoOverlayComposition *composition;
    GstVideoOverlayComposition *upstream_composition;
};

struct _GstBaseTextOverlayClass {
    GstElementClass parent_class;

    gchar *     (*get_text) (GstBaseTextOverlay *overlay, GstBuffer *video_frame);
};

GType gst_base_text_overlay_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_BASE_TEXT_OVERLAY_H */

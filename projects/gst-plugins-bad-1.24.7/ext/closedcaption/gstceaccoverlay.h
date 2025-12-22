/* GStreamer
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *     @Author: Chengjun Wang <cjun.wang@samsung.com>
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


#ifndef __GST_CEA_CC_OVERLAY_H__
#define __GST_CEA_CC_OVERLAY_H__

#include <gst/gst.h>
#include <pango/pangocairo.h>
#include <gstcea708decoder.h>

G_BEGIN_DECLS
#define GST_TYPE_CEA_CC_OVERLAY \
  (gst_cea_cc_overlay_get_type())
#define GST_CEA_CC_OVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CEA_CC_OVERLAY,GstCeaCcOverlay))
#define GST_CEA_CC_OVERLAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CEA_CC_OVERLAY,GstCeaCcOverlayClass))
#define GST_CEA_CC_OVERLAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
  GST_TYPE_CEA_CC_OVERLAY, GstCeaCcOverlayClass))
#define GST_IS_CEA_CC_OVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CEA_CC_OVERLAY))
#define GST_IS_CEA_CC_OVERLAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CEA_CC_OVERLAY))

typedef struct _GstCeaCcOverlay GstCeaCcOverlay;
typedef struct _GstCeaCcOverlayClass GstCeaCcOverlayClass;

typedef enum
{
  CCTYPE_608_CC1 = 0,
  CCTYPE_608_CC2,
  CCTYPE_708_ADD,
  CCTYPE_708_START,
} DtvccType;

/**
 * GstBaseTextOverlayHAlign:
 * @GST_CEA_CC_OVERLAY_WIN_H_LEFT: closed caption window horizontal anchor left
 * @GST_CEA_CC_OVERLAY_WIN_H_CENTER: closed caption window horizontal anchor center
 * @GST_CEA_CC_OVERLAY_WIN_H_RIGHT: closed caption window horizontal anchor right
 * @GST_CEA_CC_OVERLAY_WIN_H_AUTO: closed caption window horizontal anchor auto
 *
 * Closed Caption Window Horizontal anchor position.
 */
typedef enum
{
  GST_CEA_CC_OVERLAY_WIN_H_LEFT,
  GST_CEA_CC_OVERLAY_WIN_H_CENTER,
  GST_CEA_CC_OVERLAY_WIN_H_RIGHT,
  GST_CEA_CC_OVERLAY_WIN_H_AUTO
} GstCeaCcOverlayWinHPos;

/**
 * GstCeaCcOverlay:
 *
 * Opaque ccoverlay data structure.
 */
struct _GstCeaCcOverlay
{
  GstElement parent;
  GstPad *video_sinkpad;
  GstPad *cc_sinkpad;
  GstPad *srcpad;
  /* There are two possible 608 streams encapsulated by 708 */
  gint16 cea608_index[NUM_608_CCTYPES];
  gint16 cea708_index;
  guint8 cea608_buffer[NUM_608_CCTYPES][DTVCC_LENGTH];
  guint8 cea708_buffer[DTVCC_LENGTH];

  /* TRUE if input is CDP, FALSE if cc_data triplet */
  gboolean is_cdp;
  
  GstSegment segment;
  GstSegment cc_segment;
  GstVideoOverlayComposition *current_composition;
  guint64 current_comp_start_time;
  GstVideoOverlayComposition *next_composition;
  guint64 next_comp_start_time;
  GstCeaCcOverlayWinHPos default_window_h_pos;
  gboolean cc_pad_linked;
  gboolean video_flushing;
  gboolean video_eos;
  gboolean cc_flushing;
  gboolean cc_eos;

  GMutex lock;
  GCond cond;                   /* to signal removal of a queued text
                                 * buffer, arrival of a text buffer,
                                 * a text segment update, or a change
                                 * in status (e.g. shutdown, flushing) */

  GstVideoInfo info;
  GstVideoFormat format;
  gint width;
  gint height;
  gboolean silent;
  Cea708Dec *decoder;
  gint image_width;
  gint image_height;

  gboolean need_update;

  gboolean attach_compo_to_buffer;
};

/* FIXME : Pango context and MT-safe since 1.32.6 */
struct _GstCeaCcOverlayClass
{
  GstElementClass parent_class;

  PangoContext *pango_context;
};

GType gst_cea_cc_overlay_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (cc708overlay);

G_END_DECLS
#endif /* __GST_CEA_CC_OVERLAY_H__ */

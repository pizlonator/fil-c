/* GStreamer DVD Sub-Picture Unit
 * Copyright (C) 2007 Fluendo S.A. <info@fluendo.com>
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
#ifndef __GST_DVD_SPU_H__
#define __GST_DVD_SPU_H__

#include <gst/gst.h>

#include "gstspu-common.h"
#include "gstspu-vobsub.h"
#include "gstspu-pgs.h"

G_BEGIN_DECLS

#define GST_TYPE_DVD_SPU \
  (gst_dvd_spu_get_type())
#define GST_DVD_SPU(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DVD_SPU,GstDVDSpu))
#define GST_DVD_SPU_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DVD_SPU,GstDVDSpuClass))
#define GST_IS_DVD_SPU(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DVD_SPU))
#define GST_IS_DVD_SPU_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DVD_SPU))

#define DVD_SPU_LOCK(s) g_mutex_lock (&(s)->spu_lock);
#define DVD_SPU_UNLOCK(s) g_mutex_unlock (&(s)->spu_lock);

typedef struct _GstDVDSpuClass GstDVDSpuClass;

typedef enum SpuStateFlags SpuStateFlags;
typedef enum SpuInputType SpuInputType;
typedef struct SpuPacket SpuPacket;

enum SpuInputType {
  SPU_INPUT_TYPE_NONE   = 0x00,
  SPU_INPUT_TYPE_VOBSUB = 0x01,
  SPU_INPUT_TYPE_PGS    = 0x02
};

enum SpuStateFlags {
  SPU_STATE_NONE        = 0x00,
  /* Flags cleared on a flush */
  SPU_STATE_DISPLAY     = 0x01,
  SPU_STATE_FORCED_DSP  = 0x02,
  SPU_STATE_STILL_FRAME = 0x04,
  /* Persistent flags */
  SPU_STATE_FORCED_ONLY = 0x100
};

#define SPU_STATE_FLAGS_MASK (0xff)

struct SpuState {
  GstClockTime next_ts; /* Next event TS in running time */
  SpuStateFlags flags;

  GstVideoInfo info;

  SpuVobsubState vobsub;
  SpuPgsState pgs;
};

/* Structure used to store the queue of pending SPU packets. The start_ts is
 * stored in running time... 
 * Also used to carry in-band events so they remain serialised properly */
struct SpuPacket {
  GstClockTime event_ts;
  GstBuffer *buf;
  GstEvent *event;
};

struct _GstDVDSpu {
  GstElement element;

  GstPad *videosinkpad;
  GstPad *subpic_sinkpad;
  GstPad *srcpad;

  /* Mutex to protect state we access from different chain funcs */
  GMutex spu_lock;

  gboolean video_flushing;
  GstSegment video_seg;
  GstSegment subp_seg;

  SpuState spu_state;
  SpuInputType spu_input_type;

  /* GQueue of SpuBuf structures */
  GQueue *pending_spus;

  /* Accumulator for collecting partial SPU buffers until they're complete */
  GstBuffer *partial_spu;

  /* Store either a reference or a copy of the last video frame for duplication
   * during still-frame conditions */
  GstBuffer *ref_frame;

  /* Buffer to push after handling a DVD event, if any */
  GstBuffer *pending_frame;

  /* Overlay composition */
  gboolean attach_compo_to_buffer;
  GstVideoOverlayComposition *composition;
};

struct _GstDVDSpuClass {
  GstElementClass parent_class;
};

GType gst_dvd_spu_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (dvdspu);

typedef enum {
  GST_DVD_SPU_DEBUG_RENDER_RECTANGLE = (1 << 0),
  GST_DVD_SPU_DEBUG_HIGHLIGHT_RECTANGLE = (1 << 1)
} GstDVDSPUDebugFlags;

extern GstDVDSPUDebugFlags dvdspu_debug_flags;


G_END_DECLS

#endif /* __GST_DVD_SPU_H__ */

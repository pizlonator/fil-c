/* GStreamer Sub-Picture Unit - VobSub/DVD handling
 * Copyright (C) 2009 Jan Schmidt <thaytan@noraisin.net>
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

#ifndef __GSTSPU_VOBSUB_H__
#define __GSTSPU_VOBSUB_H__

#include "gstspu-common.h"

typedef struct SpuVobsubState SpuVobsubState;
typedef struct SpuVobsubPixCtrlI SpuVobsubPixCtrlI;
typedef struct SpuVobsubLineCtrlI SpuVobsubLineCtrlI;

/* Pixel Control Info from a Change Color Contrast command */
struct SpuVobsubPixCtrlI {
  gint16 left;
  guint32 palette;

  /* Pre-multiplied palette values, updated as
   * needed */
  SpuColour pal_cache[4];
};

struct SpuVobsubLineCtrlI {
  guint8 n_changes; /* 1 to 8 */
  SpuVobsubPixCtrlI pix_ctrl_i[8];

  gint16 top;
  gint16 bottom;
};

struct SpuVobsubState {
  GstClockTime base_ts; /* base TS for cmd blk delays in running time */
  GstBuffer *buf; /* Current SPU packet we're executing commands from */
  guint16 cur_cmd_blk; /* Offset into the buf for the current cmd block */

  /* Top + Bottom field offsets in the buffer. 0 = not set */
  guint16 pix_data[2]; 
  GstBuffer *pix_buf; /* Current SPU packet the pix_data references */
  GstMapInfo pix_buf_map; /* Mapped buffer info */
  
  SpuRect disp_rect;
  SpuRect hl_rect;
  gint frame_w, frame_h;

  guint32 current_clut[16]; /* Colour lookup table from incoming events */

  guint8 main_idx[4]; /* Indices for current main palette */
  guint8 main_alpha[4]; /* Alpha values for main palette */

  guint8 hl_idx[4]; /* Indices for current highlight palette */
  guint8 hl_alpha[4]; /* Alpha values for highlight palette */

  /* Pre-multiplied colour palette for the main palette */
  SpuColour main_pal[4];
  gboolean main_pal_dirty;

  /* Line control info for rendering the highlight palette */
  SpuVobsubLineCtrlI hl_ctrl_i;
  gboolean hl_pal_dirty; /* Indicates that the HL palette info needs refreshing */

  /* LineCtrlI Info from a Change Color & Contrast command */
  SpuVobsubLineCtrlI *line_ctrl_i;
  gint16 n_line_ctrl_i;
  gboolean line_ctrl_i_pal_dirty; /* Indicates that the palettes for the line_ctrl_i
                                   * need recalculating */

  /* Rendering state vars below */

  /* Current Y Position */
  gint16 cur_Y;

  /* Current offset in nibbles into the pix_data */
  guint16 max_offset;

  /* current ChgColCon Line Info */
  SpuVobsubLineCtrlI *cur_chg_col;
  SpuVobsubLineCtrlI *cur_chg_col_end;
};

void gstspu_vobsub_handle_new_buf (GstDVDSpu * dvdspu, GstClockTime event_ts, GstBuffer *buf);
gboolean gstspu_vobsub_execute_event (GstDVDSpu *dvdspu);
void gstspu_vobsub_render (GstDVDSpu *dvdspu, GstVideoFrame *frame);
gboolean gstspu_vobsub_handle_dvd_event (GstDVDSpu *dvdspu, GstEvent *event);
void gstspu_vobsub_get_render_geometry (GstDVDSpu *dvdspu,
    gint *display_width, gint *display_height,
    GstVideoRectangle *window_rect);
void gstspu_vobsub_flush (GstDVDSpu *dvdspu);

#endif

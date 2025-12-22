/* GStreamer Sub-Picture Unit - PGS handling
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

#ifndef __GSTSPU_PGS_H__
#define __GSTSPU_PGS_H__

#include "gstspu-common.h"

typedef struct SpuPgsState SpuPgsState;
typedef enum PgsCompositionObjectFlags PgsCompositionObjectFlags;
typedef enum PgsPresentationSegmentFlags PgsPresentationSegmentFlags;
typedef enum PgsObjectUpdateFlags PgsObjectUpdateFlags;

typedef struct PgsPresentationSegment PgsPresentationSegment;
typedef struct PgsCompositionObject PgsCompositionObject;

enum PgsPresentationSegmentFlags
{
  PGS_PRES_SEGMENT_FLAG_UPDATE_PALETTE = 0x80
};

enum PgsCompositionObjectFlags
{
  PGS_COMPOSITION_OBJECT_FLAG_CROPPED = 0x80,
  PGS_COMPOSITION_OBJECT_FLAG_FORCED = 0x40
};

enum PgsObjectUpdateFlags
{
  /* Set in an object_update if this is the beginning of new RLE data.
   * If not set, the data is a continuation to be appended */
  PGS_OBJECT_UPDATE_FLAG_START_RLE = 0x80,
  PGS_OBJECT_UPDATE_FLAG_END_RLE = 0x40 /* This one is a guess */
};

struct PgsPresentationSegment
{
  guint16 composition_no;
  guint8 composition_state;

  PgsPresentationSegmentFlags flags;

  guint8 palette_id;

  guint16 vid_w, vid_h;
  guint8  vid_fps_code;

  GArray *objects;
};

struct PgsCompositionObject
{
  guint16 id;
  guint8 version;
  PgsCompositionObjectFlags flags;

  guint8 win_id;

  guint8 rle_data_ver;
  guint8 *rle_data;
  guint32 rle_data_size;
  guint32 rle_data_used;

  /* Top left corner of this object */
  guint16 x, y;

  /* Only valid if PGS_COMPOSITION_OBJECT_FLAG_CROPPED is set */
  guint16 crop_x, crop_y, crop_w, crop_h;
};

struct SpuPgsState {
  GstBuffer *pending_cmd;

  gboolean in_presentation_segment;
  gboolean have_presentation_segment;

  PgsPresentationSegment pres_seg;

  SpuColour palette[256];

  guint16 win_x, win_y, win_w, win_h;
};

void gstspu_pgs_handle_new_buf (GstDVDSpu * dvdspu, GstClockTime event_ts, GstBuffer *buf);
gboolean gstspu_pgs_execute_event (GstDVDSpu *dvdspu);
void gstspu_pgs_render (GstDVDSpu *dvdspu, GstVideoFrame *window);
gboolean gstspu_pgs_handle_dvd_event (GstDVDSpu *dvdspu, GstEvent *event);
void gstspu_pgs_get_render_geometry (GstDVDSpu *dvdspu,
    gint *display_width, gint *display_height,
    GstVideoRectangle *window_rect);
void gstspu_pgs_flush (GstDVDSpu *dvdspu);

#endif

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libdvbsub - DVB subtitle decoding
 * Copyright (C) Mart Raudsepp 2009 <mart.raudsepp@artecdesign.ee>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _DVB_SUB_H_
#define _DVB_SUB_H_

#include <glib.h>

G_BEGIN_DECLS

typedef struct _DvbSub DvbSub;

/**
 * DVBSubtitlePicture:
 * @data: the data in the form of palette indices, each byte represents one pixel
 *   as an index into the @palette.
 * @palette: the palette used for this subtitle rectangle, up to 256 items depending
 *   on the depth of the subpicture; each palette item is in ARGB form, 8-bits per channel.
 * @palette_bits_count: the amount of bits used in indices into @palette in @data.
 * @rowstride: the number of bytes between the start of a row and the start of the next row.
 *
 * A structure representing the contents of a subtitle rectangle.
 *
 * FIXME: Expose the depth of the palette, and perhaps also the height in this struct.
 */
typedef struct DVBSubtitlePicture {
	guint8 *data;
	guint32 *palette;
	guint8 palette_bits_count;
	int rowstride;
} DVBSubtitlePicture;

/**
 * DVBSubtitleRect:
 * @x: x coordinate of top left corner
 * @y: y coordinate of top left corner
 * @w: the width of this subpicture rectangle
 * @h: the height of this subpicture rectangle
 * @pict: the content of this subpicture rectangle
 *
 * A structure representing one subtitle objects position, dimension and content.
 */
typedef struct DVBSubtitleRect {
	int x;
	int y;
	int w;
	int h;

	DVBSubtitlePicture pict;
} DVBSubtitleRect;

/**
 * DVBSubtitleWindow
 * @version: version
 * @display_window_flag: window_* are valid
 * @display_width: assumed width of display
 * @display_height: assumed height of display
 * @window_x: x coordinate of top left corner of the subtitle window
 * @window_y: y coordinate of top left corner of the subtitle window
 * @window_width: width of the subtitle window
 * @window_height: height of the subtitle window
 *
 * A structure presenting display and window information
 * display definition segment from ETSI EN 300 743 V1.3.1
 */
typedef struct DVBSubtitleWindow {
    gint version;
    gint window_flag;

    gint display_width;
    gint display_height;

    gint window_x;
    gint window_y;
    gint window_width;
    gint window_height;
} DVBSubtitleWindow;

/**
 * DVBSubtitles:
 * @num_rects: the number of #DVBSubtitleRect in @rects
 * @rects: dynamic array of #DVBSubtitleRect
 *
 * A structure representing a set of subtitle objects.
 */
typedef struct DVBSubtitles {
	guint64 pts;
	guint8 page_time_out;
	guint num_rects;
	DVBSubtitleRect *rects;
	DVBSubtitleWindow display_def;
} DVBSubtitles;

/**
 * DvbSubCallbacks:
 * @new_data: called when new subpicture data is available for display. @dvb_sub
 *    is the #DvbSub instance this callback originates from; @subs is the set of
 *    subtitle objects that should be display for no more than @page_time_out
 *    seconds at @pts; @user_data is the same user_data as was passed through
 *    dvb_sub_set_callbacks(); The callback handler is responsible for eventually
 *    cleaning up the subpicture data @subs with a call to dvb_subtitles_free()
 *
 * A set of callbacks that can be installed on the #DvbSub with
 * dvb_sub_set_callbacks().
 */
typedef struct {
	void     (*new_data) (DvbSub *dvb_sub, DVBSubtitles * subs, gpointer user_data);
	/*< private >*/
	gpointer _dvb_sub_reserved[3];
} DvbSubCallbacks;

DvbSub  *dvb_sub_new           (void);
void     dvb_sub_free          (DvbSub * sub);

gint     dvb_sub_feed_with_pts (DvbSub *dvb_sub, guint64 pts, guint8 *data, gint len);
void     dvb_sub_set_callbacks (DvbSub *dvb_sub, DvbSubCallbacks *callbacks, gpointer user_data);
void     dvb_subtitles_free    (DVBSubtitles *sub);

G_END_DECLS

#endif /* _DVB_SUB_H_ */

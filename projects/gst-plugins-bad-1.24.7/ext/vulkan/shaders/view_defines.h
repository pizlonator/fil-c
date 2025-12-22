/* GStreamer
 * Copyright (C) 2020 Matthew Waters <matthew@centricular.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* values match GstVideoMultiviewMode */
#define VIEW_MONO_DOWNMIX 0
#define VIEW_MONO_LEFT 1
#define VIEW_MONO_RIGHT 2
#define VIEW_SIDE_BY_SIDE 3
/*  GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX,*/
#define VIEW_COLUMN_INTERLEAVED 5
#define VIEW_ROW_INTERLEAVED 6
#define VIEW_TOP_BOTTOM 7
#define VIEW_CHECKERBOARD 8

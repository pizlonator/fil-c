/* GStreamer
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (C) 2005-2009 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * gsttypefindfunctionsdata.h: collection of various typefind functions
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

#ifndef __GST_TYPE_FIND_FUNCTIONS_DATA_H__
#define __GST_TYPE_FIND_FUNCTIONS_DATA_H__

#include <gst/gst.h>


/*** generic typefind for streams that have some data at a specific position***/
typedef struct
{
  const guint8 *data;
  guint size;
  guint probability;
  GstCaps *caps;
}
GstTypeFindData;

void sw_data_destroy (GstTypeFindData * sw_data);

#endif //__GST_TYPE_FIND_FUNCTIONS_DATA_H__

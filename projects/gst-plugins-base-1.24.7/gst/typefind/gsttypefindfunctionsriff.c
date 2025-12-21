/* GStreamer
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (C) 2005-2009 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * gsttypefindfunctionsriff.c: collection of various typefind functions
 * based on riff format.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gsttypefindfunctionsplugin.h"
#include "gsttypefindfunctionsdata.h"

/*** same for riff types ***/
static void
riff_type_find (GstTypeFind * tf, gpointer private)
{
  GstTypeFindData *riff_data = (GstTypeFindData *) private;
  const guint8 *data = gst_type_find_peek (tf, 0, 12);

  if (data && (memcmp (data, "RIFF", 4) == 0 || memcmp (data, "AVF0", 4) == 0)) {
    data += 8;
    if (memcmp (data, riff_data->data, 4) == 0)
      gst_type_find_suggest (tf, riff_data->probability, riff_data->caps);
  }
}

#define TYPE_FIND_REGISTER_RIFF_DEFINE(typefind_name, name, rank, ext, _data) \
G_BEGIN_DECLS \
static gboolean \
G_PASTE(_private_type_find_riff_, typefind_name) (GstPlugin * plugin) \
{ \
  GstTypeFindData *sw_data = g_new (GstTypeFindData, 1);             \
  sw_data->data = (gpointer)_data;                                      \
  sw_data->size = 4;                                                    \
  sw_data->probability = GST_TYPE_FIND_MAXIMUM;                         \
  sw_data->caps = gst_caps_new_empty_simple (name);                     \
  if (!gst_type_find_register (plugin, name, rank, riff_type_find,      \
                      ext, sw_data->caps, sw_data,                      \
                      (GDestroyNotify) (sw_data_destroy))) {            \
    sw_data_destroy (sw_data);                                          \
    return FALSE;                                                       \
  }                                                                     \
  return TRUE;                                                          \
} \
GST_TYPE_FIND_REGISTER_DEFINE_CUSTOM (typefind_name, G_PASTE(_private_type_find_riff_, typefind_name)); \
G_END_DECLS

/*RIFF type find definition */
TYPE_FIND_REGISTER_RIFF_DEFINE (avi, "video/x-msvideo", GST_RANK_PRIMARY,
    "avi", "AVI ");
TYPE_FIND_REGISTER_RIFF_DEFINE (qcp, "audio/qcelp", GST_RANK_PRIMARY,
    "qcp", "QLCM");
TYPE_FIND_REGISTER_RIFF_DEFINE (cdxa, "video/x-cdxa", GST_RANK_PRIMARY,
    "dat", "CDXA");
TYPE_FIND_REGISTER_RIFF_DEFINE (riff_mid, "audio/riff-midi",
    GST_RANK_PRIMARY, "mid,midi", "RMID");
TYPE_FIND_REGISTER_RIFF_DEFINE (wav, "audio/x-wav", GST_RANK_PRIMARY, "wav",
    "WAVE");
TYPE_FIND_REGISTER_RIFF_DEFINE (webp, "image/webp", GST_RANK_PRIMARY,
    "webp", "WEBP");
TYPE_FIND_REGISTER_RIFF_DEFINE (fourxm, "video/x-4xm", GST_RANK_PRIMARY,
    NULL, "4XMV");
TYPE_FIND_REGISTER_RIFF_DEFINE (xwma, "audio/x-xwma", GST_RANK_PRIMARY,
    NULL, "XWMA");

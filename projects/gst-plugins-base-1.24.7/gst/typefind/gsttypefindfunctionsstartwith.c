/* GStreamer
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (C) 2005-2009 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * gsttypefindfunctionsstartwith.c: collection of various typefind functions
 * using the start with pattern
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

static void
start_with_type_find (GstTypeFind * tf, gpointer private)
{
  GstTypeFindData *start_with = (GstTypeFindData *) private;
  const guint8 *data;

  GST_LOG ("trying to find mime type %s with the first %u bytes of data",
      gst_structure_get_name (gst_caps_get_structure (start_with->caps, 0)),
      start_with->size);
  data = gst_type_find_peek (tf, 0, start_with->size);
  if (data && memcmp (data, start_with->data, start_with->size) == 0) {
    gst_type_find_suggest (tf, start_with->probability, start_with->caps);
  }
}

#define TYPE_FIND_REGISTER_START_WITH_DEFINE(typefind_name, name, rank, ext, _data, _size, _probability)\
G_BEGIN_DECLS \
static gboolean \
G_PASTE(_private_type_find_start_with_, typefind_name) (GstPlugin * plugin) \
{ \
  GstTypeFindData *sw_data = g_new (GstTypeFindData, 1);             \
  sw_data->data = (const guint8 *)_data;                                \
  sw_data->size = _size;                                                \
  sw_data->probability = _probability;                                  \
  sw_data->caps = gst_caps_new_empty_simple (name);                     \
  if (!gst_type_find_register (plugin, name, rank, start_with_type_find,\
                     ext, sw_data->caps, sw_data,                       \
                     (GDestroyNotify) (sw_data_destroy))) {             \
    sw_data_destroy (sw_data);                                          \
    return FALSE; \
  } \
  return TRUE; \
}\
GST_TYPE_FIND_REGISTER_DEFINE_CUSTOM (typefind_name, G_PASTE(_private_type_find_start_with_, typefind_name)); \
G_END_DECLS

/*'Start with' type find definition */
TYPE_FIND_REGISTER_START_WITH_DEFINE (asf, "video/x-ms-asf",
    GST_RANK_SECONDARY, "asf,wm,wma,wmv",
    "\060\046\262\165\216\146\317\021\246\331\000\252\000\142\316\154", 16,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (vcd, "video/x-vcd", GST_RANK_PRIMARY,
    "dat", "\000\377\377\377\377\377\377\377\377\377\377\000", 12,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (imelody, "audio/x-imelody",
    GST_RANK_PRIMARY, "imy,ime,imelody", "BEGIN:IMELODY", 13,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (scc, "application/x-scc",
    GST_RANK_PRIMARY, "scc", "Scenarist_SCC V1.0", 18, GST_TYPE_FIND_MAXIMUM);
#if 0
TYPE_FIND_REGISTER_START_WITH_DEFINE (smoke, "video/x-smoke",
    GST_RANK_PRIMARY, NULL, "\x80smoke\x00\x01\x00", 6, GST_TYPE_FIND_MAXIMUM);
#endif
TYPE_FIND_REGISTER_START_WITH_DEFINE (rmf, "application/vnd.rn-realmedia",
    GST_RANK_SECONDARY, "ra,ram,rm,rmvb", ".RMF", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (ram, "application/x-pn-realaudio",
    GST_RANK_SECONDARY, "ra,ram,rm,rmvb", ".ra\375", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (flv, "video/x-flv",
    GST_RANK_SECONDARY, "flv", "FLV", 3, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (nist, "audio/x-nist",
    GST_RANK_SECONDARY, "nist", "NIST", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (voc, "audio/x-voc",
    GST_RANK_SECONDARY, "voc", "Creative", 8, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (w64, "audio/x-w64",
    GST_RANK_SECONDARY, "w64", "riff", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (rf64, "audio/x-rf64",
    GST_RANK_PRIMARY, "rf64", "RF64", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (gif, "image/gif", GST_RANK_PRIMARY,
    "gif", "GIF8", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (png, "image/png",
    GST_RANK_PRIMARY + 14, "png", "\211PNG\015\012\032\012", 8,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (mve, "video/x-mve",
    GST_RANK_SECONDARY, "mve",
    "Interplay MVE File\032\000\032\000\000\001\063\021", 26,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (amr, "audio/x-amr-nb-sh",
    GST_RANK_PRIMARY, "amr", "#!AMR", 5, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (amr_wb, "audio/x-amr-wb-sh",
    GST_RANK_PRIMARY, "amr", "#!AMR-WB", 7, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (sid, "audio/x-sid", GST_RANK_MARGINAL,
    "sid", "PSID", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (xcf, "image/x-xcf",
    GST_RANK_SECONDARY, "xcf", "gimp xcf", 8, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (mng, "video/x-mng",
    GST_RANK_SECONDARY, "mng", "\212MNG\015\012\032\012", 8,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (jng, "image/x-jng",
    GST_RANK_SECONDARY, "jng", "\213JNG\015\012\032\012", 8,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (xpm, "image/x-xpixmap",
    GST_RANK_SECONDARY, "xpm", "/* XPM */", 9, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (ras, "image/x-sun-raster",
    GST_RANK_SECONDARY, "ras", "\131\246\152\225", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (bz2, "application/x-bzip",
    GST_RANK_SECONDARY, "bz2", "BZh", 3, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (gz, "application/x-gzip",
    GST_RANK_SECONDARY, "gz", "\037\213", 2, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (zip, "application/zip",
    GST_RANK_SECONDARY, "zip", "PK\003\004", 4, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (z, "application/x-compress",
    GST_RANK_SECONDARY, "Z", "\037\235", 2, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (elf, "application/x-executable",
    GST_RANK_MARGINAL, NULL, "\177ELF", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (spc, "audio/x-spc",
    GST_RANK_SECONDARY, "spc", "SNES-SPC700 Sound File Data", 27,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (caf, "audio/x-caf",
    GST_RANK_SECONDARY, "caf", "caff\000\001", 6, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (rar, "application/x-rar",
    GST_RANK_SECONDARY, "rar", "Rar!", 4, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (nsf, "audio/x-nsf",
    GST_RANK_SECONDARY, "nsf", "NESM\x1a", 5, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (gym, "audio/x-gym",
    GST_RANK_SECONDARY, "gym", "GYMX", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (ay, "audio/x-ay", GST_RANK_SECONDARY,
    "ay", "ZXAYEMUL", 8, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (gbs, "audio/x-gbs",
    GST_RANK_SECONDARY, "gbs", "GBS\x01", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (vgm, "audio/x-vgm",
    GST_RANK_SECONDARY, "vgm", "Vgm\x20", 4, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (sap, "audio/x-sap",
    GST_RANK_SECONDARY, "sap", "SAP\x0d\x0a" "AUTHOR\x20", 12,
    GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (ivf, "video/x-ivf",
    GST_RANK_SECONDARY, "ivf", "DKIF", 4, GST_TYPE_FIND_NEARLY_CERTAIN);
TYPE_FIND_REGISTER_START_WITH_DEFINE (kss, "audio/x-kss",
    GST_RANK_SECONDARY, "kss", "KSSX\0", 5, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (pdf, "application/pdf",
    GST_RANK_SECONDARY, "pdf", "%PDF-", 5, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (doc, "application/msword",
    GST_RANK_SECONDARY, "doc", "\320\317\021\340\241\261\032\341", 8,
    GST_TYPE_FIND_LIKELY);
/* Mac OS X .DS_Store files tend to be taken for video/mpeg */
TYPE_FIND_REGISTER_START_WITH_DEFINE (ds_store, "application/octet-stream",
    GST_RANK_SECONDARY, "DS_Store", "\000\000\000\001Bud1", 8,
    GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (psd, "image/vnd.adobe.photoshop",
    GST_RANK_SECONDARY, "psd", "8BPS\000\001\000\000\000\000", 10,
    GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (xi, "audio/x-xi", GST_RANK_SECONDARY,
    "xi", "Extended Instrument: ", 21, GST_TYPE_FIND_MAXIMUM);
TYPE_FIND_REGISTER_START_WITH_DEFINE (dmp, "audio/x-tap-dmp",
    GST_RANK_SECONDARY, "dmp", "DC2N-TAP-RAW", 12, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (avs, "video/x-avs",
    GST_RANK_SECONDARY, NULL, "wW\x10\x00", 4, GST_TYPE_FIND_LIKELY);
TYPE_FIND_REGISTER_START_WITH_DEFINE (yuv4mpeg, "application/x-yuv4mpeg",
    GST_RANK_MARGINAL, NULL, "YUV4MPEG2", 9, GST_TYPE_FIND_MAXIMUM);

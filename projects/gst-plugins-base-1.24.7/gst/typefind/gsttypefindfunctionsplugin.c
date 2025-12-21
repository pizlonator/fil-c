/* GStreamer
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (C) 2005-2009 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * gsttypefindfunctions.c: collection of various typefind functions
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

#include "gsttypefindfunctionsplugin.h"

#include <gst/gst.h>

GST_DEBUG_CATEGORY (type_find_functions_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  /* can't initialize this via a struct as caps can't be statically initialized */
  GST_DEBUG_CATEGORY_INIT (type_find_functions_debug, "typefindfunctions",
      GST_DEBUG_FG_GREEN | GST_DEBUG_BG_RED, "generic type find functions");
  /* note: asx/wax/wmx are XML files, asf doesn't handle them */
  /* must use strings, macros don't accept initializers */

  /*Riff Type find register */
  GST_TYPE_FIND_REGISTER (fourxm, plugin);
  GST_TYPE_FIND_REGISTER (avi, plugin);
  GST_TYPE_FIND_REGISTER (qcp, plugin);
  GST_TYPE_FIND_REGISTER (cdxa, plugin);
  GST_TYPE_FIND_REGISTER (riff_mid, plugin);
  GST_TYPE_FIND_REGISTER (wav, plugin);
  GST_TYPE_FIND_REGISTER (webp, plugin);
  GST_TYPE_FIND_REGISTER (xwma, plugin);

  /*'Start with' Type find register */
  GST_TYPE_FIND_REGISTER (asf, plugin);
  GST_TYPE_FIND_REGISTER (avs, plugin);
  GST_TYPE_FIND_REGISTER (vcd, plugin);
  GST_TYPE_FIND_REGISTER (imelody, plugin);
  GST_TYPE_FIND_REGISTER (scc, plugin);
#if 0
  GST_TYPE_FIND_REGISTER (smoke, plugin);
#endif
  GST_TYPE_FIND_REGISTER (rmf, plugin);
  GST_TYPE_FIND_REGISTER (ram, plugin);
  GST_TYPE_FIND_REGISTER (flv, plugin);
  GST_TYPE_FIND_REGISTER (nist, plugin);
  GST_TYPE_FIND_REGISTER (voc, plugin);
  GST_TYPE_FIND_REGISTER (w64, plugin);
  GST_TYPE_FIND_REGISTER (rf64, plugin);
  GST_TYPE_FIND_REGISTER (gif, plugin);
  GST_TYPE_FIND_REGISTER (png, plugin);
  GST_TYPE_FIND_REGISTER (mve, plugin);
  GST_TYPE_FIND_REGISTER (amr, plugin);
  GST_TYPE_FIND_REGISTER (amr_wb, plugin);
  GST_TYPE_FIND_REGISTER (sid, plugin);
  GST_TYPE_FIND_REGISTER (xcf, plugin);
  GST_TYPE_FIND_REGISTER (mng, plugin);
  GST_TYPE_FIND_REGISTER (jng, plugin);
  GST_TYPE_FIND_REGISTER (xpm, plugin);
  GST_TYPE_FIND_REGISTER (ras, plugin);
  GST_TYPE_FIND_REGISTER (bz2, plugin);
  GST_TYPE_FIND_REGISTER (gz, plugin);
  GST_TYPE_FIND_REGISTER (zip, plugin);
  GST_TYPE_FIND_REGISTER (z, plugin);
  GST_TYPE_FIND_REGISTER (elf, plugin);
  GST_TYPE_FIND_REGISTER (spc, plugin);
  GST_TYPE_FIND_REGISTER (caf, plugin);
  GST_TYPE_FIND_REGISTER (rar, plugin);
  GST_TYPE_FIND_REGISTER (nsf, plugin);
  GST_TYPE_FIND_REGISTER (gym, plugin);
  GST_TYPE_FIND_REGISTER (ay, plugin);
  GST_TYPE_FIND_REGISTER (gbs, plugin);
  GST_TYPE_FIND_REGISTER (vgm, plugin);
  GST_TYPE_FIND_REGISTER (sap, plugin);
  GST_TYPE_FIND_REGISTER (ivf, plugin);
  GST_TYPE_FIND_REGISTER (kss, plugin);
  GST_TYPE_FIND_REGISTER (pdf, plugin);
  GST_TYPE_FIND_REGISTER (doc, plugin);
  /* Mac OS X .DS_Store files tend to be taken for video/mpeg */
  GST_TYPE_FIND_REGISTER (ds_store, plugin);
  GST_TYPE_FIND_REGISTER (psd, plugin);
  GST_TYPE_FIND_REGISTER (xi, plugin);
  GST_TYPE_FIND_REGISTER (dmp, plugin);

  /* functions Type find register */
  GST_TYPE_FIND_REGISTER (musepack, plugin);
  GST_TYPE_FIND_REGISTER (au, plugin);
  GST_TYPE_FIND_REGISTER (mcc, plugin);
  GST_TYPE_FIND_REGISTER (mid, plugin);
  GST_TYPE_FIND_REGISTER (mxmf, plugin);
  GST_TYPE_FIND_REGISTER (flx, plugin);
  GST_TYPE_FIND_REGISTER (id3v2, plugin);
  GST_TYPE_FIND_REGISTER (id3v1, plugin);
  GST_TYPE_FIND_REGISTER (apetag, plugin);
  GST_TYPE_FIND_REGISTER (tta, plugin);
  GST_TYPE_FIND_REGISTER (mod, plugin);
  GST_TYPE_FIND_REGISTER (mp3, plugin);
  GST_TYPE_FIND_REGISTER (ac3, plugin);
  GST_TYPE_FIND_REGISTER (dts, plugin);
  GST_TYPE_FIND_REGISTER (gsm, plugin);
  GST_TYPE_FIND_REGISTER (mpeg_sys, plugin);
  GST_TYPE_FIND_REGISTER (mpeg_ts, plugin);
  GST_TYPE_FIND_REGISTER (ogganx, plugin);
  GST_TYPE_FIND_REGISTER (mpeg_video_stream, plugin);
  GST_TYPE_FIND_REGISTER (mpeg4_video, plugin);
  GST_TYPE_FIND_REGISTER (h263_video, plugin);
  GST_TYPE_FIND_REGISTER (h264_video, plugin);
  GST_TYPE_FIND_REGISTER (h265_video, plugin);
  GST_TYPE_FIND_REGISTER (nuv, plugin);
  /* ISO formats */
  GST_TYPE_FIND_REGISTER (m4a, plugin);
  GST_TYPE_FIND_REGISTER (q3gp, plugin);
  GST_TYPE_FIND_REGISTER (qt, plugin);
  GST_TYPE_FIND_REGISTER (qtif, plugin);
  GST_TYPE_FIND_REGISTER (jp2, plugin);
  GST_TYPE_FIND_REGISTER (jpc, plugin);
  GST_TYPE_FIND_REGISTER (mj2, plugin);
  GST_TYPE_FIND_REGISTER (html, plugin);
  GST_TYPE_FIND_REGISTER (swf, plugin);
  GST_TYPE_FIND_REGISTER (xges, plugin);
  GST_TYPE_FIND_REGISTER (xmeml, plugin);
  GST_TYPE_FIND_REGISTER (fcpxml, plugin);
  GST_TYPE_FIND_REGISTER (otio, plugin);
  GST_TYPE_FIND_REGISTER (dash_mpd, plugin);
  GST_TYPE_FIND_REGISTER (mss_manifest, plugin);
  GST_TYPE_FIND_REGISTER (utf8, plugin);
  GST_TYPE_FIND_REGISTER (utf16, plugin);
  GST_TYPE_FIND_REGISTER (utf32, plugin);
  GST_TYPE_FIND_REGISTER (uri, plugin);
  GST_TYPE_FIND_REGISTER (itc, plugin);
  GST_TYPE_FIND_REGISTER (hls, plugin);
  GST_TYPE_FIND_REGISTER (sdp, plugin);
  GST_TYPE_FIND_REGISTER (smil, plugin);
  GST_TYPE_FIND_REGISTER (ttml_xml, plugin);
  GST_TYPE_FIND_REGISTER (xml, plugin);
  GST_TYPE_FIND_REGISTER (aiff, plugin);
  GST_TYPE_FIND_REGISTER (svx, plugin);
  GST_TYPE_FIND_REGISTER (paris, plugin);
  GST_TYPE_FIND_REGISTER (sds, plugin);
  GST_TYPE_FIND_REGISTER (ircam, plugin);
  GST_TYPE_FIND_REGISTER (shn, plugin);
  GST_TYPE_FIND_REGISTER (ape, plugin);
  GST_TYPE_FIND_REGISTER (jpeg, plugin);
  GST_TYPE_FIND_REGISTER (bmp, plugin);
  GST_TYPE_FIND_REGISTER (tiff, plugin);
  GST_TYPE_FIND_REGISTER (exr, plugin);
  GST_TYPE_FIND_REGISTER (pnm, plugin);
  GST_TYPE_FIND_REGISTER (matroska, plugin);
  GST_TYPE_FIND_REGISTER (mxf, plugin);
  GST_TYPE_FIND_REGISTER (dv, plugin);
  GST_TYPE_FIND_REGISTER (ilbc, plugin);
  GST_TYPE_FIND_REGISTER (sbc, plugin);
  GST_TYPE_FIND_REGISTER (kate, plugin);
  GST_TYPE_FIND_REGISTER (webvtt, plugin);
  GST_TYPE_FIND_REGISTER (flac, plugin);
  GST_TYPE_FIND_REGISTER (vorbis, plugin);
  GST_TYPE_FIND_REGISTER (theora, plugin);
  GST_TYPE_FIND_REGISTER (ogmvideo, plugin);
  GST_TYPE_FIND_REGISTER (ogmaudio, plugin);
  GST_TYPE_FIND_REGISTER (ogmtext, plugin);
  GST_TYPE_FIND_REGISTER (speex, plugin);
  GST_TYPE_FIND_REGISTER (celt, plugin);
  GST_TYPE_FIND_REGISTER (oggskel, plugin);
  GST_TYPE_FIND_REGISTER (cmml, plugin);
  GST_TYPE_FIND_REGISTER (aac, plugin);
  GST_TYPE_FIND_REGISTER (wavpack_wvp, plugin);
  GST_TYPE_FIND_REGISTER (wavpack_wvc, plugin);
  GST_TYPE_FIND_REGISTER (postscript, plugin);
  GST_TYPE_FIND_REGISTER (svg, plugin);
  GST_TYPE_FIND_REGISTER (tar, plugin);
  GST_TYPE_FIND_REGISTER (ar, plugin);
  GST_TYPE_FIND_REGISTER (msdos, plugin);
  GST_TYPE_FIND_REGISTER (dirac, plugin);
  GST_TYPE_FIND_REGISTER (multipart, plugin);
  GST_TYPE_FIND_REGISTER (mmsh, plugin);
  GST_TYPE_FIND_REGISTER (vivo, plugin);
  GST_TYPE_FIND_REGISTER (wbmp, plugin);
  GST_TYPE_FIND_REGISTER (y4m, plugin);
  GST_TYPE_FIND_REGISTER (windows_icon, plugin);
#ifdef USE_GIO
  GST_TYPE_FIND_REGISTER (xdgmime, plugin);
#endif
  GST_TYPE_FIND_REGISTER (degas, plugin);
  GST_TYPE_FIND_REGISTER (dvdiso, plugin);
  GST_TYPE_FIND_REGISTER (ssa, plugin);
  GST_TYPE_FIND_REGISTER (pva, plugin);
  GST_TYPE_FIND_REGISTER (aa, plugin);
  GST_TYPE_FIND_REGISTER (tap, plugin);
  GST_TYPE_FIND_REGISTER (brstm, plugin);
  GST_TYPE_FIND_REGISTER (bfstm, plugin);
  GST_TYPE_FIND_REGISTER (dsf, plugin);
  GST_TYPE_FIND_REGISTER (ea, plugin);
  GST_TYPE_FIND_REGISTER (film_cpk, plugin);
  GST_TYPE_FIND_REGISTER (gxf, plugin);
  GST_TYPE_FIND_REGISTER (iff, plugin);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    typefindfunctions,
    "default typefind functions",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)

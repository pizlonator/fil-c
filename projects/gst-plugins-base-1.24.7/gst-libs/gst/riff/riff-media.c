/* GStreamer RIFF I/O
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * riff-media.h: RIFF-id to/from caps routines
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

#include "riff-ids.h"
#include "riff-media.h"

#include <gst/audio/audio.h>

#include <string.h>
#include <math.h>

GST_DEBUG_CATEGORY_EXTERN (riff_debug);
#define GST_CAT_DEFAULT riff_debug

/**
 * gst_riff_create_video_caps:
 * @codec_fcc: fourCC codec for this codec.
 * @strh: pointer to the strh stream header structure.
 * @strf: pointer to the strf stream header structure, including any
 *        data that is within the range of strf.size, but excluding any
 *        additional data withint this chunk but outside strf.size.
 * @strf_data: a #GstBuffer containing the additional data in the strf
 *             chunk outside reach of strf.size. Usually a palette.
 * @strd_data: a #GstBuffer containing the data in the strd stream header
 *             chunk. Usually codec initialization data.
 * @codec_name: if given, will be filled with a human-readable codec name.
 */

GstCaps *
gst_riff_create_video_caps (guint32 codec_fcc,
    gst_riff_strh * strh, gst_riff_strf_vids * strf,
    GstBuffer * strf_data, GstBuffer * strd_data, char **codec_name)
{
  GstCaps *caps = NULL;
  GstBuffer *palette = NULL;

  GST_DEBUG ("video fourcc %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (codec_fcc));

  switch (codec_fcc) {
    case GST_RIFF_DIB:         /* uncompressed RGB */
    case GST_RIFF_rgb:
    case GST_RIFF_RGB:
    case GST_RIFF_RAW:
    {
      gint bpp = (strf && strf->bit_cnt != 0) ? strf->bit_cnt : 8;

      if (strf) {
        if (bpp == 8) {
          caps = gst_caps_new_simple ("video/x-raw",
              "format", G_TYPE_STRING, "RGB8P", NULL);
        } else if (bpp == 24) {
          caps = gst_caps_new_simple ("video/x-raw",
              "format", G_TYPE_STRING, "BGR", NULL);
        } else if (bpp == 32) {
          caps = gst_caps_new_simple ("video/x-raw",
              "format", G_TYPE_STRING, "BGRx", NULL);
        } else {
          GST_WARNING ("Unhandled DIB RGB depth: %d", bpp);
          return NULL;
        }
      } else {
        /* for template */
        caps =
            gst_caps_from_string ("video/x-raw, format = (string) "
            "{ RGB8P, BGR, BGRx }");
      }

      palette = strf_data;
      strf_data = NULL;
      if (codec_name) {
        if (bpp == 8)
          *codec_name = g_strdup_printf ("Palettized %d-bit RGB", bpp);
        else
          *codec_name = g_strdup_printf ("%d-bit RGB", bpp);
      }
      break;
    }

    case GST_MAKE_FOURCC ('G', 'R', 'E', 'Y'):
    case GST_MAKE_FOURCC ('Y', '8', '0', '0'):
    case GST_MAKE_FOURCC ('Y', '8', ' ', ' '):
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "GRAY8", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Uncompressed 8-bit monochrome");
      break;

    case GST_MAKE_FOURCC ('r', '2', '1', '0'):
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "r210", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Uncompressed packed RGB 10-bit 4:4:4");
      break;

    case GST_RIFF_I420:
    case GST_RIFF_i420:
    case GST_RIFF_IYUV:
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "I420", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Uncompressed planar YUV 4:2:0");
      break;

    case GST_RIFF_YUY2:
    case GST_RIFF_yuy2:
    case GST_MAKE_FOURCC ('Y', 'U', 'N', 'V'):
    case GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V'):
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "YUY2", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Uncompressed packed YUV 4:2:2");
      break;

    case GST_RIFF_YVU9:
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "YVU9", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Uncompressed packed YVU 4:1:0");
      break;

    case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
    case GST_MAKE_FOURCC ('2', 'v', 'u', 'y'):
    case GST_MAKE_FOURCC ('H', 'D', 'Y', 'C'):
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "UYVY", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Uncompressed packed YUV 4:2:2");
      break;

    case GST_RIFF_YV12:
    case GST_RIFF_yv12:
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "YV12", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Uncompressed packed YVU 4:2:2");
      break;
    case GST_MAKE_FOURCC ('v', '2', '1', '0'):
      caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "v210", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Uncompressed packed 10-bit YUV 4:2:2");
      break;

    case GST_RIFF_MJPG:        /* YUY2 MJPEG */
    case GST_RIFF_mJPG:
    case GST_MAKE_FOURCC ('A', 'V', 'R', 'n'):
    case GST_RIFF_IJPG:
    case GST_MAKE_FOURCC ('i', 'j', 'p', 'g'):
    case GST_RIFF_DMB1:
    case GST_RIFF_dmb1:
    case GST_MAKE_FOURCC ('A', 'C', 'D', 'V'):
    case GST_MAKE_FOURCC ('Q', 'I', 'V', 'G'):
      caps =
          gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("Motion JPEG");
      break;

    case GST_RIFF_JPEG:        /* generic (mostly RGB) MJPEG */
    case GST_RIFF_jpeg:
    case GST_MAKE_FOURCC ('j', 'p', 'e', 'g'): /* generic (mostly RGB) MJPEG */
      caps =
          gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("JPEG Still Image");
      break;

    case GST_MAKE_FOURCC ('P', 'I', 'X', 'L'): /* Miro/Pinnacle fourccs */
    case GST_RIFF_VIXL:        /* Miro/Pinnacle fourccs */
    case GST_RIFF_vixl:
      caps =
          gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("Miro/Pinnacle Motion JPEG");
      break;

    case GST_MAKE_FOURCC ('C', 'J', 'P', 'G'):
      caps =
          gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("Creative Webcam JPEG");
      break;

    case GST_MAKE_FOURCC ('S', 'L', 'M', 'J'):
      caps =
          gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("SL Motion JPEG");
      break;

    case GST_MAKE_FOURCC ('J', 'P', 'G', 'L'):
      caps =
          gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, TRUE,
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("Pegasus Lossless JPEG");
      break;

    case GST_MAKE_FOURCC ('L', 'O', 'C', 'O'):
      caps = gst_caps_new_empty_simple ("video/x-loco");
      if (codec_name)
        *codec_name = g_strdup ("LOCO Lossless");
      break;

    case GST_MAKE_FOURCC ('S', 'P', '5', '3'):
    case GST_MAKE_FOURCC ('S', 'P', '5', '4'):
    case GST_MAKE_FOURCC ('S', 'P', '5', '5'):
    case GST_MAKE_FOURCC ('S', 'P', '5', '6'):
    case GST_MAKE_FOURCC ('S', 'P', '5', '7'):
    case GST_MAKE_FOURCC ('S', 'P', '5', '8'):
      caps = gst_caps_new_empty_simple ("video/sp5x");
      if (codec_name)
        *codec_name = g_strdup ("Sp5x-like JPEG");
      break;

    case GST_MAKE_FOURCC ('Z', 'M', 'B', 'V'):
      caps = gst_caps_new_empty_simple ("video/x-zmbv");
      if (codec_name)
        *codec_name = g_strdup ("Zip Motion Block video");
      break;

    case GST_MAKE_FOURCC ('H', 'F', 'Y', 'U'):
      caps = gst_caps_new_empty_simple ("video/x-huffyuv");
      if (strf) {
        gst_caps_set_simple (caps, "bpp",
            G_TYPE_INT, (int) strf->bit_cnt, NULL);
      }
      if (codec_name)
        *codec_name = g_strdup ("Huffman Lossless Codec");
      break;

    case GST_MAKE_FOURCC ('F', 'F', 'V', 'H'):
      caps = gst_caps_new_empty_simple ("video/x-ffvhuff");
      if (strf) {
        gst_caps_set_simple (caps, "bpp",
            G_TYPE_INT, (int) strf->bit_cnt, NULL);
      }
      if (codec_name)
        *codec_name = g_strdup ("FFmpeg Huffman YUV variant");
      break;

    case GST_MAKE_FOURCC ('M', 'P', 'E', 'G'):
    case GST_MAKE_FOURCC ('M', 'P', 'G', 'I'):
    case GST_MAKE_FOURCC ('m', 'p', 'g', '1'):
    case GST_MAKE_FOURCC ('M', 'P', 'G', '1'):
    case GST_MAKE_FOURCC ('P', 'I', 'M', '1'):
    case GST_MAKE_FOURCC (0x01, 0x00, 0x00, 0x10):
      caps = gst_caps_new_simple ("video/mpeg",
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "mpegversion", G_TYPE_INT, 1, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-1 video");
      break;

    case GST_MAKE_FOURCC ('M', 'P', 'G', '2'):
    case GST_MAKE_FOURCC ('m', 'p', 'g', '2'):
    case GST_MAKE_FOURCC ('P', 'I', 'M', '2'):
    case GST_MAKE_FOURCC ('D', 'V', 'R', ' '):
    case GST_MAKE_FOURCC (0x02, 0x00, 0x00, 0x10):
      caps = gst_caps_new_simple ("video/mpeg",
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "mpegversion", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-2 video");
      break;

    case GST_MAKE_FOURCC ('L', 'M', 'P', '2'):
      caps = gst_caps_new_simple ("video/mpeg",
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "mpegversion", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Lead MPEG-2 video");
      break;

    case GST_RIFF_H263:
    case GST_RIFF_h263:
    case GST_RIFF_i263:
    case GST_MAKE_FOURCC ('U', '2', '6', '3'):
    case GST_MAKE_FOURCC ('v', 'i', 'v', '1'):
    case GST_MAKE_FOURCC ('T', '2', '6', '3'):
      caps = gst_caps_new_simple ("video/x-h263",
          "variant", G_TYPE_STRING, "itu", NULL);
      if (codec_name)
        *codec_name = g_strdup ("ITU H.26n");
      break;

    case GST_RIFF_L263:
      /* http://www.leadcodecs.com/Codecs/LEAD-H263.htm */
      caps = gst_caps_new_simple ("video/x-h263",
          "variant", G_TYPE_STRING, "lead", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Lead H.263");
      break;

    case GST_RIFF_M263:
    case GST_RIFF_m263:
      caps = gst_caps_new_simple ("video/x-h263",
          "variant", G_TYPE_STRING, "microsoft", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft H.263");
      break;

    case GST_RIFF_VDOW:
      caps = gst_caps_new_simple ("video/x-h263",
          "variant", G_TYPE_STRING, "vdolive", NULL);
      if (codec_name)
        *codec_name = g_strdup ("VDOLive");
      break;

    case GST_MAKE_FOURCC ('V', 'I', 'V', 'O'):
      caps = gst_caps_new_simple ("video/x-h263",
          "variant", G_TYPE_STRING, "vivo", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Vivo H.263");
      break;

    case GST_RIFF_x263:
      caps = gst_caps_new_simple ("video/x-h263",
          "variant", G_TYPE_STRING, "xirlink", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Xirlink H.263");
      break;

      /* apparently not standard H.263...? */
    case GST_MAKE_FOURCC ('I', '2', '6', '3'):
      caps = gst_caps_new_simple ("video/x-intel-h263",
          "variant", G_TYPE_STRING, "intel", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Intel H.263");
      break;

    case GST_MAKE_FOURCC ('V', 'X', '1', 'K'):
      caps = gst_caps_new_simple ("video/x-h263",
          "variant", G_TYPE_STRING, "lucent", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Lucent VX1000S H.263");
      break;

    case GST_MAKE_FOURCC ('X', '2', '6', '4'):
    case GST_MAKE_FOURCC ('x', '2', '6', '4'):
    case GST_MAKE_FOURCC ('H', '2', '6', '4'):
    case GST_MAKE_FOURCC ('h', '2', '6', '4'):
    case GST_MAKE_FOURCC ('a', 'v', 'c', '1'):
    case GST_MAKE_FOURCC ('A', 'V', 'C', '1'):
      caps = gst_caps_new_simple ("video/x-h264",
          "variant", G_TYPE_STRING, "itu", NULL);
      if (codec_name)
        *codec_name = g_strdup ("ITU H.264");
      break;

    case GST_MAKE_FOURCC ('X', '2', '6', '5'):
    case GST_MAKE_FOURCC ('x', '2', '6', '5'):
    case GST_MAKE_FOURCC ('H', '2', '6', '5'):
    case GST_MAKE_FOURCC ('h', '2', '6', '5'):
    case GST_MAKE_FOURCC ('h', 'v', 'c', '1'):
    case GST_MAKE_FOURCC ('H', 'V', 'C', '1'):
    case GST_MAKE_FOURCC ('h', 'e', 'v', 'c'):
    case GST_MAKE_FOURCC ('H', 'E', 'V', 'C'):
    case GST_MAKE_FOURCC ('h', 'e', 'v', '1'):
    case GST_MAKE_FOURCC ('H', 'E', 'V', '1'):
      caps = gst_caps_new_empty_simple ("video/x-h265");
      if (codec_name)
        *codec_name = g_strdup ("H.265");
      break;

    case GST_RIFF_VSSH:
      caps = gst_caps_new_simple ("video/x-h264",
          "variant", G_TYPE_STRING, "videosoft", NULL);
      if (codec_name)
        *codec_name = g_strdup ("VideoSoft H.264");
      break;

    case GST_MAKE_FOURCC ('L', '2', '6', '4'):
      /* http://www.leadcodecs.com/Codecs/LEAD-H264.htm */
      caps = gst_caps_new_simple ("video/x-h264",
          "variant", G_TYPE_STRING, "lead", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Lead H.264");
      break;

    case GST_MAKE_FOURCC ('S', 'E', 'D', 'G'):
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Samsung MPEG-4");
      break;

    case GST_MAKE_FOURCC ('M', '4', 'C', 'C'):
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Divio MPEG-4");
      break;

    case GST_RIFF_DIV3:
    case GST_MAKE_FOURCC ('d', 'i', 'v', '3'):
    case GST_MAKE_FOURCC ('D', 'V', 'X', '3'):
    case GST_MAKE_FOURCC ('d', 'v', 'x', '3'):
    case GST_MAKE_FOURCC ('D', 'I', 'V', '4'):
    case GST_MAKE_FOURCC ('d', 'i', 'v', '4'):
    case GST_MAKE_FOURCC ('D', 'I', 'V', '5'):
    case GST_MAKE_FOURCC ('d', 'i', 'v', '5'):
    case GST_MAKE_FOURCC ('D', 'I', 'V', '6'):
    case GST_MAKE_FOURCC ('d', 'i', 'v', '6'):
    case GST_MAKE_FOURCC ('M', 'P', 'G', '3'):
    case GST_MAKE_FOURCC ('m', 'p', 'g', '3'):
    case GST_MAKE_FOURCC ('c', 'o', 'l', '0'):
    case GST_MAKE_FOURCC ('C', 'O', 'L', '0'):
    case GST_MAKE_FOURCC ('c', 'o', 'l', '1'):
    case GST_MAKE_FOURCC ('C', 'O', 'L', '1'):
    case GST_MAKE_FOURCC ('A', 'P', '4', '1'):
      caps = gst_caps_new_simple ("video/x-divx",
          "divxversion", G_TYPE_INT, 3, NULL);
      if (codec_name)
        *codec_name = g_strdup ("DivX MS-MPEG-4 Version 3");
      break;

    case GST_MAKE_FOURCC ('d', 'i', 'v', 'x'):
    case GST_MAKE_FOURCC ('D', 'I', 'V', 'X'):
      caps = gst_caps_new_simple ("video/x-divx",
          "divxversion", G_TYPE_INT, 4, NULL);
      if (codec_name)
        *codec_name = g_strdup ("DivX MPEG-4 Version 4");
      break;

    case GST_MAKE_FOURCC ('B', 'L', 'Z', '0'):
      caps = gst_caps_new_simple ("video/x-divx",
          "divxversion", G_TYPE_INT, 4, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Blizzard DivX");
      break;

    case GST_MAKE_FOURCC ('D', 'X', '5', '0'):
      caps = gst_caps_new_simple ("video/x-divx",
          "divxversion", G_TYPE_INT, 5, NULL);
      if (codec_name)
        *codec_name = g_strdup ("DivX MPEG-4 Version 5");
      break;

    case GST_MAKE_FOURCC ('M', 'P', 'G', '4'):
    case GST_MAKE_FOURCC ('M', 'P', '4', '1'):
    case GST_MAKE_FOURCC ('m', 'p', '4', '1'):
      caps = gst_caps_new_simple ("video/x-msmpeg",
          "msmpegversion", G_TYPE_INT, 41, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft MPEG-4 4.1");
      break;

    case GST_MAKE_FOURCC ('m', 'p', '4', '2'):
    case GST_MAKE_FOURCC ('M', 'P', '4', '2'):
      caps = gst_caps_new_simple ("video/x-msmpeg",
          "msmpegversion", G_TYPE_INT, 42, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft MPEG-4 4.2");
      break;

    case GST_MAKE_FOURCC ('m', 'p', '4', '3'):
    case GST_MAKE_FOURCC ('M', 'P', '4', '3'):
      caps = gst_caps_new_simple ("video/x-msmpeg",
          "msmpegversion", G_TYPE_INT, 43, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft MPEG-4 4.3");
      break;

    case GST_MAKE_FOURCC ('M', 'P', '4', 'S'):
    case GST_MAKE_FOURCC ('M', '4', 'S', '2'):
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft ISO MPEG-4 1.1");
      break;

    case GST_MAKE_FOURCC ('F', 'M', 'P', '4'):
    case GST_MAKE_FOURCC ('U', 'M', 'P', '4'):
    case GST_MAKE_FOURCC ('F', 'F', 'D', 'S'):
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      if (codec_name)
        *codec_name = g_strdup ("FFmpeg MPEG-4");
      break;

    case GST_MAKE_FOURCC ('3', 'I', 'V', '1'):
    case GST_MAKE_FOURCC ('3', 'I', 'V', '2'):
    case GST_MAKE_FOURCC ('X', 'V', 'I', 'D'):
    case GST_MAKE_FOURCC ('x', 'v', 'i', 'd'):
    case GST_MAKE_FOURCC ('E', 'M', '4', 'A'):
    case GST_MAKE_FOURCC ('E', 'P', 'V', 'H'):
    case GST_MAKE_FOURCC ('F', 'V', 'F', 'W'):
    case GST_MAKE_FOURCC ('I', 'N', 'M', 'C'):
    case GST_MAKE_FOURCC ('D', 'I', 'G', 'I'):
    case GST_MAKE_FOURCC ('D', 'M', '2', 'K'):
    case GST_MAKE_FOURCC ('D', 'C', 'O', 'D'):
    case GST_MAKE_FOURCC ('M', 'V', 'X', 'M'):
    case GST_MAKE_FOURCC ('P', 'M', '4', 'V'):
    case GST_MAKE_FOURCC ('S', 'M', 'P', '4'):
    case GST_MAKE_FOURCC ('D', 'X', 'G', 'M'):
    case GST_MAKE_FOURCC ('V', 'I', 'D', 'M'):
    case GST_MAKE_FOURCC ('M', '4', 'T', '3'):
    case GST_MAKE_FOURCC ('G', 'E', 'O', 'X'):
    case GST_MAKE_FOURCC ('M', 'P', '4', 'V'):
    case GST_MAKE_FOURCC ('m', 'p', '4', 'v'):
    case GST_MAKE_FOURCC ('R', 'M', 'P', '4'):
      caps = gst_caps_new_simple ("video/mpeg",
          "mpegversion", G_TYPE_INT, 4,
          "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-4");
      break;

    case GST_MAKE_FOURCC ('3', 'i', 'v', 'd'):
    case GST_MAKE_FOURCC ('3', 'I', 'V', 'D'):
      caps = gst_caps_new_simple ("video/x-msmpeg",
          "msmpegversion", G_TYPE_INT, 43, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft MPEG-4 4.3");        /* FIXME? */
      break;

    case GST_MAKE_FOURCC ('C', 'F', 'H', 'D'):
      caps = gst_caps_new_empty_simple ("video/x-cineform");
      if (codec_name)
        *codec_name = g_strdup ("CineForm");
      break;

    case GST_MAKE_FOURCC ('D', 'V', 'S', 'D'):
    case GST_MAKE_FOURCC ('d', 'v', 's', 'd'):
    case GST_MAKE_FOURCC ('d', 'v', 'c', ' '):
    case GST_MAKE_FOURCC ('d', 'v', '2', '5'):
      caps = gst_caps_new_simple ("video/x-dv",
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "dvversion", G_TYPE_INT, 25, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Generic DV");
      break;

    case GST_MAKE_FOURCC ('C', 'D', 'V', 'C'):
    case GST_MAKE_FOURCC ('c', 'd', 'v', 'c'):
      caps = gst_caps_new_simple ("video/x-dv",
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "dvversion", G_TYPE_INT, 25, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Canopus DV");
      break;

    case GST_MAKE_FOURCC ('D', 'V', '5', '0'):
    case GST_MAKE_FOURCC ('d', 'v', '5', '0'):
      caps = gst_caps_new_simple ("video/x-dv",
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "dvversion", G_TYPE_INT, 50, NULL);
      if (codec_name)
        *codec_name = g_strdup ("DVCPro50 Video");
      break;

    case GST_MAKE_FOURCC ('M', 'S', 'S', '1'):
      caps = gst_caps_new_simple ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 1, "format", G_TYPE_STRING, "MSS1", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft Windows Media 7 Screen");
      break;

    case GST_MAKE_FOURCC ('M', 'S', 'S', '2'):
      caps = gst_caps_new_simple ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 3, "format", G_TYPE_STRING, "MSS2", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft Windows Media 9 Screen");
      break;

    case GST_MAKE_FOURCC ('W', 'M', 'V', '1'):
      caps = gst_caps_new_simple ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 1, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft Windows Media 7");
      break;

    case GST_MAKE_FOURCC ('W', 'M', 'V', '2'):
      caps = gst_caps_new_simple ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft Windows Media 8");
      break;

    case GST_MAKE_FOURCC ('W', 'M', 'V', '3'):
      caps = gst_caps_new_simple ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 3, "format", G_TYPE_STRING, "WMV3", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft Windows Media 9");
      break;

    case GST_MAKE_FOURCC ('W', 'M', 'V', 'A'):
      caps = gst_caps_new_simple ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 3, "format", G_TYPE_STRING, "WMVA", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft Windows Media Advanced Profile");
      break;

    case GST_MAKE_FOURCC ('W', 'V', 'C', '1'):
      caps = gst_caps_new_simple ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 3, "format", G_TYPE_STRING, "WVC1", NULL);
      if (codec_name)
        *codec_name = g_strdup ("Microsoft Windows Media VC-1");
      break;

    case GST_RIFF_cvid:
    case GST_RIFF_CVID:
      caps = gst_caps_new_empty_simple ("video/x-cinepak");
      if (codec_name)
        *codec_name = g_strdup ("Cinepak video");
      break;

    case GST_RIFF_FCCH_MSVC:
    case GST_RIFF_FCCH_msvc:
    case GST_RIFF_CRAM:
    case GST_RIFF_cram:
    case GST_RIFF_WHAM:
    case GST_RIFF_wham:
      caps = gst_caps_new_simple ("video/x-msvideocodec",
          "msvideoversion", G_TYPE_INT, 1, NULL);
      if (strf) {
        gst_caps_set_simple (caps, "bpp",
            G_TYPE_INT, (int) strf->bit_cnt, NULL);
      }
      if (codec_name)
        *codec_name = g_strdup ("MS video v1");
      palette = strf_data;
      strf_data = NULL;
      break;

    case GST_RIFF_FCCH_RLE:
    case GST_MAKE_FOURCC ('m', 'r', 'l', 'e'):
    case GST_MAKE_FOURCC (0x1, 0x0, 0x0, 0x0): /* why, why, why? */
    case GST_MAKE_FOURCC (0x2, 0x0, 0x0, 0x0): /* why, why, why? */
      caps = gst_caps_new_simple ("video/x-rle",
          "layout", G_TYPE_STRING, "microsoft", NULL);
      palette = strf_data;
      strf_data = NULL;
      if (strf) {
        gst_caps_set_simple (caps,
            "depth", G_TYPE_INT, (gint) strf->bit_cnt, NULL);
      } else {
        gst_caps_set_simple (caps, "depth", GST_TYPE_INT_RANGE, 1, 64, NULL);
      }
      if (codec_name)
        *codec_name = g_strdup ("Microsoft RLE");
      break;

    case GST_MAKE_FOURCC ('A', 'A', 'S', 'C'):
      caps = gst_caps_new_empty_simple ("video/x-aasc");
      if (codec_name)
        *codec_name = g_strdup ("Autodesk Animator");
      break;

    case GST_MAKE_FOURCC ('X', 'x', 'a', 'n'):
      caps = gst_caps_new_simple ("video/x-xan",
          "wcversion", G_TYPE_INT, 4, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Xan Wing Commander 4");
      break;

    case GST_RIFF_RT21:
    case GST_RIFF_rt21:
      caps = gst_caps_new_simple ("video/x-indeo",
          "indeoversion", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Intel Video 2");
      break;

    case GST_RIFF_IV31:
    case GST_RIFF_IV32:
    case GST_RIFF_iv31:
    case GST_RIFF_iv32:
      caps = gst_caps_new_simple ("video/x-indeo",
          "indeoversion", G_TYPE_INT, 3, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Intel Video 3");
      break;

    case GST_RIFF_IV41:
    case GST_RIFF_iv41:
      caps = gst_caps_new_simple ("video/x-indeo",
          "indeoversion", G_TYPE_INT, 4, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Intel Video 4");
      break;

    case GST_RIFF_IV50:
      caps = gst_caps_new_simple ("video/x-indeo",
          "indeoversion", G_TYPE_INT, 5, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Intel Video 5");
      break;

    case GST_MAKE_FOURCC ('M', 'S', 'Z', 'H'):
      caps = gst_caps_new_empty_simple ("video/x-mszh");
      if (codec_name)
        *codec_name = g_strdup ("Lossless MSZH Video");
      break;

    case GST_MAKE_FOURCC ('Z', 'L', 'I', 'B'):
      caps = gst_caps_new_empty_simple ("video/x-zlib");
      if (codec_name)
        *codec_name = g_strdup ("Lossless zlib video");
      break;

    case GST_MAKE_FOURCC ('C', 'L', 'J', 'R'):
    case GST_MAKE_FOURCC ('c', 'l', 'j', 'r'):
      caps = gst_caps_new_empty_simple ("video/x-cirrus-logic-accupak");
      if (codec_name)
        *codec_name = g_strdup ("Cirrus Logipak AccuPak");
      break;

    case GST_RIFF_CYUV:
    case GST_RIFF_cyuv:
      caps = gst_caps_new_empty_simple ("video/x-compressed-yuv");
      if (codec_name)
        *codec_name = g_strdup ("CYUV Lossless");
      break;

    case GST_MAKE_FOURCC ('D', 'U', 'C', 'K'):
    case GST_MAKE_FOURCC ('P', 'V', 'E', 'Z'):
      caps = gst_caps_new_simple ("video/x-truemotion",
          "trueversion", G_TYPE_INT, 1, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Duck Truemotion1");
      break;

    case GST_MAKE_FOURCC ('T', 'M', '2', '0'):
      caps = gst_caps_new_simple ("video/x-truemotion",
          "trueversion", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("TrueMotion 2.0");
      break;

    case GST_MAKE_FOURCC ('V', 'P', '3', '0'):
    case GST_MAKE_FOURCC ('v', 'p', '3', '0'):
    case GST_MAKE_FOURCC ('V', 'P', '3', '1'):
    case GST_MAKE_FOURCC ('v', 'p', '3', '1'):
    case GST_MAKE_FOURCC ('V', 'P', '3', ' '):
      caps = gst_caps_new_empty_simple ("video/x-vp3");
      if (codec_name)
        *codec_name = g_strdup ("VP3");
      break;

    case GST_RIFF_ULTI:
    case GST_RIFF_ulti:
      caps = gst_caps_new_empty_simple ("video/x-ultimotion");
      if (codec_name)
        *codec_name = g_strdup ("IBM UltiMotion");
      break;

      /* FIXME 2.0: Rename video/x-camtasia to video/x-tscc,version=1 */
    case GST_MAKE_FOURCC ('T', 'S', 'C', 'C'):
    case GST_MAKE_FOURCC ('t', 's', 'c', 'c'):{
      if (strf) {
        gint depth = (strf->bit_cnt != 0) ? (gint) strf->bit_cnt : 24;

        caps = gst_caps_new_simple ("video/x-camtasia", "depth", G_TYPE_INT,
            depth, NULL);
      } else {
        /* template caps */
        caps = gst_caps_new_empty_simple ("video/x-camtasia");
      }
      if (codec_name)
        *codec_name = g_strdup ("TechSmith Camtasia");
      break;
    }

    case GST_MAKE_FOURCC ('T', 'S', 'C', '2'):
    case GST_MAKE_FOURCC ('t', 's', 'c', '2'):{
      caps =
          gst_caps_new_simple ("video/x-tscc", "tsccversion", G_TYPE_INT, 2,
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("TechSmith Screen Capture 2");
      break;
    }

    case GST_MAKE_FOURCC ('C', 'S', 'C', 'D'):
    {
      if (strf) {
        gint depth = (strf->bit_cnt != 0) ? (gint) strf->bit_cnt : 24;

        caps = gst_caps_new_simple ("video/x-camstudio", "depth", G_TYPE_INT,
            depth, NULL);
      } else {
        /* template caps */
        caps = gst_caps_new_empty_simple ("video/x-camstudio");
      }
      if (codec_name)
        *codec_name = g_strdup ("Camstudio");
      break;
    }

    case GST_MAKE_FOURCC ('V', 'C', 'R', '1'):
      caps = gst_caps_new_simple ("video/x-ati-vcr",
          "vcrversion", G_TYPE_INT, 1, NULL);
      if (codec_name)
        *codec_name = g_strdup ("ATI VCR 1");
      break;

    case GST_MAKE_FOURCC ('V', 'C', 'R', '2'):
      caps = gst_caps_new_simple ("video/x-ati-vcr",
          "vcrversion", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("ATI VCR 2");
      break;

    case GST_MAKE_FOURCC ('A', 'S', 'V', '1'):
      caps = gst_caps_new_simple ("video/x-asus",
          "asusversion", G_TYPE_INT, 1, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Asus Video 1");
      break;

    case GST_MAKE_FOURCC ('A', 'S', 'V', '2'):
      caps = gst_caps_new_simple ("video/x-asus",
          "asusversion", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Asus Video 2");
      break;

    case GST_MAKE_FOURCC ('M', 'P', 'N', 'G'):
    case GST_MAKE_FOURCC ('m', 'p', 'n', 'g'):
    case GST_MAKE_FOURCC ('P', 'N', 'G', ' '):
    case GST_MAKE_FOURCC ('p', 'n', 'g', ' '):
      caps = gst_caps_new_empty_simple ("image/png");
      if (codec_name)
        *codec_name = g_strdup ("PNG image");
      break;

    case GST_MAKE_FOURCC ('F', 'L', 'V', '1'):
      caps = gst_caps_new_simple ("video/x-flash-video",
          "flvversion", G_TYPE_INT, 1, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Flash Video 1");
      break;

    case GST_MAKE_FOURCC ('V', 'M', 'n', 'c'):
      caps = gst_caps_new_simple ("video/x-vmnc",
          "version", G_TYPE_INT, 1, NULL);
      if (strf && strf->bit_cnt != 0)
        gst_caps_set_simple (caps, "bpp", G_TYPE_INT, strf->bit_cnt, NULL);
      if (codec_name)
        *codec_name = g_strdup ("VMWare NC Video");
      break;

    case GST_MAKE_FOURCC ('d', 'r', 'a', 'c'):
      caps = gst_caps_new_empty_simple ("video/x-dirac");
      if (codec_name)
        *codec_name = g_strdup ("Dirac");
      break;

    case GST_RIFF_rpza:
    case GST_RIFF_azpr:
    case GST_MAKE_FOURCC ('R', 'P', 'Z', 'A'):
      caps = gst_caps_new_empty_simple ("video/x-apple-video");
      if (codec_name)
        *codec_name = g_strdup ("Apple Video (RPZA)");
      break;


    case GST_MAKE_FOURCC ('F', 'F', 'V', '1'):
      caps = gst_caps_new_simple ("video/x-ffv",
          "ffvversion", G_TYPE_INT, 1, NULL);
      if (codec_name)
        *codec_name = g_strdup ("FFmpeg lossless video codec");
      break;

    case GST_MAKE_FOURCC ('K', 'M', 'V', 'C'):
      caps = gst_caps_new_empty_simple ("video/x-kmvc");
      if (codec_name)
        *codec_name = g_strdup ("Karl Morton's video codec");
      break;

    case GST_MAKE_FOURCC ('v', 'p', '5', '0'):
    case GST_MAKE_FOURCC ('V', 'P', '5', '0'):
      caps = gst_caps_new_empty_simple ("video/x-vp5");
      if (codec_name)
        *codec_name = g_strdup ("On2 VP5");
      break;

    case GST_MAKE_FOURCC ('v', 'p', '6', '0'):
    case GST_MAKE_FOURCC ('V', 'P', '6', '0'):
    case GST_MAKE_FOURCC ('v', 'p', '6', '1'):
    case GST_MAKE_FOURCC ('V', 'P', '6', '1'):
    case GST_MAKE_FOURCC ('V', 'p', '6', '2'):
    case GST_MAKE_FOURCC ('V', 'P', '6', '2'):
      caps = gst_caps_new_empty_simple ("video/x-vp6");
      if (codec_name)
        *codec_name = g_strdup ("On2 VP6");
      break;

    case GST_MAKE_FOURCC ('V', 'P', '6', 'F'):
    case GST_MAKE_FOURCC ('v', 'p', '6', 'f'):
    case GST_MAKE_FOURCC ('F', 'L', 'V', '4'):
      caps = gst_caps_new_empty_simple ("video/x-vp6-flash");
      if (codec_name)
        *codec_name = g_strdup ("On2 VP6");
      break;

    case GST_MAKE_FOURCC ('v', 'p', '7', '0'):
    case GST_MAKE_FOURCC ('V', 'P', '7', '0'):
      caps = gst_caps_new_empty_simple ("video/x-vp7");
      if (codec_name)
        *codec_name = g_strdup ("On2 VP7");
      break;

    case GST_MAKE_FOURCC ('V', 'P', '8', '0'):
      caps = gst_caps_new_empty_simple ("video/x-vp8");
      if (codec_name)
        *codec_name = g_strdup ("On2 VP8");
      break;

    case GST_MAKE_FOURCC ('L', 'M', '2', '0'):
      caps = gst_caps_new_empty_simple ("video/x-mimic");
      if (codec_name)
        *codec_name = g_strdup ("Mimic webcam");
      break;

    case GST_MAKE_FOURCC ('T', 'H', 'E', 'O'):
    case GST_MAKE_FOURCC ('t', 'h', 'e', 'o'):
      caps = gst_caps_new_empty_simple ("video/x-theora");
      if (codec_name)
        *codec_name = g_strdup ("Theora video codec");

      break;

    case GST_MAKE_FOURCC ('F', 'P', 'S', '1'):
      caps = gst_caps_new_empty_simple ("video/x-fraps");
      if (codec_name)
        *codec_name = g_strdup ("Fraps video");

      break;

    case GST_MAKE_FOURCC ('D', 'X', 'S', 'B'):
    case GST_MAKE_FOURCC ('D', 'X', 'S', 'A'):
      caps = gst_caps_new_empty_simple ("subpicture/x-xsub");
      if (codec_name)
        *codec_name = g_strdup ("XSUB subpicture stream");

      break;

    default:
      GST_WARNING ("Unknown video fourcc %" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (codec_fcc));
      return NULL;
  }

  if (strh != NULL) {
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION,
        strh->rate, strh->scale, NULL);
  } else {
    gst_caps_set_simple (caps,
        "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
  }

  if (strf != NULL) {
    /* raw rgb data is stored topdown, but instead of inverting the buffer, */
    /* some tools just negate the height field in the header (e.g. ffmpeg) */
    gst_caps_set_simple (caps,
        "width", G_TYPE_INT, strf->width,
        "height", G_TYPE_INT, ABS ((gint) strf->height), NULL);
  } else {
    gst_caps_set_simple (caps,
        "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
  }

  /* extradata */
  if (strf_data || strd_data) {
    GstBuffer *codec_data;

    codec_data = strf_data ? strf_data : strd_data;

    gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, codec_data, NULL);
  }

  /* palette */
  if (palette) {
    GstBuffer *copy;
    guint num_colors;
    gsize size;

    if (strf != NULL)
      num_colors = strf->num_colors;
    else
      num_colors = 256;

    size = gst_buffer_get_size (palette);

    if (size >= (num_colors * 4)) {
      guint8 *pdata;

      /* palette is always at least 256*4 bytes */
      pdata = g_malloc0 (MAX (size, 256 * 4));
      gst_buffer_extract (palette, 0, pdata, size);

      if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        guint8 *p = pdata;
        gint n;

        /* own endianness */
        for (n = 0; n < num_colors; n++) {
          GST_WRITE_UINT32_BE (p, GST_READ_UINT32_LE (p));
          p += sizeof (guint32);
        }
      }

      copy = gst_buffer_new_wrapped (pdata, size);
      gst_caps_set_simple (caps, "palette_data", GST_TYPE_BUFFER, copy, NULL);
      gst_buffer_unref (copy);
    } else {
      GST_WARNING ("Palette smaller than expected: broken file");
    }
  }

  return caps;
}

static const struct
{
  const guint32 ms_mask;
  const GstAudioChannelPosition gst_pos;
} layout_mapping[] = {
  {
      0x00001, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT}, {
      0x00002, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT}, {
      0x00004, GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER}, {
      0x00008, GST_AUDIO_CHANNEL_POSITION_LFE1}, {
      0x00010, GST_AUDIO_CHANNEL_POSITION_REAR_LEFT}, {
      0x00020, GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT}, {
      0x00040, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER}, {
      0x00080, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER}, {
      0x00100, GST_AUDIO_CHANNEL_POSITION_REAR_CENTER}, {
      0x00200, GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT}, {
      0x00400, GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT}, {
      0x00800, GST_AUDIO_CHANNEL_POSITION_TOP_CENTER}, {
      0x01000, GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_LEFT}, {
      0x02000, GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_CENTER}, {
      0x04000, GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_RIGHT}, {
      0x08000, GST_AUDIO_CHANNEL_POSITION_TOP_REAR_LEFT}, {
      0x10000, GST_AUDIO_CHANNEL_POSITION_TOP_REAR_CENTER}, {
      0x20000, GST_AUDIO_CHANNEL_POSITION_TOP_REAR_RIGHT}
};

#define MAX_CHANNEL_POSITIONS G_N_ELEMENTS (layout_mapping)

static gboolean
gst_riff_wavext_add_channel_mask (GstCaps * caps, gint num_channels,
    guint32 layout, gint channel_reorder_map[18])
{
  gint i, p;
  guint64 channel_mask = 0;
  GstAudioChannelPosition *from, *to;
  gboolean ret = FALSE;

  if (num_channels < 1) {
    GST_DEBUG ("invalid number of channels: %d", num_channels);
    return FALSE;
  }

  from = g_new (GstAudioChannelPosition, num_channels);
  to = g_new (GstAudioChannelPosition, num_channels);
  p = 0;
  for (i = 0; i < MAX_CHANNEL_POSITIONS; ++i) {
    if ((layout & layout_mapping[i].ms_mask) != 0) {
      if (p >= num_channels) {
        GST_WARNING ("More bits set in the channel layout map than there "
            "are channels! Setting channel-mask to 0.");
        channel_mask = 0;
        break;
      }
      channel_mask |= G_GUINT64_CONSTANT (1) << layout_mapping[i].gst_pos;
      from[p] = layout_mapping[i].gst_pos;
      ++p;
    }
  }

  if (channel_mask > 0 && channel_reorder_map) {
    if (p != num_channels) {
      /* WAVEFORMATEXTENSIBLE allows to have more channels than bits in
       * the channel mask. We accept this, too, and hope that downstream
       * can handle this */
      GST_WARNING ("Partially unknown positions in channel mask");
      for (; p < num_channels; ++p)
        from[p] = GST_AUDIO_CHANNEL_POSITION_INVALID;
    }
    memcpy (to, from, sizeof (from[0]) * num_channels);
    if (!gst_audio_channel_positions_to_valid_order (to, num_channels))
      goto fail;
    if (!gst_audio_get_channel_reorder_map (num_channels, from, to,
            channel_reorder_map))
      goto fail;
  }

  gst_caps_set_simple (caps, "channel-mask", GST_TYPE_BITMASK, channel_mask,
      NULL);

  ret = TRUE;

fail:
  g_free (from);
  g_free (to);

  return ret;
}

static gboolean
gst_riff_wave_add_default_channel_mask (GstCaps * caps,
    gint nchannels, gint channel_reorder_map[18])
{
  guint64 channel_mask = 0;
  static const gint reorder_maps[8][11] = {
    {0,},
    {0, 1},
    {-1, -1, -1},
    {0, 1, 2, 3},
    {0, 1, 3, 4, 2},
    {0, 1, 4, 5, 2, 3},
    {-1, -1, -1, -1, -1, -1, -1},
    {0, 1, 4, 5, 2, 3, 6, 7}
  };

  if (nchannels > 8) {
    GST_DEBUG ("invalid number of channels: %d", nchannels);
    return FALSE;
  }

  /* This uses the default channel mapping from ALSA which
   * is used in quite a few surround test files and seems to be
   * the defacto standard. The channel mapping from
   * WAVE_FORMAT_EXTENSIBLE doesn't seem to be used in normal
   * wav files like chan-id.wav.
   * http://bugzilla.gnome.org/show_bug.cgi?id=489010
   */
  switch (nchannels) {
    case 1:
      /* Mono => nothing */
      if (channel_reorder_map)
        channel_reorder_map[0] = 0;
      return TRUE;
    case 8:
      channel_mask |=
          G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT;
      channel_mask |=
          G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT;
      /* fall through */
    case 6:
      channel_mask |= G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_LFE1;
      /* fall through */
    case 5:
      channel_mask |=
          G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER;
      /* fall through */
    case 4:
      channel_mask |=
          G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT;
      channel_mask |=
          G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_REAR_LEFT;
      /* fall through */
    case 2:
      channel_mask |=
          G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
      channel_mask |=
          G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
      break;
    default:
      return FALSE;
  }

  if (channel_reorder_map)
    memcpy (channel_reorder_map, reorder_maps[nchannels - 1],
        sizeof (gint) * nchannels);

  gst_caps_set_simple (caps, "channel-mask", GST_TYPE_BITMASK, channel_mask,
      NULL);

  return TRUE;
}

static guint32
gst_riff_wavext_get_default_channel_mask (guint nchannels)
{
  guint32 channel_mask = 0;

  /* Set the default channel mask for the given number of channels.
   * http://www.microsoft.com/whdc/device/audio/multichaud.mspx
   */
  switch (nchannels) {
    case 11:
      channel_mask |= 0x00400;
      channel_mask |= 0x00200;
    case 9:
      channel_mask |= 0x00100;
    case 8:
      channel_mask |= 0x00080;
      channel_mask |= 0x00040;
    case 6:
      channel_mask |= 0x00020;
      channel_mask |= 0x00010;
    case 4:
      channel_mask |= 0x00008;
    case 3:
      channel_mask |= 0x00004;
    case 2:
      channel_mask |= 0x00002;
      channel_mask |= 0x00001;
      break;
  }

  return channel_mask;
}

GstCaps *
gst_riff_create_audio_caps (guint16 codec_id,
    gst_riff_strh * strh, gst_riff_strf_auds * strf,
    GstBuffer * strf_data, GstBuffer * strd_data, char **codec_name,
    gint channel_reorder_map[18])
{
  gboolean block_align = FALSE, rate_chan = TRUE;
  GstCaps *caps = NULL;
  gint i;

  if (channel_reorder_map)
    for (i = 0; i < 18; i++)
      channel_reorder_map[i] = -1;

  switch (codec_id) {
    case GST_RIFF_WAVE_FORMAT_PCM:     /* PCM */
      if (strf != NULL) {
        gint ba = strf->blockalign;
        gint ch = strf->channels;
        gint wd, ws;
        GstAudioFormat format;

        if (ba > (32 / 8) * ch) {
          GST_WARNING ("Invalid block align: %d > %d", ba, (32 / 8) * ch);
          wd = GST_ROUND_UP_8 (strf->bits_per_sample);
        } else if (ba != 0) {
          /* If we have an empty blockalign, we take the width contained in 
           * strf->bits_per_sample */
          wd = ba * 8 / ch;
        } else {
          wd = GST_ROUND_UP_8 (strf->bits_per_sample);
        }

        if (strf->bits_per_sample > 32) {
          GST_WARNING ("invalid depth (%d) of pcm audio, overwriting.",
              strf->bits_per_sample);
          strf->bits_per_sample = wd;
        }

        /* in riff, the depth is stored in the size field but it just means that
         * the _least_ significant bits are cleared. We can therefore just play
         * the sample as if it had a depth == width */
        /* For reference, the actual depth is in strf->bits_per_sample */
        ws = wd;

        format =
            gst_audio_format_build_integer (wd != 8, G_LITTLE_ENDIAN, wd, ws);
        if (format == GST_AUDIO_FORMAT_UNKNOWN) {
          GST_WARNING ("Unsupported raw audio format with width %d", wd);
          return NULL;
        }

        caps = gst_caps_new_simple ("audio/x-raw",
            "format", G_TYPE_STRING, gst_audio_format_to_string (format),
            "layout", G_TYPE_STRING, "interleaved",
            "channels", G_TYPE_INT, ch, NULL);

        /* Add default channel layout. We know no default layout for more than
         * 8 channels. */
        if (ch > 8)
          GST_WARNING ("don't know default layout for %d channels", ch);
        else if (gst_riff_wave_add_default_channel_mask (caps, ch,
                channel_reorder_map))
          GST_DEBUG ("using default channel layout for %d channels", ch);
        else
          GST_WARNING ("failed to add channel layout");
      } else {
        /* FIXME: this is pretty useless - we need fixed caps */
        caps = gst_caps_from_string ("audio/x-raw, "
            "format = (string) { S8, U8, S16LE, U16LE, S24LE, "
            "U24LE, S32LE, U32LE }, " "layout = (string) interleaved");
      }
      if (codec_name && strf)
        *codec_name = g_strdup_printf ("Uncompressed %d-bit PCM audio",
            strf->bits_per_sample);
      break;

    case GST_RIFF_WAVE_FORMAT_ADPCM:
      if (strf != NULL) {
        /* Many encoding tools create a wrong bitrate information in the header,
         * so either we calculate the bitrate or mark it as invalid as this
         * would probably confuse timing */
        strf->av_bps = 0;
        if (strf->channels != 0 && strf->rate != 0 && strf->blockalign != 0) {
          int spb = ((strf->blockalign - strf->channels * 7) / 2) * 2;
          strf->av_bps =
              gst_util_uint64_scale_int (strf->rate, strf->blockalign, spb);
          GST_DEBUG ("fixing av_bps to calculated value %d of MS ADPCM",
              strf->av_bps);
        }
      }
      caps = gst_caps_new_simple ("audio/x-adpcm",
          "layout", G_TYPE_STRING, "microsoft", NULL);
      if (codec_name)
        *codec_name = g_strdup ("ADPCM audio");
      block_align = TRUE;
      break;

    case GST_RIFF_WAVE_FORMAT_IEEE_FLOAT:
      if (strf != NULL) {
        gint ba = strf->blockalign;
        gint ch = strf->channels;

        if (ba > 0 && ch > 0 && (ba == (64 / 8) * ch || ba == (32 / 8) * ch)) {
          gint wd = ba * 8 / ch;

          caps = gst_caps_new_simple ("audio/x-raw",
              "format", G_TYPE_STRING, wd == 64 ? "F64LE" : "F32LE",
              "layout", G_TYPE_STRING, "interleaved",
              "channels", G_TYPE_INT, ch, NULL);

          /* Add default channel layout. We know no default layout for more than
           * 8 channels. */
          if (ch > 8)
            GST_WARNING ("don't know default layout for %d channels", ch);
          else if (gst_riff_wave_add_default_channel_mask (caps, ch,
                  channel_reorder_map))
            GST_DEBUG ("using default channel layout for %d channels", ch);
          else
            GST_WARNING ("failed to add channel layout");
        } else {
          GST_WARNING ("invalid block align %d or channel count %d", ba, ch);
          return NULL;
        }
      } else {
        /* FIXME: this is pretty useless - we need fixed caps */
        caps = gst_caps_from_string ("audio/x-raw, "
            "format = (string) { F32LE, F64LE }, "
            "layout = (string) interleaved");
      }
      if (codec_name && strf)
        *codec_name = g_strdup_printf ("Uncompressed %d-bit IEEE float audio",
            strf->bits_per_sample);
      break;

    case GST_RIFF_WAVE_FORMAT_IBM_CVSD:
      goto unknown;

    case GST_RIFF_WAVE_FORMAT_ALAW:
      if (strf != NULL) {
        if (strf->bits_per_sample != 8) {
          GST_WARNING ("invalid depth (%d) of alaw audio, overwriting.",
              strf->bits_per_sample);
          strf->bits_per_sample = 8;
          strf->blockalign = (strf->bits_per_sample * strf->channels) / 8;
          strf->av_bps = strf->blockalign * strf->rate;
        }
        if (strf->av_bps == 0 || strf->blockalign == 0) {
          GST_WARNING ("fixing av_bps (%d) and blockalign (%d) of alaw audio",
              strf->av_bps, strf->blockalign);
          strf->blockalign = (strf->bits_per_sample * strf->channels) / 8;
          strf->av_bps = strf->blockalign * strf->rate;
        }
      }
      caps = gst_caps_new_empty_simple ("audio/x-alaw");
      if (codec_name)
        *codec_name = g_strdup ("A-law audio");
      break;

    case GST_RIFF_WAVE_FORMAT_WMS:
      caps = gst_caps_new_empty_simple ("audio/x-wms");
      if (strf != NULL) {
        gst_caps_set_simple (caps,
            "bitrate", G_TYPE_INT, strf->av_bps * 8,
            "width", G_TYPE_INT, strf->bits_per_sample,
            "depth", G_TYPE_INT, strf->bits_per_sample, NULL);
      } else {
        gst_caps_set_simple (caps,
            "bitrate", GST_TYPE_INT_RANGE, 0, G_MAXINT, NULL);
      }
      if (codec_name)
        *codec_name = g_strdup ("Windows Media Audio Speech");
      block_align = TRUE;
      break;

    case GST_RIFF_WAVE_FORMAT_MULAW:
      if (strf != NULL) {
        if (strf->bits_per_sample != 8) {
          GST_WARNING ("invalid depth (%d) of mulaw audio, overwriting.",
              strf->bits_per_sample);
          strf->bits_per_sample = 8;
          strf->blockalign = (strf->bits_per_sample * strf->channels) / 8;
          strf->av_bps = strf->blockalign * strf->rate;
        }
        if (strf->av_bps == 0 || strf->blockalign == 0) {
          GST_WARNING ("fixing av_bps (%d) and blockalign (%d) of mulaw audio",
              strf->av_bps, strf->blockalign);
          strf->blockalign = (strf->bits_per_sample * strf->channels) / 8;
          strf->av_bps = strf->blockalign * strf->rate;
        }
      }
      caps = gst_caps_new_empty_simple ("audio/x-mulaw");
      if (codec_name)
        *codec_name = g_strdup ("Mu-law audio");
      break;

    case GST_RIFF_WAVE_FORMAT_OKI_ADPCM:
      goto unknown;

    case GST_RIFF_WAVE_FORMAT_DVI_ADPCM:
      if (strf != NULL) {
        /* Many encoding tools create a wrong bitrate information in the
         * header, so either we calculate the bitrate or mark it as invalid
         * as this would probably confuse timing */
        strf->av_bps = 0;
        if (strf->channels != 0 && strf->rate != 0 && strf->blockalign != 0) {
          int spb = ((strf->blockalign - strf->channels * 4) / 2) * 2;
          strf->av_bps =
              gst_util_uint64_scale_int (strf->rate, strf->blockalign, spb);
          GST_DEBUG ("fixing av_bps to calculated value %d of IMA DVI ADPCM",
              strf->av_bps);
        }
      }
      caps = gst_caps_new_simple ("audio/x-adpcm",
          "layout", G_TYPE_STRING, "dvi", NULL);
      if (codec_name)
        *codec_name = g_strdup ("DVI ADPCM audio");
      block_align = TRUE;
      break;

    case GST_RIFF_WAVE_FORMAT_ADPCM_G722:
      caps = gst_caps_new_empty_simple ("audio/G722");
      if (codec_name)
        *codec_name = g_strdup ("G722 audio");
      break;

    case GST_RIFF_WAVE_FORMAT_ITU_G726_ADPCM:
      if (strf != NULL) {
        gint bitrate;
        bitrate = 0;
        if (strf->av_bps == 2000 || strf->av_bps == 3000 || strf->av_bps == 4000
            || strf->av_bps == 5000) {
          strf->blockalign = strf->av_bps / 1000;
          bitrate = strf->av_bps * 8;
        } else if (strf->blockalign >= 2 && strf->blockalign <= 5) {
          bitrate = strf->blockalign * 8000;
        }
        if (bitrate > 0) {
          caps = gst_caps_new_simple ("audio/x-adpcm",
              "layout", G_TYPE_STRING, "g726", "bitrate", G_TYPE_INT, bitrate,
              NULL);
        } else {
          caps = gst_caps_new_simple ("audio/x-adpcm",
              "layout", G_TYPE_STRING, "g726", NULL);
        }
      } else {
        caps = gst_caps_new_simple ("audio/x-adpcm",
            "layout", G_TYPE_STRING, "g726", NULL);
      }
      if (codec_name)
        *codec_name = g_strdup ("G726 ADPCM audio");
      block_align = TRUE;
      break;

    case GST_RIFF_WAVE_FORMAT_DSP_TRUESPEECH:
      caps = gst_caps_new_empty_simple ("audio/x-truespeech");
      if (codec_name)
        *codec_name = g_strdup ("DSP Group TrueSpeech");
      break;

    case GST_RIFF_WAVE_FORMAT_GSM610:
    case GST_RIFF_WAVE_FORMAT_MSN:
      caps = gst_caps_new_empty_simple ("audio/ms-gsm");
      if (codec_name)
        *codec_name = g_strdup ("MS GSM audio");
      break;

    case GST_RIFF_WAVE_FORMAT_MPEGL12: /* mp1 or mp2 */
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-1 layer 2");
      break;

    case GST_RIFF_WAVE_FORMAT_MPEGL3:  /* mp3 */
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 3, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-1 layer 3");
      break;

    case GST_RIFF_WAVE_FORMAT_AMR_NB:  /* amr-nb */
      caps = gst_caps_new_empty_simple ("audio/AMR");
      if (codec_name)
        *codec_name = g_strdup ("AMR Narrow Band (NB)");
      break;

    case GST_RIFF_WAVE_FORMAT_AMR_WB:  /* amr-wb */
      caps = gst_caps_new_empty_simple ("audio/AMR-WB");
      if (codec_name)
        *codec_name = g_strdup ("AMR Wide Band (WB)");
      break;

    case GST_RIFF_WAVE_FORMAT_VORBIS1: /* ogg/vorbis mode 1 */
    case GST_RIFF_WAVE_FORMAT_VORBIS2: /* ogg/vorbis mode 2 */
    case GST_RIFF_WAVE_FORMAT_VORBIS3: /* ogg/vorbis mode 3 */
    case GST_RIFF_WAVE_FORMAT_VORBIS1PLUS:     /* ogg/vorbis mode 1+ */
    case GST_RIFF_WAVE_FORMAT_VORBIS2PLUS:     /* ogg/vorbis mode 2+ */
    case GST_RIFF_WAVE_FORMAT_VORBIS3PLUS:     /* ogg/vorbis mode 3+ */
      caps = gst_caps_new_empty_simple ("audio/x-vorbis");
      if (codec_name)
        *codec_name = g_strdup ("Vorbis");
      break;

    case GST_RIFF_WAVE_FORMAT_A52:
      caps = gst_caps_new_empty_simple ("audio/x-ac3");
      if (codec_name)
        *codec_name = g_strdup ("AC-3 audio");
      break;
    case GST_RIFF_WAVE_FORMAT_DTS:
      caps = gst_caps_new_empty_simple ("audio/x-dts");
      if (codec_name)
        *codec_name = g_strdup ("DTS audio");
      /* wavparse is not always able to specify rate/channels for DTS-in-wav */
      rate_chan = FALSE;
      break;
    case GST_RIFF_WAVE_FORMAT_AAC:
    case GST_RIFF_WAVE_FORMAT_AAC_AC:
    case GST_RIFF_WAVE_FORMAT_AAC_pm:
    {
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-4 AAC audio");
      break;
    }
    case GST_RIFF_WAVE_FORMAT_WMAV1:
    case GST_RIFF_WAVE_FORMAT_WMAV2:
    case GST_RIFF_WAVE_FORMAT_WMAV3:
    case GST_RIFF_WAVE_FORMAT_WMAV3_L:
    {
      gint version = (codec_id - GST_RIFF_WAVE_FORMAT_WMAV1) + 1;

      block_align = TRUE;

      caps = gst_caps_new_simple ("audio/x-wma",
          "wmaversion", G_TYPE_INT, version, NULL);

      if (codec_name) {
        if (codec_id == GST_RIFF_WAVE_FORMAT_WMAV3_L)
          *codec_name = g_strdup ("WMA Lossless");
        else
          *codec_name = g_strdup_printf ("WMA Version %d", version + 6);
      }

      if (strf != NULL) {
        gst_caps_set_simple (caps,
            "bitrate", G_TYPE_INT, strf->av_bps * 8,
            "depth", G_TYPE_INT, strf->bits_per_sample, NULL);
      } else {
        gst_caps_set_simple (caps,
            "bitrate", GST_TYPE_INT_RANGE, 0, G_MAXINT, NULL);
      }
      break;
    }
    case GST_RIFF_WAVE_FORMAT_SONY_ATRAC3:
      caps = gst_caps_new_empty_simple ("audio/x-vnd.sony.atrac3");
      if (codec_name)
        *codec_name = g_strdup ("Sony ATRAC3");
      break;

    case GST_RIFF_WAVE_FORMAT_SIREN:
      caps = gst_caps_new_empty_simple ("audio/x-siren");
      if (codec_name)
        *codec_name = g_strdup ("Siren7");
      rate_chan = FALSE;
      break;

    case GST_RIFF_WAVE_FORMAT_ADPCM_IMA_DK4:
      caps =
          gst_caps_new_simple ("audio/x-adpcm", "layout", G_TYPE_STRING, "dk4",
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("IMA/DK4 ADPCM");
      break;
    case GST_RIFF_WAVE_FORMAT_ADPCM_IMA_DK3:
      caps =
          gst_caps_new_simple ("audio/x-adpcm", "layout", G_TYPE_STRING, "dk3",
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("IMA/DK3 ADPCM");
      break;

    case GST_RIFF_WAVE_FORMAT_ADPCM_IMA_WAV:
      caps =
          gst_caps_new_simple ("audio/x-adpcm", "layout", G_TYPE_STRING, "dvi",
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("IMA/WAV ADPCM");
      break;
    case GST_RIFF_WAVE_FORMAT_EXTENSIBLE:{
      guint16 valid_bits_per_sample;
      guint32 channel_mask;
      guint32 subformat_guid[4];
      GstMapInfo info;
      gsize size;

      if (strf_data == NULL) {
        GST_WARNING ("WAVE_FORMAT_EXTENSIBLE but no strf_data buffer provided");
        return NULL;
      }

      /* should be at least 22 bytes */
      size = gst_buffer_get_size (strf_data);

      if (size < 22) {
        GST_WARNING ("WAVE_FORMAT_EXTENSIBLE data size is %" G_GSIZE_FORMAT
            " (expected: 22)", size);
        return NULL;
      }

      gst_buffer_map (strf_data, &info, GST_MAP_READ);
      valid_bits_per_sample = GST_READ_UINT16_LE (info.data);
      channel_mask = GST_READ_UINT32_LE (info.data + 2);
      subformat_guid[0] = GST_READ_UINT32_LE (info.data + 6);
      subformat_guid[1] = GST_READ_UINT32_LE (info.data + 10);
      subformat_guid[2] = GST_READ_UINT32_LE (info.data + 14);
      subformat_guid[3] = GST_READ_UINT32_LE (info.data + 18);
      gst_buffer_unmap (strf_data, &info);

      GST_DEBUG ("valid bps    = %u", valid_bits_per_sample);
      GST_DEBUG ("channel mask = 0x%08x", channel_mask);
      GST_DEBUG ("GUID         = %08x-%08x-%08x-%08x", subformat_guid[0],
          subformat_guid[1], subformat_guid[2], subformat_guid[3]);

      if (subformat_guid[1] == 0x00100000 &&
          subformat_guid[2] == 0xaa000080 && subformat_guid[3] == 0x719b3800) {
        if (subformat_guid[0] == 0x00000001) {
          GST_DEBUG ("PCM");
          if (strf != NULL && strf->blockalign != 0 && strf->channels != 0
              && strf->rate != 0) {
            gint ba = strf->blockalign;
            gint wd = ba * 8 / strf->channels;
            gint ws;
            GstAudioFormat format;

            /* in riff, the depth is stored in the size field but it just
             * means that the _least_ significant bits are cleared. We can
             * therefore just play the sample as if it had a depth == width */
            ws = wd;

            /* For reference, use this to get the actual depth:
             * ws = strf->bits_per_sample;
             * if (valid_bits_per_sample != 0)
             *   ws = valid_bits_per_sample; */

            format =
                gst_audio_format_build_integer (wd != 8, G_LITTLE_ENDIAN, wd,
                ws);

            caps = gst_caps_new_simple ("audio/x-raw",
                "format", G_TYPE_STRING, gst_audio_format_to_string (format),
                "layout", G_TYPE_STRING, "interleaved",
                "channels", G_TYPE_INT, strf->channels,
                "rate", G_TYPE_INT, strf->rate, NULL);

            if (codec_name) {
              *codec_name = g_strdup_printf ("Uncompressed %d-bit PCM audio",
                  strf->bits_per_sample);
            }
          }
        } else if (subformat_guid[0] == 0x00000003) {
          GST_DEBUG ("FLOAT");
          if (strf != NULL && strf->blockalign != 0 && strf->channels != 0
              && strf->rate != 0) {
            gint ba = strf->blockalign;
            gint wd = ba * 8 / strf->channels;

            caps = gst_caps_new_simple ("audio/x-raw",
                "format", G_TYPE_STRING, wd == 32 ? "F32LE" : "F64LE",
                "layout", G_TYPE_STRING, "interleaved",
                "channels", G_TYPE_INT, strf->channels,
                "rate", G_TYPE_INT, strf->rate, NULL);

            if (codec_name) {
              *codec_name =
                  g_strdup_printf ("Uncompressed %d-bit IEEE float audio",
                  strf->bits_per_sample);
            }
          }
        } else if (subformat_guid[0] == 0x0000006) {
          GST_DEBUG ("ALAW");
          if (strf != NULL) {
            if (strf->bits_per_sample != 8) {
              GST_WARNING ("invalid depth (%d) of alaw audio, overwriting.",
                  strf->bits_per_sample);
              strf->bits_per_sample = 8;
              strf->av_bps = 8;
              strf->blockalign = strf->av_bps * strf->channels;
            }
            if (strf->av_bps == 0 || strf->blockalign == 0) {
              GST_WARNING
                  ("fixing av_bps (%d) and blockalign (%d) of alaw audio",
                  strf->av_bps, strf->blockalign);
              strf->av_bps = strf->bits_per_sample;
              strf->blockalign = strf->av_bps * strf->channels;
            }
          }
          caps = gst_caps_new_empty_simple ("audio/x-alaw");

          if (codec_name)
            *codec_name = g_strdup ("A-law audio");
        } else if (subformat_guid[0] == 0x00000007) {
          GST_DEBUG ("MULAW");
          if (strf != NULL) {
            if (strf->bits_per_sample != 8) {
              GST_WARNING ("invalid depth (%d) of mulaw audio, overwriting.",
                  strf->bits_per_sample);
              strf->bits_per_sample = 8;
              strf->av_bps = 8;
              strf->blockalign = strf->av_bps * strf->channels;
            }
            if (strf->av_bps == 0 || strf->blockalign == 0) {
              GST_WARNING
                  ("fixing av_bps (%d) and blockalign (%d) of mulaw audio",
                  strf->av_bps, strf->blockalign);
              strf->av_bps = strf->bits_per_sample;
              strf->blockalign = strf->av_bps * strf->channels;
            }
          }
          caps = gst_caps_new_empty_simple ("audio/x-mulaw");
          if (codec_name)
            *codec_name = g_strdup ("Mu-law audio");
        } else if (subformat_guid[0] == 0x00000092) {
          GST_DEBUG ("FIXME: handle DOLBY AC3 SPDIF format");
          GST_DEBUG ("WAVE_FORMAT_EXTENSIBLE AC-3 SPDIF audio");
          caps = gst_caps_new_empty_simple ("audio/x-ac3");
          if (codec_name)
            *codec_name = g_strdup ("wavext AC-3 SPDIF audio");
        } else if ((subformat_guid[0] & 0xffff) ==
            GST_RIFF_WAVE_FORMAT_EXTENSIBLE) {
          GST_DEBUG ("WAVE_FORMAT_EXTENSIBLE nested");
        } else {
          /* recurse where no special consideration has yet to be identified 
           * for the subformat guid */
          caps = gst_riff_create_audio_caps (subformat_guid[0], strh, strf,
              strf_data, strd_data, codec_name, channel_reorder_map);
          if (!codec_name)
            GST_DEBUG ("WAVE_FORMAT_EXTENSIBLE audio");
          if (caps) {
            if (codec_name) {
              gchar *tmp = *codec_name;
              GST_DEBUG ("WAVE_FORMAT_EXTENSIBLE %s", *codec_name);
              *codec_name = g_strjoin ("wavext ", tmp, NULL);
              g_free (tmp);
            }
            return caps;
          }
        }
      } else if (subformat_guid[0] == 0x6ba47966 &&
          subformat_guid[1] == 0x41783f83 &&
          subformat_guid[2] == 0xf0006596 && subformat_guid[3] == 0xe59262bf) {
        caps = gst_caps_new_empty_simple ("application/x-ogg-avi");
        if (codec_name)
          *codec_name = g_strdup ("Ogg-AVI");
      }

      if (caps == NULL) {
        GST_WARNING ("Unknown WAVE_FORMAT_EXTENSIBLE audio format");
        return NULL;
      }

      if (strf != NULL) {
        /* If channel_mask == 0 and channels > 1 let's
         * assume default layout as some wav files don't have the
         * channel mask set. Don't set the layout for 1 channel. */
        if (channel_mask == 0 && strf->channels > 1)
          channel_mask =
              gst_riff_wavext_get_default_channel_mask (strf->channels);

        if ((channel_mask != 0 || strf->channels > 1) &&
            !gst_riff_wavext_add_channel_mask (caps, strf->channels,
                channel_mask, channel_reorder_map)) {
          GST_WARNING ("failed to add channel layout");
          gst_caps_unref (caps);
          caps = NULL;
        }
        rate_chan = FALSE;
      }

      break;
    }
      /* can anything decode these? pitfdll? */
    case GST_RIFF_WAVE_FORMAT_VOXWARE_AC8:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_AC10:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_AC16:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_AC20:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_METAVOICE:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_METASOUND:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_RT29HW:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_VR12:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_VR18:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_TQ40:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_TQ60:{
      caps = gst_caps_new_simple ("audio/x-voxware",
          "voxwaretype", G_TYPE_INT, (gint) codec_id, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Voxware");
      break;
    }
    default:
    unknown:
      GST_WARNING ("Unknown audio tag 0x%04x", codec_id);
      return NULL;
  }

  if (strf != NULL) {
    if (rate_chan) {
      gst_caps_set_simple (caps,
          "rate", G_TYPE_INT, strf->rate,
          "channels", G_TYPE_INT, strf->channels, NULL);
    }
    if (block_align) {
      gst_caps_set_simple (caps,
          "block_align", G_TYPE_INT, strf->blockalign, NULL);
    }
  } else {
    if (block_align) {
      gst_caps_set_simple (caps,
          "block_align", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
    }
  }

  /* extradata */
  if (strf_data || strd_data) {
    gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER,
        strf_data ? strf_data : strd_data, NULL);
  }

  return caps;
}

GstCaps *
gst_riff_create_iavs_caps (guint32 codec_fcc,
    gst_riff_strh * strh, gst_riff_strf_iavs * strf,
    GstBuffer * init_data, GstBuffer * extra_data, char **codec_name)
{
  GstCaps *caps = NULL;

  switch (codec_fcc) {
      /* is this correct? */
    case GST_MAKE_FOURCC ('D', 'V', 'S', 'D'):
    case GST_MAKE_FOURCC ('d', 'v', 's', 'd'):
      caps = gst_caps_new_simple ("video/x-dv",
          "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Generic DV");
      break;

    default:
      GST_WARNING ("Unknown IAVS fourcc %" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (codec_fcc));
      return NULL;
  }

  return caps;
}

/*
 * Functions below are for template caps. All is variable.
 */

GstCaps *
gst_riff_create_video_template_caps (void)
{
  static const guint32 tags[] = {
    GST_MAKE_FOURCC ('3', 'I', 'V', '1'),
    GST_MAKE_FOURCC ('A', 'S', 'V', '1'),
    GST_MAKE_FOURCC ('A', 'S', 'V', '2'),
    GST_MAKE_FOURCC ('C', 'F', 'H', 'D'),
    GST_MAKE_FOURCC ('C', 'L', 'J', 'R'),
    GST_MAKE_FOURCC ('C', 'S', 'C', 'D'),
    GST_MAKE_FOURCC ('C', 'Y', 'U', 'V'),
    GST_MAKE_FOURCC ('D', 'I', 'B', ' '),
    GST_MAKE_FOURCC ('D', 'I', 'V', '3'),
    GST_MAKE_FOURCC ('D', 'I', 'V', 'X'),
    GST_MAKE_FOURCC ('D', 'U', 'C', 'K'),
    GST_MAKE_FOURCC ('D', 'V', 'S', 'D'),
    GST_MAKE_FOURCC ('D', 'V', '5', '0'),
    GST_MAKE_FOURCC ('D', 'X', '5', '0'),
    GST_MAKE_FOURCC ('M', '4', 'C', 'C'),
    GST_MAKE_FOURCC ('F', 'L', 'V', '1'),
    GST_MAKE_FOURCC ('F', 'L', 'V', '4'),
    GST_MAKE_FOURCC ('H', '2', '6', '3'),
    GST_MAKE_FOURCC ('V', 'X', '1', 'K'),
    GST_MAKE_FOURCC ('H', '2', '6', '4'),
    GST_MAKE_FOURCC ('H', '2', '6', '5'),
    GST_MAKE_FOURCC ('H', 'F', 'Y', 'U'),
    GST_MAKE_FOURCC ('I', '2', '6', '3'),
    GST_MAKE_FOURCC ('I', '4', '2', '0'),
    GST_MAKE_FOURCC ('I', 'V', '3', '2'),
    GST_MAKE_FOURCC ('I', 'V', '4', '1'),
    GST_MAKE_FOURCC ('I', 'V', '5', '0'),
    GST_MAKE_FOURCC ('L', '2', '6', '3'),
    GST_MAKE_FOURCC ('L', '2', '6', '4'),
    GST_MAKE_FOURCC ('M', '2', '6', '3'),
    GST_MAKE_FOURCC ('M', '4', 'S', '2'),
    GST_MAKE_FOURCC ('M', 'J', 'P', 'G'),
    GST_MAKE_FOURCC ('M', 'P', '4', '2'),
    GST_MAKE_FOURCC ('M', 'P', '4', '3'),
    GST_MAKE_FOURCC ('M', 'P', 'E', 'G'),
    GST_MAKE_FOURCC ('M', 'P', 'G', '2'),
    GST_MAKE_FOURCC ('M', 'P', 'G', '4'),
    GST_MAKE_FOURCC ('M', 'S', 'Z', 'H'),
    GST_MAKE_FOURCC ('P', 'N', 'G', ' '),
    GST_MAKE_FOURCC ('R', 'L', 'E', ' '),
    GST_MAKE_FOURCC ('R', 'T', '2', '1'),
    GST_MAKE_FOURCC ('S', 'P', '5', '3'),
    GST_MAKE_FOURCC ('T', 'M', '2', '0'),
    GST_MAKE_FOURCC ('T', 'S', 'C', 'C'),
    GST_MAKE_FOURCC ('U', 'L', 'T', 'I'),
    GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'),
    GST_MAKE_FOURCC ('V', 'C', 'R', '1'),
    GST_MAKE_FOURCC ('V', 'C', 'R', '2'),
    GST_MAKE_FOURCC ('V', 'D', 'O', 'W'),
    GST_MAKE_FOURCC ('V', 'I', 'V', 'O'),
    GST_MAKE_FOURCC ('V', 'M', 'n', 'c'),
    GST_MAKE_FOURCC ('V', 'P', '3', ' '),
    GST_MAKE_FOURCC ('V', 'S', 'S', 'H'),
    GST_MAKE_FOURCC ('W', 'M', 'V', '1'),
    GST_MAKE_FOURCC ('W', 'M', 'V', '2'),
    GST_MAKE_FOURCC ('W', 'M', 'V', '3'),
    GST_MAKE_FOURCC ('X', 'V', 'I', 'D'),
    GST_MAKE_FOURCC ('X', 'x', 'a', 'n'),
    GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'),
    GST_MAKE_FOURCC ('Y', 'V', 'U', '9'),
    GST_MAKE_FOURCC ('Z', 'L', 'I', 'B'),
    GST_MAKE_FOURCC ('c', 'v', 'i', 'd'),
    GST_MAKE_FOURCC ('h', '2', '6', '4'),
    GST_MAKE_FOURCC ('m', 's', 'v', 'c'),
    GST_MAKE_FOURCC ('x', '2', '6', '3'),
    GST_MAKE_FOURCC ('d', 'r', 'a', 'c'),
    GST_MAKE_FOURCC ('F', 'F', 'V', '1'),
    GST_MAKE_FOURCC ('K', 'M', 'V', 'C'),
    GST_MAKE_FOURCC ('V', 'P', '5', '0'),
    GST_MAKE_FOURCC ('V', 'P', '6', '0'),
    GST_MAKE_FOURCC ('V', 'P', '6', 'F'),
    GST_MAKE_FOURCC ('V', 'P', '7', '0'),
    GST_MAKE_FOURCC ('V', 'P', '8', '0'),
    GST_MAKE_FOURCC ('L', 'M', '2', '0'),
    GST_MAKE_FOURCC ('R', 'P', 'Z', 'A'),
    GST_MAKE_FOURCC ('T', 'H', 'E', 'O'),
    GST_MAKE_FOURCC ('F', 'P', 'S', '1'),
    GST_MAKE_FOURCC ('A', 'A', 'S', 'C'),
    GST_MAKE_FOURCC ('Y', 'V', '1', '2'),
    GST_MAKE_FOURCC ('L', 'O', 'C', 'O'),
    GST_MAKE_FOURCC ('Z', 'M', 'B', 'V'),
    GST_MAKE_FOURCC ('v', '2', '1', '0'),
    GST_MAKE_FOURCC ('r', '2', '1', '0'),
    /* FILL ME */
  };
  guint i;
  GstCaps *caps, *one;

  caps = gst_caps_new_empty ();
  for (i = 0; i < G_N_ELEMENTS (tags); i++) {
    one = gst_riff_create_video_caps (tags[i], NULL, NULL, NULL, NULL, NULL);
    if (one)
      gst_caps_append (caps, one);
  }

  return caps;
}

GstCaps *
gst_riff_create_audio_template_caps (void)
{
  static const guint16 tags[] = {
    GST_RIFF_WAVE_FORMAT_GSM610,
    GST_RIFF_WAVE_FORMAT_MPEGL3,
    GST_RIFF_WAVE_FORMAT_MPEGL12,
    GST_RIFF_WAVE_FORMAT_PCM,
    GST_RIFF_WAVE_FORMAT_VORBIS1,
    GST_RIFF_WAVE_FORMAT_A52,
    GST_RIFF_WAVE_FORMAT_DTS,
    GST_RIFF_WAVE_FORMAT_AAC,
    GST_RIFF_WAVE_FORMAT_ALAW,
    GST_RIFF_WAVE_FORMAT_MULAW,
    GST_RIFF_WAVE_FORMAT_WMS,
    GST_RIFF_WAVE_FORMAT_ADPCM,
    GST_RIFF_WAVE_FORMAT_DVI_ADPCM,
    GST_RIFF_WAVE_FORMAT_DSP_TRUESPEECH,
    GST_RIFF_WAVE_FORMAT_WMAV1,
    GST_RIFF_WAVE_FORMAT_WMAV2,
    GST_RIFF_WAVE_FORMAT_WMAV3,
    GST_RIFF_WAVE_FORMAT_SONY_ATRAC3,
    GST_RIFF_WAVE_FORMAT_IEEE_FLOAT,
    GST_RIFF_WAVE_FORMAT_VOXWARE_METASOUND,
    GST_RIFF_WAVE_FORMAT_ADPCM_IMA_DK4,
    GST_RIFF_WAVE_FORMAT_ADPCM_IMA_DK3,
    GST_RIFF_WAVE_FORMAT_ADPCM_IMA_WAV,
    GST_RIFF_WAVE_FORMAT_AMR_NB,
    GST_RIFF_WAVE_FORMAT_AMR_WB,
    GST_RIFF_WAVE_FORMAT_SIREN,
    /* FILL ME */
  };
  guint i;
  GstCaps *caps, *one;

  caps = gst_caps_new_empty ();
  for (i = 0; i < G_N_ELEMENTS (tags); i++) {
    one =
        gst_riff_create_audio_caps (tags[i], NULL, NULL, NULL, NULL, NULL,
        NULL);
    if (one)
      gst_caps_append (caps, one);
  }
  one = gst_caps_new_empty_simple ("application/x-ogg-avi");
  gst_caps_append (caps, one);

  return caps;
}

GstCaps *
gst_riff_create_iavs_template_caps (void)
{
  static const guint32 tags[] = {
    GST_MAKE_FOURCC ('D', 'V', 'S', 'D')
        /* FILL ME */
  };
  guint i;
  GstCaps *caps, *one;

  caps = gst_caps_new_empty ();
  for (i = 0; i < G_N_ELEMENTS (tags); i++) {
    one = gst_riff_create_iavs_caps (tags[i], NULL, NULL, NULL, NULL, NULL);
    if (one)
      gst_caps_append (caps, one);
  }

  return caps;
}

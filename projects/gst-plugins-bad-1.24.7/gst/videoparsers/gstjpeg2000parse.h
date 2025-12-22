/* GStreamer JPEG 2000 Parser
 * Copyright (C) <2016-2017> Grok Image Compression Inc.
 *  @author Aaron Boxer <boxerab@gmail.com>
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

#ifndef __GST_JPEG2000_PARSE_H__
#define __GST_JPEG2000_PARSE_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstbaseparse.h>
#include <gst/codecparsers/gstjpeg2000sampling.h>

G_BEGIN_DECLS
#define GST_TYPE_JPEG2000_PARSE \
  (gst_jpeg2000_parse_get_type())
#define GST_JPEG2000_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_JPEG2000_PARSE,GstJPEG2000Parse))
#define GST_JPEG2000_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_JPEG2000_PARSE,GstJPEG2000ParseClass))
#define GST_IS_JPEG2000_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_JPEG2000_PARSE))
#define GST_IS_JPEG2000_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_JPEG2000_PARSE))
    GType gst_jpeg2000_parse_get_type (void);

typedef struct _GstJPEG2000Parse GstJPEG2000Parse;
typedef struct _GstJPEG2000ParseClass GstJPEG2000ParseClass;


/**
 * JPEG 2000 Profiles (stored in rsiz/capabilities field in code stream header)
 * See Table A.10 from 15444-1 (updated in various AMDs)
 *
 * For broadcast profiles, the GST_JPEG2000_PARSE_PROFILE_BC_XXXX profile value must be combined with the target
 * main level (3-0 LSBs, with value between 0 and 11).
 * Example:
 * capabilities  GST_JPEG2000_PARSE_PROFILE_BC_MULTI | 0x0005 (in this case, main level equals 5)
 *
 * For IMF profiles, the GST_JPEG2000_PARSE_PROFILE_IMF_XXXX profile value must be combined with the target main level
 * (3-0 LSBs, with value between 0 and 11), and target sub level (7-4 LSBs, with value between 0 and 9).
 * Example:
 * capabilities  GST_JPEG2000_PARSE_PROFILE_IMF_2K | 0x0040 | 0x0005 (in this case, main level equals 5 and sub level equals 4)
 *
 *
 * Broadcast main level (15444-1 AMD4,AMD8)
 *
 * Note: Mbit/s == 10^6 bits/s;  Msamples/s == 10^6 samples/s
 *
 * Level 0: no max rate
 * Level 1:	200 Mbits/s, 65  Msamples/s
 * Level 2:	200 Mbits/s, 130 Msamples/s
 * Level 3:	200 Mbits/s, 195 Msamples/s
 * Level 4:	400 Mbits/s, 260 Msamples/s
 * Level 5:	800Mbits/s,  520 Msamples/s
 * Level >= 6: 2^(Level-6) * 1600 Mbits/s, 2^(Level-6) * 1200 Msamples/s
 *
 * Broadcast tiling
 *
 * Either single-tile or multi-tile. Multi-tile only permits
 * 1 or 4 tiles per frame, where multiple tiles have identical
 * sizes, and are configured in either 2x2 or 1x4 layout.
 *
 * */

#define GST_JPEG2000_PARSE_PROFILE_NONE        0x0000 /** no profile -  defined in 15444-1 */
#define GST_JPEG2000_PARSE_PROFILE_0           0x0001 /** Profile 0 - defined in  15444-1,Table A.45 */
#define GST_JPEG2000_PARSE_PROFILE_1           0x0002 /** Profile 1 - defined in 15444-1,Table A.45 */
#define GST_JPEG2000_PARSE_PROFILE_CINEMA_2K   0x0003 /** 2K Cinema profile - defined in 15444-1 AMD1 */
#define GST_JPEG2000_PARSE_PROFILE_CINEMA_4K   0x0004 /** 4K Cinema profile - defined in 15444-1 AMD1 */
#define GST_JPEG2000_PARSE_PROFILE_CINEMA_S2K  0x0005 /** Scalable 2K Cinema profile - defined in 15444-1 AMD2 */
#define GST_JPEG2000_PARSE_PROFILE_CINEMA_S4K  0x0006 /** Scalable 4K Cinema profile - defined in 15444-1 AMD2 */
#define GST_JPEG2000_PARSE_PROFILE_CINEMA_LTS  0x0007/** Long Term Storage Cinema profile - defined in 15444-1 AMD2 */
#define GST_JPEG2000_PARSE_PROFILE_BC_SINGLE   0x0100 /** Single Tile Broadcast profile - defined in 15444-1 AMD3 */
#define GST_JPEG2000_PARSE_PROFILE_BC_MULTI    0x0200 /** Multi Tile Broadcast profile - defined in 15444-1 AMD3 */
#define GST_JPEG2000_PARSE_PROFILE_BC_MULTI_R  0x0300 /** Multi Tile Reversible Broadcast profile - defined in 15444-1 AMD3 */
#define GST_JPEG2000_PARSE_PROFILE_BC_MASK	 	0x0F0F /** Mask for broadcast profile, including main level */
#define GST_JPEG2000_PARSE_PROFILE_IMF_2K      0x0400 /** 2K Single Tile Lossy IMF profile - defined in 15444-1 AMD 8 */
#define GST_JPEG2000_PARSE_PROFILE_IMF_4K      0x0401 /** 4K Single Tile Lossy IMF profile - defined in 15444-1 AMD 8 */
#define GST_JPEG2000_PARSE_PROFILE_IMF_8K      0x0402 /** 8K Single Tile Lossy IMF profile - defined in 15444-1 AMD 8 */
#define GST_JPEG2000_PARSE_PROFILE_IMF_2K_R    0x0403 /** 2K Single/Multi Tile Reversible IMF profile - defined in 15444-1 AMD 8 */
#define GST_JPEG2000_PARSE_PROFILE_IMF_4K_R    0x0800 /** 4K Single/Multi Tile Reversible IMF profile - defined in 15444-1 AMD 8 */
#define GST_JPEG2000_PARSE_PROFILE_IMF_8K_R    0x0801 /** 8K Single/Multi Tile Reversible IMF profile - defined in 15444-1 AMD 8 */
#define GST_JPEG2000_PARSE_PROFILE_MASK 	   0xBFFF  /** Mask for profile bits */
#define GST_JPEG2000_PARSE_PROFILE_PART2 	 	0x8000 /** At least 1 extension defined in 15444-2 (Part-2) */

#define GST_JPEG2000_PARSE_MAX_SUPPORTED_COMPONENTS  4

typedef enum
{
  GST_JPEG2000_PARSE_NO_CODEC,
  GST_JPEG2000_PARSE_JPC,       /* jpeg 2000 code stream */
  GST_JPEG2000_PARSE_J2C,       /* jpeg 2000 contiguous code stream box plus code stream */
  GST_JPEG2000_PARSE_JP2,       /* jpeg 2000 part I file format */

} GstJPEG2000ParseFormats;


struct _GstJPEG2000Parse
{
  GstBaseParse baseparse;


  guint width;
  guint height;

  GstJPEG2000Sampling sampling;
  GstJPEG2000Colorspace colorspace;
  GstJPEG2000ParseFormats src_codec_format;
  GstJPEG2000ParseFormats sink_codec_format;
  gboolean parsed_j2c_box;
  guint frame_size;
};

struct _GstJPEG2000ParseClass
{
  GstBaseParseClass parent_class;


};


G_END_DECLS
#endif

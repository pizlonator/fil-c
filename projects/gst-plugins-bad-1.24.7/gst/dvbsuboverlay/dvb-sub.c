/* dvb-sub.c - DVB subtitle decoding
 * Copyright (C) Mart Raudsepp 2009 <mart.raudsepp@artecdesign.ee>
 * Copyright (C) 2010 ONELAN Ltd.
 * 
 * Heavily uses code algorithms ported from ffmpeg's libavcodec/dvbsubdec.c,
 * especially the segment parsers. The original license applies to this
 * ported code and the whole code in this file as well.
 *
 * Original copyright information follows:
 */
/*
 * DVB subtitle decoding for ffmpeg
 * Copyright (c) 2005 Ian Caulfield
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>             /* memset */
#include <gst/gstutils.h>       /* GST_READ_UINT16_BE */
#include <gst/base/gstbitreader.h>      /* GstBitReader */

#include "dvb-sub.h"

GST_DEBUG_CATEGORY_STATIC (dvbsub_debug);
#define GST_CAT_DEFAULT dvbsub_debug

static void dvb_sub_init (void);

/* FIXME: Are we waiting for an acquisition point before trying to do things? */
/* FIXME: In the end convert some of the guint8/16 (especially stack variables) back to gint for access efficiency */

/**
 * SECTION:dvb-sub
 * @title: GstDvbSub
 * @short_description: a DVB subtitle parsing class
 * @stability: Unstable
 *
 * The #DvbSub represents an object used for parsing a DVB subpicture,
 * and signalling the API user for new bitmaps to show on screen.
 */

#define AYUV(y,u,v,a) (((a) << 24) | ((y) << 16) | ((u) << 8) | (v))
#define RGBA_TO_AYUV(r,g,b,a) ((((guint32)(a)) << 24) | ((rgb_to_y(r,g,b)) << 16) | ((rgb_to_u(r,g,b)) << 8) | (rgb_to_v(r,g,b)))


typedef struct DVBSubCLUT
{
  int id;                       /* default_clut uses -1 for this, so guint8 isn't fine without adaptations first */

  guint32 clut4[4];
  guint32 clut16[16];
  guint32 clut256[256];

  struct DVBSubCLUT *next;
} DVBSubCLUT;

static DVBSubCLUT default_clut;

typedef struct DVBSubObjectDisplay
{
  /* FIXME: Use more correct sizes */
  int object_id;
  int region_id;

  int x_pos;
  int y_pos;

  int fgcolor;
  int bgcolor;

  /* FIXME: Should we use GSList? The relating interaction and pointer assignment is quite complex and perhaps unsuited for a plain GSList anyway */
  struct DVBSubObjectDisplay *region_list_next;
  struct DVBSubObjectDisplay *object_list_next;
} DVBSubObjectDisplay;

typedef struct DVBSubObject
{
  /* FIXME: Use more correct sizes */
  int id;                       /* FIXME: Use guint8 after checking it's fine in all code using it */

  int type;

  /* FIXME: Should we use GSList? */
  DVBSubObjectDisplay *display_list;
  struct DVBSubObject *next;
} DVBSubObject;

typedef struct DVBSubRegionDisplay
{                               /* FIXME: Figure out if this structure is only used temporarily in page_segment parser, or also more */
  int region_id;

  int x_pos;
  int y_pos;

  struct DVBSubRegionDisplay *next;
} DVBSubRegionDisplay;

typedef struct DVBSubRegion
{
  guint8 id;
  guint16 width;
  guint16 height;
  guint8 depth;                 /* If we want to make this a guint8, then need to ensure it isn't wrap around with reserved values in region handling code */

  guint8 clut;
  guint8 bgcolor;

  /* FIXME: Validate these fields existence and exact types */
  guint8 *pbuf;
  int buf_size;

  DVBSubObjectDisplay *display_list;

  struct DVBSubRegion *next;
} DVBSubRegion;

struct _DvbSub
{
  DvbSubCallbacks callbacks;
  gpointer user_data;

  guint8 page_time_out;
  DVBSubRegion *region_list;
  DVBSubCLUT *clut_list;
  DVBSubObject *object_list;
  /* FIXME... */
  int display_list_size;
  DVBSubRegionDisplay *display_list;
  GString *pes_buffer;
  DVBSubtitleWindow display_def;
};

typedef enum
{
  TOP_FIELD = 0,
  BOTTOM_FIELD = 1
} DvbSubPixelDataSubBlockFieldType;

static inline gint
rgb_to_y (gint r, gint g, gint b)
{
  gint ret;

  ret = (gint) (((19595 * r) >> 16) + ((38470 * g) >> 16) + ((7471 * b) >> 16));
  ret = CLAMP (ret, 0, 255);
  return ret;
}

static inline gint
rgb_to_u (gint r, gint g, gint b)
{
  gint ret;

  ret =
      (gint) (-((11059 * r) >> 16) - ((21709 * g) >> 16) + ((32768 * b) >> 16) +
      128);
  ret = CLAMP (ret, 0, 255);
  return ret;
}

static inline gint
rgb_to_v (gint r, gint g, gint b)
{
  gint ret;

  ret =
      (gint) (((32768 * r) >> 16) - ((27439 * g) >> 16) - ((5329 * b) >> 16) +
      128);
  ret = CLAMP (ret, 0, 255);
  return ret;
}

static DVBSubObject *
get_object (DvbSub * dvb_sub, guint16 object_id)
{
  DVBSubObject *ptr = dvb_sub->object_list;

  while (ptr && ptr->id != object_id) {
    ptr = ptr->next;
  }

  return ptr;
}

static DVBSubCLUT *
get_clut (DvbSub * dvb_sub, gint clut_id)
{
  DVBSubCLUT *ptr = dvb_sub->clut_list;

  while (ptr && ptr->id != clut_id) {
    ptr = ptr->next;
  }

  return ptr;
}

static DVBSubRegion *
get_region (DvbSub * dvb_sub, guint8 region_id)
{
  DVBSubRegion *ptr = dvb_sub->region_list;

  while (ptr && ptr->id != region_id) {
    ptr = ptr->next;
  }

  return ptr;
}

static void
delete_region_display_list (DvbSub * dvb_sub, DVBSubRegion * region)
{
  DVBSubObject *object, *obj2;
  DVBSubObject **obj2_ptr;
  DVBSubObjectDisplay *display, *obj_disp, **obj_disp_ptr;

  while (region->display_list) {
    display = region->display_list;

    object = get_object (dvb_sub, display->object_id);

    if (object) {
      obj_disp_ptr = &object->display_list;
      obj_disp = *obj_disp_ptr;

      while (obj_disp && obj_disp != display) {
        obj_disp_ptr = &obj_disp->object_list_next;
        obj_disp = *obj_disp_ptr;
      }

      if (obj_disp) {
        *obj_disp_ptr = obj_disp->object_list_next;

        if (!object->display_list) {
          obj2_ptr = (DVBSubObject **) & dvb_sub->object_list;  /* FIXME: Evil casting */
          obj2 = *obj2_ptr;

          while (obj2 != object) {
            g_assert (obj2);
            obj2_ptr = &obj2->next;
            obj2 = *obj2_ptr;
          }

          *obj2_ptr = obj2->next;

          g_free (obj2);
        }
      }
    }

    region->display_list = display->region_list_next;

    g_free (display);
  }
}

static void
dvb_sub_free_clut_list (DVBSubCLUT * list)
{
  DVBSubCLUT *next;

  while (list != NULL) {
    next = list->next;
    g_free (list);
    list = next;
  }
}

static void
delete_state (DvbSub * dvb_sub)
{
  DVBSubRegion *region;

  while (dvb_sub->region_list) {
    region = dvb_sub->region_list;

    dvb_sub->region_list = region->next;

    delete_region_display_list (dvb_sub, region);
    g_free (region->pbuf);

    g_free (region);
  }

  dvb_sub_free_clut_list (dvb_sub->clut_list);
  dvb_sub->clut_list = NULL;

  /* Should already be NULL */
  g_warn_if_fail (dvb_sub->object_list == NULL);
}

static void
dvb_sub_init (void)
{
  int i, r, g, b, a = 0;

  GST_DEBUG_CATEGORY_INIT (dvbsub_debug, "dvbsub", 0, "dvbsuboverlay parser");

  /* Initialize the static default_clut structure, from which other clut
   * structures are initialized from (to start off with default CLUTs
   * as defined in the specification). */
  default_clut.id = -1;

  default_clut.clut4[0] = RGBA_TO_AYUV (0, 0, 0, 0);
  default_clut.clut4[1] = RGBA_TO_AYUV (255, 255, 255, 255);
  default_clut.clut4[2] = RGBA_TO_AYUV (0, 0, 0, 255);
  default_clut.clut4[3] = RGBA_TO_AYUV (127, 127, 127, 255);

  default_clut.clut16[0] = RGBA_TO_AYUV (0, 0, 0, 0);
  for (i = 1; i < 16; i++) {
    if (i < 8) {
      r = (i & 1) ? 255 : 0;
      g = (i & 2) ? 255 : 0;
      b = (i & 4) ? 255 : 0;
    } else {
      r = (i & 1) ? 127 : 0;
      g = (i & 2) ? 127 : 0;
      b = (i & 4) ? 127 : 0;
    }
    default_clut.clut16[i] = RGBA_TO_AYUV (r, g, b, 255);
  }

  default_clut.clut256[0] = RGBA_TO_AYUV (0, 0, 0, 0);
  for (i = 1; i < 256; i++) {
    if (i < 8) {
      r = (i & 1) ? 255 : 0;
      g = (i & 2) ? 255 : 0;
      b = (i & 4) ? 255 : 0;
      a = 63;
    } else {
      switch (i & 0x88) {
        case 0x00:
          r = ((i & 1) ? 85 : 0) + ((i & 0x10) ? 170 : 0);
          g = ((i & 2) ? 85 : 0) + ((i & 0x20) ? 170 : 0);
          b = ((i & 4) ? 85 : 0) + ((i & 0x40) ? 170 : 0);
          a = 255;
          break;
        case 0x08:
          r = ((i & 1) ? 85 : 0) + ((i & 0x10) ? 170 : 0);
          g = ((i & 2) ? 85 : 0) + ((i & 0x20) ? 170 : 0);
          b = ((i & 4) ? 85 : 0) + ((i & 0x40) ? 170 : 0);
          a = 127;
          break;
        case 0x80:
          r = 127 + ((i & 1) ? 43 : 0) + ((i & 0x10) ? 85 : 0);
          g = 127 + ((i & 2) ? 43 : 0) + ((i & 0x20) ? 85 : 0);
          b = 127 + ((i & 4) ? 43 : 0) + ((i & 0x40) ? 85 : 0);
          a = 255;
          break;
        case 0x88:
          r = ((i & 1) ? 43 : 0) + ((i & 0x10) ? 85 : 0);
          g = ((i & 2) ? 43 : 0) + ((i & 0x20) ? 85 : 0);
          b = ((i & 4) ? 43 : 0) + ((i & 0x40) ? 85 : 0);
          a = 255;
          break;
      }
    }
    default_clut.clut256[i] = RGBA_TO_AYUV (r, g, b, a);
  }
}

static void
_dvb_sub_parse_page_segment (DvbSub * dvb_sub, guint16 page_id, guint8 * buf,
    gint buf_size)
{                               /* FIXME: Use guint for buf_size here and in many other places? */
  DVBSubRegionDisplay *display;
  DVBSubRegionDisplay *tmp_display_list, **tmp_ptr;

  const guint8 *buf_end = buf + buf_size;
  guint8 region_id;
  guint8 page_state;

  if (buf_size < 1)
    return;

  dvb_sub->page_time_out = *buf++;
  page_state = ((*buf++) >> 2) & 3;

#ifndef GST_DISABLE_GST_DEBUG
  {
    static const gchar *page_state_str[4] = {
      "Normal case", "ACQUISITION POINT", "Mode Change", "RESERVED"
    };

    GST_DEBUG ("PAGE: page_id = %u, length = %d, page_time_out = %u secs, "
        "page_state = %s", page_id, buf_size, dvb_sub->page_time_out,
        page_state_str[page_state]);
  }
#endif

  if (page_state == 2) {        /* Mode change */
    delete_state (dvb_sub);
  }

  tmp_display_list = dvb_sub->display_list;
  dvb_sub->display_list = NULL;
  dvb_sub->display_list_size = 0;

  while (buf + 5 < buf_end) {
    region_id = *buf++;
    buf += 1;

    display = tmp_display_list;
    tmp_ptr = &tmp_display_list;

    while (display && display->region_id != region_id) {
      tmp_ptr = &display->next;
      display = display->next;
    }

    if (!display)
      display = g_new0 (DVBSubRegionDisplay, 1);

    display->region_id = region_id;

    display->x_pos = GST_READ_UINT16_BE (buf);
    buf += 2;
    display->y_pos = GST_READ_UINT16_BE (buf);
    buf += 2;

    *tmp_ptr = display->next;

    display->next = dvb_sub->display_list;
    dvb_sub->display_list = display;
    dvb_sub->display_list_size++;

    GST_LOG ("PAGE: REGION information: ID = %u, address = %ux%u", region_id,
        display->x_pos, display->y_pos);
  }

  while (tmp_display_list) {
    display = tmp_display_list;

    tmp_display_list = display->next;

    g_free (display);
  }
}

static void
_dvb_sub_parse_region_segment (DvbSub * dvb_sub, guint16 page_id, guint8 * buf,
    gint buf_size)
{
  const guint8 *buf_end = buf + buf_size;
  guint8 region_id;
  guint16 object_id;
  DVBSubRegion *region;
  DVBSubObject *object;
  DVBSubObjectDisplay *object_display;
  gboolean fill;

  if (buf_size < 10)
    return;

  region_id = *buf++;

  region = get_region (dvb_sub, region_id);

  if (!region) {                /* Create a new region */
    region = g_new0 (DVBSubRegion, 1);
    region->id = region_id;
    region->next = dvb_sub->region_list;
    dvb_sub->region_list = region;
  }

  fill = ((*buf++) >> 3) & 1;

  region->width = GST_READ_UINT16_BE (buf);
  buf += 2;
  region->height = GST_READ_UINT16_BE (buf);
  buf += 2;

  if (region->width * region->height != region->buf_size) {     /* FIXME: Read closer from spec what happens when dimensions change */
    g_free (region->pbuf);

    region->buf_size = region->width * region->height;

    region->pbuf = g_malloc (region->buf_size);

    fill = 1;                   /* FIXME: Validate from spec that fill is forced on (in the following codes context) when dimensions change */
  }

  region->depth = 1 << (((*buf++) >> 2) & 7);
  if (region->depth < 2 || region->depth > 8) {
    GST_WARNING ("region depth %d is invalid", region->depth);
    region->depth = 4;          /* FIXME: Check from spec this is the default? */
  }

  region->clut = *buf++;

  if (region->depth == 8) {
    region->bgcolor = *buf++;
    buf += 1;                   /* Skip undefined 4-bit and 2-bit field */
  } else {
    buf += 1;

    if (region->depth == 4)
      region->bgcolor = (((*buf++) >> 4) & 15);
    else
      region->bgcolor = (((*buf++) >> 2) & 3);
  }

  GST_DEBUG ("REGION: id = %u, (%ux%u)@%u-bit", region_id, region->width,
      region->height, region->depth);

  if (fill) {
    memset (region->pbuf, region->bgcolor, region->buf_size);
    GST_DEBUG ("REGION: filling region (%u) with bgcolor = %u", region->id,
        region->bgcolor);
  }

  delete_region_display_list (dvb_sub, region); /* Delete the region display list for current region - FIXME: why? */

  while (buf + 6 <= buf_end) {
    object_id = GST_READ_UINT16_BE (buf);
    buf += 2;

    object = get_object (dvb_sub, object_id);

    if (!object) {
      object = g_new0 (DVBSubObject, 1);

      object->id = object_id;

      object->next = dvb_sub->object_list;
      dvb_sub->object_list = object;
    }

    object->type = (*buf) >> 6;

    object_display = g_new0 (DVBSubObjectDisplay, 1);

    object_display->object_id = object_id;
    object_display->region_id = region_id;

    object_display->x_pos = GST_READ_UINT16_BE (buf) & 0xfff;
    buf += 2;
    object_display->y_pos = GST_READ_UINT16_BE (buf) & 0xfff;
    buf += 2;

    if ((object->type == 1 || object->type == 2) && buf + 2 <= buf_end) {
      object_display->fgcolor = *buf++;
      object_display->bgcolor = *buf++;
    }

    object_display->region_list_next = region->display_list;
    region->display_list = object_display;

    object_display->object_list_next = object->display_list;
    object->display_list = object_display;

    GST_DEBUG ("REGION DATA: object_id = %u, region_id = %u, pos = %ux%u, "
        "obj_type = %u", object->id, region->id, object_display->x_pos,
        object_display->y_pos, object->type);

    if (object->type == 1 || object->type == 2) {
      GST_DEBUG ("REGION DATA: fgcolor = %u, bgcolor = %u",
          object_display->fgcolor, object_display->bgcolor);
    }
  }
}

static void
_dvb_sub_parse_clut_segment (DvbSub * dvb_sub, guint16 page_id, guint8 * buf,
    gint buf_size)
{
  const guint8 *buf_end = buf + buf_size;
  guint8 clut_id;
  DVBSubCLUT *clut;
  int entry_id, depth, full_range;
  int y, cr, cb, alpha;

  GST_MEMDUMP ("DVB clut packet", buf, buf_size);

  clut_id = *buf++;
  buf += 1;

  clut = get_clut (dvb_sub, clut_id);

  if (!clut) {
    clut = g_new (DVBSubCLUT, 1);

    memcpy (clut, &default_clut, sizeof (DVBSubCLUT));

    clut->id = clut_id;

    clut->next = dvb_sub->clut_list;
    dvb_sub->clut_list = clut;
  }

  while (buf + 4 < buf_end) {
    entry_id = *buf++;

    depth = (*buf) & 0xe0;

    if (depth == 0) {
      GST_WARNING ("Invalid clut depth 0x%x!", *buf);
      return;
    }

    full_range = (*buf++) & 1;

    if (full_range) {
      y = *buf++;
      cr = *buf++;
      cb = *buf++;
      alpha = *buf++;
    } else {
      y = buf[0] & 0xfc;
      cr = (((buf[0] & 3) << 2) | ((buf[1] >> 6) & 3)) << 4;
      cb = (buf[1] << 2) & 0xf0;
      alpha = (buf[1] << 6) & 0xc0;

      buf += 2;
    }

    if (y == 0)
      alpha = 0xff;

    GST_DEBUG ("CLUT DEFINITION: clut %d := (%d,%d,%d,%d)", entry_id, y, cb, cr,
        alpha);

    if (depth & 0x80)
      clut->clut4[entry_id] = AYUV (y, cb, cr, 255 - alpha);
    if (depth & 0x40)
      clut->clut16[entry_id] = AYUV (y, cb, cr, 255 - alpha);
    if (depth & 0x20)
      clut->clut256[entry_id] = AYUV (y, cb, cr, 255 - alpha);
  }
}

// FFMPEG-FIXME: The same code in ffmpeg is much more complex, it could use the same
// FFMPEG-FIXME: refactoring as done here
static int
_dvb_sub_read_2bit_string (guint8 * destbuf, gint dbuf_len,
    const guint8 ** srcbuf, gint buf_size, guint8 non_mod, guint8 * map_table)
{
  GstBitReader gb = GST_BIT_READER_INIT (*srcbuf, buf_size);
  /* FIXME: Handle FALSE returns from gst_bit_reader_get_* calls? */

  gboolean stop_parsing = FALSE;
  guint32 bits = 0;
  guint32 pixels_read = 0;

  GST_TRACE ("dbuf_len = %d", dbuf_len);

  /* Need at least 2 bits remaining */
  while (!stop_parsing && (gst_bit_reader_get_remaining (&gb) > 1)) {
    guint run_length = 0, clut_index = 0;

    bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 2);

    if (bits) {                 /* 2-bit_pixel-code */
      run_length = 1;
      clut_index = bits;
    } else {                    /* 2-bit_zero */
      bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 1);
      if (bits == 1) {          /* switch_1 == '1' */
        run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 3);
        run_length += 3;
        clut_index = gst_bit_reader_get_bits_uint32_unchecked (&gb, 2);
      } else {                  /* switch_1 == '0' */
        bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 1);
        if (bits == 1) {        /* switch_2 == '1' */
          run_length = 1;       /* 1x pseudo-colour '00' */
        } else {                /* switch_2 == '0' */
          bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 2);
          switch (bits) {       /* switch_3 */
            case 0x0:          /* end of 2-bit/pixel_code_string */
              stop_parsing = TRUE;
              break;
            case 0x1:          /* two pixels shall be set to pseudo colour (entry) '00' */
              run_length = 2;
              break;
            case 0x2:          /* the following 6 bits contain run length coded pixel data */
              run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 4);
              run_length += 12;
              clut_index = gst_bit_reader_get_bits_uint32_unchecked (&gb, 2);
              break;
            case 0x3:          /* the following 10 bits contain run length coded pixel data */
              run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 8);
              run_length += 29;
              clut_index = gst_bit_reader_get_bits_uint32_unchecked (&gb, 2);
              break;
          }
        }
      }
    }

    /* If run_length is zero, continue. Only case happening is when
     * stop_parsing is TRUE too, so next cycle shouldn't run */
    if (run_length == 0)
      continue;

    /* Trim the run_length to not go beyond the line end and consume
     * it from remaining length of dest line */
    run_length = MIN (run_length, dbuf_len);
    dbuf_len -= run_length;

    /* Make clut_index refer to the index into the desired bit depths
     * CLUT definition table */
    if (map_table)
      clut_index = map_table[clut_index];       /* now clut_index signifies the index into map_table dest */

    /* Now we can simply memset run_length count of destination bytes
     * to clut_index, but only if not non_modifying */
    GST_TRACE ("RUNLEN: setting %u pixels to color 0x%x in destination buffer, "
        "dbuf_len left is %d pixels", run_length, clut_index, dbuf_len);

    if (!(non_mod == 1 && clut_index == 1))
      memset (destbuf, clut_index, run_length);

    destbuf += run_length;
    pixels_read += run_length;
  }

  // FIXME: Test skip_to_byte instead of adding 7 bits, once everything else is working good
  //gst_bit_reader_skip_to_byte (&gb);
  *srcbuf += (gst_bit_reader_get_pos (&gb) + 7) >> 3;

  GST_TRACE ("PIXEL: returning, read %u pixels", pixels_read);
  // FIXME: Shouldn't need this variable if tracking things in the loop better
  return pixels_read;
}

// FFMPEG-FIXME: The same code in ffmpeg is much more complex, it could use the same
// FFMPEG-FIXME: refactoring as done here, explained in commit 895296c3
static int
_dvb_sub_read_4bit_string (guint8 * destbuf, gint dbuf_len,
    const guint8 ** srcbuf, gint buf_size, guint8 non_mod, guint8 * map_table)
{
  GstBitReader gb = GST_BIT_READER_INIT (*srcbuf, buf_size);
  /* FIXME: Handle FALSE returns from gst_bit_reader_get_* calls? */
  gboolean stop_parsing = FALSE;
  guint32 bits = 0;
  guint32 pixels_read = 0;

  GST_TRACE ("RUNLEN: srcbuf position %p, buf_size = %d; destination buffer "
      "size is %d @ %p", *srcbuf, buf_size, dbuf_len, destbuf);

  /* Need at least 4 bits */
  while (!stop_parsing && (gst_bit_reader_get_remaining (&gb) > 3)) {
    guint run_length = 0, clut_index = 0;

    bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 4);

    if (bits) {
      run_length = 1;
      clut_index = bits;
    } else {
      bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 1);
      if (bits == 0) {          /* switch_1 == '0' */
        run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 3);
        if (!run_length) {
          stop_parsing = TRUE;
        } else {
          run_length += 2;
        }
      } else {                  /* switch_1 == '1' */
        bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 1);
        if (bits == 0) {        /* switch_2 == '0' */
          run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 2);
          run_length += 4;
          clut_index = gst_bit_reader_get_bits_uint32_unchecked (&gb, 4);
        } else {                /* switch_2 == '1' */
          bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 2);
          switch (bits) {
            case 0x0:          /* switch_3 == '00' */
              run_length = 1;   /* 1 pixel of pseudo-color 0 */
              break;
            case 0x1:          /* switch_3 == '01' */
              run_length = 2;   /* 2 pixels of pseudo-color 0 */
              break;
            case 0x2:          /* switch_3 == '10' */
              run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 4);
              run_length += 9;
              clut_index = gst_bit_reader_get_bits_uint32_unchecked (&gb, 4);
              break;
            case 0x3:          /* switch_3 == '11' */
              run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 8);
              run_length += 25;
              clut_index = gst_bit_reader_get_bits_uint32_unchecked (&gb, 4);
              break;
          }
        }
      }
    }

    /* If run_length is zero, continue. Only case happening is when
     * stop_parsing is TRUE too, so next cycle shouldn't run */
    if (run_length == 0)
      continue;

    /* Trim the run_length to not go beyond the line end and consume
     * it from remaining length of dest line */
    run_length = MIN (run_length, dbuf_len);
    dbuf_len -= run_length;

    /* Make clut_index refer to the index into the desired bit depths
     * CLUT definition table */
    if (map_table)
      clut_index = map_table[clut_index];       /* now clut_index signifies the index into map_table dest */

    /* Now we can simply memset run_length count of destination bytes
     * to clut_index, but only if not non_modifying */
    GST_TRACE ("RUNLEN: setting %u pixels to color 0x%x in destination buffer; "
        "dbuf_len left is %d pixels", run_length, clut_index, dbuf_len);

    if (!(non_mod == 1 && clut_index == 1))
      memset (destbuf, clut_index, run_length);

    destbuf += run_length;
    pixels_read += run_length;
  }

  // FIXME: Test skip_to_byte instead of adding 7 bits, once everything else is working good
  //gst_bit_reader_skip_to_byte (&gb);
  *srcbuf += (gst_bit_reader_get_pos (&gb) + 7) >> 3;

  GST_LOG ("Returning with %u pixels read", pixels_read);

  // FIXME: Shouldn't need this variable if tracking things in the loop better
  return pixels_read;
}

static int
_dvb_sub_read_8bit_string (guint8 * destbuf, gint dbuf_len,
    const guint8 ** srcbuf, gint buf_size, guint8 non_mod, guint8 * map_table)
{
  GstBitReader gb = GST_BIT_READER_INIT (*srcbuf, buf_size);
  /* FIXME: Handle FALSE returns from gst_bit_reader_get_* calls? */

  gboolean stop_parsing = FALSE;
  guint32 bits = 0;
  guint32 pixels_read = 0;

  GST_LOG ("dbuf_len = %d", dbuf_len);

  /* FFMPEG-FIXME: ffmpeg uses a manual byte walking algorithm, which might be more performant,
   * FFMPEG-FIXME: but it does almost absolutely no buffer length checking, so could walk over
   * FFMPEG-FIXME: memory boundaries. While we don't check gst_bit_reader_get_bits_uint32
   * FFMPEG-FIXME: return values either and therefore might get some pixels corrupted, we at
   * FFMPEG-FIXME: lest have no chance of reading memory we don't own and visual corruption
   * FFMPEG-FIXME: is guaranteed anyway when not all bytes are present */
  /* Rephrased - it's better to work with bytes with default value '0' instead of reading from memory we don't own. */
  while (!stop_parsing && (gst_bit_reader_get_remaining (&gb) > 7)) {
    guint run_length = 0, clut_index = 0;
    bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 8);

    if (bits) {                 /* 8-bit_pixel-code */
      run_length = 1;
      clut_index = bits;
    } else {                    /* 8-bit_zero */
      bits = gst_bit_reader_get_bits_uint32_unchecked (&gb, 1);
      if (bits == 0) {          /* switch_1 == '0' */
        /* run_length_1-127 for pseudo-colour _entry) '0x00' */
        run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 7);
        if (run_length == 0) {  /* end_of_string_signal */
          stop_parsing = TRUE;
        }
      } else {                  /* switch_1 == '1' */
        /* run_length_3-127 */
        run_length = gst_bit_reader_get_bits_uint32_unchecked (&gb, 7);
        clut_index = gst_bit_reader_get_bits_uint32_unchecked (&gb, 8);

        if (run_length < 3) {
          GST_WARNING ("runlength value was %u, but the spec requires it "
              "must be >=3", run_length);
        }
      }
    }

    /* If run_length is zero, continue. Only case happening is when
     * stop_parsing is TRUE too, so next cycle shouldn't run */
    if (run_length == 0)
      continue;

    /* Trim the run_length to not go beyond the line end and consume
     * it from remaining length of dest line */
    run_length = MIN (run_length, dbuf_len);
    dbuf_len -= run_length;

    /* Make clut_index refer to the index into the desired bit depths
     * CLUT definition table */
    if (map_table)
      clut_index = map_table[clut_index];       /* now clut_index signifies the index into map_table dest */

    /* Now we can simply memset run_length count of destination bytes
     * to clut_index, but only if not non_modifying */
    GST_TRACE ("RUNLEN: setting %u pixels to color 0x%x in destination buffer; "
        "dbuf_len left is %d pixels", run_length, clut_index, dbuf_len);

    if (!(non_mod == 1 && clut_index == 1))
      memset (destbuf, clut_index, run_length);

    destbuf += run_length;
    pixels_read += run_length;
  }

  GST_LOG ("Returning with %u pixels read", pixels_read);

  *srcbuf += (gst_bit_reader_get_pos (&gb) + 7) >> 3;

  // FIXME: Shouldn't need this variable if tracking things in the loop better
  return pixels_read;
}

static void
_dvb_sub_parse_pixel_data_block (DvbSub * dvb_sub,
    DVBSubObjectDisplay * display, const guint8 * buf, gint buf_size,
    DvbSubPixelDataSubBlockFieldType top_bottom, guint8 non_mod)
{
  DVBSubRegion *region = get_region (dvb_sub, display->region_id);
  const guint8 *buf_end = buf + buf_size;
  guint8 *pbuf;
  int x_pos, y_pos;
  int i;
  gboolean dest_buf_filled = FALSE;

  guint8 map2to4[] = { 0x0, 0x7, 0x8, 0xf };
  guint8 map2to8[] = { 0x00, 0x77, 0x88, 0xff };
  guint8 map4to8[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
  };
  guint8 *map_table;

  GST_LOG ("DVB pixel block size %d, %s field:", buf_size,
      top_bottom ? "bottom" : "top");

  GST_MEMDUMP ("packet", buf, buf_size);

  if (region == NULL) {
    GST_LOG ("Region is NULL, returning");
    return;
  }

  pbuf = region->pbuf;

  x_pos = display->x_pos;
  y_pos = display->y_pos;

  if ((y_pos & 1) != top_bottom)
    y_pos++;

  while (buf < buf_end) {
    GST_LOG ("Iteration start, %u bytes remaining; buf = %p, "
        "buf_end = %p; Region is number %u, with a dimension of %dx%d; "
        "We are at position %dx%d", (guint) (buf_end - buf), buf, buf_end,
        region->id, region->width, region->height, x_pos, y_pos);

    // FFMPEG-FIXME: ffmpeg doesn't check for equality and so can overflow destination buffer later on with bad input data
    // FFMPEG-FIXME: However that makes it warn on end_of_object_line and map tables as well, so we add the dest_buf_filled tracking
    // FIXME: Removed x_pos checking here, because we don't want to turn dest_buf_filled to TRUE permanently in that case
    // FIXME: We assume that region->width - x_pos as dbuf_len to read_nbit_string will take care of that case nicely;
    // FIXME: That is, that read_nbit_string never scribbles anything if dbuf_len passed to it is zero due to this.
    if (y_pos >= region->height) {
      dest_buf_filled = TRUE;
    }

    switch (*buf++) {
      case 0x10:
        if (dest_buf_filled) {
          /* FIXME: Be more verbose */
          GST_WARNING ("Invalid object location for data_type 0x%x!",
              *(buf - 1));
          GST_MEMDUMP ("Remaining data after invalid object location:", buf,
              (guint) (buf_end - buf));
          return;
        }

        if (region->depth == 8)
          map_table = map2to8;
        else if (region->depth == 4)
          map_table = map2to4;
        else
          map_table = NULL;

        // FFMPEG-FIXME: ffmpeg code passes buf_size instead of buf_end - buf, and could
        // FFMPEG-FIXME: therefore potentially walk over the memory area we own
        x_pos +=
            _dvb_sub_read_2bit_string (pbuf + (y_pos * region->width) + x_pos,
            region->width - x_pos, &buf, buf_end - buf, non_mod, map_table);
        break;
      case 0x11:
        if (dest_buf_filled) {
          /* FIXME: Be more verbose */
          GST_WARNING ("Invalid object location for data_type 0x%x!",
              *(buf - 1));
          GST_MEMDUMP ("Remaining data after invalid object location:", buf,
              buf_end - buf);
          return;               // FIXME: Perhaps tell read_nbit_string that dbuf_len is zero and let it walk the bytes regardless? (Same FIXME for 2bit and 8bit)
        }

        if (region->depth < 4) {
          GST_WARNING ("4-bit pixel string in %d-bit region!", region->depth);
          return;
        }

        if (region->depth == 8)
          map_table = map4to8;
        else
          map_table = NULL;

        GST_LOG ("READ_4BIT_STRING: String data into position %dx%d; "
            "buf before is %p", x_pos, y_pos, buf);
        // FFMPEG-FIXME: ffmpeg code passes buf_size instead of buf_end - buf, and could
        // FFMPEG-FIXME: therefore potentially walk over the memory area we own
        x_pos +=
            _dvb_sub_read_4bit_string (pbuf + (y_pos * region->width) + x_pos,
            region->width - x_pos, &buf, buf_end - buf, non_mod, map_table);
        GST_DEBUG ("READ_4BIT_STRING finished: buf pointer now %p", buf);
        break;
      case 0x12:
        if (dest_buf_filled) {
          /* FIXME: Be more verbose */
          GST_WARNING ("Invalid object location for data_type 0x%x!",
              *(buf - 1));
          GST_MEMDUMP ("Remaining data after invalid object location:",
              buf, (guint) (buf_end - buf));
          return;
        }

        if (region->depth < 8) {
          GST_WARNING ("8-bit pixel string in %d-bit region!", region->depth);
          return;
        }
        // FFMPEG-FIXME: ffmpeg code passes buf_size instead of buf_end - buf, and could
        // FFMPEG-FIXME: therefore potentially walk over the memory area we own
        x_pos +=
            _dvb_sub_read_8bit_string (pbuf + (y_pos * region->width) + x_pos,
            region->width - x_pos, &buf, buf_end - buf, non_mod, NULL);
        break;

      case 0x20:
        GST_DEBUG ("handling map2to4 table data");
        /* FIXME: I don't see any guards about buffer size here - buf++ happens with the switch, but
         * FIXME: buffer is walked without length checks? Same deal in other map table cases */
        map2to4[0] = (*buf) >> 4;
        map2to4[1] = (*buf++) & 0xf;
        map2to4[2] = (*buf) >> 4;
        map2to4[3] = (*buf++) & 0xf;
        break;
      case 0x21:
        GST_DEBUG ("handling map2to8 table data");
        for (i = 0; i < 4; i++)
          map2to8[i] = *buf++;
        break;
      case 0x22:
        GST_DEBUG ("handling map4to8 table data");
        for (i = 0; i < 16; i++)
          map4to8[i] = *buf++;
        break;
      case 0xf0:
        GST_DEBUG ("end of object line code encountered");
        x_pos = display->x_pos;
        y_pos += 2;
        break;
      default:
        /* FIXME: Do we consume word align stuffing byte that could follow top/bottom data? */
        GST_WARNING ("Unknown/unsupported pixel block 0x%x", *(buf - 1));
    }
  }
}

static void
_dvb_sub_parse_object_segment (DvbSub * dvb_sub, guint16 page_id, guint8 * buf,
    gint buf_size)
{
  const guint8 *buf_end = buf + buf_size;
  guint object_id;
  DVBSubObject *object;

  guint8 coding_method, non_modifying_color;

  object_id = GST_READ_UINT16_BE (buf);
  buf += 2;

  object = get_object (dvb_sub, object_id);

  GST_DEBUG ("OBJECT: a new object segment has occurred for object_id = %u",
      object_id);

  if (!object) {
    GST_WARNING ("Nothing known about object with ID %u yet, bailing out",
        object_id);
    return;
  }

  coding_method = ((*buf) >> 2) & 3;
  non_modifying_color = ((*buf++) >> 1) & 1;

  if (coding_method == 0) {
    const guint8 *block;
    DVBSubObjectDisplay *display;
    guint16 top_field_len, bottom_field_len;

    top_field_len = GST_READ_UINT16_BE (buf);
    buf += 2;
    bottom_field_len = GST_READ_UINT16_BE (buf);
    buf += 2;

    if (buf + top_field_len + bottom_field_len > buf_end) {
      GST_WARNING ("Field data size too large");
      return;
    }

    /* FIXME: Potential optimization opportunity here - parse the object pixmap only once, and copy it to all the
     * FIXME: regions that need it. One object being in multiple regions is a rare occurrence in real life, however */
    for (display = object->display_list; display;
        display = display->object_list_next) {
      block = buf;

      GST_DEBUG ("OBJECT: parsing top and bottom part of object id %d; "
          "top_field_len = %u, bottom_field_len = %u",
          display->object_id, top_field_len, bottom_field_len);

      _dvb_sub_parse_pixel_data_block (dvb_sub, display, block, top_field_len,
          TOP_FIELD, non_modifying_color);

      if (bottom_field_len > 0)
        block = buf + top_field_len;
      else
        bottom_field_len = top_field_len;

      _dvb_sub_parse_pixel_data_block (dvb_sub, display, block,
          bottom_field_len, BOTTOM_FIELD, non_modifying_color);
    }

  } else if (coding_method == 1) {
    GST_FIXME ("'a string of characters' coding method not supported yet!");
  } else {
    GST_WARNING ("Unknown object coding 0x%x", coding_method);
  }
}

static gint
_dvb_sub_parse_display_definition_segment (DvbSub * dvb_sub, guint8 * buf,
    gint buf_size)
{
  int dds_version, info_byte;
  int display_width, display_height;

  if (buf_size < 5)
    return -1;

  info_byte = *buf++;
  dds_version = info_byte >> 4;

  display_width = GST_READ_UINT16_BE (buf) + 1;
  buf += 2;
  display_height = GST_READ_UINT16_BE (buf) + 1;
  buf += 2;

  if ((display_width != dvb_sub->display_def.display_width)
      || (display_height != dvb_sub->display_def.display_height)) {
    dvb_sub->display_def.display_width = display_width;
    dvb_sub->display_def.display_height = display_height;

    dvb_sub->display_def.version = -1;
  }

  if (dvb_sub->display_def.version == dds_version)
    return 0;                   /* already have this display definition version */

  dvb_sub->display_def.version = dds_version;

  dvb_sub->display_def.window_flag = info_byte & 1 << 3;

  if (buf_size >= 13 && dvb_sub->display_def.window_flag) {
    dvb_sub->display_def.window_x = GST_READ_UINT16_BE (buf);
    buf += 2;
    dvb_sub->display_def.window_width =
        GST_READ_UINT16_BE (buf) - dvb_sub->display_def.window_x + 1;
    buf += 2;
    dvb_sub->display_def.window_y = GST_READ_UINT16_BE (buf);
    buf += 2;
    dvb_sub->display_def.window_height =
        GST_READ_UINT16_BE (buf) - dvb_sub->display_def.window_y + 1;
  }

  return 0;
}

static gint
_dvb_sub_parse_end_of_display_set (DvbSub * dvb_sub, guint16 page_id,
    guint64 pts)
{
  DVBSubRegionDisplay *display;
  DVBSubtitles *sub;
  DVBSubCLUT *clut;
  guint32 *clut_table;
  int i;

  GST_DEBUG ("DISPLAY SET END: page_id = %u", page_id);

  sub = g_new0 (DVBSubtitles, 1);

#if 0                           /* FIXME: PTS stuff not figured out yet */
  sub->start_display_time = 0;
  sub->end_display_time = priv->page_time_out * 1000;
  sub->format = 0;              /* 0 = graphics */
#endif

  /* N.B. g_new0() will return NULL if num_rects is 0 */
  sub->num_rects = dvb_sub->display_list_size;
  sub->rects = g_new0 (DVBSubtitleRect, sub->num_rects);

  i = 0;

  /* copy subtitle display and window information */
  sub->display_def = dvb_sub->display_def;

  for (display = dvb_sub->display_list; display; display = display->next) {
    DVBSubtitleRect *rect;
    DVBSubRegion *region;

    region = get_region (dvb_sub, display->region_id);

    if (!region)
      continue;

    rect = &sub->rects[i];
    rect->x = display->x_pos;
    rect->y = display->y_pos;
    rect->w = region->width;
    rect->h = region->height;
#if 0                           /* FIXME: Don't think we need to save the number of colors in the palette when we are saving as RGBA? */
    rect->nb_colors = 16;
#endif
#if 0                           /* FIXME: Needed to be specified once we support strings of characters based subtitles */
    rect->type = SUBTITLE_BITMAP;
#endif
    rect->pict.rowstride = region->width;
    rect->pict.palette_bits_count = region->depth;

    clut = get_clut (dvb_sub, region->clut);

    if (!clut)
      clut = &default_clut;

    switch (region->depth) {
      case 2:
        clut_table = clut->clut4;
        break;
      case 8:
        clut_table = clut->clut256;
        break;
      case 4:
      default:
        clut_table = clut->clut16;
        break;
    }

    /* FIXME: Tweak this to be saved in a format most suitable for Qt and GStreamer instead.
     * Currently kept in AVPicture for quick save_display_set testing */
    rect->pict.palette = g_new (guint32, (1 << region->depth));
    memcpy (rect->pict.palette, clut_table,
        (1 << region->depth) * sizeof (guint32));

    GST_MEMDUMP ("rect->pict.data.palette content",
        (guint8 *) rect->pict.palette, (1 << region->depth) * sizeof (guint32));

    rect->pict.data = g_malloc (region->buf_size);
    memcpy (rect->pict.data, region->pbuf, region->buf_size);

    GST_DEBUG ("DISPLAY: an object rect created: iteration %u, "
        "pos: %d:%d, size: %dx%d", i, rect->x, rect->y, rect->w, rect->h);

    GST_MEMDUMP ("rect->pict.data content", rect->pict.data, region->buf_size);

    ++i;
  }

  sub->pts = pts;
  sub->page_time_out = dvb_sub->page_time_out;
  sub->num_rects = i;

  if (dvb_sub->callbacks.new_data) {
    dvb_sub->callbacks.new_data (dvb_sub, sub, dvb_sub->user_data);
  } else {
    /* No-one responsible to clean up memory, so do it ourselves */
    /* FIXME: Just don't bother with all this palette image creation in the first place then... */
    dvb_subtitles_free (sub);
  }

  return 1;                     /* FIXME: The caller of this function is probably supposed to do something with the return value */
}

void
dvb_subtitles_free (DVBSubtitles * sub)
{
  int i;

  if (sub == NULL)
    return;

  /* Now free up all the temporary memory we allocated */
  for (i = 0; i < sub->num_rects; ++i) {
    g_free (sub->rects[i].pict.palette);
    g_free (sub->rects[i].pict.data);
  }
  g_free (sub->rects);
  g_free (sub);
}

DvbSub *
dvb_sub_new (void)
{
  static gsize inited = 0;
  DvbSub *sub;

  if (g_once_init_enter (&inited)) {
    dvb_sub_init ();
    g_once_init_leave (&inited, TRUE);
  }

  sub = g_new0 (DvbSub, 1);

  /* TODO: Add initialization code here */
  /* FIXME: Do we have a reason to initiate the members to zero, or are we guaranteed that anyway? */
  sub->region_list = NULL;
  sub->object_list = NULL;
  sub->page_time_out = 0;       /* FIXME: Maybe 255 instead? */
  sub->pes_buffer = g_string_new (NULL);

  /* display/window information */
  sub->display_def.version = -1;
  sub->display_def.window_flag = 0;
  sub->display_def.display_width = 720;
  sub->display_def.display_height = 576;

  return sub;
}

void
dvb_sub_free (DvbSub * sub)
{
  /* TODO: Add deinitalization code here */
  /* FIXME: Clear up region_list contents */
  delete_state (sub);
  while (sub->display_list) {
    DVBSubRegionDisplay *tmp = sub->display_list->next;
    g_free (sub->display_list);
    sub->display_list = tmp;
  }
  g_string_free (sub->pes_buffer, TRUE);
  g_free (sub);
}

#define DVB_SUB_SEGMENT_PAGE_COMPOSITION 0x10
#define DVB_SUB_SEGMENT_REGION_COMPOSITION 0x11
#define DVB_SUB_SEGMENT_CLUT_DEFINITION 0x12
#define DVB_SUB_SEGMENT_OBJECT_DATA 0x13
#define DVB_SUB_SEGMENT_DISPLAY_DEFINITION 0x14
#define DVB_SUB_SEGMENT_END_OF_DISPLAY_SET 0x80
#define DVB_SUB_SEGMENT_STUFFING 0xFF

#define DVB_SUB_SYNC_BYTE 0x0f
/**
 * dvb_sub_feed_with_pts:
 * @dvb_sub: a #DvbSub
 * @pts: The PTS of the data
 * @data: The data to feed to the parser
 * @len: Length of the data
 *
 * Feeds the DvbSub parser with new binary data to parse,
 * with an associated PTS value. E.g, data left after PES
 * packet header has been already parsed, which contains
 * the PTS information).
 *
 * Return value: -1 if data was unhandled (e.g, not a subtitle packet),
 *				 -2 if data parsing was unsuccessful (e.g, length was invalid),
 *				  0 or positive if data was handled. If positive, then amount of data consumed on success. FIXME: List the positive return values.
 */
gint
dvb_sub_feed_with_pts (DvbSub * dvb_sub, guint64 pts, guint8 * data, gint len)
{
  unsigned int pos = 0;
  guint8 segment_type;
  guint16 segment_len;
  guint16 page_id;

  GST_DEBUG ("pts=%" G_GUINT64_FORMAT " and length %d", pts, len);

  g_return_val_if_fail (data != NULL || len == 0, -1);

  if (G_UNLIKELY (data == NULL)) {
    GST_DEBUG ("no data; forcing end-of-display-set");
    _dvb_sub_parse_end_of_display_set (dvb_sub, 0, pts);
    return 0;
  }

  if (len <= 3) {               /* len(0x20 0x00 end_of_PES_data_field_marker) */
    GST_WARNING ("Data length too short");
    return -1;
  }

  if (data[pos++] != 0x20) {
    GST_WARNING ("Tried to handle a PES packet private data that isn't a "
        "subtitle packet (does not start with 0x20)");
    return -1;
  }

  if (data[pos++] != 0x00) {
    GST_WARNING ("'Subtitle stream in this PES packet' was not 0x00, so this "
        "is in theory not a DVB subtitle stream (but some other subtitle "
        "standard?); bailing out");
    return -1;
  }

  while (data[pos++] == DVB_SUB_SYNC_BYTE) {
    if ((len - pos) < (2 * 2 + 1)) {
      GST_WARNING ("Data after SYNC BYTE too short, less than needed to "
          "even get to segment_length");
      return -2;
    }
    segment_type = data[pos++];
    GST_DEBUG ("=== Segment type is 0x%x", segment_type);
    page_id = (data[pos] << 8) | data[pos + 1];
    GST_DEBUG ("page_id is 0x%x", page_id);
    pos += 2;
    segment_len = (data[pos] << 8) | data[pos + 1];
    GST_DEBUG ("segment_length is %d (0x%x 0x%x)", segment_len, data[pos],
        data[pos + 1]);
    pos += 2;
    if ((len - pos) < segment_len) {
      GST_WARNING ("segment_length was told to be %u, but we only have "
          "%d bytes left", segment_len, len - pos);
      return -2;
    }
    // TODO: Parse the segment per type  (this is probably a leftover TODO that is now done?)
    /* FIXME: Handle differing PTS values - all segments of a given display set must be with the same PTS,
     * FIXME: but we let it slip and just take it for granted in end_of_display_set */
    switch (segment_type) {
      case DVB_SUB_SEGMENT_PAGE_COMPOSITION:
        GST_DEBUG ("Page composition segment at buffer pos %u", pos);
        _dvb_sub_parse_page_segment (dvb_sub, page_id, data + pos, segment_len);        /* FIXME: Not sure about args */
        break;
      case DVB_SUB_SEGMENT_REGION_COMPOSITION:
        GST_DEBUG ("Region composition segment at buffer pos %u", pos);
        _dvb_sub_parse_region_segment (dvb_sub, page_id, data + pos, segment_len);      /* FIXME: Not sure about args */
        break;
      case DVB_SUB_SEGMENT_CLUT_DEFINITION:
        GST_DEBUG ("CLUT definition segment at buffer pos %u", pos);
        _dvb_sub_parse_clut_segment (dvb_sub, page_id, data + pos, segment_len);        /* FIXME: Not sure about args */
        break;
      case DVB_SUB_SEGMENT_OBJECT_DATA:
        GST_DEBUG ("Object data segment at buffer pos %u", pos);
        _dvb_sub_parse_object_segment (dvb_sub, page_id, data + pos, segment_len);      /* FIXME: Not sure about args */
        break;
      case DVB_SUB_SEGMENT_DISPLAY_DEFINITION:
        GST_DEBUG ("display definition segment at buffer pos %u", pos);
        _dvb_sub_parse_display_definition_segment (dvb_sub, data + pos,
            segment_len);
        break;
      case DVB_SUB_SEGMENT_END_OF_DISPLAY_SET:
        GST_DEBUG ("End of display set at buffer pos %u", pos);
        _dvb_sub_parse_end_of_display_set (dvb_sub, page_id, pts);      /* FIXME: Not sure about args */
        break;
      default:
        GST_FIXME ("Unhandled segment type 0x%x", segment_type);
        break;
    }

    pos += segment_len;

    if (pos == len) {
      GST_WARNING ("Data ended without a PES data end marker");
      return 1;
    }
  }

  GST_LOG ("Processed %d bytes out of %d", pos, len);
  return pos;
}

/**
 * dvb_sub_set_callbacks:
 * @dvb_sub: a #DvbSub
 * @callbacks: the callbacks to install
 * @user_data: a user_data argument for the callback
 *
 * Set callback which will be executed when new subpictures are available.
 */
void
dvb_sub_set_callbacks (DvbSub * dvb_sub, DvbSubCallbacks * callbacks,
    gpointer user_data)
{
  g_return_if_fail (dvb_sub != NULL);
  g_return_if_fail (callbacks != NULL);

  dvb_sub->callbacks = *callbacks;
  dvb_sub->user_data = user_data;
}

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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gst/gst.h>

#include "gstdvdspu.h"
#include "gstspu-pgs.h"

const struct PgsFrameRateEntry
{
  guint8 id;
  guint fps_n;
  guint fps_d;
} PgsFrameRates[] = {
  {
      64, 30000, 1001}          /* 29.97 FPS */
};

typedef enum PgsCommandType PgsCommandType;

enum PgsCommandType
{
  PGS_COMMAND_SET_PALETTE = 0x14,
  PGS_COMMAND_SET_OBJECT_DATA = 0x15,
  PGS_COMMAND_PRESENTATION_SEGMENT = 0x16,
  PGS_COMMAND_SET_WINDOW = 0x17,
  PGS_COMMAND_INTERACTIVE_SEGMENT = 0x18,

  PGS_COMMAND_END_DISPLAY = 0x80,

  PGS_COMMAND_INVALID = 0xFFFF
};

static gint gstspu_exec_pgs_buffer (GstDVDSpu * dvdspu, GstBuffer * buf);

#define DUMP_CMDS 0
#define DUMP_FULL_IMAGE 0
#define DUMP_FULL_PALETTE 0

#if DUMP_CMDS
#define PGS_DUMP(...) g_print(__VA_ARGS__)
#else
#define PGS_DUMP(...)
#endif

static void
dump_bytes (guint8 * data, guint16 len)
{
  gint i;

  /* Dump the numbers */
  for (i = 0; i < len; i++) {
    PGS_DUMP ("0x%02x ", data[i]);
    if (!((i + 1) % 16))
      PGS_DUMP ("\n");
  }
  if (len > 0 && (i % 16))
    PGS_DUMP ("\n");
}

static void
dump_rle_data (GstDVDSpu * dvdspu, guint8 * data, guint32 len)
{
#if DUMP_FULL_IMAGE
  guint16 obj_h;
  guint16 obj_w;
  guint8 *end = data + len;
  guint x = 0;

  if (data + 4 > end)
    return;

  /* RLE data: */
  obj_w = GST_READ_UINT16_BE (data);
  obj_h = GST_READ_UINT16_BE (data + 2);
  data += 4;
  PGS_DUMP ("RLE image is %ux%u\n", obj_w, obj_h);

  while (data < end) {
    guint8 pal_id;
    guint16 run_len;

    pal_id = *data++;
    if (pal_id != 0) {
      // PGS_DUMP ("data 0x%02x\n", data[0]);
      run_len = 1;
    } else {
      if (data + 1 > end)
        return;
      switch (data[0] & 0xC0) {
        case 0x00:
          //PGS_DUMP ("data 0x%02x\n", data[0]);
          run_len = (data[0] & 0x3f);
          data++;
          break;
        case 0x40:
          if (data + 2 > end)
            return;
          //PGS_DUMP ("data 0x%02x 0x%02x\n", data[0], data[1]);
          run_len = ((data[0] << 8) | data[1]) & 0x3fff;
          data += 2;
          break;
        case 0x80:
          if (data + 2 > end)
            return;
          //PGS_DUMP ("data 0x%02x 0x%02x\n", data[0], data[1]);
          run_len = (data[0] & 0x3f);
          pal_id = data[1];
          data += 2;
          break;
        case 0xC0:
          if (data + 3 > end)
            return;
          //PGS_DUMP ("data 0x%02x 0x%02x 0x%02x\n", data[0], data[1], data[2]);
          run_len = ((data[0] << 8) | data[1]) & 0x3fff;
          pal_id = data[2];
          data += 3;
          break;
        default:
          run_len = 0;
          break;
      }
    }

    {
      gint i;
#if 1
      if (dvdspu->spu_state.pgs.palette[pal_id].A) {
        guint8 val = dvdspu->spu_state.pgs.palette[pal_id].A;
        for (i = 0; i < run_len; i++)
          PGS_DUMP ("%02x ", val);
      } else {
        for (i = 0; i < run_len; i++)
          PGS_DUMP ("   ");
      }
      if (!run_len || (x + run_len) > obj_w)
        PGS_DUMP ("\n");
#else
      PGS_DUMP ("Run x: %d pix: %d col: %d\n", x, run_len, pal_id);
#endif
    }

    x += run_len;
    if (!run_len || x > obj_w)
      x = 0;
  };

  PGS_DUMP ("\n");
#endif
}

static void
pgs_composition_object_render (PgsCompositionObject * obj, SpuState * state,
    GstVideoFrame * window)
{
  SpuColour *colour;
  guint8 *pixels, *p;
  gint stride;
  gint win_w;
  gint win_h;
  guint8 *data, *end;
  guint16 obj_w, obj_h;
  gint obj_x, obj_y;
  gint min_x, max_x;
  gint min_y, max_y;
  gint x, y, i;

  if (G_UNLIKELY (obj->rle_data == NULL || obj->rle_data_size == 0
          || obj->rle_data_used != obj->rle_data_size))
    return;

  data = obj->rle_data;
  end = data + obj->rle_data_used;

  if (data + 4 > end)
    return;

  pixels = GST_VIDEO_FRAME_PLANE_DATA (window, 0);
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (window, 0);
  win_w = GST_VIDEO_FRAME_WIDTH (window);
  win_h = GST_VIDEO_FRAME_HEIGHT (window);

  obj_w = GST_READ_UINT16_BE (data);
  obj_h = GST_READ_UINT16_BE (data + 2);
  data += 4;

  /* Calculate object coordinates relative to the window */
  min_x = obj_x = (gint) obj->x - (gint) state->pgs.win_x;
  min_y = obj_y = (gint) obj->y - (gint) state->pgs.win_y;

  if (obj->flags & PGS_COMPOSITION_OBJECT_FLAG_CROPPED) {
    obj_x -= obj->crop_x;
    obj_y -= obj->crop_y;
    obj_w = MIN (obj_w, obj->crop_w);
    obj_h = MIN (obj_h, obj->crop_h);
  }

  max_x = min_x + obj_w;
  max_y = min_y + obj_h;

  /* Early out if object is out of the window */
  if (max_x <= 0 || max_y < 0 || min_x >= win_w || min_y >= win_h)
    return;

  /* Crop inside window */
  if (min_x < 0)
    min_x = 0;
  if (max_x > win_w)
    max_x = win_w;
  if (min_y < 0)
    min_y = 0;
  if (max_y > win_h)
    max_y = win_h;

  /* Write RLE data to the plane */
  x = obj_x;
  y = obj_y;
  p = pixels + y * stride;

  while (data < end) {
    guint8 pal_id;
    guint16 run_len;

    pal_id = *data++;
    if (pal_id != 0) {
      run_len = 1;
    } else {
      if (data + 1 > end)
        return;
      switch (data[0] & 0xC0) {
        case 0x00:
          run_len = (data[0] & 0x3f);
          data++;
          break;
        case 0x40:
          if (data + 2 > end)
            return;
          run_len = ((data[0] << 8) | data[1]) & 0x3fff;
          data += 2;
          break;
        case 0x80:
          if (data + 2 > end)
            return;
          run_len = (data[0] & 0x3f);
          pal_id = data[1];
          data += 2;
          break;
        case 0xC0:
          if (data + 3 > end)
            return;
          run_len = ((data[0] << 8) | data[1]) & 0x3fff;
          pal_id = data[2];
          data += 3;
          break;
        default:
          run_len = 0;
          break;
      }
    }

    if (!run_len) {
      x = obj_x;
      y++;
      if (y >= max_y)
        break;

      p = pixels + y * stride;
      continue;
    }

    if (y < min_y)
      continue;

    if (x >= max_x)
      continue;

    if (x < min_x) {
      if (x + run_len <= min_x) {
        x += run_len;
        continue;
      } else {
        run_len -= min_x - x;
        x = min_x;
      }
    }

    colour = &state->pgs.palette[pal_id];

    if (colour->A > 0) {
      guint8 inv_A = 255 - colour->A;

      if (G_UNLIKELY (x + run_len > max_x))
        run_len = max_x - x;

      for (i = 0; i < run_len; i++) {
        SpuColour *pix = &((SpuColour *) p)[x++];

        if (pix->A == 0) {
          memcpy (pix, colour, sizeof (*pix));
        } else {
          pix->A = colour->A;
          pix->R = colour->R + pix->R * inv_A / 255;
          pix->G = colour->G + pix->G * inv_A / 255;
          pix->B = colour->B + pix->B * inv_A / 255;
        }
      }
    } else {
      x += run_len;
    }
  }
}

static void
pgs_composition_object_clear (PgsCompositionObject * obj)
{
  if (obj->rle_data) {
    g_free (obj->rle_data);
    obj->rle_data = NULL;
  }
  obj->rle_data_size = obj->rle_data_used = 0;
}

static void
pgs_presentation_segment_set_object_count (PgsPresentationSegment * ps,
    guint8 n_objects)
{
  if (ps->objects == NULL) {
    if (n_objects == 0)
      return;
    ps->objects =
        g_array_sized_new (FALSE, TRUE, sizeof (PgsCompositionObject),
        n_objects);
    g_array_set_size (ps->objects, n_objects);
    return;
  }

  /* Clear memory in any extraneous objects */
  if (ps->objects->len > n_objects) {
    guint i;
    for (i = n_objects; i < ps->objects->len; i++) {
      PgsCompositionObject *cur =
          &g_array_index (ps->objects, PgsCompositionObject, i);
      pgs_composition_object_clear (cur);
    }
  }

  g_array_set_size (ps->objects, n_objects);

  if (n_objects == 0) {
    g_array_free (ps->objects, TRUE);
    ps->objects = NULL;
  }
}

static PgsCompositionObject *
pgs_presentation_segment_find_object (PgsPresentationSegment * ps,
    guint16 obj_id)
{
  guint i;
  if (ps->objects == NULL)
    return NULL;

  for (i = 0; i < ps->objects->len; i++) {
    PgsCompositionObject *cur =
        &g_array_index (ps->objects, PgsCompositionObject, i);
    if (cur->id == obj_id)
      return cur;
  }

  return NULL;
}

static int
parse_presentation_segment (GstDVDSpu * dvdspu, guint8 type, guint8 * payload,
    guint16 len)
{
  guint8 *end = payload + len;
  PgsPresentationSegment *ps = &dvdspu->spu_state.pgs.pres_seg;
  guint8 n_objects, palette_id;
  gint i;

  /* Parse video descriptor */
  if (payload + 5 > end)
    return 0;

  ps->vid_w = GST_READ_UINT16_BE (payload);
  ps->vid_h = GST_READ_UINT16_BE (payload + 2);
  ps->vid_fps_code = payload[4];
  payload += 5;

  /* Parse composition descriptor */
  if (payload + 3 > end)
    return 0;
  ps->composition_no = GST_READ_UINT16_BE (payload);
  ps->composition_state = payload[2];
  payload += 3;

  /* Parse other bits */
  if (payload + 3 > end)
    return 0;

  ps->flags = payload[0];

  palette_id = payload[1];
  n_objects = payload[2];
  payload += 3;

  if (ps->flags & PGS_PRES_SEGMENT_FLAG_UPDATE_PALETTE)
    ps->palette_id = palette_id;

  PGS_DUMP ("Video width %u height %u fps code %u\n", ps->vid_w, ps->vid_h,
      ps->vid_fps_code);
  PGS_DUMP
      ("Composition num %u state 0x%02x flags 0x%02x palette id %u n_objects %u\n",
      ps->composition_no, ps->composition_state, ps->flags, ps->palette_id,
      n_objects);

  pgs_presentation_segment_set_object_count (ps, n_objects);

  for (i = 0; i < (gint) n_objects; i++) {
    PgsCompositionObject *obj =
        &g_array_index (ps->objects, PgsCompositionObject, i);

    if (payload + 8 > end)
      break;
    obj->id = GST_READ_UINT16_BE (payload);
    obj->win_id = payload[2];
    obj->flags = payload[3];
    obj->x = GST_READ_UINT16_BE (payload + 4);
    obj->y = GST_READ_UINT16_BE (payload + 6);
    obj->rle_data_size = obj->rle_data_used = 0;

    payload += 8;

    PGS_DUMP ("Composition object %d Object ID %u Window ID %u flags 0x%02x "
        "x %u y %u\n", i, obj->id, obj->win_id, obj->flags, obj->x, obj->y);

    if (obj->flags & PGS_COMPOSITION_OBJECT_FLAG_CROPPED) {
      if (payload + 8 > end) {
        obj->flags &= ~PGS_COMPOSITION_OBJECT_FLAG_CROPPED;
        break;
      }

      obj->crop_x = GST_READ_UINT16_BE (payload);
      obj->crop_y = GST_READ_UINT16_BE (payload + 2);
      obj->crop_w = GST_READ_UINT16_BE (payload + 4);
      obj->crop_h = GST_READ_UINT16_BE (payload + 6);

      payload += 8;

      PGS_DUMP ("Cropping window x %u y %u w %u h %u\n",
          obj->crop_x, obj->crop_y, obj->crop_w, obj->crop_h);
    }

    if (obj->flags & ~(PGS_COMPOSITION_OBJECT_FLAG_CROPPED |
            PGS_COMPOSITION_OBJECT_FLAG_FORCED))
      GST_ERROR ("PGS Composition Object has unknown flags: 0x%02x",
          obj->flags);
  }

  if (payload != end) {
    GST_ERROR ("PGS Composition Object: %" G_GSSIZE_FORMAT
        " bytes not consumed", (gssize) (end - payload));
    dump_bytes (payload, end - payload);
  }

  return 0;
}

static int
parse_set_palette (GstDVDSpu * dvdspu, guint8 type, guint8 * payload,
    guint16 len)
{
  SpuState *state = &dvdspu->spu_state;

  const gint PGS_PALETTE_ENTRY_SIZE = 5;
  guint8 *end = payload + len;
  guint8 palette_id G_GNUC_UNUSED;
  guint8 palette_version G_GNUC_UNUSED;
  gint n_entries, i;

  if (len < 2)                  /* Palette command too short */
    return 0;
  palette_id = payload[0];
  palette_version = payload[1];
  payload += 2;

  n_entries = (len - 2) / PGS_PALETTE_ENTRY_SIZE;

  PGS_DUMP ("Palette ID %u version %u. %d entries\n",
      palette_id, palette_version, n_entries);
  for (i = 0; i < 256; i++)
    state->pgs.palette[i].A = 0;
  for (i = 0; i < n_entries; i++) {
    guint8 n, Y, U, V, A;
    gint R, G, B;
    n = payload[0];
    Y = payload[1];
    V = payload[2];
    U = payload[3];
    A = payload[4];

#if DUMP_FULL_PALETTE
    PGS_DUMP ("Entry %3d: Y %3d U %3d V %3d A %3d  ", n, Y, U, V, A);
#endif

    /* Convert to ARGB */
    R = (298 * Y + 459 * V - 63514) >> 8;
    G = (298 * Y - 55 * U - 136 * V + 19681) >> 8;
    B = (298 * Y + 541 * U - 73988) >> 8;

    R = CLAMP (R, 0, 255);
    G = CLAMP (G, 0, 255);
    B = CLAMP (B, 0, 255);

#if DUMP_FULL_PALETTE
    PGS_DUMP ("Entry %3d: A %3d R %3d G %3d B %3d\n", n, A, R, G, B);
#endif

    /* Premultiply the palette entries by the alpha */
    state->pgs.palette[n].A = A;
    state->pgs.palette[n].R = R * A / 255;
    state->pgs.palette[n].G = G * A / 255;
    state->pgs.palette[n].B = B * A / 255;

    payload += PGS_PALETTE_ENTRY_SIZE;
  }

#if DUMP_FULL_PALETTE
  if (n_entries > 0 && (i % 2))
    PGS_DUMP ("\n");
#endif

  if (payload != end) {
    GST_ERROR ("PGS Set Palette: %" G_GSSIZE_FORMAT " bytes not consumed",
        (gssize) (end - payload));
    dump_bytes (payload, end - payload);
  }

  return 0;
}

static int
parse_set_window (GstDVDSpu * dvdspu, guint8 type, guint8 * payload,
    guint16 len)
{
  SpuState *state = &dvdspu->spu_state;
  guint8 *end = payload + len;
  guint8 win_count, win_id G_GNUC_UNUSED;
  gint i;

  if (payload + 1 > end)
    return 0;

  dump_bytes (payload, len);

  win_count = payload[0];
  payload++;

  for (i = 0; i < win_count; i++) {
    if (payload + 9 > end)
      return 0;

    /* FIXME: Store each window ID separately into an array */
    win_id = payload[0];
    state->pgs.win_x = GST_READ_UINT16_BE (payload + 1);
    state->pgs.win_y = GST_READ_UINT16_BE (payload + 3);
    state->pgs.win_w = GST_READ_UINT16_BE (payload + 5);
    state->pgs.win_h = GST_READ_UINT16_BE (payload + 7);
    payload += 9;

    PGS_DUMP ("Win ID %u x %d y %d w %d h %d\n",
        win_id, state->pgs.win_x, state->pgs.win_y, state->pgs.win_w,
        state->pgs.win_h);
  }

  if (payload != end) {
    GST_ERROR ("PGS Set Window: %" G_GSSIZE_FORMAT " bytes not consumed",
        (gssize) (end - payload));
    dump_bytes (payload, end - payload);
  }

  return 0;
}

static int
parse_set_object_data (GstDVDSpu * dvdspu, guint8 type, guint8 * payload,
    guint16 len)
{
  SpuPgsState *pgs_state = &dvdspu->spu_state.pgs;
  PgsCompositionObject *obj;
  guint8 *end = payload + len;
  guint16 obj_id;
  guint8 obj_ver, flags;

  if (payload + 4 > end)
    return 0;

  obj_id = GST_READ_UINT16_BE (payload);
  obj_ver = payload[2];
  flags = payload[3];
  payload += 4;

  obj = pgs_presentation_segment_find_object (&(pgs_state->pres_seg), obj_id);

  PGS_DUMP ("Object ID %d ver %u flags 0x%02x\n", obj_id, obj_ver, flags);

  if (!obj) {
    GST_ERROR ("unknown Object ID %d", obj_id);
    return 0;
  }

  if (flags & PGS_OBJECT_UPDATE_FLAG_START_RLE) {
    obj->rle_data_ver = obj_ver;

    if (payload + 3 > end)
      return 0;

    obj->rle_data_size = GST_READ_UINT24_BE (payload);
    payload += 3;

    if (end - payload > obj->rle_data_size)
      return 0;

    PGS_DUMP ("%d bytes of RLE data, of %d bytes total.\n",
        (int) (end - payload), obj->rle_data_size);

    obj->rle_data = g_realloc (obj->rle_data, obj->rle_data_size);
    obj->rle_data_used = end - payload;
    memcpy (obj->rle_data, payload, end - payload);
    payload = end;
  } else {
    PGS_DUMP ("%d bytes of additional RLE data\n", (int) (end - payload));
    /* Check that the data chunk is for this object version, and fits in the buffer */
    if (obj->rle_data_ver == obj_ver &&
        end - payload <= obj->rle_data_size &&
        obj->rle_data_used <= obj->rle_data_size - (end - payload)) {

      memcpy (obj->rle_data + obj->rle_data_used, payload, end - payload);
      obj->rle_data_used += end - payload;
      payload = end;
    }
  }

  if (obj->rle_data_size == obj->rle_data_used)
    dump_rle_data (dvdspu, obj->rle_data, obj->rle_data_size);

  if (payload != end) {
    GST_ERROR ("PGS Set Object Data: %" G_GSSIZE_FORMAT " bytes not consumed",
        (gssize) (end - payload));
    dump_bytes (payload, end - payload);
  }

  return 0;
}

static int
parse_pgs_packet (GstDVDSpu * dvdspu, guint8 type, guint8 * payload,
    guint16 len)
{
  SpuPgsState *pgs_state = &dvdspu->spu_state.pgs;
  int ret = 0;

  if (!pgs_state->in_presentation_segment
      && type != PGS_COMMAND_PRESENTATION_SEGMENT) {
    PGS_DUMP ("Expected BEGIN PRESENTATION SEGMENT command. "
        "Got command type 0x%02x len %u. Skipping\n", type, len);
    return 0;
  }

  switch (type) {
    case PGS_COMMAND_PRESENTATION_SEGMENT:
      PGS_DUMP ("*******************************************\n"
          "Begin PRESENTATION_SEGMENT (0x%02x) packet len %u\n", type, len);
      pgs_state->in_presentation_segment =
          pgs_state->have_presentation_segment = TRUE;
      ret = parse_presentation_segment (dvdspu, type, payload, len);
      break;
    case PGS_COMMAND_SET_OBJECT_DATA:
      PGS_DUMP ("***   Set Object Data (0x%02x) packet len %u\n", type, len);
      ret = parse_set_object_data (dvdspu, type, payload, len);
      break;
    case PGS_COMMAND_SET_PALETTE:
      PGS_DUMP ("***   Set Palette (0x%02x) packet len %u\n", type, len);
      ret = parse_set_palette (dvdspu, type, payload, len);
      break;
    case PGS_COMMAND_SET_WINDOW:
      PGS_DUMP ("***   Set Window command (0x%02x) packet len %u\n", type, len);
      ret = parse_set_window (dvdspu, type, payload, len);
      break;
    case PGS_COMMAND_INTERACTIVE_SEGMENT:
      PGS_DUMP ("***   Interactive Segment command(0x%02x) packet len %u\n",
          type, len);
      dump_bytes (payload, len);
      break;
    case PGS_COMMAND_END_DISPLAY:
      PGS_DUMP ("***   End Display command (0x%02x) packet len %u\n", type,
          len);
      pgs_state->in_presentation_segment = FALSE;
      break;
    default:
      GST_ERROR ("Unknown PGS command: type 0x%02x len %u", type, len);
      dump_bytes (payload, len);
      break;
  }
  PGS_DUMP ("\n");

  return ret;
}

gint
gstspu_exec_pgs_buffer (GstDVDSpu * dvdspu, GstBuffer * buf)
{
  GstMapInfo map;
  guint8 *pos, *end;
  guint8 type;
  guint16 packet_len;
  gint remaining;

  gst_buffer_map (buf, &map, GST_MAP_READ);

  pos = map.data;
  end = pos + map.size;

  /* Need at least 3 bytes */
  if (pos + 3 > end) {
    PGS_DUMP ("Not enough bytes to be a PGS packet\n");
    goto error;
  }

  PGS_DUMP ("Begin dumping command buffer of size %u ts %" GST_TIME_FORMAT "\n",
      (guint) (end - pos), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));
  do {
    type = *pos++;
    packet_len = GST_READ_UINT16_BE (pos);
    pos += 2;

    if (pos + packet_len > end) {
      PGS_DUMP ("Invalid packet length %u (only have %u bytes)\n", packet_len,
          (guint) (end - pos));
      goto error;
    }

    if (parse_pgs_packet (dvdspu, type, pos, packet_len))
      goto error;

    pos += packet_len;
  } while (pos + 3 <= end);

  PGS_DUMP ("End dumping command buffer with %u bytes remaining\n",
      (guint) (end - pos));
  remaining = (gint) (pos - map.data);
  gst_buffer_unmap (buf, &map);
  return remaining;

  /* ERRORS */
error:
  {
    gst_buffer_unmap (buf, &map);
    return -1;
  }
}

void
gstspu_pgs_handle_new_buf (GstDVDSpu * dvdspu, GstClockTime event_ts,
    GstBuffer * buf)
{
  SpuState *state = &dvdspu->spu_state;

  state->next_ts = event_ts;
  state->pgs.pending_cmd = buf;
}

gboolean
gstspu_pgs_execute_event (GstDVDSpu * dvdspu)
{
  SpuState *state = &dvdspu->spu_state;

  if (state->pgs.pending_cmd) {
    gstspu_exec_pgs_buffer (dvdspu, state->pgs.pending_cmd);
    gst_buffer_unref (state->pgs.pending_cmd);
    state->pgs.pending_cmd = NULL;
  }

  state->next_ts = GST_CLOCK_TIME_NONE;

  state->flags &= ~SPU_STATE_DISPLAY;
  if (state->pgs.have_presentation_segment) {
    if (state->pgs.pres_seg.objects && state->pgs.pres_seg.objects->len > 0)
      state->flags |= SPU_STATE_DISPLAY;
  }
  return FALSE;
}

void
gstspu_pgs_render (GstDVDSpu * dvdspu, GstVideoFrame * window)
{
  SpuState *state = &dvdspu->spu_state;
  PgsPresentationSegment *ps = &state->pgs.pres_seg;
  guint i;

  if (ps->objects == NULL)
    return;

  for (i = 0; i < ps->objects->len; i++) {
    PgsCompositionObject *cur =
        &g_array_index (ps->objects, PgsCompositionObject, i);
    pgs_composition_object_render (cur, state, window);
  }
}

gboolean
gstspu_pgs_handle_dvd_event (GstDVDSpu * dvdspu, GstEvent * event)
{
  gst_event_unref (event);
  return FALSE;
}

void
gstspu_pgs_get_render_geometry (GstDVDSpu * dvdspu,
    gint * display_width, gint * display_height,
    GstVideoRectangle * window_rect)
{
  SpuPgsState *pgs_state = &dvdspu->spu_state.pgs;

  if (window_rect) {
    window_rect->x = pgs_state->win_x;
    window_rect->y = pgs_state->win_y;
    window_rect->w = pgs_state->win_w;
    window_rect->h = pgs_state->win_h;
  }

  if (display_width)
    *display_width = pgs_state->pres_seg.vid_w;

  if (display_height)
    *display_height = pgs_state->pres_seg.vid_h;
}

void
gstspu_pgs_flush (GstDVDSpu * dvdspu)
{
  SpuPgsState *pgs_state = &dvdspu->spu_state.pgs;

  if (pgs_state->pending_cmd) {
    gst_buffer_unref (pgs_state->pending_cmd);
    pgs_state->pending_cmd = NULL;
  }

  pgs_state->have_presentation_segment = FALSE;
  pgs_state->in_presentation_segment = FALSE;
  pgs_presentation_segment_set_object_count (&pgs_state->pres_seg, 0);

  pgs_state->win_x = pgs_state->win_y = pgs_state->win_w = pgs_state->win_h = 0;
}

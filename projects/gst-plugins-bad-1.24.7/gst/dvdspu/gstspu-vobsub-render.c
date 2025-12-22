/* GStreamer DVD Sub-Picture Unit
 * Copyright (C) 2007 Fluendo S.A. <info@fluendo.com>
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

GST_DEBUG_CATEGORY_EXTERN (dvdspu_debug);
#define GST_CAT_DEFAULT dvdspu_debug

static void
gstspu_vobsub_recalc_palette (GstDVDSpu * dvdspu,
    SpuColour * dest, guint8 * idx, guint8 * alpha)
{
  SpuState *state = &dvdspu->spu_state;
  gint i;

  if (state->vobsub.current_clut[idx[0]] != 0) {
    for (i = 0; i < 4; i++, dest++) {
      guint32 col = state->vobsub.current_clut[idx[i]];
      gint A, Y, U, V;
      gint R, G, B;

      /* Convert incoming 4-bit alpha to 8 bit for blending */
      A = (alpha[i] << 4) | alpha[i];
      Y = ((col >> 16) & 0xff);
      /* U/V are stored as V/U in the clut words, so switch them */
      V = ((col >> 8) & 0xff);
      U = (col & 0xff);

      R = (298 * Y + 459 * V - 63514) >> 8;
      G = (298 * Y - 55 * U - 136 * V + 19681) >> 8;
      B = (298 * Y + 541 * U - 73988) >> 8;

      R = CLAMP (R, 0, 255);
      G = CLAMP (G, 0, 255);
      B = CLAMP (B, 0, 255);

      dest->A = A;
      dest->R = R * A / 255;
      dest->G = G * A / 255;
      dest->B = B * A / 255;
    }
  } else {
    int c = 255;

    /* The CLUT presumably hasn't been set, so we'll just guess some
     * values for the non-transparent colors (white, grey, black) */
    for (i = 0; i < 4; i++, dest++) {
      dest->A = (alpha[i] << 4) | alpha[i];
      if (alpha[i] != 0) {
        dest->R = dest->G = dest->B = c * dest->A / 255;
        c -= 128;
        if (c < 0)
          c = 0;
      }
    }
  }
}

/* Recalculate the main, HL & ChgCol palettes */
static void
gstspu_vobsub_update_palettes (GstDVDSpu * dvdspu, SpuState * state)
{
  guint8 index[4];              /* Indices for the palette */
  guint8 alpha[4];              /* Alpha values the palette */

  if (state->vobsub.main_pal_dirty) {
    gstspu_vobsub_recalc_palette (dvdspu, state->vobsub.main_pal,
        state->vobsub.main_idx, state->vobsub.main_alpha);

    /* Need to refresh the hl_ctrl info copies of the main palette too */
    memcpy (state->vobsub.hl_ctrl_i.pix_ctrl_i[0].pal_cache,
        state->vobsub.main_pal, 4 * sizeof (SpuColour));
    memcpy (state->vobsub.hl_ctrl_i.pix_ctrl_i[2].pal_cache,
        state->vobsub.main_pal, 4 * sizeof (SpuColour));

    state->vobsub.main_pal_dirty = FALSE;
  }

  if (state->vobsub.hl_pal_dirty) {
    gstspu_vobsub_recalc_palette (dvdspu,
        state->vobsub.hl_ctrl_i.pix_ctrl_i[1].pal_cache, state->vobsub.hl_idx,
        state->vobsub.hl_alpha);
    state->vobsub.hl_pal_dirty = FALSE;
  }

  /* Update the offset positions for the highlight region */
  if (state->vobsub.hl_rect.top != -1) {
    state->vobsub.hl_ctrl_i.top = state->vobsub.hl_rect.top;
    state->vobsub.hl_ctrl_i.bottom = state->vobsub.hl_rect.bottom;
    state->vobsub.hl_ctrl_i.n_changes = 3;
    state->vobsub.hl_ctrl_i.pix_ctrl_i[0].left = 0;
    state->vobsub.hl_ctrl_i.pix_ctrl_i[1].left = state->vobsub.hl_rect.left;
    state->vobsub.hl_ctrl_i.pix_ctrl_i[2].left =
        state->vobsub.hl_rect.right + 1;
  }

  if (state->vobsub.line_ctrl_i_pal_dirty) {
    gint16 l, c;
    GST_LOG_OBJECT (dvdspu, "Updating chg-col-con palettes");
    for (l = 0; l < state->vobsub.n_line_ctrl_i; l++) {
      SpuVobsubLineCtrlI *cur_line_ctrl = state->vobsub.line_ctrl_i + l;

      for (c = 0; c < cur_line_ctrl->n_changes; c++) {
        SpuVobsubPixCtrlI *cur = cur_line_ctrl->pix_ctrl_i + c;

        index[3] = (cur->palette >> 28) & 0x0f;
        index[2] = (cur->palette >> 24) & 0x0f;
        index[1] = (cur->palette >> 20) & 0x0f;
        index[0] = (cur->palette >> 16) & 0x0f;

        alpha[3] = (cur->palette >> 12) & 0x0f;
        alpha[2] = (cur->palette >> 8) & 0x0f;
        alpha[1] = (cur->palette >> 4) & 0x0f;
        alpha[0] = (cur->palette) & 0x0f;
        gstspu_vobsub_recalc_palette (dvdspu, cur->pal_cache, index, alpha);
      }
    }
    state->vobsub.line_ctrl_i_pal_dirty = FALSE;
  }
}

static inline guint8
gstspu_vobsub_get_nibble (SpuState * state, guint16 * rle_offset)
{
  guint8 ret;

  if (G_UNLIKELY (*rle_offset >= state->vobsub.max_offset))
    return 0;                   /* Overran the buffer */

  ret = state->vobsub.pix_buf_map.data[(*rle_offset) / 2];

  /* If the offset is even, we shift the answer down 4 bits, otherwise not */
  if (*rle_offset & 0x01)
    ret &= 0x0f;
  else
    ret = ret >> 4;

  (*rle_offset)++;
  return ret;
}

static guint16
gstspu_vobsub_get_rle_code (SpuState * state, guint16 * rle_offset)
{
  guint16 code;

  code = gstspu_vobsub_get_nibble (state, rle_offset);
  if (code < 0x4) {             /* 4 .. f */
    code = (code << 4) | gstspu_vobsub_get_nibble (state, rle_offset);
    if (code < 0x10) {          /* 1x .. 3x */
      code = (code << 4) | gstspu_vobsub_get_nibble (state, rle_offset);
      if (code < 0x40) {        /* 04x .. 0fx */
        code = (code << 4) | gstspu_vobsub_get_nibble (state, rle_offset);
      }
    }
  }
  return code;
}

static inline gboolean
gstspu_vobsub_draw_rle_run (SpuState * state, GstVideoFrame * frame,
    gint16 x, gint16 end, SpuColour * colour)
{
  GST_TRACE ("Y: %d x: %d end %d %d %d %d %d",
      state->vobsub.cur_Y, x, end, colour->R, colour->G, colour->B, colour->A);

  if (colour->A > 0) {
    gint i;
    guint8 *data;
    guint8 inv_A = 255 - colour->A;

    data = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
    data += GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0) *
        (state->vobsub.cur_Y - state->vobsub.disp_rect.top);

    x -= state->vobsub.disp_rect.left;
    end -= state->vobsub.disp_rect.left;

    for (i = x; i < end; i++) {
      SpuColour *pix = &((SpuColour *) data)[x++];

      if (pix->A == 0) {
        memcpy (pix, colour, sizeof (*pix));
      } else {
        pix->A = colour->A;
        pix->R = colour->R + pix->R * inv_A / 255;
        pix->G = colour->G + pix->G * inv_A / 255;
        pix->B = colour->B + pix->B * inv_A / 255;
      }
    }

    return TRUE;
  }
  return FALSE;
}

static inline gint16
rle_end_x (guint16 rle_code, gint16 x, gint16 end)
{
  /* run length = rle_code >> 2 */
  if (G_UNLIKELY (((rle_code >> 2) == 0)))
    return end;
  else
    return MIN (end, x + (rle_code >> 2));
}

static gboolean gstspu_vobsub_render_line_with_chgcol (SpuState * state,
    GstVideoFrame * frame, guint16 * rle_offset);
static gboolean gstspu_vobsub_update_chgcol (SpuState * state);

static gboolean
gstspu_vobsub_render_line (SpuState * state, GstVideoFrame * frame,
    guint16 * rle_offset)
{
  gint16 x, next_x, end, rle_code, next_draw_x;
  SpuColour *colour;
  gboolean visible = FALSE;

  /* Check for special case of chg_col info to use (either highlight or
   * ChgCol command */
  if (state->vobsub.cur_chg_col != NULL) {
    if (gstspu_vobsub_update_chgcol (state)) {
      /* Check the top & bottom, because we might not be within the region yet */
      if (state->vobsub.cur_Y >= state->vobsub.cur_chg_col->top &&
          state->vobsub.cur_Y <= state->vobsub.cur_chg_col->bottom) {
        return gstspu_vobsub_render_line_with_chgcol (state, frame, rle_offset);
      }
    }
  }

  /* No special case. Render as normal */

  /* We always need to start our RLE decoding byte_aligned */
  *rle_offset = GST_ROUND_UP_2 (*rle_offset);

  x = state->vobsub.disp_rect.left;
  end = state->vobsub.disp_rect.right + 1;
  while (x < end) {
    rle_code = gstspu_vobsub_get_rle_code (state, rle_offset);
    colour = &state->vobsub.main_pal[rle_code & 3];
    next_x = rle_end_x (rle_code, x, end);
    next_draw_x = next_x;
    if (next_draw_x > state->vobsub.disp_rect.right)
      next_draw_x = state->vobsub.disp_rect.right;      /* ensure no overflow */
    /* Now draw the run between [x,next_x) */
    visible |=
        gstspu_vobsub_draw_rle_run (state, frame, x, next_draw_x, colour);
    x = next_x;
  }

  return visible;
}

static gboolean
gstspu_vobsub_update_chgcol (SpuState * state)
{
  if (state->vobsub.cur_chg_col == NULL)
    return FALSE;

  if (state->vobsub.cur_Y <= state->vobsub.cur_chg_col->bottom)
    return TRUE;

  while (state->vobsub.cur_chg_col < state->vobsub.cur_chg_col_end) {
    if (state->vobsub.cur_Y >= state->vobsub.cur_chg_col->top &&
        state->vobsub.cur_Y <= state->vobsub.cur_chg_col->bottom) {
#if 0
      g_print ("Stopped @ entry %d with top %d bottom %d, cur_y %d",
          (gint16) (state->vobsub.cur_chg_col - state->vobsub.line_ctrl_i),
          state->vobsub.cur_chg_col->top, state->vobsub.cur_chg_col->bottom, y);
#endif
      return TRUE;
    }
    state->vobsub.cur_chg_col++;
  }

  /* Finished all our cur_chg_col entries. Use the main palette from here on */
  state->vobsub.cur_chg_col = NULL;
  return FALSE;
}

static gboolean
gstspu_vobsub_render_line_with_chgcol (SpuState * state, GstVideoFrame * frame,
    guint16 * rle_offset)
{
  SpuVobsubLineCtrlI *chg_col = state->vobsub.cur_chg_col;

  gint16 x, next_x, disp_end, rle_code, run_end, run_draw_end;
  SpuColour *colour;
  SpuVobsubPixCtrlI *cur_pix_ctrl;
  SpuVobsubPixCtrlI *next_pix_ctrl;
  SpuVobsubPixCtrlI *end_pix_ctrl;
  SpuVobsubPixCtrlI dummy_pix_ctrl;
  gboolean visible = FALSE;
  gint16 cur_reg_end;
  gint i;

  /* We always need to start our RLE decoding byte_aligned */
  *rle_offset = GST_ROUND_UP_2 (*rle_offset);

  /* Our run will cover the display rect */
  x = state->vobsub.disp_rect.left;
  disp_end = state->vobsub.disp_rect.right + 1;

  /* Work out the first pixel control info, which may point to the dummy entry if
   * the global palette/alpha need using initially */
  cur_pix_ctrl = chg_col->pix_ctrl_i;
  end_pix_ctrl = chg_col->pix_ctrl_i + chg_col->n_changes;

  if (cur_pix_ctrl->left != 0) {
    next_pix_ctrl = cur_pix_ctrl;
    cur_pix_ctrl = &dummy_pix_ctrl;
    for (i = 0; i < 4; i++)     /* Copy the main palette to our dummy entry */
      dummy_pix_ctrl.pal_cache[i] = state->vobsub.main_pal[i];
  } else {
    next_pix_ctrl = cur_pix_ctrl + 1;
  }
  if (next_pix_ctrl < end_pix_ctrl)
    cur_reg_end = next_pix_ctrl->left;
  else
    cur_reg_end = disp_end;

  /* Render stuff */
  while (x < disp_end) {
    rle_code = gstspu_vobsub_get_rle_code (state, rle_offset);
    next_x = rle_end_x (rle_code, x, disp_end);

    /* Now draw the run between [x,next_x), crossing palette regions as needed */
    while (x < next_x) {
      run_end = MIN (next_x, cur_reg_end);

      run_draw_end = run_end;
      if (run_draw_end > state->vobsub.disp_rect.right)
        run_draw_end = state->vobsub.disp_rect.right;   /* ensure no overflow */

      if (G_LIKELY (x < run_end)) {
        colour = &cur_pix_ctrl->pal_cache[rle_code & 3];
        visible |= gstspu_vobsub_draw_rle_run (state, frame, x,
            run_draw_end, colour);
        x = run_end;
      }

      if (x >= cur_reg_end) {
        /* Advance to next region */
        cur_pix_ctrl = next_pix_ctrl;
        next_pix_ctrl++;

        if (next_pix_ctrl < end_pix_ctrl)
          cur_reg_end = next_pix_ctrl->left;
        else
          cur_reg_end = disp_end;
      }
    }
  }

  return visible;
}

static void
gstspu_vobsub_draw_highlight (SpuState * state,
    GstVideoFrame * frame, SpuRect * rect)
{
  SpuColour *cur;
  SpuRect r;
  guint8 *data;
  guint stride;
  gint16 pos;

  r.left = rect->left - state->vobsub.disp_rect.left;
  r.right = rect->right - state->vobsub.disp_rect.left;
  r.top = rect->top - state->vobsub.disp_rect.top;
  r.bottom = rect->bottom - state->vobsub.disp_rect.top;
  rect = &r;

  data = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  cur = (SpuColour *) (data + stride * rect->top);
  for (pos = rect->left; pos < rect->right; pos++)
    cur[pos].A = 0x80;

  cur = (SpuColour *) (data + stride * (rect->bottom - 1));
  for (pos = rect->left; pos < rect->right; pos++)
    cur[pos].A = 0x80;

  for (pos = rect->top; pos < rect->bottom; pos++) {
    cur = (SpuColour *) (data + stride * pos);
    cur[rect->left].A = 0x80;
    cur[rect->right - 1].A = 0x80;
  }
}

void
gstspu_vobsub_render (GstDVDSpu * dvdspu, GstVideoFrame * frame)
{
  SpuState *state = &dvdspu->spu_state;
  gint y, last_y;
  guint16 cur_offsets[2];

  /* Set up our initial state */
  if (G_UNLIKELY (state->vobsub.pix_buf == NULL))
    return;

  if (!gst_buffer_map (state->vobsub.pix_buf, &state->vobsub.pix_buf_map,
          GST_MAP_READ))
    return;

  GST_DEBUG_OBJECT (dvdspu,
      "Rendering SPU. disp_rect %d,%d to %d,%d. hl_rect %d,%d to %d,%d",
      state->vobsub.disp_rect.left, state->vobsub.disp_rect.top,
      state->vobsub.disp_rect.right, state->vobsub.disp_rect.bottom,
      state->vobsub.hl_rect.left, state->vobsub.hl_rect.top,
      state->vobsub.hl_rect.right, state->vobsub.hl_rect.bottom);

  /* Update all the palette caches */
  gstspu_vobsub_update_palettes (dvdspu, state);

  /* Set up HL or Change Color & Contrast rect tracking */
  if (state->vobsub.hl_rect.top != -1) {
    state->vobsub.cur_chg_col = &state->vobsub.hl_ctrl_i;
    state->vobsub.cur_chg_col_end = state->vobsub.cur_chg_col + 1;
  } else if (state->vobsub.n_line_ctrl_i > 0) {
    state->vobsub.cur_chg_col = state->vobsub.line_ctrl_i;
    state->vobsub.cur_chg_col_end =
        state->vobsub.cur_chg_col + state->vobsub.n_line_ctrl_i;
  } else
    state->vobsub.cur_chg_col = NULL;

  /* We start rendering from the first line of the display rect */
  y = state->vobsub.disp_rect.top;
  last_y = state->vobsub.disp_rect.bottom;

  /* When reading RLE data, we track the offset in nibbles... */
  state->vobsub.max_offset = state->vobsub.pix_buf_map.size * 2;
  if (y & 1) {
    cur_offsets[1] = state->vobsub.pix_data[0] * 2;
    cur_offsets[0] = state->vobsub.pix_data[1] * 2;
  } else {
    cur_offsets[0] = state->vobsub.pix_data[0] * 2;
    cur_offsets[1] = state->vobsub.pix_data[1] * 2;
  }

  /* Render line by line */
  for (state->vobsub.cur_Y = y; state->vobsub.cur_Y <= last_y;
      state->vobsub.cur_Y++) {
    gstspu_vobsub_render_line (state, frame,
        &cur_offsets[state->vobsub.cur_Y & 1]);
  }

  /* for debugging purposes, draw a faint rectangle at the edges of the disp_rect */
  if ((dvdspu_debug_flags & GST_DVD_SPU_DEBUG_RENDER_RECTANGLE) != 0) {
    gstspu_vobsub_draw_highlight (state, frame, &state->vobsub.disp_rect);
  }
  /* For debugging purposes, draw a faint rectangle around the highlight rect */
  if ((dvdspu_debug_flags & GST_DVD_SPU_DEBUG_HIGHLIGHT_RECTANGLE) != 0
      && state->vobsub.hl_rect.top != -1) {
    gstspu_vobsub_draw_highlight (state, frame, &state->vobsub.hl_rect);
  }

  gst_buffer_unmap (state->vobsub.pix_buf, &state->vobsub.pix_buf_map);
}

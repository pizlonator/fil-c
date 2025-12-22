/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
#include <config.h>
#endif

#include "gsth264picture-private.h"
#include "gstcodecpicture-private.h"

#include <stdlib.h>

GST_DEBUG_CATEGORY_EXTERN (gst_h264_decoder_debug);
#define GST_CAT_DEFAULT gst_h264_decoder_debug

GST_DEFINE_MINI_OBJECT_TYPE (GstH264Picture, gst_h264_picture);

/**
 * gst_h264_picture_new:
 *
 * Create new #GstH264Picture
 *
 * Returns: a new #GstH264Picture
 */
GstH264Picture *
gst_h264_picture_new (void)
{
  GstH264Picture *pic;

  pic = g_new0 (GstH264Picture, 1);

  pic->top_field_order_cnt = G_MAXINT32;
  pic->bottom_field_order_cnt = G_MAXINT32;
  pic->field = GST_H264_PICTURE_FIELD_FRAME;

  gst_mini_object_init (GST_MINI_OBJECT_CAST (pic), 0,
      GST_TYPE_H264_PICTURE, NULL, NULL,
      (GstMiniObjectFreeFunction) gst_codec_picture_free);

  return pic;
}

struct _GstH264Dpb
{
  GArray *pic_list;
  gint max_num_frames;
  gint num_output_needed;
  guint32 max_num_reorder_frames;
  gint32 last_output_poc;
  gboolean last_output_non_ref;

  gboolean interlaced;
};

static void
gst_h264_dpb_init (GstH264Dpb * dpb)
{
  dpb->num_output_needed = 0;
  dpb->last_output_poc = G_MININT32;
  dpb->last_output_non_ref = FALSE;
}

/**
 * gst_h264_dpb_new: (skip)
 *
 * Create new #GstH264Dpb
 *
 * Returns: a new #GstH264Dpb
 */
GstH264Dpb *
gst_h264_dpb_new (void)
{
  GstH264Dpb *dpb;

  dpb = g_new0 (GstH264Dpb, 1);
  gst_h264_dpb_init (dpb);

  dpb->pic_list =
      g_array_sized_new (FALSE, TRUE, sizeof (GstH264Picture *),
      GST_H264_DPB_MAX_SIZE);
  g_array_set_clear_func (dpb->pic_list,
      (GDestroyNotify) gst_clear_h264_picture);

  return dpb;
}

/**
 * gst_h264_dpb_set_max_num_frames:
 * @dpb: a #GstH264Dpb
 * @max_num_frames: the maximum number of picture
 *
 * Set the number of maximum allowed frames to store
 *
 * Since: 1.20
 */
void
gst_h264_dpb_set_max_num_frames (GstH264Dpb * dpb, gint max_num_frames)
{
  g_return_if_fail (dpb != NULL);

  dpb->max_num_frames = max_num_frames;
}

/**
 * gst_h264_dpb_get_max_num_frames:
 * @dpb: a #GstH264Dpb
 *
 * Returns: the number of maximum frames
 *
 * Since: 1.20
 */
gint
gst_h264_dpb_get_max_num_frames (GstH264Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, 0);

  return dpb->max_num_frames;
}

/**
 * gst_h264_dpb_set_interlaced:
 * @dpb: a #GstH264Dpb
 * @interlaced: %TRUE if interlaced
 *
 * Since: 1.20
 */
void
gst_h264_dpb_set_interlaced (GstH264Dpb * dpb, gboolean interlaced)
{
  g_return_if_fail (dpb != NULL);

  dpb->interlaced = interlaced;
}

/**
 * gst_h264_dpb_get_interlaced:
 * @dpb: a #GstH264Dpb
 *
 * Returns: %TRUE if @dpb is configured for interlaced stream
 *
 * Since: 1.20
 */
gboolean
gst_h264_dpb_get_interlaced (GstH264Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, FALSE);

  return dpb->interlaced;
}

/**
 * gst_h264_dpb_get_last_output_poc:
 * @dpb: a #GstH264Dpb
 *
 * Returns: the last outputted picture order count
 *
 * Since: 1.24
 */
gint32
gst_h264_dpb_get_last_output_poc (GstH264Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, G_MININT32);

  return dpb->last_output_poc;
}

/**
 * gst_h264_dpb_free:
 * @dpb: a #GstH264Dpb to free
 *
 * Free the @dpb
 */
void
gst_h264_dpb_free (GstH264Dpb * dpb)
{
  g_return_if_fail (dpb != NULL);

  gst_h264_dpb_clear (dpb);
  g_array_unref (dpb->pic_list);
  g_free (dpb);
}

/**
 * gst_h264_dpb_clear:
 * @dpb: a #GstH264Dpb
 *
 * Clear all stored #GstH264Picture
 */
void
gst_h264_dpb_clear (GstH264Dpb * dpb)
{
  g_return_if_fail (dpb != NULL);

  g_array_set_size (dpb->pic_list, 0);
  gst_h264_dpb_init (dpb);
}

/**
 * gst_h264_dpb_set_max_num_reorder_frames:
 * @dpb: a #GstH264Dpb
 * @max_num_reorder_frames: the max number of reorder frames, which
 * should not exceed the max size of DPB.
 *
 * Since: 1.20
 */
void
gst_h264_dpb_set_max_num_reorder_frames (GstH264Dpb * dpb,
    guint32 max_num_reorder_frames)
{
  g_return_if_fail (dpb != NULL);
  g_return_if_fail (max_num_reorder_frames <= dpb->max_num_frames);

  dpb->max_num_reorder_frames = max_num_reorder_frames;
}

/**
 * gst_h264_dpb_get_max_num_reorder_frames:
 * @dpb: a #GstH264Dpb
 *
 * Returns: Maximum number of reorder frames
 *
 * Since: 1.22.2
 */
guint32
gst_h264_dpb_get_max_num_reorder_frames (GstH264Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, GST_H264_DPB_MAX_SIZE);

  return dpb->max_num_reorder_frames;
}

/**
 * gst_h264_dpb_add:
 * @dpb: a #GstH264Dpb
 * @picture: (transfer full): a #GstH264Picture
 *
 * Store the @picture
 */
void
gst_h264_dpb_add (GstH264Dpb * dpb, GstH264Picture * picture)
{
  g_return_if_fail (dpb != NULL);
  g_return_if_fail (GST_IS_H264_PICTURE (picture));

  /* C.4.2 Decoding of gaps in frame_num and storage of "non-existing" pictures
   *
   * The "non-existing" frame is stored in an empty frame buffer and is marked
   * as "not needed for output", and the DPB fullness is incremented by one */
  if (!picture->nonexisting) {
    picture->needed_for_output = TRUE;

    if (GST_H264_PICTURE_IS_FRAME (picture)) {
      dpb->num_output_needed++;
    } else {
      /* We can do output only when field pair are complete */
      if (picture->second_field) {
        dpb->num_output_needed++;
      }
    }
  } else {
    picture->needed_for_output = FALSE;
  }

  /* Link each field */
  if (picture->second_field && picture->other_field) {
    picture->other_field->other_field = picture;
  }

  g_array_append_val (dpb->pic_list, picture);

  if (dpb->pic_list->len > dpb->max_num_frames * (dpb->interlaced + 1))
    GST_ERROR ("DPB size is %d, exceed the max size %d",
        dpb->pic_list->len, dpb->max_num_frames * (dpb->interlaced + 1));

  /* The IDR frame or mem_mgmt_5 */
  if (picture->pic_order_cnt == 0) {
    GST_TRACE ("last_output_poc reset because of IDR or mem_mgmt_5");
    dpb->last_output_poc = G_MININT32;
    dpb->last_output_non_ref = FALSE;
  }
}

/**
 * gst_h264_dpb_delete_unused:
 * @dpb: a #GstH264Dpb
 *
 * Delete already outputted and not referenced all pictures from dpb
 */
void
gst_h264_dpb_delete_unused (GstH264Dpb * dpb)
{
  gint i;

  g_return_if_fail (dpb != NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    /* NOTE: don't use g_array_remove_index_fast here since the last picture
     * need to be referenced for bumping decision */
    if (!picture->needed_for_output && !GST_H264_PICTURE_IS_REF (picture)) {
      GST_TRACE
          ("remove picture %p (frame num: %d, poc: %d, field: %d) from dpb",
          picture, picture->frame_num, picture->pic_order_cnt, picture->field);
      g_array_remove_index (dpb->pic_list, i);
      i--;
    }
  }
}

/**
 * gst_h264_dpb_num_ref_frames:
 * @dpb: a #GstH264Dpb
 *
 * Returns: The number of referenced frames
 *
 * Since: 1.20
 */
gint
gst_h264_dpb_num_ref_frames (GstH264Dpb * dpb)
{
  gint i;
  gint ret = 0;

  g_return_val_if_fail (dpb != NULL, -1);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    /* Count frame, not field picture */
    if (picture->second_field)
      continue;

    if (GST_H264_PICTURE_IS_REF (picture))
      ret++;
  }

  return ret;
}

/**
 * gst_h264_dpb_mark_all_non_ref:
 * @dpb: a #GstH264Dpb
 *
 * Mark all pictures are not referenced
 */
void
gst_h264_dpb_mark_all_non_ref (GstH264Dpb * dpb)
{
  gint i;

  g_return_if_fail (dpb != NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    gst_h264_picture_set_reference (picture, GST_H264_PICTURE_REF_NONE, FALSE);
  }
}

/**
 * gst_h264_dpb_get_short_ref_by_pic_num:
 * @dpb: a #GstH264Dpb
 * @pic_num: a picture number
 *
 * Find a short term reference picture which has matching picture number
 *
 * Returns: (nullable) (transfer none): a #GstH264Picture
 */
GstH264Picture *
gst_h264_dpb_get_short_ref_by_pic_num (GstH264Dpb * dpb, gint pic_num)
{
  gint i;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    if (GST_H264_PICTURE_IS_SHORT_TERM_REF (picture)
        && picture->pic_num == pic_num)
      return picture;
  }

  GST_WARNING ("No short term reference picture for %d", pic_num);

  return NULL;
}

/**
 * gst_h264_dpb_get_long_ref_by_long_term_pic_num:
 * @dpb: a #GstH264Dpb
 * @long_term_pic_num: a long term picture number
 *
 * Find a long term reference picture which has matching long term picture number
 *
 * Returns: (nullable) (transfer none): a #GstH264Picture
 *
 * Since: 1.20
 */
GstH264Picture *
gst_h264_dpb_get_long_ref_by_long_term_pic_num (GstH264Dpb * dpb,
    gint long_term_pic_num)
{
  gint i;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    if (GST_H264_PICTURE_IS_LONG_TERM_REF (picture) &&
        picture->long_term_pic_num == long_term_pic_num)
      return picture;
  }

  GST_WARNING ("No long term reference picture for %d", long_term_pic_num);

  return NULL;
}

/**
 * gst_h264_dpb_get_lowest_frame_num_short_ref:
 * @dpb: a #GstH264Dpb
 *
 * Find a short term reference picture which has the lowest frame_num_wrap
 *
 * Returns: (transfer full) (nullable): a #GstH264Picture
 */
GstH264Picture *
gst_h264_dpb_get_lowest_frame_num_short_ref (GstH264Dpb * dpb)
{
  gint i;
  GstH264Picture *ret = NULL;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    if (GST_H264_PICTURE_IS_SHORT_TERM_REF (picture) &&
        (!ret || picture->frame_num_wrap < ret->frame_num_wrap))
      ret = picture;
  }

  if (ret)
    gst_h264_picture_ref (ret);

  return ret;
}

/**
 * gst_h264_dpb_get_pictures_short_term_ref:
 * @dpb: a #GstH264Dpb
 * @include_non_existing: %TRUE if non-existing pictures need to be included
 * @include_second_field: %TRUE if the second field pictures need to be included
 * @out: (out) (element-type GstH264Picture) (transfer full): an array
 *   of #GstH264Picture pointers
 *
 * Retrieve all short-term reference pictures from @dpb. The picture will be
 * appended to the array.
 *
 * Since: 1.20
 */
void
gst_h264_dpb_get_pictures_short_term_ref (GstH264Dpb * dpb,
    gboolean include_non_existing, gboolean include_second_field, GArray * out)
{
  gint i;

  g_return_if_fail (dpb != NULL);
  g_return_if_fail (out != NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    if (!include_second_field && picture->second_field)
      continue;

    if (GST_H264_PICTURE_IS_SHORT_TERM_REF (picture) &&
        (include_non_existing || (!include_non_existing &&
                !picture->nonexisting))) {
      gst_h264_picture_ref (picture);
      g_array_append_val (out, picture);
    }
  }
}

/**
 * gst_h264_dpb_get_pictures_long_term_ref:
 * @dpb: a #GstH264Dpb
 * @include_second_field: %TRUE if the second field pictures need to be included
 * @out: (out) (element-type GstH264Picture) (transfer full): an array
 *   of #GstH264Picture pointer
 *
 * Retrieve all long-term reference pictures from @dpb. The picture will be
 * appended to the array.
 *
 * Since: 1.20
 */
void
gst_h264_dpb_get_pictures_long_term_ref (GstH264Dpb * dpb,
    gboolean include_second_field, GArray * out)
{
  gint i;

  g_return_if_fail (dpb != NULL);
  g_return_if_fail (out != NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    if (!include_second_field && picture->second_field)
      continue;

    if (GST_H264_PICTURE_IS_LONG_TERM_REF (picture)) {
      gst_h264_picture_ref (picture);
      g_array_append_val (out, picture);
    }
  }
}

/**
 * gst_h264_dpb_get_pictures_all:
 * @dpb: a #GstH264Dpb
 *
 * Return: (element-type GstH264Picture) (transfer full): a #GArray of
 *   #GstH264Picture stored in @dpb
 */
GArray *
gst_h264_dpb_get_pictures_all (GstH264Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, NULL);

  return g_array_ref (dpb->pic_list);
}

/**
 * gst_h264_dpb_get_size:
 * @dpb: a #GstH264Dpb
 *
 * Return: the length of stored dpb array
 */
gint
gst_h264_dpb_get_size (GstH264Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, -1);

  return dpb->pic_list->len;
}

/**
 * gst_h264_dpb_get_picture:
 * @dpb: a #GstH264Dpb
 * @system_frame_number The system frame number
 *
 * Returns: (transfer full) (nullable): the picture identified with the specified
 * @system_frame_number, or %NULL if DPB does not contain a #GstH264Picture
 * corresponding to the @system_frame_number
 *
 * Since: 1.18
 */
GstH264Picture *
gst_h264_dpb_get_picture (GstH264Dpb * dpb, guint32 system_frame_number)
{
  gint i;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    if (GST_CODEC_PICTURE_FRAME_NUMBER (picture) == system_frame_number) {
      gst_h264_picture_ref (picture);
      return picture;
    }
  }

  return NULL;
}

/**
 * gst_h264_dpb_has_empty_frame_buffer:
 * @dpb: a #GstH264Dpb
 *
 * Returns: %TRUE if @dpb still has empty frame buffers.
 *
 * Since: 1.20
 */
gboolean
gst_h264_dpb_has_empty_frame_buffer (GstH264Dpb * dpb)
{
  if (!dpb->interlaced) {
    if (dpb->pic_list->len < dpb->max_num_frames)
      return TRUE;
  } else {
    gint i;
    gint count = 0;
    /* Count the number of complementary field pairs */
    for (i = 0; i < dpb->pic_list->len; i++) {
      GstH264Picture *picture =
          g_array_index (dpb->pic_list, GstH264Picture *, i);

      if (picture->second_field)
        continue;

      if (GST_H264_PICTURE_IS_FRAME (picture) || picture->other_field)
        count++;
    }

    if (count < dpb->max_num_frames)
      return TRUE;
  }

  return FALSE;
}

static gint
gst_h264_dpb_get_lowest_output_needed_picture (GstH264Dpb * dpb, gboolean force,
    GstH264Picture ** picture)
{
  gint i;
  GstH264Picture *lowest = NULL;
  gint index = -1;

  *picture = NULL;

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH264Picture *picture =
        g_array_index (dpb->pic_list, GstH264Picture *, i);

    if (!force && !picture->needed_for_output)
      continue;

    if (!GST_H264_PICTURE_IS_FRAME (picture) &&
        (!picture->other_field || picture->second_field))
      continue;

    if (!lowest) {
      lowest = picture;
      index = i;
      continue;
    }

    if (picture->pic_order_cnt < lowest->pic_order_cnt) {
      lowest = picture;
      index = i;
    }
  }

  if (lowest)
    *picture = gst_h264_picture_ref (lowest);

  return index;
}

/**
 * gst_h264_dpb_needs_bump:
 * @dpb: a #GstH264Dpb
 * @to_insert: the current #GstH264Picture to insert to dpb.
 * @latency_mode: The required #GstH264DpbBumpMode for bumping.
 *
 * Returns: %TRUE if bumping is required
 *
 * Since: 1.20
 */
gboolean
gst_h264_dpb_needs_bump (GstH264Dpb * dpb, GstH264Picture * to_insert,
    GstH264DpbBumpMode latency_mode)
{
  GstH264Picture *picture = NULL;
  gint32 lowest_poc;
  gboolean is_ref_picture;
  gint lowest_index;

  g_return_val_if_fail (dpb != NULL, FALSE);
  g_assert (dpb->num_output_needed >= 0);

  lowest_poc = G_MAXINT32;
  is_ref_picture = FALSE;
  lowest_index = gst_h264_dpb_get_lowest_output_needed_picture (dpb,
      FALSE, &picture);
  if (lowest_index >= 0) {
    lowest_poc = picture->pic_order_cnt;
    is_ref_picture = picture->ref_pic;
    gst_h264_picture_unref (picture);
  } else {
    goto normal_bump;
  }

  if (latency_mode >= GST_H264_DPB_BUMP_LOW_LATENCY) {
    /* If low latency, we should not wait for the DPB becoming full.
       We try to bump the picture as soon as possible without the
       frames disorder. The policy is from the safe to some risk. */

    /* Do not support interlaced mode. */
    if (gst_h264_dpb_get_interlaced (dpb))
      goto normal_bump;

    /* Equal to normal bump. */
    if (!gst_h264_dpb_has_empty_frame_buffer (dpb))
      goto normal_bump;

    /* In case of POC type 2, decoding order is equal to output order */
    if (picture->pic_order_cnt_type == 2) {
      GST_TRACE ("POC type == 2, bumping");
      return TRUE;
    }

    /* 7.4.1.2.2: The values of picture order count for the coded pictures
       in consecutive access units in decoding order containing non-reference
       pictures shall be non-decreasing. Safe. */
    if (dpb->last_output_non_ref && !is_ref_picture) {
      g_assert (dpb->last_output_poc < G_MAXINT32);
      GST_TRACE ("Continuous non-reference frame poc: %d -> %d,"
          " bumping for low-latency.", dpb->last_output_poc, lowest_poc);
      return TRUE;
    }

    /* num_reorder_frames indicates the maximum number of frames, that
       precede any frame in the coded video sequence in decoding order
       and follow it in output order. Safe. */
    if (lowest_index >= dpb->max_num_reorder_frames) {
      guint i, need_output;

      need_output = 0;
      for (i = 0; i < lowest_index; i++) {
        GstH264Picture *p = g_array_index (dpb->pic_list, GstH264Picture *, i);
        if (p->needed_for_output)
          need_output++;
      }

      if (need_output >= dpb->max_num_reorder_frames) {
        GST_TRACE ("frame with lowest poc %d has %d precede frame, already"
            " satisfy num_reorder_frames %d, bumping for low-latency.",
            dpb->last_output_poc, lowest_index, dpb->max_num_reorder_frames);
        return TRUE;
      }
    }

    /* Bump leading picture with the negative POC if already found positive
       POC. It's even impossible to insert another negative POC after the
       positive POCs. Almost safe. */
    if (to_insert && to_insert->pic_order_cnt > 0 && lowest_poc < 0) {
      GST_TRACE ("The negative poc %d, bumping for low-latency.", lowest_poc);
      return TRUE;
    }

    /* There may be leading frames with negative POC following the IDR
       frame in decoder order, so when IDR comes, we need to check the
       following pictures. In most cases, leading pictures are in increasing
       POC order. Bump and should be safe. */
    if (lowest_poc == 0 && gst_h264_dpb_get_size (dpb) <= 1) {
      if (to_insert && to_insert->pic_order_cnt > lowest_poc) {
        GST_TRACE ("The IDR or mem_mgmt_5 frame, bumping for low-latency.");
        return TRUE;
      }

      GST_TRACE ("The IDR or mem_mgmt_5 frame is not the first frame.");
      goto normal_bump;
    }

    /* When non-ref frame has the lowest POC, it's unlike to insert another
       ref frame with very small POC. Bump and should be safe. */
    if (!is_ref_picture) {
      GST_TRACE ("non ref with lowest-poc: %d bumping for low-latency",
          lowest_poc);
      return TRUE;
    }

    /* When insert non-ref frame with bigger POC, it's unlike to insert
       another ref frame with very small POC. Bump and should be safe. */
    if (to_insert && !to_insert->ref_pic
        && lowest_poc < to_insert->pic_order_cnt) {
      GST_TRACE ("lowest-poc: %d < to insert non ref pic: %d, bumping "
          "for low-latency", lowest_poc, to_insert->pic_order_cnt);
      return TRUE;
    }

    if (latency_mode >= GST_H264_DPB_BUMP_VERY_LOW_LATENCY) {
      /* PicOrderCnt increment by <=2. Not all streams meet this, but in
         practice this condition can be used.
         For stream with 2 poc increment like:
         0(IDR), 2(P), 4(P), 6(P), 12(P), 8(B), 10(B)....
         This can work well, but for streams with 1 poc increment like:
         0(IDR), 2(P), 4(P), 1(B), 3(B) ...
         This can cause picture disorder. Most stream in practice has the
         2 poc increment, but this may have risk and be careful. */
      if (lowest_poc > dpb->last_output_poc
          && lowest_poc - dpb->last_output_poc <= 2) {
        GST_TRACE ("lowest-poc: %d, last-output-poc: %d, diff <= 2, "
            "bumping for very-low-latency", lowest_poc, dpb->last_output_poc);
        return TRUE;
      }
    }
  }

normal_bump:
  /* C.4.5.3: The "bumping" process is invoked in the following cases.
     - There is no empty frame buffer and a empty frame buffer is needed
     for storage of an inferred "non-existing" frame.
     - There is no empty frame buffer and an empty frame buffer is needed
     for storage of a decoded (non-IDR) reference picture.
     - There is no empty frame buffer and the current picture is a non-
     reference picture that is not the second field of a complementary
     non-reference field pair and there are pictures in the DPB that are
     marked as "needed for output" that precede the current non-reference
     picture in output order. */
  if (gst_h264_dpb_has_empty_frame_buffer (dpb)) {
    GST_TRACE ("DPB has empty frame buffer, no need bumping.");
    return FALSE;
  }

  if (to_insert && to_insert->ref_pic) {
    GST_TRACE ("No empty frame buffer for ref frame, need bumping.");
    return TRUE;
  }

  if (to_insert && to_insert->pic_order_cnt > lowest_poc) {
    GST_TRACE ("No empty frame buffer, lowest poc %d < current poc %d,"
        " need bumping.", lowest_poc, to_insert->pic_order_cnt);
    return TRUE;
  }

  if (to_insert) {
    GST_TRACE ("No empty frame buffer, but lowest poc %d > current poc %d,"
        " no need bumping.", lowest_poc, to_insert->pic_order_cnt);
  }

  return FALSE;
}

/**
 * gst_h264_dpb_bump:
 * @dpb: a #GstH265Dpb
 * @drain: whether draining or not
 *
 * Perform bumping process as defined in C.4.5.3 "Bumping" process.
 * If @drain is %TRUE, @dpb will remove a #GstH264Picture from internal array
 * so that returned #GstH264Picture could hold the last reference of it
 *
 * Returns: (nullable) (transfer full): a #GstH264Picture which is needed to be
 * outputted
 *
 * Since: 1.20
 */
GstH264Picture *
gst_h264_dpb_bump (GstH264Dpb * dpb, gboolean drain)
{
  GstH264Picture *picture;
  GstH264Picture *other_picture;
  gint i;
  gint index;
  gboolean output_needed = TRUE;

  g_return_val_if_fail (dpb != NULL, NULL);

  index = gst_h264_dpb_get_lowest_output_needed_picture (dpb, FALSE, &picture);
  /* Bumping is needed but has no output needed pictures. Pick the smallest
   * POC picture */
  if (!picture && !drain) {
    index = gst_h264_dpb_get_lowest_output_needed_picture (dpb, TRUE, &picture);
    if (picture)
      output_needed = FALSE;
  }

  if (!picture || index < 0)
    return NULL;

  picture->needed_for_output = FALSE;

  if (output_needed)
    dpb->num_output_needed--;

  g_assert (dpb->num_output_needed >= 0);

  /* NOTE: don't use g_array_remove_index_fast here since the last picture
   * need to be referenced for bumping decision */
  if (!GST_H264_PICTURE_IS_REF (picture) || drain ||
      /* Or in case of emergency bumping, remove this picture from dpb as well */
      !output_needed) {
    g_array_remove_index (dpb->pic_list, index);
  }

  other_picture = picture->other_field;
  if (other_picture) {
    other_picture->needed_for_output = FALSE;

    /* At this moment, this picture should be interlaced */
    picture->buffer_flags |= GST_VIDEO_BUFFER_FLAG_INTERLACED;

    /* FIXME: need to check picture timing SEI for the case where top/bottom poc
     * are identical */
    if (picture->pic_order_cnt < other_picture->pic_order_cnt)
      picture->buffer_flags |= GST_VIDEO_BUFFER_FLAG_TFF;

    if (!other_picture->ref) {
      for (i = 0; i < dpb->pic_list->len; i++) {
        GstH264Picture *tmp =
            g_array_index (dpb->pic_list, GstH264Picture *, i);

        if (tmp == other_picture) {
          g_array_remove_index (dpb->pic_list, i);
          break;
        }
      }
    }
    /* Now other field may or may not exist */
  }

  dpb->last_output_poc = picture->pic_order_cnt;
  dpb->last_output_non_ref = !picture->ref_pic;

  return picture;
}

/**
 * gst_h264_dpb_set_last_output:
 * @dpb: a #GstH264Dpb
 * @picture: a #GstH264Picture of the last output.
 *
 * Notify the DPB that @picture is output directly without storing
 * in the DPB.
 *
 * Since: 1.20
 */
void
gst_h264_dpb_set_last_output (GstH264Dpb * dpb, GstH264Picture * picture)
{
  g_return_if_fail (dpb != NULL);
  g_return_if_fail (GST_IS_H264_PICTURE (picture));

  dpb->last_output_poc = picture->pic_order_cnt;
  dpb->last_output_non_ref = !picture->ref_pic;
}

static gint
get_picNumX (GstH264Picture * picture, GstH264RefPicMarking * ref_pic_marking)
{
  return picture->pic_num -
      (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
}

/**
 * gst_h264_dpb_perform_memory_management_control_operation:
 * @dpb: a #GstH265Dpb
 * @ref_pic_marking: a #GstH264RefPicMarking
 * @picture: a #GstH264Picture
 *
 * Perform "8.2.5.4 Adaptive memory control decoded reference picture marking process"
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.20
 */
gboolean
gst_h264_dpb_perform_memory_management_control_operation (GstH264Dpb * dpb,
    GstH264RefPicMarking * ref_pic_marking, GstH264Picture * picture)
{
  guint8 type;
  gint pic_num_x;
  gint max_long_term_frame_idx;
  GstH264Picture *other;
  gint i;

  g_return_val_if_fail (dpb != NULL, FALSE);
  g_return_val_if_fail (ref_pic_marking != NULL, FALSE);
  g_return_val_if_fail (picture != NULL, FALSE);

  type = ref_pic_marking->memory_management_control_operation;

  switch (type) {
    case 0:
      /* Normal end of operations' specification */
      break;
    case 1:
      /* 8.2.5.4.1 Mark a short term reference picture as unused so it can be
       * removed if outputted */
      pic_num_x = get_picNumX (picture, ref_pic_marking);
      other = gst_h264_dpb_get_short_ref_by_pic_num (dpb, pic_num_x);
      if (other) {
        gst_h264_picture_set_reference (other,
            GST_H264_PICTURE_REF_NONE, GST_H264_PICTURE_IS_FRAME (picture));
        GST_TRACE ("MMCO-1: unmark short-term ref picture %p, (poc %d)",
            other, other->pic_order_cnt);
      } else {
        GST_WARNING ("Invalid picNumX %d for operation type 1", pic_num_x);
        return FALSE;
      }
      break;
    case 2:
      /* 8.2.5.4.2 Mark a long term reference picture as unused so it can be
       * removed if outputted */
      other = gst_h264_dpb_get_long_ref_by_long_term_pic_num (dpb,
          ref_pic_marking->long_term_pic_num);
      if (other) {
        gst_h264_picture_set_reference (other,
            GST_H264_PICTURE_REF_NONE, FALSE);
        GST_TRACE ("MMCO-2: unmark long-term ref picture %p, (poc %d)",
            other, other->pic_order_cnt);
      } else {
        GST_WARNING ("Invalid LongTermPicNum %d for operation type 2",
            ref_pic_marking->long_term_pic_num);
        return FALSE;
      }
      break;
    case 3:
      /* 8.2.5.4.3 Mark a short term reference picture as long term reference */

      pic_num_x = get_picNumX (picture, ref_pic_marking);

      other = gst_h264_dpb_get_short_ref_by_pic_num (dpb, pic_num_x);
      if (!other) {
        GST_WARNING ("Invalid picNumX %d for operation type 3", pic_num_x);
        return FALSE;
      }

      /* If we have long-term ref picture for LongTermFrameIdx,
       * mark the picture as non-reference */
      for (i = 0; i < dpb->pic_list->len; i++) {
        GstH264Picture *tmp =
            g_array_index (dpb->pic_list, GstH264Picture *, i);

        if (GST_H264_PICTURE_IS_LONG_TERM_REF (tmp)
            && tmp->long_term_frame_idx == ref_pic_marking->long_term_frame_idx) {
          if (GST_H264_PICTURE_IS_FRAME (tmp)) {
            /* When long_term_frame_idx is already assigned to a long-term
             * reference frame, that frame is marked as "unused for reference"
             */
            gst_h264_picture_set_reference (tmp,
                GST_H264_PICTURE_REF_NONE, TRUE);
            GST_TRACE ("MMCO-3: unmark old long-term frame %p (poc %d)",
                tmp, tmp->pic_order_cnt);
          } else if (tmp->other_field &&
              GST_H264_PICTURE_IS_LONG_TERM_REF (tmp->other_field) &&
              tmp->other_field->long_term_frame_idx ==
              ref_pic_marking->long_term_frame_idx) {
            /* When long_term_frame_idx is already assigned to a long-term
             * reference field pair, that complementary field pair and both of
             * its fields are marked as "unused for reference"
             */
            gst_h264_picture_set_reference (tmp,
                GST_H264_PICTURE_REF_NONE, TRUE);
            GST_TRACE ("MMCO-3: unmark old long-term field-pair %p (poc %d)",
                tmp, tmp->pic_order_cnt);
          } else {
            /* When long_term_frame_idx is already assigned to a reference field,
             * and that reference field is not part of a complementary field
             * pair that includes the picture specified by picNumX,
             * that field is marked as "unused for reference"
             */

            /* Check "tmp" (a long-term ref pic) is part of
             * "other" (a picture to be updated from short-term to long-term)
             * complementary field pair */

            /* NOTE: "other" here is short-ref, so "other" and "tmp" must not be
             * identical picture */
            if (!tmp->other_field) {
              gst_h264_picture_set_reference (tmp,
                  GST_H264_PICTURE_REF_NONE, FALSE);
              GST_TRACE ("MMCO-3: unmark old long-term field %p (poc %d)",
                  tmp, tmp->pic_order_cnt);
            } else if (tmp->other_field != other &&
                (!other->other_field || other->other_field != tmp)) {
              gst_h264_picture_set_reference (tmp,
                  GST_H264_PICTURE_REF_NONE, FALSE);
              GST_TRACE ("MMCO-3: unmark old long-term field %p (poc %d)",
                  tmp, tmp->pic_order_cnt);
            }
          }
          break;
        }
      }

      gst_h264_picture_set_reference (other,
          GST_H264_PICTURE_REF_LONG_TERM, GST_H264_PICTURE_IS_FRAME (picture));
      other->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;

      GST_TRACE ("MMCO-3: mark long-term ref pic %p, index %d, (poc %d)",
          other, other->long_term_frame_idx, other->pic_order_cnt);

      if (other->other_field &&
          GST_H264_PICTURE_IS_LONG_TERM_REF (other->other_field)) {
        other->other_field->long_term_frame_idx =
            ref_pic_marking->long_term_frame_idx;
      }
      break;
    case 4:
      /* 8.2.5.4.4  All pictures for which LongTermFrameIdx is greater than
       * max_long_term_frame_idx_plus1 − 1 and that are marked as
       * "used for long-term reference" are marked as "unused for reference */
      max_long_term_frame_idx =
          ref_pic_marking->max_long_term_frame_idx_plus1 - 1;

      GST_TRACE ("MMCO-4: max_long_term_frame_idx %d", max_long_term_frame_idx);

      for (i = 0; i < dpb->pic_list->len; i++) {
        other = g_array_index (dpb->pic_list, GstH264Picture *, i);

        if (GST_H264_PICTURE_IS_LONG_TERM_REF (other) &&
            other->long_term_frame_idx > max_long_term_frame_idx) {
          gst_h264_picture_set_reference (other,
              GST_H264_PICTURE_REF_NONE, FALSE);
          GST_TRACE ("MMCO-4: unmark long-term ref pic %p, index %d, (poc %d)",
              other, other->long_term_frame_idx, other->pic_order_cnt);
        }
      }
      break;
    case 5:
      /* 8.2.5.4.5 Unmark all reference pictures */
      for (i = 0; i < dpb->pic_list->len; i++) {
        other = g_array_index (dpb->pic_list, GstH264Picture *, i);
        gst_h264_picture_set_reference (other,
            GST_H264_PICTURE_REF_NONE, FALSE);
      }
      picture->mem_mgmt_5 = TRUE;
      picture->frame_num = 0;
      /* When the current picture includes a memory management control operation
         equal to 5, after the decoding of the current picture, tempPicOrderCnt
         is set equal to PicOrderCnt( CurrPic ), TopFieldOrderCnt of the current
         picture (if any) is set equal to TopFieldOrderCnt - tempPicOrderCnt,
         and BottomFieldOrderCnt of the current picture (if any) is set equal to
         BottomFieldOrderCnt - tempPicOrderCnt. */
      if (picture->field == GST_H264_PICTURE_FIELD_TOP_FIELD) {
        picture->top_field_order_cnt = picture->pic_order_cnt = 0;
      } else if (picture->field == GST_H264_PICTURE_FIELD_BOTTOM_FIELD) {
        picture->bottom_field_order_cnt = picture->pic_order_cnt = 0;
      } else {
        picture->top_field_order_cnt -= picture->pic_order_cnt;
        picture->bottom_field_order_cnt -= picture->pic_order_cnt;
        picture->pic_order_cnt = MIN (picture->top_field_order_cnt,
            picture->bottom_field_order_cnt);
      }
      break;
    case 6:
      /* 8.2.5.4.6 Replace long term reference pictures with current picture.
       * First unmark if any existing with this long_term_frame_idx */

      /* If we have long-term ref picture for LongTermFrameIdx,
       * mark the picture as non-reference */
      for (i = 0; i < dpb->pic_list->len; i++) {
        other = g_array_index (dpb->pic_list, GstH264Picture *, i);

        if (GST_H264_PICTURE_IS_LONG_TERM_REF (other) &&
            other->long_term_frame_idx ==
            ref_pic_marking->long_term_frame_idx) {
          GST_TRACE ("MMCO-6: unmark old long-term ref pic %p (poc %d)",
              other, other->pic_order_cnt);
          gst_h264_picture_set_reference (other,
              GST_H264_PICTURE_REF_NONE, TRUE);
          break;
        }
      }

      gst_h264_picture_set_reference (picture,
          GST_H264_PICTURE_REF_LONG_TERM, picture->second_field);
      picture->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
      if (picture->other_field &&
          GST_H264_PICTURE_IS_LONG_TERM_REF (picture->other_field)) {
        picture->other_field->long_term_frame_idx =
            ref_pic_marking->long_term_frame_idx;
      }
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

/**
 * gst_h264_picture_set_reference:
 * @picture: a #GstH264Picture
 * @reference: a GstH264PictureReference
 * @other_field: %TRUE if @reference needs to be applied to the
 * other field if any
 *
 * Update reference picture type of @picture with @reference
 *
 * Since: 1.20
 */
void
gst_h264_picture_set_reference (GstH264Picture * picture,
    GstH264PictureReference reference, gboolean other_field)
{
  g_return_if_fail (picture != NULL);

  picture->ref = reference;
  if (reference > GST_H264_PICTURE_REF_NONE)
    picture->ref_pic = TRUE;

  if (other_field && picture->other_field) {
    picture->other_field->ref = reference;

    if (reference > GST_H264_PICTURE_REF_NONE)
      picture->other_field->ref_pic = TRUE;
  }
}

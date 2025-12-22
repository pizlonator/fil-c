/* GStreamer
 * Copyright (C) 2020 He Junyan <junyan.he@intel.com>
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

#include "gstav1picture.h"
#include "gstcodecpicture-private.h"

GST_DEBUG_CATEGORY_EXTERN (gst_av1_decoder_debug);
#define GST_CAT_DEFAULT gst_av1_decoder_debug

GST_DEFINE_MINI_OBJECT_TYPE (GstAV1Picture, gst_av1_picture);

/**
 * gst_av1_picture_new:
 *
 * Create new #GstAV1Picture
 *
 * Returns: a new #GstAV1Picture
 *
 * Since: 1.20
 */
GstAV1Picture *
gst_av1_picture_new (void)
{
  GstAV1Picture *pic;

  pic = g_new0 (GstAV1Picture, 1);

  gst_mini_object_init (GST_MINI_OBJECT_CAST (pic), 0,
      GST_TYPE_AV1_PICTURE, NULL, NULL,
      (GstMiniObjectFreeFunction) gst_codec_picture_free);

  GST_TRACE ("New picture %p", pic);

  return pic;
}

/**
 * gst_av1_dpb_new: (skip)
 *
 * Create new #GstAV1Dpb
 *
 * Returns: a new #GstAV1Dpb
 *
 * Since: 1.20
 */
GstAV1Dpb *
gst_av1_dpb_new (void)
{
  GstAV1Dpb *dpb;

  dpb = g_new0 (GstAV1Dpb, 1);

  return dpb;
}

/**
 * gst_av1_dpb_free:
 * @dpb: a #GstAV1Dpb to free
 *
 * Free the @dpb
 *
 * Since: 1.20
 */
void
gst_av1_dpb_free (GstAV1Dpb * dpb)
{
  g_return_if_fail (dpb != NULL);

  gst_av1_dpb_clear (dpb);
  g_free (dpb);
}

/**
 * gst_av1_dpb_clear:
 * @dpb: a #GstAV1Dpb
 *
 * Clear all stored #GstAV1Picture
 *
 * Since: 1.20
 */
void
gst_av1_dpb_clear (GstAV1Dpb * dpb)
{
  gint i;

  g_return_if_fail (dpb != NULL);

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++)
    gst_clear_av1_picture (&dpb->pic_list[i]);
}

/**
 * gst_av1_dpb_add:
 * @dpb: a #GstAV1Dpb
 * @picture: (transfer full): a #GstAV1Picture
 *
 * Store the @picture
 *
 * Since: 1.20
 */
void
gst_av1_dpb_add (GstAV1Dpb * dpb, GstAV1Picture * picture)
{
  GstAV1FrameHeaderOBU *fh;
  guint i;

  g_return_if_fail (dpb != NULL);
  g_return_if_fail (GST_IS_AV1_PICTURE (picture));

  fh = &picture->frame_hdr;

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if ((fh->refresh_frame_flags >> i) & 1) {
      GST_TRACE ("reference frame %p to ref slot:%d", picture, i);
      gst_av1_picture_replace (&dpb->pic_list[i], picture);
    }
  }

  gst_av1_picture_unref (picture);
}

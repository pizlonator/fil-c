/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstcodecpicture.h"
#include "gstcodecpicture-private.h"

void
gst_codec_picture_free (GstCodecPicture * picture)
{
  if (picture->notify)
    picture->notify (picture->user_data);

  if (picture->discont_state)
    gst_video_codec_state_unref (picture->discont_state);

  g_free (picture);
}

/**
 * gst_codec_picture_set_user_data:
 * @picture: a #GstCodecPicture
 * @user_data: (nullable): private data
 * @notify: (closure user_data): a #GDestroyNotify
 *
 * Sets @user_data on the picture and the #GDestroyNotify that will be called when
 * the picture is freed.
 *
 * If a @user_data was previously set, then the previous set @notify will be called
 * before the @user_data is replaced.
 *
 * Since: 1.24
 */
void
gst_codec_picture_set_user_data (GstCodecPicture * picture, gpointer user_data,
    GDestroyNotify notify)
{
  g_return_if_fail (picture);

  if (picture->notify)
    picture->notify (picture->user_data);

  picture->notify = notify;
  picture->user_data = user_data;
}

/**
 * gst_codec_picture_get_user_data:
 * @picture: a #GstCodecPicture
 *
 * Gets private data set on the picture via
 * gst_codec_picture_set_user_data() previously.
 *
 * Returns: (transfer none) (nullable): The previously set user_data
 *
 * Since: 1.24
 */
gpointer
gst_codec_picture_get_user_data (GstCodecPicture * picture)
{
  g_return_val_if_fail (picture, NULL);

  return picture->user_data;
}

/**
 * gst_codec_picture_set_discont_state:
 * @picture: a #GstCodecPicture
 * @discont_state: (transfer none) (allow-none): a #GstVideoCodecState
 *
 * Sets @discont_state to @picture
 *
 * Since: 1.24
 */
void
gst_codec_picture_set_discont_state (GstCodecPicture * picture,
    GstVideoCodecState * discont_state)
{
  g_return_if_fail (picture);

  g_clear_pointer (&picture->discont_state, gst_video_codec_state_unref);

  if (discont_state)
    picture->discont_state = gst_video_codec_state_ref (discont_state);
}

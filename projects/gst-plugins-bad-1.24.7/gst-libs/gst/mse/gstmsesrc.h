/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2013, 2022, 2023 Collabora Ltd.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014, 2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016, 2018, 2019, 2020, 2021 Igalia, S.L
 * Copyright (C) 2015, 2016, 2018, 2019, 2020, 2021 Metrological Group B.V.
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

#pragma once

#include <gst/gst.h>
#include <gst/mse/mse-prelude.h>

G_BEGIN_DECLS

/**
 * GstMseSrcReadyState:
 * @GST_MSE_SRC_READY_STATE_HAVE_NOTHING: No information is available about the
 * stream
 * @GST_MSE_SRC_READY_STATE_HAVE_METADATA: The duration is available and video
 * dimensions are available if the stream contains video
 * @GST_MSE_SRC_READY_STATE_HAVE_CURRENT_DATA: The current playback position can
 * be presented but future information is not available
 * @GST_MSE_SRC_READY_STATE_HAVE_FUTURE_DATA: There is data for the current
 * position and some amount in the future and any text tracks are ready.
 * @GST_MSE_SRC_READY_STATE_HAVE_ENOUGH_DATA: Either there is enough data to
 * play the stream through at the current playback and input rate or the input
 * buffer is full.
 *
 * Describes how much information a #GstMseSrc has about the media it is playing
 * back at the current playback #GstMseSrc:position. This type corresponds
 * directly to the ready state of a HTML Media Element and is a separate concept
 * from #GstMediaSourceReadyState.
 *
 * [Specification](https://html.spec.whatwg.org/multipage/media.html#ready-states)
 *
 * Since: 1.24
 */
typedef enum
{
  GST_MSE_SRC_READY_STATE_HAVE_NOTHING,
  GST_MSE_SRC_READY_STATE_HAVE_METADATA,
  GST_MSE_SRC_READY_STATE_HAVE_CURRENT_DATA,
  GST_MSE_SRC_READY_STATE_HAVE_FUTURE_DATA,
  GST_MSE_SRC_READY_STATE_HAVE_ENOUGH_DATA,
} GstMseSrcReadyState;

#define GST_TYPE_MSE_SRC (gst_mse_src_get_type())
#define GST_TYPE_MSE_SRC_PAD (gst_mse_src_pad_get_type())

GST_MSE_API
G_DECLARE_FINAL_TYPE (GstMseSrc, gst_mse_src, GST, MSE_SRC, GstElement);

GST_MSE_API
G_DECLARE_FINAL_TYPE (GstMseSrcPad, gst_mse_src_pad, GST, MSE_SRC_PAD, GstPad);

GST_MSE_API
GstClockTime gst_mse_src_get_position (GstMseSrc * self);

GST_MSE_API
GstClockTime gst_mse_src_get_duration (GstMseSrc * self);

GST_MSE_API
GstMseSrcReadyState gst_mse_src_get_ready_state (GstMseSrc * self);

GST_MSE_API
guint gst_mse_src_get_n_audio (GstMseSrc * self);

GST_MSE_API
guint gst_mse_src_get_n_text (GstMseSrc * self);

GST_MSE_API
guint gst_mse_src_get_n_video (GstMseSrc * self);

G_END_DECLS

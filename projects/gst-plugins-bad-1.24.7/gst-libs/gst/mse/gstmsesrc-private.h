/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2022, 2023 Collabora Ltd.
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

#include <glib.h>
#include <gst/gst.h>
#include "gstmsesrc.h"
#include "gstmediasource.h"
#include "gstmediasourcetrack-private.h"

G_BEGIN_DECLS

GST_MSE_PRIVATE
void gst_mse_src_set_duration (GstMseSrc * self, GstClockTime duration);

GST_MSE_PRIVATE
void gst_mse_src_network_error (GstMseSrc * self);

GST_MSE_PRIVATE
void gst_mse_src_decode_error (GstMseSrc * self);

GST_MSE_PRIVATE
void gst_mse_src_emit_streams (GstMseSrc * self, GstMediaSourceTrack ** tracks,
    gsize n_tracks);

GST_MSE_PRIVATE
void gst_mse_src_update_ready_state (GstMseSrc * self);

GST_MSE_PRIVATE
void gst_mse_src_attach (GstMseSrc * self, GstMediaSource * media_source);

GST_MSE_PRIVATE
void gst_mse_src_detach (GstMseSrc * self);

G_END_DECLS

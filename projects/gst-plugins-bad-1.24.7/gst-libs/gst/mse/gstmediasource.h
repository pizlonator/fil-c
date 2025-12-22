/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Igalia, S.L
 * Copyright (C) 2015, 2016 Metrological Group B.V.
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

#include <gst/gst.h>
#include <gst/mse/mse-prelude.h>
#include <gst/mse/gstmsesrc.h>

#include "gstsourcebufferlist.h"

G_BEGIN_DECLS

/**
 * GstMediaSourceReadyState:
 * @GST_MEDIA_SOURCE_READY_STATE_CLOSED: The #GstMediaSource is not connected to
 * any playback element.
 * @GST_MEDIA_SOURCE_READY_STATE_OPEN: The #GstMediaSource is connected to a
 * playback element and ready to append data to its #GstSourceBuffer (s).
 * @GST_MEDIA_SOURCE_READY_STATE_ENDED: gst_media_source_end_of_stream() has
 * been called on the current #GstMediaSource
 *
 * Describes the possible states of the Media Source.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-readystate)
 *
 * Since: 1.24
 */
typedef enum
{
  GST_MEDIA_SOURCE_READY_STATE_CLOSED,
  GST_MEDIA_SOURCE_READY_STATE_OPEN,
  GST_MEDIA_SOURCE_READY_STATE_ENDED,
} GstMediaSourceReadyState;

/**
 * GstMediaSourceError:
 * @GST_MEDIA_SOURCE_ERROR_INVALID_STATE:
 * @GST_MEDIA_SOURCE_ERROR_TYPE:
 * @GST_MEDIA_SOURCE_ERROR_NOT_SUPPORTED:
 * @GST_MEDIA_SOURCE_ERROR_NOT_FOUND:
 * @GST_MEDIA_SOURCE_ERROR_QUOTA_EXCEEDED:
 *
 * Any error that can occur within #GstMediaSource or #GstSourceBuffer APIs.
 * These values correspond directly to those in the Web IDL specification.
 *
 * [Specification](https://webidl.spec.whatwg.org/#idl-DOMException-error-names)
 *
 * Since: 1.24
 */
typedef enum
{
  GST_MEDIA_SOURCE_ERROR_INVALID_STATE,
  GST_MEDIA_SOURCE_ERROR_TYPE,
  GST_MEDIA_SOURCE_ERROR_NOT_SUPPORTED,
  GST_MEDIA_SOURCE_ERROR_NOT_FOUND,
  GST_MEDIA_SOURCE_ERROR_QUOTA_EXCEEDED,
} GstMediaSourceError;

/**
 * GstMediaSourceEOSError:
 * @GST_MEDIA_SOURCE_EOS_ERROR_NONE: End the stream successfully
 * @GST_MEDIA_SOURCE_EOS_ERROR_NETWORK: End the stream due to a networking error
 * @GST_MEDIA_SOURCE_EOS_ERROR_DECODE: End the stream due to a decoding error
 *
 * Reasons for ending a #GstMediaSource using gst_media_source_end_of_stream().
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-endofstreamerror)
 *
 * Since: 1.24
 */
typedef enum
{
  GST_MEDIA_SOURCE_EOS_ERROR_NONE,
  GST_MEDIA_SOURCE_EOS_ERROR_NETWORK,
  GST_MEDIA_SOURCE_EOS_ERROR_DECODE,
} GstMediaSourceEOSError;

/**
 * GstMediaSourceRange:
 * @start: The start of this range.
 * @end: The end of this range.
 *
 * A structure describing a simplified version of the TimeRanges concept in the
 * HTML specification, only representing a single @start and @end time.
 *
 * [Specification](https://html.spec.whatwg.org/multipage/media.html#timeranges)
 *
 * Since: 1.24
 */
typedef struct
{
  GstClockTime start;
  GstClockTime end;
} GstMediaSourceRange;

GST_MSE_API
gboolean gst_media_source_is_type_supported (const gchar * type);

#define GST_TYPE_MEDIA_SOURCE (gst_media_source_get_type())
#define GST_MEDIA_SOURCE_ERROR (gst_media_source_error_quark())

/**
 * gst_media_source_error_quark:
 *
 * Any error type that can be reported by the Media Source API.
 *
 * Since: 1.24
 */
GST_MSE_API
GQuark gst_media_source_error_quark (void);

GST_MSE_API
G_DECLARE_FINAL_TYPE (GstMediaSource, gst_media_source, GST, MEDIA_SOURCE,
    GstObject);

GST_MSE_API
GstMediaSource *gst_media_source_new (void);

GST_MSE_API
void gst_media_source_attach (GstMediaSource * self, GstMseSrc * element);

GST_MSE_API
void gst_media_source_detach (GstMediaSource * self);

GST_MSE_API
GstSourceBufferList * gst_media_source_get_source_buffers (
    GstMediaSource * self);

GST_MSE_API
GstSourceBufferList * gst_media_source_get_active_source_buffers (
    GstMediaSource * self);

GST_MSE_API
GstMediaSourceReadyState gst_media_source_get_ready_state (
    GstMediaSource * self);

GST_MSE_API
GstClockTime gst_media_source_get_position (GstMediaSource * self);

GST_MSE_API
GstClockTime gst_media_source_get_duration (GstMediaSource * self);

GST_MSE_API
gboolean gst_media_source_set_duration (GstMediaSource * self,
                                        GstClockTime duration,
                                        GError ** error);

GST_MSE_API
GstSourceBuffer * gst_media_source_add_source_buffer (GstMediaSource * self,
                                                      const gchar * type,
                                                      GError ** error);

GST_MSE_API
gboolean gst_media_source_remove_source_buffer (GstMediaSource * self,
                                                GstSourceBuffer * buffer,
                                                GError ** error);

GST_MSE_API
gboolean gst_media_source_end_of_stream (GstMediaSource * self,
                                         GstMediaSourceEOSError eos_error,
                                         GError ** error);

GST_MSE_API
gboolean gst_media_source_set_live_seekable_range (GstMediaSource * self,
                                                   GstClockTime start,
                                                   GstClockTime end,
                                                   GError ** error);

GST_MSE_API
gboolean gst_media_source_clear_live_seekable_range (GstMediaSource * self,
                                                     GError ** error);

GST_MSE_API
void gst_media_source_get_live_seekable_range (GstMediaSource * self,
    GstMediaSourceRange * range);

G_END_DECLS

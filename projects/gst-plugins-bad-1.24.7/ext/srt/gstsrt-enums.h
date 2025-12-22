/* GStreamer
 * Copyright (C) 2018, Collabora Ltd.
 * Copyright (C) 2018, SK Telecom, Co., Ltd.
 *   Author: Jeongseok Kim <jeongseok.kim@sk.com>
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

#ifndef __GST_SRT_ENUM_H__
#define __GST_SRT_ENUM_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/**
 * GstSRTConnectionMode:
 * @GST_SRT_CONNECTION_MODE_NONE: not connected
 * @GST_SRT_CONNECTION_MODE_CALLER: The mode to send the connection request like a client
 * @GST_SRT_CONNECTION_MODE_LISTENER: The mode to wait for being connected by peer caller
 * @GST_SRT_CONNECTION_MODE_RENDEZVOUS: The mode to support one-to-one only connection
 *
 * SRT connection types.
 */
typedef enum
{
  GST_SRT_CONNECTION_MODE_NONE = 0,
  GST_SRT_CONNECTION_MODE_CALLER,
  GST_SRT_CONNECTION_MODE_LISTENER,
  GST_SRT_CONNECTION_MODE_RENDEZVOUS,
} GstSRTConnectionMode;

/**
 * GstSRTKeyLengthBits:
 * @GST_SRT_KEY_LENGTH_NO_KEY: no encryption
 * @GST_SRT_KEY_LENGTH_0: no encryption
 * @GST_SRT_KEY_LENGTH_16: 16 bytes (128-bit) length
 * @GST_SRT_KEY_LENGTH_24: 24 bytes (192-bit) length
 * @GST_SRT_KEY_LENGTH_32: 32 bytes (256-bit) length
 *
 * Crypto key length in bits
 */
typedef enum
{
  GST_SRT_KEY_LENGTH_NO_KEY = 0,
  GST_SRT_KEY_LENGTH_0 = GST_SRT_KEY_LENGTH_NO_KEY,
  GST_SRT_KEY_LENGTH_16 = 16,
  GST_SRT_KEY_LENGTH_24 = 24,
  GST_SRT_KEY_LENGTH_32 = 32,
} GstSRTKeyLength;

G_END_DECLS

#endif // __GST_SRT_ENUM_H__

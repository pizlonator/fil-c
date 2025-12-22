/*
 * camutils.h - GStreamer CAM (EN50221) support
 * Copyright (C) 2007 Alessandro Decina
 *
 * Authors:
 *   Alessandro Decina <alessandro.d@gmail.com>
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

#ifndef CAM_UTILS_H
#define CAM_UTILS_H

#include <glib.h>
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

#define TPDU_HEADER_SIZE_INDICATOR 0x80

#define CAM_FAILED(ret) (ret <= CAM_RETURN_ERROR)

typedef enum
{
  /* generic */
  CAM_RETURN_OK = 0,
  CAM_RETURN_ERROR = -1,

  /* transport specific */
  CAM_RETURN_TRANSPORT_ERROR = -10,
  CAM_RETURN_TRANSPORT_TOO_MANY_CONNECTIONS = -11,
  CAM_RETURN_TRANSPORT_TIMEOUT = -12,
  CAM_RETURN_TRANSPORT_POLL = -13,

  /* session specific */
  CAM_RETURN_SESSION_ERROR = -30,
  CAM_RETURN_SESSION_TOO_MANY_SESSIONS = -31,

  /* application specific */
  CAM_RETURN_APPLICATION_ERROR = -40,
} CamReturn;

guint8 cam_calc_length_field_size (guint length);
guint8 cam_write_length_field (guint8 *buff, guint length);
guint8 cam_read_length_field (guint8 *buff, guint *length);
guint8 *cam_build_ca_pmt (GstMpegtsPMT *pmt, guint8 list_management, guint8 cmd_id, guint *size);

#endif /* CAM_UTILS_H */

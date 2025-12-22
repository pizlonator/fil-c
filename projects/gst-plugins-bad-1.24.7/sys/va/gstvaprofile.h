/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
#include <va/va.h>

G_BEGIN_DECLS

typedef enum
{
  AV1 = GST_MAKE_FOURCC ('A', 'V', '0', '1'),
  H263 = GST_MAKE_FOURCC ('H', '2', '6', '3'),
  H264 = GST_MAKE_FOURCC ('H', '2', '6', '4'),
  HEVC = GST_MAKE_FOURCC ('H', '2', '6', '5'),
  JPEG = GST_MAKE_FOURCC ('J', 'P', 'E', 'G'),
  MPEG2 = GST_MAKE_FOURCC ('M', 'P', 'E', 'G'),
  MPEG4 = GST_MAKE_FOURCC ('M', 'P', 'G', '4'),
  VC1 = GST_MAKE_FOURCC ('W', 'M', 'V', '3'),
  VP8 = GST_MAKE_FOURCC ('V', 'P', '8', '0'),
  VP9 = GST_MAKE_FOURCC ('V', 'P', '9', '0'),
} GstVaCodecs;

guint32               gst_va_profile_codec                (VAProfile profile);
GstCaps *             gst_va_profile_caps                 (VAProfile profile,
                                                           VAEntrypoint entrypoint);
const gchar *         gst_va_profile_name                 (VAProfile profile);
VAProfile             gst_va_profile_from_name            (GstVaCodecs codec,
                                                           const gchar * name);

G_END_DECLS

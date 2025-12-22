/* GStreamer
 * Copyright (C) 2023 Edward Hervey <edward@centricular.com>
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

/* Implementation of RDD48 Amd1 - mapping of RFC 9043 FFV1 Video Coding Format
 * Versions 0, 1, and 3 to RDD 48 and the MXF Generic Container
 */

#ifndef __MXF_FFV1_H__
#define __MXF_FFV1_H__

#include <gst/gst.h>

void mxf_ffv1_init (void);

#endif /* __MXF_FFV1_H__ */

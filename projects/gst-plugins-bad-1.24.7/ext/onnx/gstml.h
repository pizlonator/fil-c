/*
 * GStreamer gstreamer-ml
 * Copyright (C) 2021 Collabora Ltd
 *
 * gstml.h
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
#ifndef __GST_ML_H__
#define __GST_ML_H__


/**
 * GstMlInputImageFormat:
 *
 * @GST_ML_INPUT_IMAGE_FORMAT_HWC Height Width Channel (a.k.a. interleaved) format
 * @GST_ML_INPUT_IMAGE_FORMAT_CHW Channel Height Width  (a.k.a. planar) format
 *
 * Since: 1.20
 */
typedef enum {
  GST_ML_INPUT_IMAGE_FORMAT_HWC,
  GST_ML_INPUT_IMAGE_FORMAT_CHW,
} GstMlInputImageFormat;



#endif

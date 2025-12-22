/* 
 * Copyright (C) 2013 Sebastian Dröge <slomo@circular-chaos.org>
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
 *
 */

#ifndef __GST_OPENJPEG_H__
#define __GST_OPENJPEG_H__

#include <openjpeg.h>

typedef enum
{
  OPENJPEG_ERROR_NONE = 0,
  OPENJPEG_ERROR_INIT,
  OPENJPEG_ERROR_ENCODE,
  OPENJPEG_ERROR_DECODE,
  OPENJPEG_ERROR_OPEN,
  OPENJPEG_ERROR_MAP_READ,
  OPENJPEG_ERROR_MAP_WRITE,
  OPENJPEG_ERROR_FILL_IMAGE,
  OPENJPEG_ERROR_NEGOCIATE,
  OPENJPEG_ERROR_ALLOCATE,
} OpenJPEGErrorCode;

typedef struct
{
  GstVideoCodecFrame *frame;
  GstBuffer *output_buffer;
  GstBuffer *input_buffer;
  gint stripe;
  OpenJPEGErrorCode last_error;
  gboolean direct;
  gboolean last_subframe;
} GstOpenJPEGCodecMessage;

#endif /* __GST_OPENJPEG_H__ */

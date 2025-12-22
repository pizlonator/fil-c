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

#pragma once

#include <gst/gst.h>
#include <gst/d3d11/gstd3d11.h>
#include <d2d1_1.h>
#include <dwrite.h>

#ifdef HAVE_DWRITE_COLOR_FONT
#include <d2d1_3.h>
#include <dwrite_3.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_DWRITE_TEXT_ALIGNMENT            (gst_dwrite_text_alignment_get_type ())
GType gst_dwrite_text_alignment_get_type (void);

#define GST_TYPE_DWRITE_PARAGRAPH_ALIGNMENT       (gst_dwrite_paragraph_alignment_get_type ())
GType gst_dwrite_paragraph_alignment_get_type (void);

#define GST_TYPE_DWRITE_FONT_WEIGHT               (gst_dwrite_font_weight_get_type ())
GType gst_dwrite_font_weight_get_type (void);

#define GST_TYPE_DWRITE_FONT_STYLE                (gst_dwrite_font_style_get_type ())
GType gst_dwrite_font_style_get_type (void);

#define GST_TYPE_DWRITE_FONT_STRETCH              (gst_dwrite_font_stretch_get_type ())
GType gst_dwrite_font_stretch_get_type (void);

G_END_DECLS

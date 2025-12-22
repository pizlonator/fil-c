/*
 * GStreamer
 * Copyright (C) 2010 Luis de Bethencourt <luis@debethencourt.com>
 *
 * Burn - curve adjustment film video effect.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

#ifndef __GST_BURN_H__
#define __GST_BURN_H__

#include <gst/gst.h>

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_BURN \
  (gst_burn_get_type())
#define GST_BURN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BURN,GstBurn))
#define GST_BURN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BURN,GstBurnClass))
#define GST_IS_BURN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BURN))
#define GST_IS_BURN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BURN))

typedef struct _GstBurn      GstBurn;
typedef struct _GstBurnClass GstBurnClass;

struct _GstBurn
{
  GstVideoFilter videofilter;

  /* < private > */
  gint adjustment;
};

struct _GstBurnClass
{
  GstVideoFilterClass parent_class;
};

GType gst_burn_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (burn);

G_END_DECLS

#endif /* __GST_BURN_H__ */

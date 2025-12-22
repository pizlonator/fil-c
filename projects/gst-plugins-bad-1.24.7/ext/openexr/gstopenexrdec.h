/* 
 * Copyright (C) 2013 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_OPENEXR_DEC_H__
#define __GST_OPENEXR_DEC_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstopenexr.h"

G_BEGIN_DECLS

#define GST_TYPE_OPENEXR_DEC \
  (gst_openexr_dec_get_type())
#define GST_OPENEXR_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENEXR_DEC,GstOpenEXRDec))
#define GST_OPENEXR_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENEXR_DEC,GstOpenEXRDecClass))
#define GST_IS_OPENEXR_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPENEXR_DEC))
#define GST_IS_OPENEXR_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPENEXR_DEC))

typedef struct _GstOpenEXRDec GstOpenEXRDec;
typedef struct _GstOpenEXRDecClass GstOpenEXRDecClass;

struct _GstOpenEXRDec
{
  GstVideoDecoder parent;

  /* < private > */
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;
};

struct _GstOpenEXRDecClass
{
  GstVideoDecoderClass parent_class;
};

GType gst_openexr_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (openexrdec);

G_END_DECLS

#endif /* __GST_OPENEXR_DEC_H__ */

/* GStreamer bz2 decoder
 * Copyright (C) 2006 Lutz Müller <lutz topfrose de>

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

#ifndef __GST_BZ2DEC_H__
#define __GST_BZ2DEC_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_BZ2DEC            (gst_bz2dec_get_type())
#define GST_BZ2DEC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BZ2DEC,GstBz2dec))
#define GST_BZ2DEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BZ2DEC,GstBz2decClass))
#define GST_IS_BZ2DEC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BZ2DEC))
#define GST_IS_BZ2DEC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BZ2DEC))
typedef struct _GstBz2dec GstBz2dec;
typedef struct _GstBz2decClass GstBz2decClass;

GType
gst_bz2dec_get_type (void)
    G_GNUC_CONST;

GST_ELEMENT_REGISTER_DECLARE (bz2dec);

G_END_DECLS
#endif /* __GST_BZ2DEC_H__ */

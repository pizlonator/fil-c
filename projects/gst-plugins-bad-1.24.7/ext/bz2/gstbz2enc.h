/* GStreamer bz2 encoder
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

#ifndef __GST_BZ2ENC_H__
#define __GST_BZ2ENC_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_BZ2ENC            (gst_bz2enc_get_type())
#define GST_BZ2ENC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BZ2ENC,GstBz2enc))
#define GST_BZ2ENC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BZ2ENC,GstBz2encClass))
#define GST_IS_BZ2ENC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BZ2ENC))
#define GST_IS_BZ2ENC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BZ2ENC))
typedef struct _GstBz2enc GstBz2enc;
typedef struct _GstBz2encClass GstBz2encClass;

GType
gst_bz2enc_get_type (void)
    G_GNUC_CONST;

GST_ELEMENT_REGISTER_DECLARE (bz2enc);

G_END_DECLS
#endif /* __GST_BZ2ENC_H__ */

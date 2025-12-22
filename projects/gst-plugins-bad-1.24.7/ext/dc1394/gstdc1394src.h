/* GStreamer
 * Copyright (C) <2006> Eric Jonas <jonas@mit.edu>
 * Copyright (C) <2006> Antoine Tremblay <hexa00@gmail.com>
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

#ifndef __GST_DC1394_H__
#define __GST_DC1394_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <dc1394/control.h>

G_BEGIN_DECLS

#define GST_TYPE_DC1394_SRC            (gst_dc1394_src_get_type())
#define GST_DC1394_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DC1394_SRC,GstDC1394Src))
#define GST_DC1394_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DC1394_SRC,GstDC1394SrcClass))
#define GST_IS_DC1394_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DC1394_SRC))
#define GST_IS_DC1394_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DC1394_SRC))

typedef struct _GstDC1394Src GstDC1394Src;
typedef struct _GstDC1394SrcClass GstDC1394SrcClass;

struct _GstDC1394Src {
  GstPushSrc pushsrc;

  GstCaps * caps;

  uint64_t guid;
  int unit;
  dc1394speed_t iso_speed;
  uint32_t dma_buffer_size;
  dc1394camera_t * camera;
  dc1394_t * dc1394;
};

struct _GstDC1394SrcClass {
  GstPushSrcClass parent_class;
};

GType gst_dc1394_src_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (dc1394src);

G_END_DECLS

#endif /* __GST_DC1394_H__ */

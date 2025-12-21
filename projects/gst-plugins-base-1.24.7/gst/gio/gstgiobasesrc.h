/* GStreamer
 *
 * Copyright (C) 2007 Rene Stadler <mail@renestadler.de>
 * Copyright (C) 2007-2009 Sebastian Dröge <slomo@circular-chaos.org>
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

#ifndef __GST_GIO_BASE_SRC_H__
#define __GST_GIO_BASE_SRC_H__

#include <gst/gst.h>
#include <gio/gio.h>
#include <gst/base/gstbasesrc.h>

G_BEGIN_DECLS

#define GST_TYPE_GIO_BASE_SRC \
  (gst_gio_base_src_get_type())
#define GST_GIO_BASE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GIO_BASE_SRC,GstGioBaseSrc))
#define GST_GIO_BASE_SRC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_GIO_BASE_SRC, GstGioBaseSrcClass))
#define GST_GIO_BASE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GIO_BASE_SRC,GstGioBaseSrcClass))
#define GST_IS_GIO_BASE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GIO_BASE_SRC))
#define GST_IS_GIO_BASE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GIO_BASE_SRC))

typedef struct _GstGioBaseSrc      GstGioBaseSrc;
typedef struct _GstGioBaseSrcClass GstGioBaseSrcClass;

struct _GstGioBaseSrc
{
  GstBaseSrc src;

  /* < protected > */
  GCancellable *cancel;
  guint64 position;

  /* < private > */
  GInputStream *stream;
  GstBuffer *cache;
};

struct _GstGioBaseSrcClass
{
  GstBaseSrcClass parent_class;

  GInputStream * (*get_stream) (GstGioBaseSrc *bsrc);

  /* Returns TRUE if the files grew and we should try
    reading again, FALSE otherwise */
  gboolean (*wait_for_data) (GstGioBaseSrc *bsrc);
  void (*waited_for_data) (GstGioBaseSrc *bsrc);

  gboolean close_on_stop;
};

GType gst_gio_base_src_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstGioBaseSrc, gst_object_unref)

G_END_DECLS

#endif /* __GST_GIO_BASE_SRC_H__ */

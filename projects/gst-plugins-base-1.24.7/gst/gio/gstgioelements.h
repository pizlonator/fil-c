/* GStreamer
 *
 * Copyright (C) 2007 Rene Stadler <mail@renestadler.de>
 * Copyright (C) 2007 Sebastian Dröge <slomo@circular-chaos.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GST_GIO_ELEMENTS_H__
#define __GST_GIO_ELEMENTS_H__


#include <gio/gio.h>
#include <gst/gst.h>

#include "gstgiobasesink.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void gio_element_init (GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (giosink);
GST_ELEMENT_REGISTER_DECLARE (giosrc);
GST_ELEMENT_REGISTER_DECLARE (giostreamsink);
GST_ELEMENT_REGISTER_DECLARE (giostreamsrc);

#define GST_GIO_ERROR_MATCHES(err, code) g_error_matches (err, G_IO_ERROR, G_IO_ERROR_##code)

#define GST_GIO_STREAM_IS_SEEKABLE(stream) (G_IS_SEEKABLE (stream) && g_seekable_can_seek (G_SEEKABLE (stream)))

gboolean gst_gio_error (gpointer element, const gchar *func_name,
    GError **err, GstFlowReturn *ret);
GstFlowReturn gst_gio_seek (gpointer element, GSeekable *stream, guint64 offset,
    GCancellable *cancel);
void gst_gio_uri_handler_do_init (GType type);

G_END_DECLS

#endif /* __GST_GIO_ELEMENTS_H__ */

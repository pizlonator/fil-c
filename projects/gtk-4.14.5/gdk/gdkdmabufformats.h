/* gdkdmabufformats.h
 *
 * Copyright 2023 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_DMABUF_FORMATS (gdk_dmabuf_formats_get_type ())

GDK_AVAILABLE_IN_4_14
GType              gdk_dmabuf_formats_get_type        (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_14
GdkDmabufFormats * gdk_dmabuf_formats_ref             (GdkDmabufFormats *formats);

GDK_AVAILABLE_IN_4_14
void               gdk_dmabuf_formats_unref           (GdkDmabufFormats *formats);

GDK_AVAILABLE_IN_4_14
gsize              gdk_dmabuf_formats_get_n_formats   (GdkDmabufFormats *formats) G_GNUC_PURE;

GDK_AVAILABLE_IN_4_14
void               gdk_dmabuf_formats_get_format      (GdkDmabufFormats *formats,
                                                       gsize             idx,
                                                       guint32          *fourcc,
                                                       guint64          *modifier);

GDK_AVAILABLE_IN_4_14
gboolean           gdk_dmabuf_formats_contains        (GdkDmabufFormats *formats,
                                                       guint32           fourcc,
                                                       guint64           modifier) G_GNUC_PURE;

GDK_AVAILABLE_IN_4_14
gboolean           gdk_dmabuf_formats_equal           (const GdkDmabufFormats *formats1,
                                                       const GdkDmabufFormats *formats2);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GdkDmabufFormats, gdk_dmabuf_formats_unref);

G_END_DECLS

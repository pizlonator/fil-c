/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Red Hat, Inc.
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

#include <gdk/gdk.h>

G_BEGIN_DECLS

GdkPixbuf *gtk_make_symbolic_pixbuf_from_data       (const char    *data,
                                                     gsize          len,
                                                     int            width,
                                                     int            height,
                                                     double         scale,
                                                     const char    *debug_output_to,
                                                     GError       **error);

GdkTexture *gdk_texture_new_from_filename_with_fg   (const char    *filename,
                                                     gboolean      *only_fg,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_resource_with_fg   (const char    *path,
                                                     gboolean      *only_fg);
GdkTexture *gdk_texture_new_from_stream_with_fg     (GInputStream  *stream,
                                                     gboolean      *only_fg,
                                                     GCancellable  *cancellable,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_stream_at_scale    (GInputStream  *stream,
                                                     int            width,
                                                     int            height,
                                                     gboolean       aspect,
                                                     gboolean      *only_fg,
                                                     GCancellable  *cancellable,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_resource_at_scale  (const char    *path,
                                                     int            width,
                                                     int            height,
                                                     gboolean       aspect,
                                                     gboolean      *only_fg,
                                                     GError       **error);

GdkTexture *gdk_texture_new_from_filename_symbolic  (const char    *path,
                                                     int            width,
                                                     int            height,
                                                     double         scale,
                                                     gboolean      *only_fg,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_file_symbolic      (GFile         *file,
                                                     int            width,
                                                     int            height,
                                                     double         scale,
                                                     gboolean      *only_fg,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_resource_symbolic  (const char    *path,
                                                     int            width,
                                                     int            height,
                                                     double         scale,
                                                     gboolean      *only_fg,
                                                     GError       **error);

GdkTexture *gtk_load_symbolic_texture_from_file     (GFile         *file);
GdkTexture *gtk_load_symbolic_texture_from_resource (const char    *path);

GdkPaintable *gdk_paintable_new_from_filename_scaled (const char    *filename,
                                                      double         scale);
GdkPaintable *gdk_paintable_new_from_resource_scaled (const char    *path,
                                                      double         scale);
GdkPaintable *gdk_paintable_new_from_file_scaled     (GFile         *file,
                                                      double         scale);

G_END_DECLS

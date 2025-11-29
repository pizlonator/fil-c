/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <gdk/gdk.h>
#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GSK_TYPE_SHADER_PAINTABLE (gsk_shader_paintable_get_type ())

G_DECLARE_FINAL_TYPE (GskShaderPaintable, gsk_shader_paintable, GSK, SHADER_PAINTABLE, GObject)

GdkPaintable *   gsk_shader_paintable_new              (GskGLShader        *shader,
                                                        GBytes             *data);

GskGLShader *    gsk_shader_paintable_get_shader       (GskShaderPaintable *self);
void             gsk_shader_paintable_set_shader       (GskShaderPaintable *self,
                                                        GskGLShader        *shader);
GBytes *         gsk_shader_paintable_get_args         (GskShaderPaintable *self);
void             gsk_shader_paintable_set_args         (GskShaderPaintable *self,
                                                        GBytes             *data);
void             gsk_shader_paintable_update_time      (GskShaderPaintable *self,
                                                        int                 time_idx,
                                                        gint64              frame_time);
G_END_DECLS

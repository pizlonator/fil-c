/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif


#include <gsk/gskpath.h>
#include <gsk/gskpathpoint.h>

G_BEGIN_DECLS

#define GSK_TYPE_PATH_MEASURE (gsk_path_measure_get_type ())

GDK_AVAILABLE_IN_4_14
GType                   gsk_path_measure_get_type               (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_4_14
GskPathMeasure *        gsk_path_measure_new                    (GskPath                *path);
GDK_AVAILABLE_IN_4_14
GskPathMeasure *        gsk_path_measure_new_with_tolerance     (GskPath                *path,
                                                                 float                   tolerance);

GDK_AVAILABLE_IN_4_14
GskPathMeasure *        gsk_path_measure_ref                    (GskPathMeasure         *self);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_measure_unref                  (GskPathMeasure         *self);

GDK_AVAILABLE_IN_4_14
GskPath *               gsk_path_measure_get_path               (GskPathMeasure         *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_14
float                   gsk_path_measure_get_tolerance          (GskPathMeasure         *self) G_GNUC_PURE;

GDK_AVAILABLE_IN_4_14
float                   gsk_path_measure_get_length             (GskPathMeasure         *self);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_measure_get_point              (GskPathMeasure         *self,
                                                                 float                   distance,
                                                                 GskPathPoint           *result);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskPathMeasure, gsk_path_measure_unref)

G_END_DECLS

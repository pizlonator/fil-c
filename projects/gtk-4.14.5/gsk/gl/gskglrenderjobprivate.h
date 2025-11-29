/* gskglrenderjobprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gskgltypesprivate.h"

GskGLRenderJob *gsk_gl_render_job_new                (GskGLDriver           *driver,
                                                      const graphene_rect_t *viewport,
                                                      float                  scale,
                                                      const cairo_region_t  *region,
                                                      guint                  framebuffer,
                                                      gboolean               clear_framebuffer);
void            gsk_gl_render_job_free               (GskGLRenderJob        *job);
void            gsk_gl_render_job_render             (GskGLRenderJob        *job,
                                                      GskRenderNode         *root);
void            gsk_gl_render_job_render_flipped     (GskGLRenderJob        *job,
                                                      GskRenderNode         *root);


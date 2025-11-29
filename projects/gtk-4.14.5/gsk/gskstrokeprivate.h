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

#include "gskstroke.h"

G_BEGIN_DECLS

struct _GskStroke
{
  float line_width;
  GskLineCap line_cap;
  GskLineJoin line_join;
  float miter_limit;

  float *dash;
  gsize n_dash;
  float dash_length; /* sum of all dashes in the array */
  float dash_offset;
};

#define GSK_STROKE_INIT_COPY(_stroke) ((GskStroke) {\
    .line_width = _stroke->line_width, \
    .line_cap = _stroke->line_cap, \
    .line_join = _stroke->line_join, \
    .miter_limit = _stroke->miter_limit, \
    .dash = g_memdup (_stroke->dash, _stroke->n_dash * sizeof (float)), \
    .n_dash = _stroke->n_dash, \
    .dash_length = _stroke->dash_length, \
    .dash_offset = _stroke->dash_offset, \
})

static inline void
gsk_stroke_clear (GskStroke *stroke)
{
  g_clear_pointer (&stroke->dash, g_free);
  stroke->n_dash = 0; /* better safe than sorry */
}

float gsk_stroke_get_join_width (const GskStroke *stroke);

G_END_DECLS

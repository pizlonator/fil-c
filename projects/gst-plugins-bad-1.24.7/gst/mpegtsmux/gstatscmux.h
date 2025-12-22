/* ATSC Transport Stream muxer
 * Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
 *
 * gstatscmux.h:
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
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __ATSCMUX_H__
#define __ATSCMUX_H__

#include "gstbasetsmux.h"
#include <gst/base/base.h>

G_BEGIN_DECLS

#define GST_TYPE_ATSCMUX  (gst_atsc_mux_get_type())

typedef struct GstATSCMux GstATSCMux;
typedef struct GstATSCMuxClass GstATSCMuxClass;

struct GstATSCMux {
  GstBaseTsMux parent;
};

struct GstATSCMuxClass {
  GstBaseTsMuxClass parent_class;
};

GType gst_atsc_mux_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (atscmux);

G_END_DECLS

#endif /* __ATSCMUX_H__ */

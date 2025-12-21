/* Copyright (C) <2014> Intel Corporation
 * Copyright (C) <2014> Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 * Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
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

#ifndef __GST_PLAYBACK_UTILS_H__
#define __GST_PLAYBACK_UTILS_H__

G_BEGIN_DECLS

#include <gst/gst.h>
#include "gstplay-enum.h"

G_GNUC_INTERNAL
guint
gst_playback_utils_get_n_common_capsfeatures (GstElementFactory * fact1,
                                        GstElementFactory * fact2,
                                        GstPlayFlags flags,
                                        gboolean isaudioelement);
G_GNUC_INTERNAL
gint
gst_playback_utils_compare_factories_func (gconstpointer p1, gconstpointer p2);
G_END_DECLS

#endif /* __GST_PLAYBACK_UTILS_H__ */

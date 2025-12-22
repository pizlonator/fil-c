/* GStreamer
 * Copyright (C) 2023 Collabora Ltd
 *
 * analytics-meta-prelude.h
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
 * You should have received a copy of the GNU Library General Publicn
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_ANALYTICS_META_PRELUDE_H__
#define __GST_ANALYTICS_META_PRELUDE_H__

#include <gst/gst.h>

#ifndef GST_ANALYTICS_META_API
# ifdef BUILDING_GST_ANALYTICS
#   define GST_ANALYTICS_META_API GST_API_EXPORT
# else
#   define GST_ANALYTICS_META_API GST_API_IMPORT
# endif
#endif

#endif /* __GST_ANALYTICS_META_PRELUDE_H__ */


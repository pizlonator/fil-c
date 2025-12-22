/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2022, 2023 Collabora Ltd.
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

#pragma once

#include <gst/gst.h>

#ifndef GST_MSE_API
# if defined(BUILDING_GST_MSE) && defined(GST_API_EXPORT)
#  define GST_MSE_API GST_API_EXPORT
# elif defined(GST_API_IMPORT)
#  define GST_MSE_API GST_API_IMPORT
# else
#  define GST_MSE_API
# endif
#endif

#ifndef GST_MSE_PRIVATE
# if defined(BUILDING_GST_MSE_TEST)
#  define GST_MSE_PRIVATE GST_MSE_API
# else
#  define GST_MSE_PRIVATE G_GNUC_INTERNAL
# endif
#endif

#ifndef GST_USE_UNSTABLE_API
#warning "The MSE library from gst-plugins-bad is an unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

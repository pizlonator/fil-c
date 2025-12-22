/*
 * GStreamer Wayland Library
 * Copyright (C) 2014 Collabora Ltd.
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

#ifndef GST_USE_UNSTABLE_API
#warning "The GStreamer wayland library is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <wayland-client.h>

#include <gst/wayland/wayland-prelude.h>
#include <gst/wayland/gstwl_fwd.h>
#include <gst/wayland/gstwlbuffer.h>
#include <gst/wayland/gstwlcontext.h>
#include <gst/wayland/gstwldisplay.h>
#include <gst/wayland/gstwllinuxdmabuf.h>
#include <gst/wayland/gstwlshmallocator.h>
#include <gst/wayland/gstwlvideobufferpool.h>
#include <gst/wayland/gstwlvideoformat.h>
#include <gst/wayland/gstwlwindow.h>

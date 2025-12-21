/* GStreamer
 * Copyright (C) 2007 David Schleef <ds@schleef.org>
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
/**
 * SECTION:element-appsrc
 * @title: appsrc
 *
 * The appsrc element can be used by applications to insert data into a
 * GStreamer pipeline. Unlike most GStreamer elements, Appsrc provides
 * external API functions.
 *
 * For the documentation of the API, please see the
 * <link linkend="gst-plugins-base-libs-appsrc">libgstapp</link> section in the
 * GStreamer Plugins Base Libraries documentation.
 */


#include "gstappelements.h"

GST_ELEMENT_REGISTER_DEFINE (appsrc, "appsrc", GST_RANK_NONE, GST_TYPE_APP_SRC);

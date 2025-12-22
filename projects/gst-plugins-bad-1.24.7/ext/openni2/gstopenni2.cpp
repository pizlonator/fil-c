/* GStreamer
 * Copyright (C) 2013 Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>
 *
 * gstopenni2.c: plugin registration
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
 * SECTION:plugin-openni2src
 *
 * OpenNI2 is a library to access 3D sensors such as those based on PrimeSense
 * depth sensor. Examples of such sensors are the Kinect used in Microsoft Xbox
 * consoles and Asus WAVI Xtion. Notably recordings of 3D sessions can also be
 * replayed as the original devices. See www.openni.org for more details.
 *
 * OpenNI2 can be downloaded from source, compiled and installed in Linux, Mac
 * and Windows devices(https://github.com/OpenNI/OpenNI2). However is better to
 * rely on Debian packages as part of the PCL library (or http://goo.gl/0o87EB).
 * More concretely on the "libopenni2-dev" and "libopenni2" packages - that can
 * be downloaded in http://goo.gl/2H6SZ6.
 *
 * ## Examples
 *
 * Some recorded .oni files are available at <http://people.cs.pitt.edu/~chang/1635/proj11/kinectRecord>
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst.h>
#include "gstopenni2src.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (openni2src, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    openni2,
    "GStreamer Openni2 Plugins",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)

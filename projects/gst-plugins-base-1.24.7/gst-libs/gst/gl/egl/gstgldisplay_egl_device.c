/*
 * GStreamer
 * Copyright (C) 2014 Matthew Waters <ystreet00@gmail.com>
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
 * SECTION:gstgldisplay_egl_device
 * @short_description: EGL EGLDeviceEXT object
 * @title: GstGLDisplayEGLDevice
 * @see_also: #GstGLDisplay, #GstGLDisplayEGL
 *
 * #GstGLDisplayEGLDevice represents a `EGLDeviceEXT` handle created internally
 * (gst_gl_display_egl_device_new()) or wrapped by the application
 * (gst_gl_display_egl_device_new_with_egl_device())
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgldisplay_egl.h"
#include "gstgldisplay_egl_device.h"

#include <gst/gl/gstglfeature.h>

#include "gstegl.h"
#include "gstglmemoryegl.h"

#ifndef EGL_DEVICE_EXT
typedef void *EGLDeviceEXT;
#endif

GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug

typedef EGLBoolean (*eglQueryDevicesEXT_type) (EGLint max_devices,
    EGLDeviceEXT * devices, EGLint * num_devices);

G_DEFINE_TYPE (GstGLDisplayEGLDevice, gst_gl_display_egl_device,
    GST_TYPE_GL_DISPLAY);

static guintptr gst_gl_display_egl_device_get_handle (GstGLDisplay * display);

static void
gst_gl_display_egl_device_class_init (GstGLDisplayEGLDeviceClass * klass)
{
  GstGLDisplayClass *display_class = GST_GL_DISPLAY_CLASS (klass);

  display_class->get_handle =
      GST_DEBUG_FUNCPTR (gst_gl_display_egl_device_get_handle);
}

static void
gst_gl_display_egl_device_init (GstGLDisplayEGLDevice * self)
{
  GstGLDisplay *display = GST_GL_DISPLAY (self);

  display->type = GST_GL_DISPLAY_TYPE_EGL_DEVICE;

  gst_gl_memory_egl_init_once ();
}

static guintptr
gst_gl_display_egl_device_get_handle (GstGLDisplay * display)
{
  GstGLDisplayEGLDevice *self = GST_GL_DISPLAY_EGL_DEVICE (display);

  return (guintptr) self->device;
}

/**
 * gst_gl_display_egl_device_new:
 * @device_index: the index of device to use
 *
 * Create a new #GstGLDisplayEGLDevice with an EGLDevice supported device
 *
 * Returns: (transfer full) (nullable): a new #GstGLDisplayEGLDevice or %NULL
 *
 * Since: 1.18
 */
GstGLDisplayEGLDevice *
gst_gl_display_egl_device_new (guint device_index)
{
  GstGLDisplayEGLDevice *ret;
  eglQueryDevicesEXT_type query_device_func;
  EGLint num_devices = 0;
  EGLDeviceEXT *device_list;

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  query_device_func =
      (eglQueryDevicesEXT_type) eglGetProcAddress ("eglQueryDevicesEXT");

  if (!query_device_func) {
    GST_ERROR ("eglQueryDevicesEXT is unavailable");
    return NULL;
  }

  if (query_device_func (0, NULL, &num_devices) == EGL_FALSE) {
    GST_ERROR ("eglQueryDevicesEXT fail");
    return NULL;
  } else if (num_devices < 1) {
    GST_ERROR ("no EGLDevice supported device");
    return NULL;
  }

  if (num_devices <= device_index) {
    GST_ERROR ("requested index %d exceeds the number of devices %d",
        device_index, num_devices);
    return NULL;
  }

  device_list = g_alloca (sizeof (EGLDeviceEXT) * num_devices);
  query_device_func (num_devices, device_list, &num_devices);

  ret = g_object_new (GST_TYPE_GL_DISPLAY_EGL_DEVICE, NULL);
  gst_object_ref_sink (ret);

  ret->device = device_list[device_index];

  return ret;
}

/**
 * gst_gl_display_egl_device_new_with_egl_device:
 * @device: an existing EGLDeviceEXT
 *
 * Creates a new #GstGLDisplayEGLDevice with EGLDeviceEXT .
 * The @device must be created using EGLDevice enumeration.
 *
 * Returns: (transfer full): a new #GstGLDisplayEGLDevice
 *
 * Since: 1.18
 */
GstGLDisplayEGLDevice *
gst_gl_display_egl_device_new_with_egl_device (gpointer device)
{
  GstGLDisplayEGLDevice *ret;

  g_return_val_if_fail (device != NULL, NULL);

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  ret = g_object_new (GST_TYPE_GL_DISPLAY_EGL_DEVICE, NULL);
  gst_object_ref_sink (ret);

  ret->device = device;

  return ret;
}

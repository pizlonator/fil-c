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

#ifndef __GST_GL_DISPLAY_EGL_DEVICE_H__
#define __GST_GL_DISPLAY_EGL_DEVICE_H__

#include <gst/gl/gstgldisplay.h>

G_BEGIN_DECLS

GST_GL_API
GType gst_gl_display_egl_device_get_type (void);

#define GST_TYPE_GL_DISPLAY_EGL_DEVICE             (gst_gl_display_egl_device_get_type())
#define GST_GL_DISPLAY_EGL_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_DISPLAY_EGL_DEVICE,GstGLDisplayEGLDevice))
#define GST_GL_DISPLAY_EGL_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GL_DISPLAY_EGL_DEVICE,GstGLDisplayEGLDeviceClass))
#define GST_IS_GL_DISPLAY_EGL_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_DISPLAY_EGL_DEVICE))
#define GST_IS_GL_DISPLAY_EGL_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GL_DISPLAY_EGL_DEVICE))
#define GST_GL_DISPLAY_EGL_DEVICE_CAST(obj)        ((GstGLDisplayEGLDevice*)(obj))

typedef struct _GstGLDisplayEGLDevice GstGLDisplayEGLDevice;
typedef struct _GstGLDisplayEGLDeviceClass GstGLDisplayEGLDeviceClass;

/**
 * GstGLDisplayEGLDevice:
 *
 * the contents of a #GstGLDisplayEGLDevice are private and should only be accessed
 * through the provided API
 *
 * Since: 1.18
 */
struct _GstGLDisplayEGLDevice
{
  GstGLDisplay parent;

  gpointer device;

  gpointer _padding[GST_PADDING];
};

/**
 * GstGLDisplayEGLDeviceClass:
 *
 * Opaque #GstGLDisplayEGLDeviceClass struct
 *
 * Since: 1.18
 */
struct _GstGLDisplayEGLDeviceClass
{
  GstGLDisplayClass object_class;

  gpointer _padding[GST_PADDING];
};

GST_GL_API
GstGLDisplayEGLDevice *gst_gl_display_egl_device_new (guint device_index);

GST_GL_API
GstGLDisplayEGLDevice *gst_gl_display_egl_device_new_with_egl_device (gpointer device);


G_END_DECLS

#endif /* __GST_GL_DISPLAY_EGL_DEVICE_H__ */

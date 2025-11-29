/*
 * Copyright © 2021 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <IOSurface/IOSurface.h>
#include <Foundation/Foundation.h>
#include <OpenGL/CGLIOSurface.h>
#include <QuartzCore/QuartzCore.h>

#include "gdkmacosbuffer-private.h"

struct _GdkMacosBuffer
{
  GObject          parent_instance;
  cairo_region_t  *damage;
  IOSurfaceRef     surface;
  int              lock_count;
  guint            bytes_per_element;
  guint            bits_per_pixel;
  guint            width;
  guint            height;
  guint            stride;
  double           device_scale;
  guint            flipped : 1;
};

G_DEFINE_TYPE (GdkMacosBuffer, gdk_macos_buffer, G_TYPE_OBJECT)

static void
gdk_macos_buffer_dispose (GObject *object)
{
  GdkMacosBuffer *self = (GdkMacosBuffer *)object;

  if (self->lock_count != 0)
    g_critical ("Attempt to dispose %s while lock is held",
                G_OBJECT_TYPE_NAME (self));

  /* We could potentially force the unload of our surface here with
   * IOSurfaceSetPurgeable (self->surface, kIOSurfacePurgeableEmpty, NULL)
   * but that would cause it to empty when the layers may still be attached
   * to it. Better to just let it get GC'd by the system after they have
   * moved on to a new buffer.
   */
  g_clear_pointer (&self->surface, CFRelease);
  g_clear_pointer (&self->damage, cairo_region_destroy);

  G_OBJECT_CLASS (gdk_macos_buffer_parent_class)->dispose (object);
}

static void
gdk_macos_buffer_class_init (GdkMacosBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gdk_macos_buffer_dispose;
}

static void
gdk_macos_buffer_init (GdkMacosBuffer *self)
{
}

static void
add_int (CFMutableDictionaryRef dict,
         const CFStringRef      key,
         int                    value)
{
  CFNumberRef number = CFNumberCreate (NULL, kCFNumberIntType, &value);
  CFDictionaryAddValue (dict, key, number);
  CFRelease (number);
}

static IOSurfaceRef
create_surface (int   width,
                int   height,
                int   bytes_per_element,
                guint *stride)
{
  CFMutableDictionaryRef props;
  IOSurfaceRef ret;
  size_t bytes_per_row;
  size_t total_bytes;

  props = CFDictionaryCreateMutable (kCFAllocatorDefault,
                                     16,
                                     &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
  if (props == NULL)
    return NULL;

  bytes_per_row = IOSurfaceAlignProperty (kIOSurfaceBytesPerRow, width * bytes_per_element);
  total_bytes = IOSurfaceAlignProperty (kIOSurfaceAllocSize, height * bytes_per_row);

  add_int (props, kIOSurfaceAllocSize, total_bytes);
  add_int (props, kIOSurfaceBytesPerElement, bytes_per_element);
  add_int (props, kIOSurfaceBytesPerRow, bytes_per_row);
  add_int (props, kIOSurfaceHeight, height);
  add_int (props, kIOSurfacePixelFormat, (int)'BGRA');
  add_int (props, kIOSurfaceWidth, width);

  ret = IOSurfaceCreate (props);

  CFRelease (props);

  *stride = bytes_per_row;

  return ret;
}

GdkMacosBuffer *
_gdk_macos_buffer_new (int    width,
                       int    height,
                       double device_scale,
                       int    bytes_per_element,
                       int    bits_per_pixel)
{
  GdkMacosBuffer *self;

  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  self = g_object_new (GDK_TYPE_MACOS_BUFFER, NULL);
  self->bytes_per_element = bytes_per_element;
  self->bits_per_pixel = bits_per_pixel;
  self->surface = create_surface (width, height, bytes_per_element, &self->stride);
  self->width = width;
  self->height = height;
  self->device_scale = device_scale;
  self->lock_count = 0;

  if (self->surface == NULL)
    g_clear_object (&self);

  return self;
}

IOSurfaceRef
_gdk_macos_buffer_get_native (GdkMacosBuffer *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_BUFFER (self), NULL);

  return self->surface;
}

/**
 * _gdk_macos_buffer_lock:
 *
 * This function matches the IOSurfaceLock() name but what it really
 * does is page the buffer back for the CPU to access from VRAM.
 *
 * Generally we don't want to do that, but we do need to in some
 * cases such as when we are rendering with Cairo. There might
 * be an opportunity later to avoid that, but since we are using
 * GL pretty much everywhere already, we don't try.
 */
void
_gdk_macos_buffer_lock (GdkMacosBuffer *self)
{
  g_return_if_fail (GDK_IS_MACOS_BUFFER (self));
  g_return_if_fail (self->lock_count == 0);

  self->lock_count++;

  IOSurfaceLock (self->surface, 0, NULL);
}

void
_gdk_macos_buffer_unlock (GdkMacosBuffer *self)
{
  g_return_if_fail (GDK_IS_MACOS_BUFFER (self));
  g_return_if_fail (self->lock_count == 1);

  self->lock_count--;

  IOSurfaceUnlock (self->surface, 0, NULL);
}

/**
 * _gdk_macos_buffer_lock_readonly:
 *
 * Like _gdk_macos_buffer_lock() but uses the read-only flag to
 * indicate we are not interested in retrieving the updates from
 * the GPU before modifying the CPU-side cache.
 *
 * Must be used with _gdk_macos_buffer_unlock_readonly().
 */
void
_gdk_macos_buffer_read_lock (GdkMacosBuffer *self)
{
  kern_return_t ret;

  g_return_if_fail (GDK_IS_MACOS_BUFFER (self));
  g_return_if_fail (self->lock_count == 0);

  self->lock_count++;

  ret = IOSurfaceLock (self->surface, kIOSurfaceLockReadOnly, NULL);

  g_return_if_fail (ret == KERN_SUCCESS);
}

void
_gdk_macos_buffer_read_unlock (GdkMacosBuffer *self)
{
  kern_return_t ret;

  g_return_if_fail (GDK_IS_MACOS_BUFFER (self));
  g_return_if_fail (self->lock_count == 1);

  self->lock_count--;

  ret = IOSurfaceUnlock (self->surface, kIOSurfaceLockReadOnly, NULL);

  g_return_if_fail (ret == KERN_SUCCESS);
}

guint
_gdk_macos_buffer_get_width (GdkMacosBuffer *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_BUFFER (self), 0);

  return self->width;
}

guint
_gdk_macos_buffer_get_height (GdkMacosBuffer *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_BUFFER (self), 0);

  return self->height;
}

guint
_gdk_macos_buffer_get_stride (GdkMacosBuffer *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_BUFFER (self), 0);

  return self->stride;
}

double
_gdk_macos_buffer_get_device_scale (GdkMacosBuffer *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_BUFFER (self), 1.0);

  return self->device_scale;
}

const cairo_region_t *
_gdk_macos_buffer_get_damage (GdkMacosBuffer *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_BUFFER (self), NULL);

  return self->damage;
}

void
_gdk_macos_buffer_set_damage (GdkMacosBuffer *self,
                              cairo_region_t *damage)
{
  g_return_if_fail (GDK_IS_MACOS_BUFFER (self));

  if (damage == self->damage)
    return;

  g_clear_pointer (&self->damage, cairo_region_destroy);
  self->damage = cairo_region_copy (damage);
}

gpointer
_gdk_macos_buffer_get_data (GdkMacosBuffer *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_BUFFER (self), NULL);

  return IOSurfaceGetBaseAddress (self->surface);
}

gboolean
_gdk_macos_buffer_get_flipped (GdkMacosBuffer *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_BUFFER (self), FALSE);

  return self->flipped;
}

void
_gdk_macos_buffer_set_flipped (GdkMacosBuffer *self,
                               gboolean        flipped)
{
  g_return_if_fail (GDK_IS_MACOS_BUFFER (self));

  self->flipped = !!flipped;
}

/* GStreamer
 * Copyright (C) 2020 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * SECTION:gstvadisplaywrapped
 * @title: GstVaDisplayWrapped
 * @short_description: User's custom VADisplay
 * @sources:
 * - gstvadisplay_wrapped.h
 *
 * This is a #GstVaDisplay instantiaton subclass for custom created
 * VADisplay, such as X11 or Wayland, wrapping it.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvadisplay_wrapped.h"

/**
 * GstVaDisplayWrapped:
 * @parent: parent #GstVaDisplay
 *
 * Since: 1.20
 */
struct _GstVaDisplayWrapped
{
  GstVaDisplay parent;
};

/**
 * GstVaDisplayWrappedClass:
 * @parent_class: parent #GstVaDisplayClass
 *
 * Since: 1.20
 */
struct _GstVaDisplayWrappedClass
{
  GstVaDisplayClass parent_class;
};

#define gst_va_display_wrapped_parent_class parent_class
G_DEFINE_TYPE (GstVaDisplayWrapped, gst_va_display_wrapped,
    GST_TYPE_VA_DISPLAY);

static void
gst_va_display_wrapped_class_init (GstVaDisplayWrappedClass * klass)
{
}

static void
gst_va_display_wrapped_init (GstVaDisplayWrapped * self)
{
}

/**
 * gst_va_display_wrapped_new:
 * @handle: a VADisplay to wrap
 *
 * Creates a #GstVaDisplay wrapping an already created and initialized
 * VADisplay.
 *
 * The lifetime of @handle must be hold by the provider while the
 * pipeline is instantiated. Do not call vaTerminate on it while the
 * pipeline is not in NULL state.
 *
 * Returns: (transfer full): a new #GstVaDisplay if @handle is valid,
 *     Otherwise %NULL.
 *
 * Since: 1.20
 **/
GstVaDisplay *
gst_va_display_wrapped_new (gpointer handle)
{
  GstVaDisplay *dpy;

  g_return_val_if_fail (handle, NULL);

  dpy = g_object_new (GST_TYPE_VA_DISPLAY_WRAPPED, "va-display", handle, NULL);
  if (!gst_va_display_initialize (dpy)) {
    gst_object_unref (dpy);
    return NULL;
  }

  return gst_object_ref_sink (dpy);
}

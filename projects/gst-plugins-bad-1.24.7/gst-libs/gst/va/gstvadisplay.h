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

#pragma once

#include <gst/gst.h>
#include <gst/va/va_fwd.h>
#include <gst/va/va-prelude.h>

G_BEGIN_DECLS

/**
 * GstVaImplementation:
 * @GST_VA_IMPLEMENTATION_MESA_GALLIUM: The mesa gallium implementation.
 * @GST_VA_IMPLEMENTATION_INTEL_I965: The legacy i965 intel implementation.
 * @GST_VA_IMPLEMENTATION_INTEL_IHD: The iHD intel implementation.
 * @GST_VA_IMPLEMENTATION_OTHER: Other implementation.
 * @GST_VA_IMPLEMENTATION_INVALID: Invalid implementation.
 *
 * Types of different VA API implemented drivers. These are the typical and
 * the most widely used VA drivers.
 *
 * Since: 1.20
 */
typedef enum
{
  GST_VA_IMPLEMENTATION_MESA_GALLIUM,
  GST_VA_IMPLEMENTATION_INTEL_I965,
  GST_VA_IMPLEMENTATION_INTEL_IHD,
  GST_VA_IMPLEMENTATION_OTHER,
  GST_VA_IMPLEMENTATION_INVALID,
} GstVaImplementation;

/**
 * GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR:
 *
 * Since: 1.20
 */
#define GST_VA_DISPLAY_HANDLE_CONTEXT_TYPE_STR "gst.va.display.handle"

/**
 * GST_CAPS_FEATURE_MEMORY_VA:
 *
 * Since: 1.20
 */
#define GST_CAPS_FEATURE_MEMORY_VA "memory:VAMemory"

/**
 * GST_VA_DISPLAY_IS_IMPLEMENTATION: (skip)
 *
 * Check whether the display is the implementation of the specified
 * #GstVaImplementation type.
 *
 * Since: 1.20
 */
#define GST_VA_DISPLAY_IS_IMPLEMENTATION(display, impl) \
  (gst_va_display_is_implementation (display, G_PASTE (GST_VA_IMPLEMENTATION_, impl)))

#define GST_TYPE_VA_DISPLAY            (gst_va_display_get_type())
#define GST_VA_DISPLAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VA_DISPLAY, GstVaDisplay))
#define GST_VA_DISPLAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VA_DISPLAY, GstVaDisplayClass))
#define GST_IS_VA_DISPLAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VA_DISPLAY))
#define GST_IS_VA_DISPLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VA_DISPLAY))
#define GST_VA_DISPLAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_VA_DISPLAY, GstVaDisplayClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstVaDisplay, gst_object_unref)

/**
 * GstVaDisplay:
 * @parent: parent #GstObject
 *
 * The common VA display object structure.
 *
 * Since: 1.20
 */
struct _GstVaDisplay
{
  GstObject parent;
};

/**
 * GstVaDisplayClass:
 * @parent_class: parent #GstObjectClass
 *
 * The common VA display object class structure.
 *
 * Since: 1.20
 */
struct _GstVaDisplayClass
{
  GstObjectClass parent_class;

  /**
   * GstVaDisplayClass::create_va_display:
   * @self: a #GstVaDisplay instance
   *
   * This is called when the subclass has to create the internal
   * VADisplay.
   *
   * Returns: The created VADisplay
   */
  gpointer (*create_va_display) (GstVaDisplay * self);
};

GST_VA_API
GType                 gst_va_display_get_type             (void);
GST_VA_API
gboolean              gst_va_display_initialize           (GstVaDisplay * self);
GST_VA_API
gpointer              gst_va_display_get_va_dpy           (GstVaDisplay * self);
GST_VA_API
GstVaImplementation   gst_va_display_get_implementation   (GstVaDisplay * self);
GST_VA_API
gboolean              gst_va_display_check_version        (GstVaDisplay * self,
                                                           guint major,
                                                           guint minor);

/**
 * gst_va_display_is_implementation:
 * @display: the #GstVaDisplay to check.
 * @impl: the specified #GstVaImplementation.
 *
 * Check whether the @display is the implementation of the @impl type.
 *
 * Returns: %TRUE if the @display is the implementation of the @impl type.
 *
 * Since: 1.20
 */
static inline gboolean
gst_va_display_is_implementation (GstVaDisplay * display, GstVaImplementation impl)
{
  return (gst_va_display_get_implementation (display) == impl);
}

G_END_DECLS

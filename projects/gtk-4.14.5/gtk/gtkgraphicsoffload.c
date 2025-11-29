/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2023  Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkbinlayout.h"
#include "gtkgraphicsoffload.h"
#include "gtksnapshotprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"
#include "gdk/gdksurfaceprivate.h"
#include "gdk/gdksubsurfaceprivate.h"
#include "gtktypebuiltins.h"

/**
 * GtkGraphicsOffload:
 *
 * A widget that allows to bypass gsk rendering for its child by passing the content
 * directly to the compositor.
 *
 * Graphics offload is an optimization to reduce overhead and battery use that is
 * most useful for video content. It only works on some platforms and in certain
 * situations. GTK will automatically fall back to normal rendering if it doesn't.
 *
 * Graphics offload is most efficient if there are no controls drawn on top of the
 * video content.
 *
 * You should consider using graphics offload for your main widget if it shows
 * frequently changing content (such as a video, or a VM display) and you provide
 * the content in the form of dmabuf textures (see [class@Gdk.DmabufTextureBuilder]),
 * in particular if it may be fullscreen.
 *
 * Numerous factors can prohibit graphics offload:
 *
 * - Unsupported platforms. Currently, graphics offload only works on Linux with Wayland.
 *
 * - Clipping, such as rounded corners that cause the video content to not be rectangular
 *
 * - Unsupported dmabuf formats (see [method@Gdk.Display.get_dmabuf_formats])
 *
 * - Translucent video content (content with an alpha channel, even if it isn't used)
 *
 * - Transforms that are more complex than translations and scales
 *
 * - Filters such as opacity, grayscale or similar
 *
 * To investigate problems related graphics offload, GTK offers debug flags to print
 * out information about graphics offload and dmabuf use:
 *
 *     GDK_DEBUG=offload
 *     GDK_DEBUG=dmabuf
 *
 * The GTK inspector provides a visual debugging tool for graphics offload.
 *
 * Since: 4.14
 */

struct _GtkGraphicsOffload
{
  GtkWidget parent_instance;

  GtkWidget *child;

  GdkSubsurface *subsurface;

  GtkGraphicsOffloadEnabled enabled;
};

struct _GtkGraphicsOffloadClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_0,
  PROP_CHILD,
  PROP_ENABLED,
  LAST_PROP,
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

G_DEFINE_TYPE (GtkGraphicsOffload, gtk_graphics_offload, GTK_TYPE_WIDGET)

static void
gtk_graphics_offload_init (GtkGraphicsOffload *self)
{
  self->enabled = GTK_GRAPHICS_OFFLOAD_ENABLED;
}

static void
gtk_graphics_offload_dispose (GObject *object)
{
  GtkGraphicsOffload *self = GTK_GRAPHICS_OFFLOAD (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_graphics_offload_parent_class)->dispose (object);
}

static void
gtk_graphics_offload_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkGraphicsOffload *self = GTK_GRAPHICS_OFFLOAD (object);

  switch (property_id)
    {
    case PROP_CHILD:
      gtk_graphics_offload_set_child (self, g_value_get_object (value));
      break;

    case PROP_ENABLED:
      gtk_graphics_offload_set_enabled (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_graphics_offload_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkGraphicsOffload *self = GTK_GRAPHICS_OFFLOAD (object);

  switch (property_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, gtk_graphics_offload_get_child (self));
      break;

    case PROP_ENABLED:
      g_value_set_enum (value, gtk_graphics_offload_get_enabled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
sync_subsurface (GtkGraphicsOffload *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  if (gtk_widget_get_realized (widget) && self->enabled != GTK_GRAPHICS_OFFLOAD_DISABLED)
    {
      if (!self->subsurface)
        self->subsurface = gdk_surface_create_subsurface (gtk_widget_get_surface (widget));
    }
  else
    {
      g_clear_object (&self->subsurface);
    }
}

static void
gtk_graphics_offload_realize (GtkWidget *widget)
{
  GtkGraphicsOffload *self = GTK_GRAPHICS_OFFLOAD (widget);

  GTK_WIDGET_CLASS (gtk_graphics_offload_parent_class)->realize (widget);

  sync_subsurface (self);
}

static void
gtk_graphics_offload_unrealize (GtkWidget *widget)
{
  GtkGraphicsOffload *self = GTK_GRAPHICS_OFFLOAD (widget);

  GTK_WIDGET_CLASS (gtk_graphics_offload_parent_class)->unrealize (widget);

  sync_subsurface (self);
}

static void
gtk_graphics_offload_snapshot (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  GtkGraphicsOffload *self = GTK_GRAPHICS_OFFLOAD (widget);

  if (self->subsurface)
    gtk_snapshot_push_subsurface (snapshot, self->subsurface);

  if (self->child)
    gtk_widget_snapshot_child (widget, self->child, snapshot);

  if (self->subsurface)
    gtk_snapshot_pop (snapshot);
}

static void
gtk_graphics_offload_class_init (GtkGraphicsOffloadClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_graphics_offload_dispose;
  object_class->set_property = gtk_graphics_offload_set_property;
  object_class->get_property = gtk_graphics_offload_get_property;

  widget_class->realize = gtk_graphics_offload_realize;
  widget_class->unrealize = gtk_graphics_offload_unrealize;
  widget_class->snapshot = gtk_graphics_offload_snapshot;

  /**
   * GtkGraphicsOffload:child: (attributes org.gtk.Property.get=gtk_graphics_offload_get_child org.gtk.Property.set=gtk_graphics_offload_set_child)
   *
   * The child widget.
   *
   * Since: 4.14
   */
  properties[PROP_CHILD] = g_param_spec_object ("child", NULL, NULL,
                                                GTK_TYPE_WIDGET,
                                                GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGraphicsOffload:enabled: (attributes org.gtk.Property.get=gtk_graphics_offload_get_enabled org.gtk.Property.set=gtk_graphics_offload_set_enabled)
   *
   * Whether graphics offload is enabled.
   *
   * Since: 4.14
   */
  properties[PROP_ENABLED] = g_param_spec_enum ("enabled", NULL, NULL,
                                                GTK_TYPE_GRAPHICS_OFFLOAD_ENABLED,
                                                GTK_GRAPHICS_OFFLOAD_ENABLED,
                                                GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "graphicsoffload");
}

/**
 * gtk_graphics_offload_new:
 * @child: (nullable): the child widget
 *
 * Creates a new GtkGraphicsOffload widget.
 *
 * Returns: the new widget
 *
 * Since: 4.14
 */
GtkWidget *
gtk_graphics_offload_new (GtkWidget *child)
{
  return g_object_new (GTK_TYPE_GRAPHICS_OFFLOAD,
                       "child", child,
                       NULL);
}

/**
 * gtk_graphics_offload_set_child:
 * @self: a `GtkGraphicsOffload`
 * @child: (nullable): the child widget
 *
 * Sets the child of @self.
 *
 * Since: 4.14
 */
void
gtk_graphics_offload_set_child (GtkGraphicsOffload *self,
                                GtkWidget           *child)
{
  g_return_if_fail (GTK_IS_GRAPHICS_OFFLOAD (self));
  g_return_if_fail (child == NULL || self->child == child || (GTK_IS_WIDGET (child) &&gtk_widget_get_parent (child) == NULL));

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child)
    {
      self->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

/**
 * gtk_graphics_offload_get_child:
 * @self: a `GtkGraphicsOffload`
 *
 * Gets the child of @self.
 *
 * Returns: (nullable) (transfer none): the child widget
 *
 * Since: 4.14
 */
GtkWidget *
gtk_graphics_offload_get_child (GtkGraphicsOffload *self)
{
  g_return_val_if_fail (GTK_IS_GRAPHICS_OFFLOAD (self), NULL);

  return self->child;
}

/**
 * gtk_graphics_offload_set_enabled:
 * @self: a `GtkGraphicsOffload`
 * @enabled: whether to enable offload
 *
 * Sets whether this GtkGraphicsOffload widget will attempt
 * to offload the content of its child widget.
 *
 * Since: 4.14
 */
void
gtk_graphics_offload_set_enabled (GtkGraphicsOffload        *self,
                                  GtkGraphicsOffloadEnabled  enabled)
{
  g_return_if_fail (GTK_IS_GRAPHICS_OFFLOAD (self));

  if (self->enabled == enabled)
    return;

  self->enabled = enabled;

  sync_subsurface (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLED]);
}

/**
 * gtk_graphics_offload_get_enabled:
 * @self: a `GtkGraphicsOffload`
 *
 * Returns whether offload is enabled for @self.
 *
 * Returns: whether offload is enabled
 *
 * Since: 4.14
 */
GtkGraphicsOffloadEnabled
gtk_graphics_offload_get_enabled (GtkGraphicsOffload *self)
{
  g_return_val_if_fail (GTK_IS_GRAPHICS_OFFLOAD (self), TRUE);

  return self->enabled;
}

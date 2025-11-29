/*
 * Copyright © 2018 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkwidgetpaintableprivate.h"

#include "gtksnapshot.h"
#include "gtkrendernodepaintableprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"

/**
 * GtkWidgetPaintable:
 *
 * `GtkWidgetPaintable` is a `GdkPaintable` that displays the contents
 * of a widget.
 *
 * `GtkWidgetPaintable` will also take care of the widget not being in a
 * state where it can be drawn (like when it isn't shown) and just draw
 * nothing or where it does not have a size (like when it is hidden) and
 * report no size in that case.
 *
 * Of course, `GtkWidgetPaintable` allows you to monitor widgets for size
 * changes by emitting the [signal@Gdk.Paintable::invalidate-size] signal
 * whenever the size of the widget changes as well as for visual changes by
 * emitting the [signal@Gdk.Paintable::invalidate-contents] signal whenever
 * the widget changes.
 *
 * You can use a `GtkWidgetPaintable` everywhere a `GdkPaintable` is allowed,
 * including using it on a `GtkPicture` (or one of its parents) that it was
 * set on itself via gtk_picture_set_paintable(). The paintable will take care
 * of recursion when this happens. If you do this however, ensure that the
 * [property@Gtk.Picture:can-shrink] property is set to %TRUE or you might
 * end up with an infinitely growing widget.
 */
struct _GtkWidgetPaintable
{
  GObject parent_instance;

  GtkWidget *widget;
  guint snapshot_count;

  guint         pending_update_cb;      /* the idle source that updates the valid image to be the new current image */

  GdkPaintable *current_image;          /* the image that we are presenting */
  GdkPaintable *pending_image;          /* the image that we should be presenting */
};

struct _GtkWidgetPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_WIDGET,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_widget_paintable_paintable_snapshot (GdkPaintable *paintable,
                                         GdkSnapshot  *snapshot,
                                         double        width,
                                         double        height)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (paintable);

  if (self->snapshot_count > 4)
    return;
  else if (self->snapshot_count > 0)
    {
      graphene_rect_t bounds;

      gtk_snapshot_push_clip (snapshot,
                              &GRAPHENE_RECT_INIT(0, 0, width, height));

      if (gtk_widget_compute_bounds (self->widget, self->widget, &bounds))
        {
          gtk_snapshot_scale (snapshot, width / bounds.size.width, height / bounds.size.height);
          gtk_snapshot_translate (snapshot, &bounds.origin);
        }

      gtk_widget_snapshot (self->widget, snapshot);

      gtk_snapshot_pop (snapshot);
    }
  else
    {
      gdk_paintable_snapshot (self->current_image, snapshot, width, height);
    }
}

static GdkPaintable *
gtk_widget_paintable_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (paintable);

  return g_object_ref (self->current_image);
}

static int
gtk_widget_paintable_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_width (self->current_image);
}

static int
gtk_widget_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_height (self->current_image);
}

static void
gtk_widget_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_widget_paintable_paintable_snapshot;
  iface->get_current_image = gtk_widget_paintable_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_widget_paintable_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_widget_paintable_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_EXTENDED (GtkWidgetPaintable, gtk_widget_paintable, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_widget_paintable_paintable_init))

static void
gtk_widget_paintable_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)

{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      gtk_widget_paintable_set_widget (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_paintable_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, self->widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_paintable_unset_widget (GtkWidgetPaintable *self)
{
  if (self->widget == NULL)
    return;

  self->widget->priv->paintables = g_slist_remove (self->widget->priv->paintables,
                                                   self);

  self->widget = NULL;

  g_clear_object (&self->pending_image);
  if (self->pending_update_cb)
    {
      g_source_remove (self->pending_update_cb);
      self->pending_update_cb = 0;
    }
}

static void
gtk_widget_paintable_dispose (GObject *object)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (object);

  gtk_widget_paintable_unset_widget (self);

  G_OBJECT_CLASS (gtk_widget_paintable_parent_class)->dispose (object);
}

static void
gtk_widget_paintable_finalize (GObject *object)
{
  GtkWidgetPaintable *self = GTK_WIDGET_PAINTABLE (object);

  g_object_unref (self->current_image);

  G_OBJECT_CLASS (gtk_widget_paintable_parent_class)->finalize (object);
}

static void
gtk_widget_paintable_class_init (GtkWidgetPaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_widget_paintable_get_property;
  gobject_class->set_property = gtk_widget_paintable_set_property;
  gobject_class->dispose = gtk_widget_paintable_dispose;
  gobject_class->finalize = gtk_widget_paintable_finalize;

  /**
   * GtkWidgetPaintable:widget: (attributes org.gtk.Property.get=gtk_widget_paintable_get_widget org.gtk.Property.set=gtk_widget_paintable_set_widget)
   *
   * The observed widget or %NULL if none.
   */
  properties[PROP_WIDGET] =
    g_param_spec_object ("widget", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_widget_paintable_init (GtkWidgetPaintable *self)
{
  self->current_image = gdk_paintable_new_empty (0, 0);
}

/**
 * gtk_widget_paintable_new:
 * @widget: (nullable) (transfer none): a `GtkWidget`
 *
 * Creates a new widget paintable observing the given widget.
 *
 * Returns: (transfer full) (type GtkWidgetPaintable): a new `GtkWidgetPaintable`
 */
GdkPaintable *
gtk_widget_paintable_new (GtkWidget *widget)
{
  g_return_val_if_fail (widget == NULL || GTK_IS_WIDGET (widget), NULL);

  return g_object_new (GTK_TYPE_WIDGET_PAINTABLE,
                       "widget", widget,
                       NULL);
}

static GdkPaintable *
gtk_widget_paintable_snapshot_widget (GtkWidgetPaintable *self)
{
  graphene_rect_t bounds;

  if (self->widget == NULL)
    return gdk_paintable_new_empty (0, 0);

  if (!gtk_widget_compute_bounds (self->widget, self->widget, &bounds))
    return gdk_paintable_new_empty (0, 0);

  if (self->widget->priv->render_node == NULL)
    return gdk_paintable_new_empty (bounds.size.width, bounds.size.height);
  
  return gtk_render_node_paintable_new (self->widget->priv->render_node, &bounds);
}

/**
 * gtk_widget_paintable_get_widget: (attributes org.gtk.Method.get_property=widget)
 * @self: a `GtkWidgetPaintable`
 *
 * Returns the widget that is observed or %NULL if none.
 *
 * Returns: (transfer none) (nullable): the observed widget.
 */
GtkWidget *
gtk_widget_paintable_get_widget (GtkWidgetPaintable *self)
{
  g_return_val_if_fail (GTK_IS_WIDGET_PAINTABLE (self), NULL);

  return self->widget;
}

/**
 * gtk_widget_paintable_set_widget: (attributes org.gtk.Method.set_property=widget)
 * @self: a `GtkWidgetPaintable`
 * @widget: (nullable): the widget to observe
 *
 * Sets the widget that should be observed.
 */
void
gtk_widget_paintable_set_widget (GtkWidgetPaintable *self,
                                 GtkWidget          *widget)
{

  g_return_if_fail (GTK_IS_WIDGET_PAINTABLE (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (self->widget == widget)
    return;

  gtk_widget_paintable_unset_widget (self);

  /* We do not ref the widget to not cause ref cycles when a widget
   * is told to observe itself or one of its parent.
   */
  self->widget = widget;

  if (widget)
    widget->priv->paintables = g_slist_prepend (widget->priv->paintables, self);

  g_object_unref (self->current_image);
  self->current_image = gtk_widget_paintable_snapshot_widget (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDGET]);
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static gboolean
gtk_widget_paintable_update_func (gpointer data)
{
  GtkWidgetPaintable *self = data;
  GdkPaintable *old_image;

  if (self->current_image != self->pending_image)
    {
      old_image = self->current_image;
      self->current_image = self->pending_image;
      self->pending_image = NULL;
      self->pending_update_cb = 0;

      if (gdk_paintable_get_intrinsic_width (self->current_image) != gdk_paintable_get_intrinsic_width (old_image) ||
          gdk_paintable_get_intrinsic_height (self->current_image) != gdk_paintable_get_intrinsic_height (old_image))
        gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

      g_object_unref (old_image);

      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
    }
  else
    {
      g_clear_object (&self->pending_image);
      self->pending_update_cb = 0;
    }

  return G_SOURCE_REMOVE;
}

void
gtk_widget_paintable_update_image (GtkWidgetPaintable *self)
{
  GdkPaintable *pending_image;

  if (self->pending_update_cb == 0)
    {
      self->pending_update_cb = g_idle_add_full (G_PRIORITY_HIGH,
                                                 gtk_widget_paintable_update_func,
                                                 self,
                                                 NULL);
      gdk_source_set_static_name_by_id (self->pending_update_cb, "[gtk] gtk_widget_paintable_update_func");
    }

  pending_image = gtk_widget_paintable_snapshot_widget (self);
  g_set_object (&self->pending_image, pending_image);
  g_object_unref (pending_image);
}

void
gtk_widget_paintable_push_snapshot_count (GtkWidgetPaintable *self)
{
  self->snapshot_count++;
}

void
gtk_widget_paintable_pop_snapshot_count (GtkWidgetPaintable *self)
{
  self->snapshot_count--;
}

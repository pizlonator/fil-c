/* gtklayoutmanager.c: Layout manager base class
 * Copyright 2018  The GNOME Foundation
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
 * Author: Emmanuele Bassi
 */

/**
 * GtkLayoutManager:
 *
 * Layout managers are delegate classes that handle the preferred size
 * and the allocation of a widget.
 *
 * You typically subclass `GtkLayoutManager` if you want to implement a
 * layout policy for the children of a widget, or if you want to determine
 * the size of a widget depending on its contents.
 *
 * Each `GtkWidget` can only have a `GtkLayoutManager` instance associated
 * to it at any given time; it is possible, though, to replace the layout
 * manager instance using [method@Gtk.Widget.set_layout_manager].
 *
 * ## Layout properties
 *
 * A layout manager can expose properties for controlling the layout of
 * each child, by creating an object type derived from [class@Gtk.LayoutChild]
 * and installing the properties on it as normal `GObject` properties.
 *
 * Each `GtkLayoutChild` instance storing the layout properties for a
 * specific child is created through the [method@Gtk.LayoutManager.get_layout_child]
 * method; a `GtkLayoutManager` controls the creation of its `GtkLayoutChild`
 * instances by overriding the GtkLayoutManagerClass.create_layout_child()
 * virtual function. The typical implementation should look like:
 *
 * ```c
 * static GtkLayoutChild *
 * create_layout_child (GtkLayoutManager *manager,
 *                      GtkWidget        *container,
 *                      GtkWidget        *child)
 * {
 *   return g_object_new (your_layout_child_get_type (),
 *                        "layout-manager", manager,
 *                        "child-widget", child,
 *                        NULL);
 * }
 * ```
 *
 * The [property@Gtk.LayoutChild:layout-manager] and
 * [property@Gtk.LayoutChild:child-widget] properties
 * on the newly created `GtkLayoutChild` instance are mandatory. The
 * `GtkLayoutManager` will cache the newly created `GtkLayoutChild` instance
 * until the widget is removed from its parent, or the parent removes the
 * layout manager.
 *
 * Each `GtkLayoutManager` instance creating a `GtkLayoutChild` should use
 * [method@Gtk.LayoutManager.get_layout_child] every time it needs to query
 * the layout properties; each `GtkLayoutChild` instance should call
 * [method@Gtk.LayoutManager.layout_changed] every time a property is
 * updated, in order to queue a new size measuring and allocation.
 */

#include "config.h"

#include "gtklayoutmanagerprivate.h"
#include "gtklayoutchild.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtkpopover.h"
#include "gtktexthandleprivate.h"
#include "gtktooltipwindowprivate.h"

#ifdef G_ENABLE_DEBUG
#define LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED(m,method)   G_STMT_START {  \
        GObject *_obj = G_OBJECT (m);                                   \
        g_warning ("Layout managers of type %s do not implement "       \
                   "the GtkLayoutManager::%s method",                   \
                   G_OBJECT_TYPE_NAME (_obj),                           \
                   #method);                           } G_STMT_END
#else
#define LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED(m,method)
#endif

typedef struct {
  GtkWidget *widget;
  GtkRoot *root;

  /* HashTable<Widget, LayoutChild> */
  GHashTable *layout_children;
} GtkLayoutManagerPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkLayoutManager, gtk_layout_manager, G_TYPE_OBJECT)

static void
gtk_layout_manager_real_root (GtkLayoutManager *manager)
{
}

static void
gtk_layout_manager_real_unroot (GtkLayoutManager *manager)
{
}

static GtkSizeRequestMode
gtk_layout_manager_real_get_request_mode (GtkLayoutManager *manager,
                                          GtkWidget        *widget)
{
  int hfw = 0, wfh = 0;
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkSizeRequestMode res = gtk_widget_get_request_mode (child);

      switch (res)
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw += 1;
          break;

        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh += 1;
          break;

        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }

 if (hfw == 0 && wfh == 0)
   return GTK_SIZE_REQUEST_CONSTANT_SIZE;

 return hfw > wfh ? GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH
                  : GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
gtk_layout_manager_real_measure (GtkLayoutManager *manager,
                                 GtkWidget        *widget,
                                 GtkOrientation    orientation,
                                 int               for_size,
                                 int              *minimum,
                                 int              *natural,
                                 int              *baseline_minimum,
                                 int              *baseline_natural)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, measure);

  if (minimum != NULL)
    *minimum = 0;

  if (natural != NULL)
    *natural = 0;

  if (baseline_minimum != NULL)
    *baseline_minimum = 0;

  if (baseline_natural != NULL)
    *baseline_natural = 0;
}

static void
gtk_layout_manager_real_allocate (GtkLayoutManager *manager,
                                  GtkWidget        *widget,
                                  int               width,
                                  int               height,
                                  int               baseline)
{
  LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, allocate);
}

static GtkLayoutChild *
gtk_layout_manager_real_create_layout_child (GtkLayoutManager *manager,
                                             GtkWidget        *widget,
                                             GtkWidget        *child)
{
  GtkLayoutManagerClass *manager_class = GTK_LAYOUT_MANAGER_GET_CLASS (manager);

  if (manager_class->layout_child_type == G_TYPE_INVALID)
    {
      LAYOUT_MANAGER_WARN_NOT_IMPLEMENTED (manager, create_layout_child);
      return NULL;
    }

  return g_object_new (manager_class->layout_child_type,
                       "layout-manager", manager,
                       "child-widget", child,
                       NULL);
}

static void
gtk_layout_manager_finalize (GObject *gobject)
{
  GtkLayoutManager *self = GTK_LAYOUT_MANAGER (gobject);
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (self);

  g_clear_pointer (&priv->layout_children, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_layout_manager_parent_class)->finalize (gobject);
}

static void
gtk_layout_manager_class_init (GtkLayoutManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_layout_manager_finalize;

  klass->get_request_mode = gtk_layout_manager_real_get_request_mode;
  klass->measure = gtk_layout_manager_real_measure;
  klass->allocate = gtk_layout_manager_real_allocate;
  klass->create_layout_child = gtk_layout_manager_real_create_layout_child;
  klass->root = gtk_layout_manager_real_root;
  klass->unroot = gtk_layout_manager_real_unroot;
}

static void
gtk_layout_manager_init (GtkLayoutManager *self)
{
}

/*< private >
 * gtk_layout_manager_set_widget:
 * @layout_manager: a `GtkLayoutManager`
 * @widget: (nullable): a `GtkWidget`
 *
 * Sets a back pointer from @widget to @layout_manager.
 */
void
gtk_layout_manager_set_widget (GtkLayoutManager *layout_manager,
                               GtkWidget        *widget)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (layout_manager);

  if (widget != NULL && priv->widget != NULL)
    {
      g_critical ("The layout manager %p of type %s is already in use "
                  "by widget %p '%s', and cannot be used by widget %p '%s'",
                  layout_manager, G_OBJECT_TYPE_NAME (layout_manager),
                  priv->widget, gtk_widget_get_name (priv->widget),
                  widget, gtk_widget_get_name (widget));
      return;
    }

  priv->widget = widget;

  if (widget != NULL)
    gtk_layout_manager_set_root (layout_manager, gtk_widget_get_root (widget));
}

/*< private >
 * gtk_layout_manager_set_root:
 * @layout_manager: a i`GtkLayoutManager`
 * @root: (nullable): a `GtkWidget` implementing `GtkRoot`
 *
 * Sets a back pointer from @root to @layout_manager.
 *
 * This function is called by `GtkWidget` when getting rooted and unrooted,
 * and will call `GtkLayoutManagerClass.root()` or `GtkLayoutManagerClass.unroot()`
 * depending on whether @root is a `GtkWidget` or %NULL.
 */
void
gtk_layout_manager_set_root (GtkLayoutManager *layout_manager,
                             GtkRoot          *root)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (layout_manager);
  GtkRoot *old_root = priv->root;

  priv->root = root;

  if (old_root != root)
    {
      if (priv->root != NULL)
        GTK_LAYOUT_MANAGER_GET_CLASS (layout_manager)->root (layout_manager);
      else
        GTK_LAYOUT_MANAGER_GET_CLASS (layout_manager)->unroot (layout_manager);
    }
}

/**
 * gtk_layout_manager_measure:
 * @manager: a `GtkLayoutManager`
 * @widget: the `GtkWidget` using @manager
 * @orientation: the orientation to measure
 * @for_size: Size for the opposite of @orientation; for instance, if
 *   the @orientation is %GTK_ORIENTATION_HORIZONTAL, this is the height
 *   of the widget; if the @orientation is %GTK_ORIENTATION_VERTICAL, this
 *   is the width of the widget. This allows to measure the height for the
 *   given width, and the width for the given height. Use -1 if the size
 *   is not known
 * @minimum: (out) (optional): the minimum size for the given size and
 *   orientation
 * @natural: (out) (optional): the natural, or preferred size for the
 *   given size and orientation
 * @minimum_baseline: (out) (optional): the baseline position for the
 *   minimum size
 * @natural_baseline: (out) (optional): the baseline position for the
 *   natural size
 *
 * Measures the size of the @widget using @manager, for the
 * given @orientation and size.
 *
 * See the [class@Gtk.Widget] documentation on layout management for
 * more details.
 */
void
gtk_layout_manager_measure (GtkLayoutManager *manager,
                            GtkWidget        *widget,
                            GtkOrientation    orientation,
                            int               for_size,
                            int              *minimum,
                            int              *natural,
                            int              *minimum_baseline,
                            int              *natural_baseline)
{
  GtkLayoutManagerClass *klass;
  int min_size = 0;
  int nat_size = 0;
  int min_baseline = -1;
  int nat_baseline = -1;


  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);

  klass->measure (manager, widget, orientation,
                  for_size,
                  &min_size, &nat_size,
                  &min_baseline, &nat_baseline);

  if (minimum)
    *minimum = min_size;

  if (natural)
    *natural = nat_size;

  if (minimum_baseline)
    *minimum_baseline = min_baseline;

  if (natural_baseline)
    *natural_baseline = nat_baseline;
}

static void
allocate_native_children (GtkWidget *widget)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_POPOVER (child))
        gtk_popover_present (GTK_POPOVER (child));
      else if (GTK_IS_TEXT_HANDLE (child))
        gtk_text_handle_present (GTK_TEXT_HANDLE (child));
      else if (GTK_IS_TOOLTIP_WINDOW (child))
        gtk_tooltip_window_present (GTK_TOOLTIP_WINDOW (child));
      else if (GTK_IS_NATIVE (child))
        g_warning ("Unable to present a to the layout manager unknown auxiliary child surface widget type %s",
                   G_OBJECT_TYPE_NAME (child));
    }
}

/**
 * gtk_layout_manager_allocate:
 * @manager: a `GtkLayoutManager`
 * @widget: the `GtkWidget` using @manager
 * @width: the new width of the @widget
 * @height: the new height of the @widget
 * @baseline: the baseline position of the @widget, or -1
 *
 * Assigns the given @width, @height, and @baseline to
 * a @widget, and computes the position and sizes of the children of
 * the @widget using the layout management policy of @manager.
 */
void
gtk_layout_manager_allocate (GtkLayoutManager *manager,
                             GtkWidget        *widget,
                             int               width,
                             int               height,
                             int               baseline)
{
  GtkLayoutManagerClass *klass;

  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (baseline >= -1);

  allocate_native_children (widget);

  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);

  klass->allocate (manager, widget, width, height, baseline);
}

/**
 * gtk_layout_manager_get_request_mode:
 * @manager: a `GtkLayoutManager`
 *
 * Retrieves the request mode of @manager.
 *
 * Returns: a `GtkSizeRequestMode`
 */
GtkSizeRequestMode
gtk_layout_manager_get_request_mode (GtkLayoutManager *manager)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (manager);
  GtkLayoutManagerClass *klass;

  g_return_val_if_fail (GTK_IS_LAYOUT_MANAGER (manager), GTK_SIZE_REQUEST_CONSTANT_SIZE);
  g_return_val_if_fail (priv->widget != NULL, GTK_SIZE_REQUEST_CONSTANT_SIZE);

  klass = GTK_LAYOUT_MANAGER_GET_CLASS (manager);

  return klass->get_request_mode (manager, priv->widget);
}

/**
 * gtk_layout_manager_get_widget:
 * @manager: a `GtkLayoutManager`
 *
 * Retrieves the `GtkWidget` using the given `GtkLayoutManager`.
 *
 * Returns: (transfer none) (nullable): a `GtkWidget`
 */
GtkWidget *
gtk_layout_manager_get_widget (GtkLayoutManager *manager)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (manager);

  g_return_val_if_fail (GTK_IS_LAYOUT_MANAGER (manager), NULL);

  return priv->widget;
}

/**
 * gtk_layout_manager_layout_changed:
 * @manager: a `GtkLayoutManager`
 *
 * Queues a resize on the `GtkWidget` using @manager, if any.
 *
 * This function should be called by subclasses of `GtkLayoutManager`
 * in response to changes to their layout management policies.
 */
void
gtk_layout_manager_layout_changed (GtkLayoutManager *manager)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (manager);

  g_return_if_fail (GTK_IS_LAYOUT_MANAGER (manager));

  if (priv->widget != NULL)
    gtk_widget_queue_resize (priv->widget);
}

/*< private >
 * gtk_layout_manager_remove_layout_child:
 * @manager: a `GtkLayoutManager`
 * @widget: a `GtkWidget`
 *
 * Removes the `GtkLayoutChild` associated with @widget from the
 * given `GtkLayoutManager`, if any is set.
 */
void
gtk_layout_manager_remove_layout_child (GtkLayoutManager *manager,
                                        GtkWidget        *widget)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (manager);

  if (priv->layout_children != NULL)
    {
      g_hash_table_remove (priv->layout_children, widget);
      if (g_hash_table_size (priv->layout_children) == 0)
        g_clear_pointer (&priv->layout_children, g_hash_table_unref);
    }
}

/**
 * gtk_layout_manager_get_layout_child:
 * @manager: a `GtkLayoutManager`
 * @child: a `GtkWidget`
 *
 * Retrieves a `GtkLayoutChild` instance for the `GtkLayoutManager`,
 * creating one if necessary.
 *
 * The @child widget must be a child of the widget using @manager.
 *
 * The `GtkLayoutChild` instance is owned by the `GtkLayoutManager`,
 * and is guaranteed to exist as long as @child is a child of the
 * `GtkWidget` using the given `GtkLayoutManager`.
 *
 * Returns: (transfer none): a `GtkLayoutChild`
 */
GtkLayoutChild *
gtk_layout_manager_get_layout_child (GtkLayoutManager *manager,
                                     GtkWidget        *child)
{
  GtkLayoutManagerPrivate *priv = gtk_layout_manager_get_instance_private (manager);
  GtkLayoutChild *res;
  GtkWidget *parent;

  g_return_val_if_fail (GTK_IS_LAYOUT_MANAGER (manager), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  parent = _gtk_widget_get_parent (child);
  g_return_val_if_fail (parent != NULL, NULL);

  if (priv->widget != parent)
    {
      g_critical ("The parent %s %p of the widget %s %p does not "
                  "use the given layout manager of type %s %p",
                  gtk_widget_get_name (parent), parent,
                  gtk_widget_get_name (child), child,
                  G_OBJECT_TYPE_NAME (manager), manager);
      return NULL;
    }

  if (priv->layout_children == NULL)
    {
      priv->layout_children = g_hash_table_new_full (NULL, NULL,
                                                     NULL,
                                                     (GDestroyNotify) g_object_unref);
    }

  res = g_hash_table_lookup (priv->layout_children, child);
  if (res != NULL)
    {
      /* If the LayoutChild instance is stale, and refers to another
       * layout manager, then we simply ask the LayoutManager to
       * replace it, as it means the layout manager for the parent
       * widget was replaced
       */
      if (gtk_layout_child_get_layout_manager (res) == manager)
        return res;
    }

  res = GTK_LAYOUT_MANAGER_GET_CLASS (manager)->create_layout_child (manager, parent, child);
  if (res == NULL)
    {
      g_critical ("The layout manager of type %s %p does not create "
                  "GtkLayoutChild instances",
                  G_OBJECT_TYPE_NAME (manager), manager);
      return NULL;
    }

  g_assert (g_type_is_a (G_OBJECT_TYPE (res), GTK_TYPE_LAYOUT_CHILD));
  g_hash_table_insert (priv->layout_children, child, res);

  return res;
}

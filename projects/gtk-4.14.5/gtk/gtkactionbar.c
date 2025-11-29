/*
 * Copyright (c) 2013 - 2014 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "gtkactionbar.h"
#include "gtkbuildable.h"
#include "gtktypebuiltins.h"
#include "gtkbox.h"
#include "gtkrevealer.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"
#include "gtkcenterbox.h"
#include "gtkbinlayout.h"

#include <string.h>

/**
 * GtkActionBar:
 *
 * `GtkActionBar` is designed to present contextual actions.
 *
 * ![An example GtkActionBar](action-bar.png)
 *
 * It is expected to be displayed below the content and expand
 * horizontally to fill the area.
 *
 * It allows placing children at the start or the end. In addition, it
 * contains an internal centered box which is centered with respect to
 * the full width of the box, even if the children at either side take
 * up different amounts of space.
 *
 * # GtkActionBar as GtkBuildable
 *
 * The `GtkActionBar` implementation of the `GtkBuildable` interface supports
 * adding children at the start or end sides by specifying “start” or “end” as
 * the “type” attribute of a `<child>` element, or setting the center widget
 * by specifying “center” value.
 *
 * # CSS nodes
 *
 * ```
 * actionbar
 * ╰── revealer
 *     ╰── box
 *         ├── box.start
 *         │   ╰── [start children]
 *         ├── [center widget]
 *         ╰── box.end
 *             ╰── [end children]
 * ```
 *
 * A `GtkActionBar`'s CSS node is called `actionbar`. It contains a `revealer`
 * subnode, which contains a `box` subnode, which contains two `box` subnodes at
 * the start and end of the action bar, with `start` and `end style classes
 * respectively, as well as a center node that represents the center child.
 *
 * Each of the boxes contains children packed for that side.
 */

typedef struct _GtkActionBarClass         GtkActionBarClass;

struct _GtkActionBar
{
  GtkWidget parent;

  GtkWidget *center_box;
  GtkWidget *start_box;
  GtkWidget *end_box;
  GtkWidget *revealer;
};

struct _GtkActionBarClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_REVEALED,
  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { NULL, };

static void gtk_action_bar_buildable_interface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkActionBar, gtk_action_bar, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_action_bar_buildable_interface_init))

static void
gtk_action_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkActionBar *self = GTK_ACTION_BAR (object);

  switch (prop_id)
    {
    case PROP_REVEALED:
      gtk_action_bar_set_revealed (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_action_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkActionBar *self = GTK_ACTION_BAR (object);

  switch (prop_id)
    {
    case PROP_REVEALED:
      g_value_set_boolean (value, gtk_action_bar_get_revealed (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_action_bar_dispose (GObject *object)
{
  GtkActionBar *self = GTK_ACTION_BAR (object);

  g_clear_pointer (&self->revealer, gtk_widget_unparent);

  self->center_box = NULL;
  self->start_box = NULL;
  self->end_box = NULL;

  G_OBJECT_CLASS (gtk_action_bar_parent_class)->dispose (object);
}

static void
gtk_action_bar_class_init (GtkActionBarClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gtk_action_bar_set_property;
  object_class->get_property = gtk_action_bar_get_property;
  object_class->dispose = gtk_action_bar_dispose;

  widget_class->focus = gtk_widget_focus_child;

  /**
   * GtkActionBar:revealed: (attributes org.gtk.Property.get=gtk_action_bar_get_revealed org.gtk.Property.set=gtk_action_bar_set_revealed)
   *
   * Controls whether the action bar shows its contents.
   */
  props[PROP_REVEALED] =
    g_param_spec_boolean ("revealed", NULL, NULL,
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("actionbar"));
}

static void
gtk_action_bar_init (GtkActionBar *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  self->revealer = gtk_revealer_new ();
  gtk_widget_set_parent (self->revealer, widget);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
  gtk_revealer_set_transition_type (GTK_REVEALER (self->revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);

  self->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  self->end_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_widget_add_css_class (self->start_box, "start");
  gtk_widget_add_css_class (self->end_box, "end");

  self->center_box = gtk_center_box_new ();
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (self->center_box), self->start_box);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (self->center_box), self->end_box);

  gtk_revealer_set_child (GTK_REVEALER (self->revealer), self->center_box);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_action_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const char   *type)
{
  GtkActionBar *self = GTK_ACTION_BAR (buildable);

  if (g_strcmp0 (type, "start") == 0)
    gtk_action_bar_pack_start (self, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "center") == 0)
    gtk_action_bar_set_center_widget (self, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "end") == 0)
    gtk_action_bar_pack_end (self, GTK_WIDGET (child));
  else if (type == NULL && GTK_IS_WIDGET (child))
    gtk_action_bar_pack_start (self, GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_action_bar_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_action_bar_buildable_add_child;
}

/**
 * gtk_action_bar_pack_start:
 * @action_bar: A `GtkActionBar`
 * @child: the `GtkWidget` to be added to @action_bar
 *
 * Adds @child to @action_bar, packed with reference to the
 * start of the @action_bar.
 */
void
gtk_action_bar_pack_start (GtkActionBar *action_bar,
                           GtkWidget    *child)
{
  gtk_box_append (GTK_BOX (action_bar->start_box), child);
}

/**
 * gtk_action_bar_pack_end:
 * @action_bar: A `GtkActionBar`
 * @child: the `GtkWidget` to be added to @action_bar
 *
 * Adds @child to @action_bar, packed with reference to the
 * end of the @action_bar.
 */
void
gtk_action_bar_pack_end (GtkActionBar *action_bar,
                         GtkWidget    *child)
{
  gtk_box_insert_child_after (GTK_BOX (action_bar->end_box), child, NULL);
}

/**
 * gtk_action_bar_remove:
 * @action_bar: a `GtkActionBar`
 * @child: the `GtkWidget` to be removed
 *
 * Removes a child from @action_bar.
 */
void
gtk_action_bar_remove (GtkActionBar *action_bar,
                       GtkWidget    *child)
{
  if (gtk_widget_get_parent (child) == action_bar->start_box)
    gtk_box_remove (GTK_BOX (action_bar->start_box), child);
  else if (gtk_widget_get_parent (child) == action_bar->end_box)
    gtk_box_remove (GTK_BOX (action_bar->end_box), child);
  else if (child == gtk_center_box_get_center_widget (GTK_CENTER_BOX (action_bar->center_box)))
    gtk_center_box_set_center_widget (GTK_CENTER_BOX (action_bar->center_box), NULL);
  else
    g_warning ("Can't remove non-child %s %p from GtkActionBar %p",
               G_OBJECT_TYPE_NAME (child), child, action_bar);
}

/**
 * gtk_action_bar_set_center_widget:
 * @action_bar: a `GtkActionBar`
 * @center_widget: (nullable): a widget to use for the center
 *
 * Sets the center widget for the `GtkActionBar`.
 */
void
gtk_action_bar_set_center_widget (GtkActionBar *action_bar,
                                  GtkWidget    *center_widget)
{
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (action_bar->center_box), center_widget);
}

/**
 * gtk_action_bar_get_center_widget:
 * @action_bar: a `GtkActionBar`
 *
 * Retrieves the center bar widget of the bar.
 *
 * Returns: (transfer none) (nullable): the center `GtkWidget`
 */
GtkWidget *
gtk_action_bar_get_center_widget (GtkActionBar *action_bar)
{
  g_return_val_if_fail (GTK_IS_ACTION_BAR (action_bar), NULL);

  return gtk_center_box_get_center_widget (GTK_CENTER_BOX (action_bar->center_box));
}

/**
 * gtk_action_bar_new:
 *
 * Creates a new `GtkActionBar` widget.
 *
 * Returns: a new `GtkActionBar`
 */
GtkWidget *
gtk_action_bar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_ACTION_BAR, NULL));
}

/**
 * gtk_action_bar_set_revealed: (attributes org.gtk.Method.set_property=revealed)
 * @action_bar: a `GtkActionBar`
 * @revealed: The new value of the property
 *
 * Reveals or conceals the content of the action bar.
 *
 * Note: this does not show or hide @action_bar in the
 * [property@Gtk.Widget:visible] sense, so revealing has
 * no effect if the action bar is hidden.
 */
void
gtk_action_bar_set_revealed (GtkActionBar *action_bar,
                             gboolean      revealed)
{
  g_return_if_fail (GTK_IS_ACTION_BAR (action_bar));

  if (revealed == gtk_revealer_get_reveal_child (GTK_REVEALER (action_bar->revealer)))
    return;

  gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar->revealer), revealed);
  g_object_notify_by_pspec (G_OBJECT (action_bar), props[PROP_REVEALED]);
}

/**
 * gtk_action_bar_get_revealed: (attributes org.gtk.Method.get_property=revealed)
 * @action_bar: a `GtkActionBar`
 *
 * Gets whether the contents of the action bar are revealed.
 *
 * Returns: the current value of the [property@Gtk.ActionBar:revealed]
 *   property
 */
gboolean
gtk_action_bar_get_revealed (GtkActionBar *action_bar)
{
  g_return_val_if_fail (GTK_IS_ACTION_BAR (action_bar), FALSE);

  return gtk_revealer_get_reveal_child (GTK_REVEALER (action_bar->revealer));
}

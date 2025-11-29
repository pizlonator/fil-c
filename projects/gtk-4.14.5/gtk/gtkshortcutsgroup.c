/* gtkshortcutsgroup.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutsgroup.h"

#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtklabel.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkshortcutsshortcut.h"
#include "gtksizegroup.h"
#include "gtkaccessible.h"

/**
 * GtkShortcutsGroup:
 *
 * A `GtkShortcutsGroup` represents a group of related keyboard shortcuts
 * or gestures.
 *
 * The group has a title. It may optionally be associated with a view
 * of the application, which can be used to show only relevant shortcuts
 * depending on the application context.
 *
 * This widget is only meant to be used with [class@Gtk.ShortcutsWindow].
 *
 * The recommended way to construct a `GtkShortcutsGroup` is with
 * [class@Gtk.Builder], by using the `<child>` tag to populate a
 * `GtkShortcutsGroup` with one or more [class@Gtk.ShortcutsShortcut]
 * instances.
 *
 * If you need to add a shortcut programmatically, use
 * [method@Gtk.ShortcutsGroup.add_shortcut].
 */

struct _GtkShortcutsGroup
{
  GtkBox    parent_instance;

  GtkLabel *title;
  char     *view;
  guint     height;

  GtkSizeGroup *accel_size_group;
  GtkSizeGroup *title_size_group;
};

struct _GtkShortcutsGroupClass
{
  GtkBoxClass parent_class;
};

static void gtk_shortcuts_group_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkShortcutsGroup, gtk_shortcuts_group, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_shortcuts_group_buildable_iface_init))

enum {
  PROP_0,
  PROP_TITLE,
  PROP_VIEW,
  PROP_ACCEL_SIZE_GROUP,
  PROP_TITLE_SIZE_GROUP,
  PROP_HEIGHT,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
gtk_shortcuts_group_apply_accel_size_group (GtkShortcutsGroup *group,
                                            GtkWidget         *child)
{
  if (GTK_IS_SHORTCUTS_SHORTCUT (child))
    g_object_set (child, "accel-size-group", group->accel_size_group, NULL);
}

static void
gtk_shortcuts_group_apply_title_size_group (GtkShortcutsGroup *group,
                                            GtkWidget         *child)
{
  if (GTK_IS_SHORTCUTS_SHORTCUT (child))
    g_object_set (child, "title-size-group", group->title_size_group, NULL);
}

static void
gtk_shortcuts_group_set_accel_size_group (GtkShortcutsGroup *group,
                                          GtkSizeGroup      *size_group)
{
  GtkWidget *child;

  g_set_object (&group->accel_size_group, size_group);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (group));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    gtk_shortcuts_group_apply_accel_size_group (group, child);
}

static void
gtk_shortcuts_group_set_title_size_group (GtkShortcutsGroup *group,
                                          GtkSizeGroup      *size_group)
{
  GtkWidget *child;

  g_set_object (&group->title_size_group, size_group);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (group));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    gtk_shortcuts_group_apply_title_size_group (group, child);
}

static guint
gtk_shortcuts_group_get_height (GtkShortcutsGroup *group)
{
  GtkWidget *child;
  guint height;

  height = 1;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (group));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_get_visible (child))
        continue;
      else if (GTK_IS_SHORTCUTS_SHORTCUT (child))
        height += 1;
    }

  return height;
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_shortcuts_group_buildable_add_child (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const char   *type)
{
  if (GTK_IS_SHORTCUTS_SHORTCUT (child))
    {
      gtk_shortcuts_group_add_shortcut (GTK_SHORTCUTS_GROUP (buildable),
                                        GTK_SHORTCUTS_SHORTCUT (child));
    }
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_shortcuts_group_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_shortcuts_group_buildable_add_child;
}

static void
gtk_shortcuts_group_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkShortcutsGroup *self = GTK_SHORTCUTS_GROUP (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    case PROP_VIEW:
      g_value_set_string (value, self->view);
      break;

    case PROP_HEIGHT:
      g_value_set_uint (value, gtk_shortcuts_group_get_height (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_group_direction_changed (GtkWidget        *widget,
                                       GtkTextDirection  previous_dir)
{
  GTK_WIDGET_CLASS (gtk_shortcuts_group_parent_class)->direction_changed (widget, previous_dir);
  g_object_notify (G_OBJECT (widget), "height");
}

static void
gtk_shortcuts_group_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkShortcutsGroup *self = GTK_SHORTCUTS_GROUP (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_VIEW:
      g_free (self->view);
      self->view = g_value_dup_string (value);
      break;

    case PROP_ACCEL_SIZE_GROUP:
      gtk_shortcuts_group_set_accel_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    case PROP_TITLE_SIZE_GROUP:
      gtk_shortcuts_group_set_title_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_group_finalize (GObject *object)
{
  GtkShortcutsGroup *self = GTK_SHORTCUTS_GROUP (object);

  g_free (self->view);
  g_set_object (&self->accel_size_group, NULL);
  g_set_object (&self->title_size_group, NULL);

  G_OBJECT_CLASS (gtk_shortcuts_group_parent_class)->finalize (object);
}

static void
gtk_shortcuts_group_dispose (GObject *object)
{
  GtkShortcutsGroup *self = GTK_SHORTCUTS_GROUP (object);

  g_clear_pointer ((GtkWidget **)&self->title, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_shortcuts_group_parent_class)->dispose (object);
}

static void
gtk_shortcuts_group_class_init (GtkShortcutsGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_shortcuts_group_finalize;
  object_class->get_property = gtk_shortcuts_group_get_property;
  object_class->set_property = gtk_shortcuts_group_set_property;
  object_class->dispose = gtk_shortcuts_group_dispose;

  widget_class->direction_changed = gtk_shortcuts_group_direction_changed;

  /**
   * GtkShortcutsGroup:title:
   *
   * The title for this group of shortcuts.
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         "",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsGroup:view:
   *
   * An optional view that the shortcuts in this group are relevant for.
   *
   * The group will be hidden if the [property@Gtk.ShortcutsWindow:view-name]
   * property does not match the view of this group.
   *
   * Set this to %NULL to make the group always visible.
   */
  properties[PROP_VIEW] =
    g_param_spec_string ("view", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsGroup:accel-size-group:
   *
   * The size group for the accelerator portion of shortcuts in this group.
   *
   * This is used internally by GTK, and must not be modified by applications.
   */
  properties[PROP_ACCEL_SIZE_GROUP] =
    g_param_spec_object ("accel-size-group", NULL, NULL,
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsGroup:title-size-group:
   *
   * The size group for the textual portion of shortcuts in this group.
   *
   * This is used internally by GTK, and must not be modified by applications.
   */
  properties[PROP_TITLE_SIZE_GROUP] =
    g_param_spec_object ("title-size-group", NULL, NULL,
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsGroup:height:
   *
   * A rough measure for the number of lines in this group.
   *
   * This is used internally by GTK, and is not useful for applications.
   */
  properties[PROP_HEIGHT] =
    g_param_spec_uint ("height", NULL, NULL,
                       0, G_MAXUINT, 1,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_css_name (widget_class, I_("shortcuts-group"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
gtk_shortcuts_group_init (GtkShortcutsGroup *self)
{
  PangoAttrList *attrs;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), 10);

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  self->title = g_object_new (GTK_TYPE_LABEL,
                              "accessible-role", GTK_ACCESSIBLE_ROLE_CAPTION,
                              "attributes", attrs,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              NULL);
  pango_attr_list_unref (attrs);

  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->title));

  gtk_accessible_update_relation (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->title, NULL,
                                  -1);
}

/**
 * gtk_shortcuts_group_add_shortcut:
 * @self: a `GtkShortcutsGroup`
 * @shortcut: the `GtkShortcutsShortcut` to add
 *
 * Adds a shortcut to the shortcuts group.
 *
 * This is the programmatic equivalent to using [class@Gtk.Builder] and a
 * `<child>` tag to add the child. Adding children with other API is not
 * appropriate as `GtkShortcutsGroup` manages its children internally.
 *
 * Since: 4.14
 */
void
gtk_shortcuts_group_add_shortcut (GtkShortcutsGroup    *self,
                                  GtkShortcutsShortcut *shortcut)
{
  g_return_if_fail (GTK_IS_SHORTCUTS_GROUP (self));
  g_return_if_fail (GTK_IS_SHORTCUTS_SHORTCUT (shortcut));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (shortcut)) == NULL);

  GtkWidget *widget = GTK_WIDGET (shortcut);
  gtk_box_append (GTK_BOX (self), widget);
  gtk_shortcuts_group_apply_accel_size_group (self, widget);
  gtk_shortcuts_group_apply_title_size_group (self, widget);
}

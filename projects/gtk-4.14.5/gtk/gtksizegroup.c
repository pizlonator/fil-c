/* GTK - The GIMP Toolkit
 * gtksizegroup.c: 
 * Copyright (C) 2001 Red Hat Software
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
 */

#include "config.h"
#include <string.h>

#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkwidget.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtksizegroup-private.h"
#include "gtkwidgetprivate.h"


/**
 * GtkSizeGroup
 *
 * `GtkSizeGroup` groups widgets together so they all request the same size.
 *
 * This is typically useful when you want a column of widgets to have the
 * same size, but you can’t use a `GtkGrid`.
 *
 * In detail, the size requested for each widget in a `GtkSizeGroup` is
 * the maximum of the sizes that would have been requested for each
 * widget in the size group if they were not in the size group. The mode
 * of the size group (see [method@Gtk.SizeGroup.set_mode]) determines whether
 * this applies to the horizontal size, the vertical size, or both sizes.
 *
 * Note that size groups only affect the amount of space requested, not
 * the size that the widgets finally receive. If you want the widgets in
 * a `GtkSizeGroup` to actually be the same size, you need to pack them in
 * such a way that they get the size they request and not more.
 *
 * `GtkSizeGroup` objects are referenced by each widget in the size group,
 * so once you have added all widgets to a `GtkSizeGroup`, you can drop
 * the initial reference to the size group with g_object_unref(). If the
 * widgets in the size group are subsequently destroyed, then they will
 * be removed from the size group and drop their references on the size
 * group; when all widgets have been removed, the size group will be
 * freed.
 *
 * Widgets can be part of multiple size groups; GTK will compute the
 * horizontal size of a widget from the horizontal requisition of all
 * widgets that can be reached from the widget by a chain of size groups
 * of type %GTK_SIZE_GROUP_HORIZONTAL or %GTK_SIZE_GROUP_BOTH, and the
 * vertical size from the vertical requisition of all widgets that can be
 * reached from the widget by a chain of size groups of type
 * %GTK_SIZE_GROUP_VERTICAL or %GTK_SIZE_GROUP_BOTH.
 *
 * Note that only non-contextual sizes of every widget are ever consulted
 * by size groups (since size groups have no knowledge of what size a widget
 * will be allocated in one dimension, it cannot derive how much height
 * a widget will receive for a given width). When grouping widgets that
 * trade height for width in mode %GTK_SIZE_GROUP_VERTICAL or %GTK_SIZE_GROUP_BOTH:
 * the height for the minimum width will be the requested height for all
 * widgets in the group. The same is of course true when horizontally grouping
 * width for height widgets.
 *
 * Widgets that trade height-for-width should set a reasonably large minimum
 * width by way of [property@Gtk.Label:width-chars] for instance. Widgets with
 * static sizes as well as widgets that grow (such as ellipsizing text) need no
 * such considerations.
 *
 * # GtkSizeGroup as GtkBuildable
 *
 * Size groups can be specified in a UI definition by placing an `<object>`
 * element with `class="GtkSizeGroup"` somewhere in the UI definition. The
 * widgets that belong to the size group are specified by a `<widgets>` element
 * that may contain multiple `<widget>` elements, one for each member of the
 * size group. The ”name” attribute gives the id of the widget.
 *
 * An example of a UI definition fragment with `GtkSizeGroup`:
 * ```xml
 * <object class="GtkSizeGroup">
 *   <property name="mode">horizontal</property>
 *   <widgets>
 *     <widget name="radio1"/>
 *     <widget name="radio2"/>
 *   </widgets>
 * </object>
 * ```
 */

typedef struct _GtkSizeGroupClass GtkSizeGroupClass;
typedef struct _GtkSizeGroupPrivate GtkSizeGroupPrivate;

struct _GtkSizeGroupClass
{
  GObjectClass parent_class;
};

struct _GtkSizeGroupPrivate
{
  GSList         *widgets;

  guint8          mode;
};

enum {
  PROP_0,
  PROP_MODE
};

static void gtk_size_group_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec);
static void gtk_size_group_get_property (GObject      *object,
					 guint         prop_id,
					 GValue       *value,
					 GParamSpec   *pspec);

/* GtkBuildable */
static void     gtk_size_group_buildable_init             (GtkBuildableIface  *iface);
static gboolean gtk_size_group_buildable_custom_tag_start (GtkBuildable       *buildable,
                                                           GtkBuilder         *builder,
                                                           GObject            *child,
                                                           const char         *tagname,
                                                           GtkBuildableParser *parser,
                                                           gpointer           *data);
static void     gtk_size_group_buildable_custom_finished  (GtkBuildable       *buildable,
                                                           GtkBuilder         *builder,
                                                           GObject            *child,
                                                           const char         *tagname,
                                                           gpointer            user_data);

G_STATIC_ASSERT (GTK_SIZE_GROUP_HORIZONTAL == (1 << GTK_ORIENTATION_HORIZONTAL));
G_STATIC_ASSERT (GTK_SIZE_GROUP_VERTICAL == (1 << GTK_ORIENTATION_VERTICAL));
G_STATIC_ASSERT (GTK_SIZE_GROUP_BOTH == (GTK_SIZE_GROUP_HORIZONTAL | GTK_SIZE_GROUP_VERTICAL));

G_DEFINE_TYPE_WITH_CODE (GtkSizeGroup, gtk_size_group, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkSizeGroup)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_size_group_buildable_init))

static void
add_widget_to_closure (GHashTable *widgets,
                       GHashTable *groups,
                       GtkWidget  *widget,
		       int         orientation)
{
  GSList *tmp_groups, *tmp_widgets;

  if (g_hash_table_lookup (widgets, widget))
    return;

  g_hash_table_add (widgets, widget);

  for (tmp_groups = _gtk_widget_get_sizegroups (widget); tmp_groups; tmp_groups = tmp_groups->next)
    {
      GtkSizeGroup        *tmp_group = tmp_groups->data;
      GtkSizeGroupPrivate *tmp_priv  = gtk_size_group_get_instance_private (tmp_group);

      if (g_hash_table_lookup (groups, tmp_group))
        continue;

      if (orientation >= 0 && !(tmp_priv->mode & (1 << orientation)))
        continue;

      g_hash_table_add (groups, tmp_group);

      for (tmp_widgets = tmp_priv->widgets; tmp_widgets; tmp_widgets = tmp_widgets->next)
        add_widget_to_closure (widgets, groups, tmp_widgets->data, orientation);
    }
}

GHashTable *
_gtk_size_group_get_widget_peers (GtkWidget      *for_widget,
                                  GtkOrientation  orientation)
{
  GHashTable *widgets, *groups;

  widgets = g_hash_table_new (NULL, NULL);
  groups = g_hash_table_new (NULL, NULL);

  add_widget_to_closure (widgets, groups, for_widget, orientation);

  g_hash_table_unref (groups);

  return widgets;
}

static void
queue_resize_on_group (GtkSizeGroup *size_group)
{
  GtkSizeGroupPrivate *priv = gtk_size_group_get_instance_private (size_group);
  GSList *list;

  for (list = priv->widgets; list; list = list->next)
    {
      gtk_widget_queue_resize (list->data);
    }
}

static void
gtk_size_group_class_init (GtkSizeGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_size_group_set_property;
  gobject_class->get_property = gtk_size_group_get_property;

  /**
   * GtkSizeGroup:mode: (attributes org.gtk.Property.get=gtk_size_group_get_mode org.gtk.Property.set=gtk_size_group_set_mode)
   *
   * The direction in which the size group affects requested sizes.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode", NULL, NULL,
                                                      GTK_TYPE_SIZE_GROUP_MODE,
                                                      GTK_SIZE_GROUP_HORIZONTAL,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
gtk_size_group_init (GtkSizeGroup *size_group)
{
  GtkSizeGroupPrivate *priv;

  priv = gtk_size_group_get_instance_private (size_group);

  priv->widgets = NULL;
  priv->mode = GTK_SIZE_GROUP_HORIZONTAL;
}

static void
gtk_size_group_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_size_group_buildable_custom_tag_start;
  iface->custom_finished = gtk_size_group_buildable_custom_finished;
}

static void
gtk_size_group_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  GtkSizeGroup *size_group = GTK_SIZE_GROUP (object);

  switch (prop_id)
    {
    case PROP_MODE:
      gtk_size_group_set_mode (size_group, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_size_group_get_property (GObject      *object,
			     guint         prop_id,
			     GValue       *value,
			     GParamSpec   *pspec)
{
  GtkSizeGroup *size_group = GTK_SIZE_GROUP (object);
  GtkSizeGroupPrivate *priv = gtk_size_group_get_instance_private (size_group);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, priv->mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_size_group_new:
 * @mode: the mode for the new size group.
 *
 * Create a new `GtkSizeGroup`.
 *
 * Returns: a newly created `GtkSizeGroup`
 */
GtkSizeGroup *
gtk_size_group_new (GtkSizeGroupMode mode)
{
  GtkSizeGroup *size_group = g_object_new (GTK_TYPE_SIZE_GROUP, NULL);
  GtkSizeGroupPrivate *priv = gtk_size_group_get_instance_private (size_group);

  priv->mode = mode;

  return size_group;
}

/**
 * gtk_size_group_set_mode: (attributes org.gtk.Method.set_property=mode)
 * @size_group: a `GtkSizeGroup`
 * @mode: the mode to set for the size group.
 *
 * Sets the `GtkSizeGroupMode` of the size group.
 *
 * The mode of the size group determines whether the widgets in the
 * size group should all have the same horizontal requisition
 * (%GTK_SIZE_GROUP_HORIZONTAL) all have the same vertical requisition
 * (%GTK_SIZE_GROUP_VERTICAL), or should all have the same requisition
 * in both directions (%GTK_SIZE_GROUP_BOTH).
 */
void
gtk_size_group_set_mode (GtkSizeGroup     *size_group,
			 GtkSizeGroupMode  mode)
{
  GtkSizeGroupPrivate *priv = gtk_size_group_get_instance_private (size_group);

  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));

  if (priv->mode != mode)
    {
      if (priv->mode != GTK_SIZE_GROUP_NONE)
	queue_resize_on_group (size_group);
      priv->mode = mode;
      if (priv->mode != GTK_SIZE_GROUP_NONE)
	queue_resize_on_group (size_group);

      g_object_notify (G_OBJECT (size_group), "mode");
    }
}

/**
 * gtk_size_group_get_mode: (attributes org.gtk.Method.get_property=mode)
 * @size_group: a `GtkSizeGroup`
 *
 * Gets the current mode of the size group.
 *
 * Returns: the current mode of the size group.
 */
GtkSizeGroupMode
gtk_size_group_get_mode (GtkSizeGroup *size_group)
{
  GtkSizeGroupPrivate *priv = gtk_size_group_get_instance_private (size_group);

  g_return_val_if_fail (GTK_IS_SIZE_GROUP (size_group), GTK_SIZE_GROUP_BOTH);

  return priv->mode;
}

/**
 * gtk_size_group_add_widget:
 * @size_group: a `GtkSizeGroup`
 * @widget: the `GtkWidget` to add
 *
 * Adds a widget to a `GtkSizeGroup`.
 *
 * In the future, the requisition
 * of the widget will be determined as the maximum of its requisition
 * and the requisition of the other widgets in the size group.
 * Whether this applies horizontally, vertically, or in both directions
 * depends on the mode of the size group.
 * See [method@Gtk.SizeGroup.set_mode].
 *
 * When the widget is destroyed or no longer referenced elsewhere, it
 * will be removed from the size group.
 */
void
gtk_size_group_add_widget (GtkSizeGroup *size_group,
			   GtkWidget    *widget)
{
  GtkSizeGroupPrivate *priv = gtk_size_group_get_instance_private (size_group);
  GSList *groups;
  
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  groups = _gtk_widget_get_sizegroups (widget);

  if (!g_slist_find (groups, size_group))
    {
      _gtk_widget_add_sizegroup (widget, size_group);

      priv->widgets = g_slist_prepend (priv->widgets, widget);

      g_object_ref (size_group);
    }
  
  queue_resize_on_group (size_group);
}

/**
 * gtk_size_group_remove_widget:
 * @size_group: a `GtkSizeGroup`
 * @widget: the `GtkWidget` to remove
 *
 * Removes a widget from a `GtkSizeGroup`.
 */
void
gtk_size_group_remove_widget (GtkSizeGroup *size_group,
			      GtkWidget    *widget)
{
  GtkSizeGroupPrivate *priv = gtk_size_group_get_instance_private (size_group);
  
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_return_if_fail (g_slist_find (priv->widgets, widget));

  _gtk_widget_remove_sizegroup (widget, size_group);

  priv->widgets = g_slist_remove (priv->widgets, widget);
  queue_resize_on_group (size_group);
  gtk_widget_queue_resize (widget);

  g_object_unref (size_group);
}

/**
 * gtk_size_group_get_widgets:
 * @size_group: a `GtkSizeGroup`
 *
 * Returns the list of widgets associated with @size_group.
 *
 * Returns:  (element-type GtkWidget) (transfer none): a `GSList` of
 *   widgets. The list is owned by GTK and should not be modified.
 */
GSList *
gtk_size_group_get_widgets (GtkSizeGroup *size_group)
{
  GtkSizeGroupPrivate *priv = gtk_size_group_get_instance_private (size_group);

  return priv->widgets;
}

typedef struct {
  char *name;
  int line;
  int col;
} ItemData;

static void
item_data_free (gpointer data)
{
  ItemData *item_data = data;

  g_free (item_data->name);
  g_free (item_data);
}

typedef struct {
  GObject *object;
  GtkBuilder *builder;
  GSList *items;
} GSListSubParserData;

static void
size_group_start_element (GtkBuildableParseContext  *context,
                          const char                *element_name,
                          const char               **names,
                          const char               **values,
                          gpointer                   user_data,
                          GError                   **error)
{
  GSListSubParserData *data = (GSListSubParserData*)user_data;

  if (strcmp (element_name, "widget") == 0)
    {
      const char *name;
      ItemData *item_data;

      if (!_gtk_builder_check_parent (data->builder, context, "widgets", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      item_data = g_new (ItemData, 1);
      item_data->name = g_strdup (name);
      gtk_buildable_parse_context_get_position (context, &item_data->line, &item_data->col);
      data->items = g_slist_prepend (data->items, item_data);
    }
  else if (strcmp (element_name, "widgets") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkSizeGroup", element_name,
                                        error);
    }
}

static const GtkBuildableParser size_group_parser =
  {
    size_group_start_element
  };

static gboolean
gtk_size_group_buildable_custom_tag_start (GtkBuildable       *buildable,
                                           GtkBuilder         *builder,
                                           GObject            *child,
                                           const char         *tagname,
                                           GtkBuildableParser *parser,
                                           gpointer           *parser_data)
{
  GSListSubParserData *data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "widgets") == 0)
    {
      data = g_new0 (GSListSubParserData, 1);
      data->items = NULL;
      data->object = G_OBJECT (buildable);
      data->builder = builder;

      *parser = size_group_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_size_group_buildable_custom_finished (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const char    *tagname,
                                          gpointer       user_data)
{
  GSList *l;
  GSListSubParserData *data;
  GObject *object;

  if (strcmp (tagname, "widgets") != 0)
    return;

  data = (GSListSubParserData*)user_data;
  data->items = g_slist_reverse (data->items);

  for (l = data->items; l; l = l->next)
    {
      ItemData *item_data = l->data;
      object = _gtk_builder_lookup_object (builder, item_data->name, item_data->line, item_data->col);
      if (!object)
        continue;
      gtk_size_group_add_widget (GTK_SIZE_GROUP (data->object), GTK_WIDGET (object));
    }
  g_slist_free_full (data->items, item_data_free);
  g_free (data);
}

/* gtkbuildable.c
 * Copyright (C) 2006-2007 Async Open Source,
 *                         Johan Dahlin <jdahlin@async.com.br>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkBuildable:
 *
 * `GtkBuildable` allows objects to extend and customize their deserialization
 * from ui files.
 *
 * The interface includes methods for setting names and properties of objects,
 * parsing custom tags and constructing child objects.
 *
 * The `GtkBuildable` interface is implemented by all widgets and
 * many of the non-widget objects that are provided by GTK. The
 * main user of this interface is [class@Gtk.Builder]. There should be
 * very little need for applications to call any of these functions directly.
 *
 * An object only needs to implement this interface if it needs to extend the
 * `GtkBuilder` XML format or run any extra routines at deserialization time.
 */

#include "config.h"
#include "gtkbuildableprivate.h"


typedef GtkBuildableIface GtkBuildableInterface;
G_DEFINE_INTERFACE (GtkBuildable, gtk_buildable, G_TYPE_OBJECT)

static void
gtk_buildable_default_init (GtkBuildableInterface *iface)
{
}

/*< private >
 * gtk_buildable_set_buildable_id:
 * @buildable: a `GtkBuildable`
 * @id: name to set
 *
 * Sets the ID of the @buildable object.
 */
void
gtk_buildable_set_buildable_id (GtkBuildable *buildable,
                                const char   *id)
{
  GtkBuildableIface *iface;

  g_return_if_fail (GTK_IS_BUILDABLE (buildable));
  g_return_if_fail (id != NULL);

  iface = GTK_BUILDABLE_GET_IFACE (buildable);

  if (iface->set_id)
    (* iface->set_id) (buildable, id);
  else
    g_object_set_data_full (G_OBJECT (buildable),
			    "gtk-builder-id",
			    g_strdup (id),
			    g_free);
}

/**
 * gtk_buildable_get_buildable_id:
 * @buildable: a `GtkBuildable`
 *
 * Gets the ID of the @buildable object.
 *
 * `GtkBuilder` sets the name based on the ID attribute
 * of the `<object>` tag used to construct the @buildable.
 *
 * Returns: (nullable): the ID of the buildable object
 **/
const char *
gtk_buildable_get_buildable_id (GtkBuildable *buildable)
{
  GtkBuildableIface *iface;

  g_return_val_if_fail (GTK_IS_BUILDABLE (buildable), NULL);

  iface = GTK_BUILDABLE_GET_IFACE (buildable);

  if (iface->get_id)
    return (* iface->get_id) (buildable);
  else
    return (const char *)g_object_get_data (G_OBJECT (buildable),
					    "gtk-builder-id");
}

/*< private >
 * gtk_buildable_add_child:
 * @buildable: a `GtkBuildable`
 * @builder: a `GtkBuilder`
 * @child: child to add
 * @type: (nullable): kind of child
 *
 * Adds a child to @buildable. @type is an optional string
 * describing how the child should be added.
 */
void
gtk_buildable_add_child (GtkBuildable *buildable,
			 GtkBuilder   *builder,
			 GObject      *child,
			 const char   *type)
{
  GtkBuildableIface *iface;

  g_return_if_fail (GTK_IS_BUILDABLE (buildable));
  g_return_if_fail (GTK_IS_BUILDER (builder));

  iface = GTK_BUILDABLE_GET_IFACE (buildable);
  g_return_if_fail (iface->add_child != NULL);

  (* iface->add_child) (buildable, builder, child, type);
}

/*< private >
 * gtk_buildable_parser_finished:
 * @buildable: a `GtkBuildable`
 * @builder: a `GtkBuilder`
 *
 * Called when the builder finishes the parsing of a
 * GtkBuilder UI definition.
 *
 * Note that this will be called once for each time
 * gtk_builder_add_from_file() or gtk_builder_add_from_string()
 * is called on a builder.
 */
void
gtk_buildable_parser_finished (GtkBuildable *buildable,
			       GtkBuilder   *builder)
{
  GtkBuildableIface *iface;

  g_return_if_fail (GTK_IS_BUILDABLE (buildable));
  g_return_if_fail (GTK_IS_BUILDER (builder));

  iface = GTK_BUILDABLE_GET_IFACE (buildable);
  if (iface->parser_finished)
    (* iface->parser_finished) (buildable, builder);
}

/*< private >
 * gtk_buildable_construct_child:
 * @buildable: A `GtkBuildable`
 * @builder: `GtkBuilder` used to construct this object
 * @name: name of child to construct
 *
 * Constructs a child of @buildable with the name @name.
 *
 * `GtkBuilder` calls this function if a “constructor” has been
 * specified in the UI definition.
 *
 * Returns: (transfer full): the constructed child
 */
GObject *
gtk_buildable_construct_child (GtkBuildable *buildable,
                               GtkBuilder   *builder,
                               const char   *name)
{
  GtkBuildableIface *iface;

  g_return_val_if_fail (GTK_IS_BUILDABLE (buildable), NULL);
  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  iface = GTK_BUILDABLE_GET_IFACE (buildable);
  g_return_val_if_fail (iface->construct_child != NULL, NULL);

  return (* iface->construct_child) (buildable, builder, name);
}

/*< private >
 * gtk_buildable_custom_tag_start:
 * @buildable: a `GtkBuildable`
 * @builder: a `GtkBuilder` used to construct this object
 * @child: (nullable): child object or %NULL for non-child tags
 * @tagname: name of tag
 * @parser: (out): a `GMarkupParser` to fill in
 * @data: (out): return location for user data that will be passed in
 *   to parser functions
 *
 * This is called for each unknown element under `<child>`.
 *
 * Returns: %TRUE if an object has a custom implementation, %FALSE
 *   if it doesn't.
 */
gboolean
gtk_buildable_custom_tag_start (GtkBuildable       *buildable,
                                GtkBuilder         *builder,
                                GObject            *child,
                                const char         *tagname,
                                GtkBuildableParser *parser,
                                gpointer           *data)
{
  GtkBuildableIface *iface;

  g_return_val_if_fail (GTK_IS_BUILDABLE (buildable), FALSE);
  g_return_val_if_fail (GTK_IS_BUILDER (builder), FALSE);
  g_return_val_if_fail (tagname != NULL, FALSE);

  iface = GTK_BUILDABLE_GET_IFACE (buildable);
  g_return_val_if_fail (iface->custom_tag_start != NULL, FALSE);

  return (* iface->custom_tag_start) (buildable, builder, child,
                                      tagname, parser, data);
}

/*< private >
 * gtk_buildable_custom_tag_end:
 * @buildable: A `GtkBuildable`
 * @builder: `GtkBuilder` used to construct this object
 * @child: (nullable): child object or %NULL for non-child tags
 * @tagname: name of tag
 * @data: user data that will be passed in to parser functions
 *
 * This is called at the end of each custom element handled by
 * the buildable.
 */
void
gtk_buildable_custom_tag_end (GtkBuildable  *buildable,
                              GtkBuilder    *builder,
                              GObject       *child,
                              const char    *tagname,
                              gpointer       data)
{
  GtkBuildableIface *iface;

  g_return_if_fail (GTK_IS_BUILDABLE (buildable));
  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (tagname != NULL);

  iface = GTK_BUILDABLE_GET_IFACE (buildable);
  if (iface->custom_tag_end)
    (* iface->custom_tag_end) (buildable, builder, child, tagname, data);
}

/*< private >
 * gtk_buildable_custom_finished:
 * @buildable: a `GtkBuildable`
 * @builder: a `GtkBuilder`
 * @child: (nullable): child object or %NULL for non-child tags
 * @tagname: the name of the tag
 * @data: user data created in custom_tag_start
 *
 * This is similar to gtk_buildable_parser_finished() but is
 * called once for each custom tag handled by the @buildable.
 */
void
gtk_buildable_custom_finished (GtkBuildable  *buildable,
			       GtkBuilder    *builder,
			       GObject       *child,
			       const char    *tagname,
			       gpointer       data)
{
  GtkBuildableIface *iface;

  g_return_if_fail (GTK_IS_BUILDABLE (buildable));
  g_return_if_fail (GTK_IS_BUILDER (builder));

  iface = GTK_BUILDABLE_GET_IFACE (buildable);
  if (iface->custom_finished)
    (* iface->custom_finished) (buildable, builder, child, tagname, data);
}

/*< private >
 * gtk_buildable_get_internal_child:
 * @buildable: a `GtkBuildable`
 * @builder: a `GtkBuilder`
 * @childname: name of child
 *
 * Get the internal child called @childname of the @buildable object.
 *
 * Returns: (transfer none): the internal child of the buildable object
 */
GObject *
gtk_buildable_get_internal_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  const char   *childname)
{
  GtkBuildableIface *iface;

  g_return_val_if_fail (GTK_IS_BUILDABLE (buildable), NULL);
  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (childname != NULL, NULL);

  iface = GTK_BUILDABLE_GET_IFACE (buildable);
  if (!iface->get_internal_child)
    return NULL;

  return (* iface->get_internal_child) (buildable, builder, childname);
}

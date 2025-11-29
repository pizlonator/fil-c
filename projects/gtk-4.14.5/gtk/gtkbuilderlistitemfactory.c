/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkbuilderlistitemfactory.h"

#include "gtkbuilder.h"
#include "gtkbuilderprivate.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitemprivate.h"

/**
 * GtkBuilderListItemFactory:
 *
 * `GtkBuilderListItemFactory` is a `GtkListItemFactory` that creates
 * widgets by instantiating `GtkBuilder` UI templates.
 *
 * The templates must be extending `GtkListItem`, and typically use
 * `GtkExpression`s to obtain data from the items in the model.
 *
 * Example:
 * ```xml
 *   <interface>
 *     <template class="GtkListItem">
 *       <property name="child">
 *         <object class="GtkLabel">
 *           <property name="xalign">0</property>
 *           <binding name="label">
 *             <lookup name="name" type="SettingsKey">
 *               <lookup name="item">GtkListItem</lookup>
 *             </lookup>
 *           </binding>
 *         </object>
 *       </property>
 *     </template>
 *   </interface>
 * ```
 */

struct _GtkBuilderListItemFactory
{
  GtkListItemFactory parent_instance;

  GtkBuilderScope *scope;
  GBytes *bytes;
  GBytes *data;
  char *resource;
};

struct _GtkBuilderListItemFactoryClass
{
  GtkListItemFactoryClass parent_class;
};

enum {
  PROP_0,
  PROP_BYTES,
  PROP_RESOURCE,
  PROP_SCOPE,

  N_PROPS
};

G_DEFINE_TYPE (GtkBuilderListItemFactory, gtk_builder_list_item_factory, GTK_TYPE_LIST_ITEM_FACTORY)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_builder_list_item_factory_setup (GtkListItemFactory *factory,
                                     GObject            *item,
                                     gboolean            bind,
                                     GFunc               func,
                                     gpointer            data)
{
  GtkBuilderListItemFactory *self = GTK_BUILDER_LIST_ITEM_FACTORY (factory);
  GtkBuilder *builder;
  GError *error = NULL;

  GTK_LIST_ITEM_FACTORY_CLASS (gtk_builder_list_item_factory_parent_class)->setup (factory, item, bind, func, data);

  builder = gtk_builder_new ();

  gtk_builder_set_current_object (builder, item);
  if (self->scope)
    gtk_builder_set_scope (builder, self->scope);

  gtk_builder_set_allow_template_parents (builder, TRUE);
  if (!gtk_builder_extend_with_template (builder, G_OBJECT (item), G_OBJECT_TYPE (item),
                                         (const char *)g_bytes_get_data (self->data, NULL),
                                         g_bytes_get_size (self->data),
                                         &error))
    {
      g_critical ("Error building template for list item: %s", error->message);
      g_error_free (error);

      /* This should never happen, if the template XML cannot be built
       * then it is a critical programming error.
       */
      g_object_unref (builder);
      return;
    }

  g_object_unref (builder);
}

static void
gtk_builder_list_item_factory_get_property (GObject    *object,
                                            guint       property_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  GtkBuilderListItemFactory *self = GTK_BUILDER_LIST_ITEM_FACTORY (object);

  switch (property_id)
    {
    case PROP_BYTES:
      g_value_set_boxed (value, self->bytes);
      break;

    case PROP_RESOURCE:
      g_value_set_string (value, self->resource);
      break;

    case PROP_SCOPE:
      g_value_set_object (value, self->scope);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gboolean
gtk_builder_list_item_factory_set_bytes (GtkBuilderListItemFactory *self,
                                         GBytes                    *bytes)
{
  if (bytes == NULL)
    return FALSE;

  if (self->bytes)
    {
      g_critical ("Data for GtkBuilderListItemFactory has already been set.");
      return FALSE;
    }

  self->bytes = g_bytes_ref (bytes);

  if (!_gtk_buildable_parser_is_precompiled (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes)))
    {
      GError *error = NULL;
      GBytes *data;

      data = _gtk_buildable_parser_precompile (g_bytes_get_data (bytes, NULL),
                                               g_bytes_get_size (bytes),
                                               &error);
      if (data == NULL)
        {
          g_warning ("Failed to precompile template for GtkBuilderListItemFactory: %s", error->message);
          g_error_free (error);
          self->data = g_bytes_ref (bytes);
        }
      else
        {
          self->data = data;
        }
    }

  return TRUE;
}

static void
gtk_builder_list_item_factory_set_property (GObject      *object,
                                            guint         property_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  GtkBuilderListItemFactory *self = GTK_BUILDER_LIST_ITEM_FACTORY (object);

  switch (property_id)
    {
    case PROP_BYTES:
      gtk_builder_list_item_factory_set_bytes (self, g_value_get_boxed (value));
      break;

    case PROP_RESOURCE:
      {
        GError *error = NULL;
        GBytes *bytes;  
        const char *resource;

        resource = g_value_get_string (value);
        if (resource == NULL)
          break;

        bytes = g_resources_lookup_data (resource, 0, &error);
        if (bytes)
          {
            if (gtk_builder_list_item_factory_set_bytes (self, bytes))
              self->resource = g_strdup (resource);
            g_bytes_unref (bytes);
          }
        else
          {
            g_critical ("Unable to load resource for list item template: %s", error->message);
            g_error_free (error);
          }
      }
      break;

    case PROP_SCOPE:
      self->scope = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_builder_list_item_factory_finalize (GObject *object)
{
  GtkBuilderListItemFactory *self = GTK_BUILDER_LIST_ITEM_FACTORY (object);

  g_clear_object (&self->scope);
  g_bytes_unref (self->bytes);
  g_bytes_unref (self->data);
  g_free (self->resource);

  G_OBJECT_CLASS (gtk_builder_list_item_factory_parent_class)->finalize (object);
}

static void
gtk_builder_list_item_factory_class_init (GtkBuilderListItemFactoryClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkListItemFactoryClass *factory_class = GTK_LIST_ITEM_FACTORY_CLASS (klass);

  gobject_class->finalize = gtk_builder_list_item_factory_finalize;
  gobject_class->get_property = gtk_builder_list_item_factory_get_property;
  gobject_class->set_property = gtk_builder_list_item_factory_set_property;

  factory_class->setup = gtk_builder_list_item_factory_setup;

  /**
   * GtkBuilderListItemFactory:bytes: (attributes org.gtk.Property.get=gtk_builder_list_item_factory_get_bytes)
   *
   * `GBytes` containing the UI definition.
   */
  properties[PROP_BYTES] =
    g_param_spec_boxed ("bytes", NULL, NULL,
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkBuilderListItemFactory:resource: (attributes org.gtk.Property.get=gtk_builder_list_item_factory_get_resource)
   *
   * Path of the resource containing the UI definition.
   */
  properties[PROP_RESOURCE] =
    g_param_spec_string ("resource", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkBuilderListItemFactory:scope: (attributes org.gtk.Property.get=gtk_builder_list_item_factory_get_scope)
   *
   * `GtkBuilderScope` to use when instantiating listitems
   */
  properties[PROP_SCOPE] =
    g_param_spec_object ("scope", NULL, NULL,
                         GTK_TYPE_BUILDER_SCOPE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_builder_list_item_factory_init (GtkBuilderListItemFactory *self)
{
}

/**
 * gtk_builder_list_item_factory_new_from_bytes:
 * @scope: (nullable) (transfer none): A scope to use when instantiating
 * @bytes: the `GBytes` containing the ui file to instantiate
 *
 * Creates a new `GtkBuilderListItemFactory` that instantiates widgets
 * using @bytes as the data to pass to `GtkBuilder`.
 *
 * Returns: a new `GtkBuilderListItemFactory`
 **/
GtkListItemFactory *
gtk_builder_list_item_factory_new_from_bytes (GtkBuilderScope *scope,
                                              GBytes          *bytes)
{
  g_return_val_if_fail (bytes != NULL, NULL);

  return g_object_new (GTK_TYPE_BUILDER_LIST_ITEM_FACTORY,
                       "bytes", bytes,
                       "scope", scope,
                       NULL);
}

/**
 * gtk_builder_list_item_factory_new_from_resource:
 * @scope: (nullable) (transfer none): A scope to use when instantiating
 * @resource_path: valid path to a resource that contains the data
 *
 * Creates a new `GtkBuilderListItemFactory` that instantiates widgets
 * using data read from the given @resource_path to pass to `GtkBuilder`.
 *
 * Returns: a new `GtkBuilderListItemFactory`
 **/
GtkListItemFactory *
gtk_builder_list_item_factory_new_from_resource (GtkBuilderScope *scope,
                                                 const char      *resource_path)
{
  g_return_val_if_fail (scope == NULL || GTK_IS_BUILDER_SCOPE (scope), NULL);
  g_return_val_if_fail (resource_path != NULL, NULL);

  return g_object_new (GTK_TYPE_BUILDER_LIST_ITEM_FACTORY,
                       "resource", resource_path,
                       "scope", scope,
                       NULL);
}

/**
 * gtk_builder_list_item_factory_get_bytes: (attributes org.gtk.Method.get_property=bytes)
 * @self: a `GtkBuilderListItemFactory`
 *
 * Gets the data used as the `GtkBuilder` UI template for constructing
 * listitems.
 *
 * Returns: (transfer none): The `GtkBuilder` data
 */
GBytes *
gtk_builder_list_item_factory_get_bytes (GtkBuilderListItemFactory *self)
{
  g_return_val_if_fail (GTK_IS_BUILDER_LIST_ITEM_FACTORY (self), NULL);

  return self->bytes;
}

/**
 * gtk_builder_list_item_factory_get_resource: (attributes org.gtk.Method.get_property=resource)
 * @self: a `GtkBuilderListItemFactory`
 *
 * If the data references a resource, gets the path of that resource.
 *
 * Returns: (transfer none) (nullable): The path to the resource
 */
const char *
gtk_builder_list_item_factory_get_resource (GtkBuilderListItemFactory *self)
{
  g_return_val_if_fail (GTK_IS_BUILDER_LIST_ITEM_FACTORY (self), NULL);

  return self->resource;
}

/**
 * gtk_builder_list_item_factory_get_scope: (attributes org.gtk.Method.get_property=scope)
 * @self: a `GtkBuilderListItemFactory`
 *
 * Gets the scope used when constructing listitems.
 *
 * Returns: (transfer none) (nullable): The scope used when constructing listitems
 */
GtkBuilderScope *
gtk_builder_list_item_factory_get_scope (GtkBuilderListItemFactory *self)
{
  g_return_val_if_fail (GTK_IS_BUILDER_LIST_ITEM_FACTORY (self), NULL);

  return self->scope;
}

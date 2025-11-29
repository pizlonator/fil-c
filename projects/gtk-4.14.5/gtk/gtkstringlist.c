/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkstringlist.h"

#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkprivate.h"

/**
 * GtkStringList:
 *
 * `GtkStringList` is a list model that wraps an array of strings.
 *
 * The objects in the model are of type [class@Gtk.StringObject] and have
 * a "string" property that can be used inside expressions.
 *
 * `GtkStringList` is well-suited for any place where you would
 * typically use a `char*[]`, but need a list model.
 *
 * ## GtkStringList as GtkBuildable
 *
 * The `GtkStringList` implementation of the `GtkBuildable` interface
 * supports adding items directly using the `<items>` element and
 * specifying `<item>` elements for each item. Each `<item>` element
 * supports the regular translation attributes “translatable”,
 * “context” and “comments”.
 *
 * Here is a UI definition fragment specifying a `GtkStringList`
 *
 * ```xml
 * <object class="GtkStringList">
 *   <items>
 *     <item translatable="yes">Factory</item>
 *     <item translatable="yes">Home</item>
 *     <item translatable="yes">Subway</item>
 *   </items>
 * </object>
 * ```
 */

/* {{{ GtkStringObject */

/**
 * GtkStringObject:
 *
 * `GtkStringObject` is the type of items in a `GtkStringList`.
 *
 * A `GtkStringObject` is a wrapper around a `const char*`; it has
 * a [property@Gtk.StringObject:string] property that can be used
 * for property bindings and expressions.
 */

#define GDK_ARRAY_ELEMENT_TYPE GtkStringObject *
#define GDK_ARRAY_NAME objects
#define GDK_ARRAY_TYPE_NAME Objects
#define GDK_ARRAY_FREE_FUNC g_object_unref
#include "gdk/gdkarrayimpl.c"

struct _GtkStringObject
{
  GObject parent_instance;
  char *string;
};

enum {
  PROP_STRING = 1,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringObject, gtk_string_object, G_TYPE_OBJECT);

static void
gtk_string_object_init (GtkStringObject *object)
{
}

static void
gtk_string_object_finalize (GObject *object)
{
  GtkStringObject *self = GTK_STRING_OBJECT (object);

  g_free (self->string);

  G_OBJECT_CLASS (gtk_string_object_parent_class)->finalize (object);
}

static void
gtk_string_object_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkStringObject *self = GTK_STRING_OBJECT (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_value_set_string (value, self->string);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_string_object_class_init (GtkStringObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = gtk_string_object_finalize;
  object_class->get_property = gtk_string_object_get_property;

  /**
   * GtkStringObject:string: (attributes org.gtk.Property.get=gtk_string_object_get_string)
   *
   * The string.
   */
  pspec = g_param_spec_string ("string", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_STRING, pspec);

}

static GtkStringObject *
gtk_string_object_new_take (char *string)
{
  GtkStringObject *obj;

  obj = g_object_new (GTK_TYPE_STRING_OBJECT, NULL);
  obj->string = string;

  return obj;
}

/**
 * gtk_string_object_new:
 * @string: (not nullable): The string to wrap
 *
 * Wraps a string in an object for use with `GListModel`.
 *
 * Returns: a new `GtkStringObject`
 */
GtkStringObject *
gtk_string_object_new (const char *string)
{
  return gtk_string_object_new_take (g_strdup (string));
}

/**
 * gtk_string_object_get_string: (attributes org.gtk.Method.get_property=string)
 * @self: a `GtkStringObject`
 *
 * Returns the string contained in a `GtkStringObject`.
 *
 * Returns: the string of @self
 */
const char *
gtk_string_object_get_string (GtkStringObject *self)
{
  g_return_val_if_fail (GTK_IS_STRING_OBJECT (self), NULL);

  return self->string;
}

/* }}} */
/* {{{ List model implementation */

struct _GtkStringList
{
  GObject parent_instance;

  Objects items;
};

struct _GtkStringListClass
{
  GObjectClass parent_class;
};

static GType
gtk_string_list_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_string_list_get_n_items (GListModel *list)
{
  GtkStringList *self = GTK_STRING_LIST (list);

  return objects_get_size (&self->items);
}

static gpointer
gtk_string_list_get_item (GListModel *list,
                          guint       position)
{
  GtkStringList *self = GTK_STRING_LIST (list);

  if (position >= objects_get_size (&self->items))
    return NULL;

  return g_object_ref (objects_get (&self->items, position));
}

static void
gtk_string_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_string_list_get_item_type;
  iface->get_n_items = gtk_string_list_get_n_items;
  iface->get_item = gtk_string_list_get_item;
}

/* }}} */
/* {{{ Buildable implementation */

typedef struct
{
  GtkBuilder    *builder;
  GtkStringList *list;
  GString       *string;
  const char    *domain;
  char          *context;

  guint          translatable : 1;
  guint          is_text      : 1;
} ItemParserData;

static void
item_start_element (GtkBuildableParseContext  *context,
                    const char                *element_name,
                    const char               **names,
                    const char               **values,
                    gpointer                   user_data,
                    GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  if (strcmp (element_name, "items") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "item") == 0)
    {
      gboolean translatable = FALSE;
      const char *msg_context = NULL;

      if (!_gtk_builder_check_parent (data->builder, context, "items", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &msg_context,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->is_text = TRUE;
      data->translatable = translatable;
      data->context = g_strdup (msg_context);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkStringList", element_name,
                                        error);
    }
}

static void
item_text (GtkBuildableParseContext  *context,
           const char                *text,
           gsize                      text_len,
           gpointer                   user_data,
           GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
item_end_element (GtkBuildableParseContext  *context,
                  const char                *element_name,
                  gpointer                   user_data,
                  GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  /* Append the translated strings */
  if (data->string->len)
    {
      if (data->translatable)
        {
          const char *translated;

          translated = _gtk_builder_parser_translate (data->domain,
                                                      data->context,
                                                      data->string->str);
          g_string_assign (data->string, translated);
        }

      gtk_string_list_append (data->list, data->string->str);
    }

  data->translatable = FALSE;
  g_string_set_size (data->string, 0);
  g_clear_pointer (&data->context, g_free);
  data->is_text = FALSE;
}

static const GtkBuildableParser item_parser =
{
  item_start_element,
  item_end_element,
  item_text
};

static gboolean
gtk_string_list_buildable_custom_tag_start (GtkBuildable       *buildable,
                                            GtkBuilder         *builder,
                                            GObject            *child,
                                            const char         *tagname,
                                            GtkBuildableParser *parser,
                                            gpointer           *parser_data)
{
  if (strcmp (tagname, "items") == 0)
    {
      ItemParserData *data;

      data = g_new0 (ItemParserData, 1);
      data->builder = g_object_ref (builder);
      data->list = g_object_ref (GTK_STRING_LIST (buildable));
      data->domain = gtk_builder_get_translation_domain (builder);
      data->string = g_string_new ("");

      *parser = item_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_string_list_buildable_custom_finished (GtkBuildable *buildable,
                                           GtkBuilder   *builder,
                                           GObject      *child,
                                           const char   *tagname,
                                           gpointer      user_data)
{
  if (strcmp (tagname, "items") == 0)
    {
      ItemParserData *data;

      data = (ItemParserData*)user_data;
      g_object_unref (data->list);
      g_object_unref (data->builder);
      g_string_free (data->string, TRUE);
      g_free (data);
    }
}

static void
gtk_string_list_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_string_list_buildable_custom_tag_start;
  iface->custom_finished = gtk_string_list_buildable_custom_finished;
}

/* }}} */
/* {{{ GObject implementation */

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_N_ITEMS,
  PROP_STRINGS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE_WITH_CODE (GtkStringList, gtk_string_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_string_list_buildable_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                gtk_string_list_model_init))

static void
gtk_string_list_dispose (GObject *object)
{
  GtkStringList *self = GTK_STRING_LIST (object);

  objects_clear (&self->items);

  G_OBJECT_CLASS (gtk_string_list_parent_class)->dispose (object);
}

static void
gtk_string_list_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkStringList *self = GTK_STRING_LIST (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, gtk_string_list_get_item_type (G_LIST_MODEL (self)));
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, gtk_string_list_get_n_items (G_LIST_MODEL (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_string_list_set_property (GObject      *object,
                              unsigned int  prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkStringList *self = GTK_STRING_LIST (object);

  switch (prop_id)
    {
    case PROP_STRINGS:
      gtk_string_list_splice (self, 0, 0,
                              (const char * const *) g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_string_list_class_init (GtkStringListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = gtk_string_list_dispose;
  gobject_class->get_property = gtk_string_list_get_property;
  gobject_class->set_property = gtk_string_list_set_property;

  /**
   * GtkStringList:item-type:
   *
   * The type of items. See [method@Gio.ListModel.get_item_type].
   *
   * Since: 4.14
   **/
  properties[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        G_TYPE_OBJECT,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkStringList:n-items:
   *
   * The number of items. See [method@Gio.ListModel.get_n_items].
   *
   * Since: 4.14
   **/
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkStringList:strings:
   *
   * Since: 4.10
   */
  properties[PROP_STRINGS] =
      g_param_spec_boxed ("strings", NULL, NULL,
                          G_TYPE_STRV,
                          G_PARAM_WRITABLE|G_PARAM_STATIC_STRINGS|G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_string_list_init (GtkStringList *self)
{
  objects_init (&self->items);
}

/* }}} */
/* {{{ Public API */

/**
 * gtk_string_list_new:
 * @strings: (array zero-terminated=1) (nullable): The strings to put in the model
 *
 * Creates a new `GtkStringList` with the given @strings.
 *
 * Returns: a new `GtkStringList`
 */
GtkStringList *
gtk_string_list_new (const char * const *strings)
{
  return g_object_new (GTK_TYPE_STRING_LIST,
                       "strings", strings,
                       NULL);
}

/**
 * gtk_string_list_splice:
 * @self: a `GtkStringList`
 * @position: the position at which to make the change
 * @n_removals: the number of strings to remove
 * @additions: (array zero-terminated=1) (nullable): The strings to add
 *
 * Changes @self by removing @n_removals strings and adding @additions
 * to it.
 *
 * This function is more efficient than [method@Gtk.StringList.append]
 * and [method@Gtk.StringList.remove], because it only emits the
 * ::items-changed signal once for the change.
 *
 * This function copies the strings in @additions.
 *
 * The parameters @position and @n_removals must be correct (ie:
 * @position + @n_removals must be less than or equal to the length
 * of the list at the time this function is called).
 */
void
gtk_string_list_splice (GtkStringList      *self,
                        guint               position,
                        guint               n_removals,
                        const char * const *additions)
{
  guint i, n_additions;

  g_return_if_fail (GTK_IS_STRING_LIST (self));
  g_return_if_fail (position + n_removals >= position); /* overflow */
  g_return_if_fail (position + n_removals <= objects_get_size (&self->items));

  if (additions)
    n_additions = g_strv_length ((char **) additions);
  else
    n_additions = 0;

  objects_splice (&self->items, position, n_removals, FALSE, NULL, n_additions);

  for (i = 0; i < n_additions; i++)
    {
      *objects_index (&self->items, position + i) = gtk_string_object_new (additions[i]);
    }

  if (n_removals || n_additions)
    g_list_model_items_changed (G_LIST_MODEL (self), position, n_removals, n_additions);

  if (n_removals != n_additions)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

/**
 * gtk_string_list_append:
 * @self: a `GtkStringList`
 * @string: the string to insert
 *
 * Appends @string to @self.
 *
 * The @string will be copied. See
 * [method@Gtk.StringList.take] for a way to avoid that.
 */
void
gtk_string_list_append (GtkStringList *self,
                        const char    *string)
{
  g_return_if_fail (GTK_IS_STRING_LIST (self));

  objects_append (&self->items, gtk_string_object_new (string));

  g_list_model_items_changed (G_LIST_MODEL (self), objects_get_size (&self->items) - 1, 0, 1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

/**
 * gtk_string_list_take:
 * @self: a `GtkStringList`
 * @string: (transfer full): the string to insert
 *
 * Adds @string to self at the end, and takes
 * ownership of it.
 *
 * This variant of [method@Gtk.StringList.append]
 * is convenient for formatting strings:
 *
 * ```c
 * gtk_string_list_take (self, g_strdup_print ("%d dollars", lots));
 * ```
 */
void
gtk_string_list_take (GtkStringList *self,
                      char          *string)
{
  g_return_if_fail (GTK_IS_STRING_LIST (self));

  objects_append (&self->items, gtk_string_object_new_take (string));

  g_list_model_items_changed (G_LIST_MODEL (self), objects_get_size (&self->items) - 1, 0, 1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

/**
 * gtk_string_list_remove:
 * @self: a `GtkStringList`
 * @position: the position of the string that is to be removed
 *
 * Removes the string at @position from @self.
 *
 * @position must be smaller than the current
 * length of the list.
 */
void
gtk_string_list_remove (GtkStringList *self,
                        guint          position)
{
  g_return_if_fail (GTK_IS_STRING_LIST (self));

  gtk_string_list_splice (self, position, 1, NULL);
}

/**
 * gtk_string_list_get_string:
 * @self: a `GtkStringList`
 * @position: the position to get the string for
 *
 * Gets the string that is at @position in @self.
 *
 * If @self does not contain @position items, %NULL is returned.
 *
 * This function returns the const char *. To get the
 * object wrapping it, use g_list_model_get_item().
 *
 * Returns: (nullable): the string at the given position
 */
const char *
gtk_string_list_get_string (GtkStringList *self,
                            guint          position)
{
  g_return_val_if_fail (GTK_IS_STRING_LIST (self), NULL);

  if (position >= objects_get_size (&self->items))
    return NULL;

  return objects_get (&self->items, position)->string;
}

/* }}} */
/* vim:set foldmethod=marker expandtab: */

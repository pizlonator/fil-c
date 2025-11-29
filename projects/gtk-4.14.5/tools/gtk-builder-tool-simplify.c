/*  Copyright 2015 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtkbuilderprivate.h"
#include "gtk-builder-tool.h"

typedef struct Element Element;
struct Element {
  Element *parent;
  char *element_name;
  char **attribute_names;
  char **attribute_values;
  char *data;
  GList *children;

  int line_number;
  int char_number;
};

static void
free_element (gpointer data)
{
  Element *element = data;
  g_list_free_full (element->children, free_element);
  g_free (element->element_name);
  g_strfreev (element->attribute_names);
  g_strfreev (element->attribute_values);
  g_free (element->data);
  g_free (element);
}

typedef struct {
  Element *root;
  Element *current;
  GString *value;
  GtkBuilder *builder;
  const char *input_filename;
  char *output_filename;
  FILE *output;
  gboolean convert3to4;
  gboolean has_gtk_requires;
} MyParserData;

static void
start_element (GMarkupParseContext  *context,
               const char           *element_name,
               const char          **attribute_names,
               const char          **attribute_values,
               gpointer              user_data,
               GError              **error)
{
  MyParserData *data = user_data;
  Element *elt;

  elt = g_new0 (Element, 1);
  elt->parent = data->current;
  elt->element_name = g_strdup (element_name);
  elt->attribute_names = g_strdupv ((char **)attribute_names);
  elt->attribute_values = g_strdupv ((char **)attribute_values);

  g_markup_parse_context_get_position (context, &elt->line_number, &elt->char_number);

  if (data->current)
    data->current->children = g_list_append (data->current->children, elt);
  data->current = elt;

  if (data->root == NULL)
    data->root = elt;

  g_string_truncate (data->value, 0);
}

static void
end_element (GMarkupParseContext  *context,
             const char           *element_name,
             gpointer              user_data,
             GError              **error)
{
  MyParserData *data = user_data;

  data->current->data = g_strdup (data->value->str);

  data->current = data->current->parent;
}

static void
text (GMarkupParseContext  *context,
      const char           *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
  MyParserData *data = user_data;

  if (data->value)
    {
      g_string_append_len (data->value, text, text_len);
      return;
    }
}

static GMarkupParser parser = {
  start_element,
  end_element,
  text,
  NULL,
  NULL
};

static const char *
canonical_boolean_value (MyParserData *data,
                         const char   *string)
{
  GValue value = G_VALUE_INIT;
  gboolean b = FALSE;

  if (gtk_builder_value_from_string_type (data->builder, G_TYPE_BOOLEAN, string, &value, NULL))
    b = g_value_get_boolean (&value);

  return b ? "1" : "0";
}

typedef enum {
  PROP_KIND_OBJECT,
  PROP_KIND_PACKING,
  PROP_KIND_CELL_PACKING,
  PROP_KIND_LAYOUT
} PropKind;

static PropKind
get_prop_kind (Element *element)
{
  g_assert (g_str_equal (element->element_name, "property"));

  if (g_str_equal (element->parent->element_name, "packing"))
    return PROP_KIND_PACKING;
  else if (g_str_equal (element->parent->element_name, "layout"))
    return PROP_KIND_LAYOUT;
  else if (g_str_equal (element->parent->element_name, "cell-packing"))
    return PROP_KIND_CELL_PACKING;
  else
    return PROP_KIND_OBJECT;
}

/* A number of properties unfortunately can't be omitted even
 * if they are nominally set to their default value. In many
 * cases, this is due to subclasses not overriding the default
 * value from the superclass.
 */
static gboolean
needs_explicit_setting (GParamSpec *pspec,
                        PropKind    kind)
{
  struct _Prop {
    const char *class;
    const char *property;
    PropKind kind;
  } props[] = {
    { "GtkAboutDialog", "program-name", PROP_KIND_OBJECT },
    { "GtkCalendar", "year", PROP_KIND_OBJECT },
    { "GtkCalendar", "month", PROP_KIND_OBJECT },
    { "GtkCalendar", "day", PROP_KIND_OBJECT },
    { "GtkPlacesSidebar", "show-desktop", PROP_KIND_OBJECT },
    { "GtkRadioButton", "draw-indicator", PROP_KIND_OBJECT },
    { "GtkWidget", "hexpand", PROP_KIND_OBJECT },
    { "GtkWidget", "vexpand", PROP_KIND_OBJECT },
    { "GtkGridLayoutChild", "row", PROP_KIND_LAYOUT },
    { "GtkGridLayoutChild", "column", PROP_KIND_LAYOUT },
  };
  gboolean found;
  int k;
  const char *class_name;

  class_name = g_type_name (pspec->owner_type);

  found = FALSE;
  for (k = 0; k < G_N_ELEMENTS (props); k++)
    {
      if (strcmp (class_name, props[k].class) == 0 &&
          strcmp (pspec->name, props[k].property) == 0 &&
          kind == props[k].kind)
        {
          found = TRUE;
          break;
        }
    }

  return found;
}

static gboolean
has_attribute (Element    *elt,
               const char *name,
               const char *value)
{
  int i;

  for (i = 0; elt->attribute_names[i]; i++)
    {
      if (strcmp (elt->attribute_names[i], name) == 0 &&
          (value == NULL || strcmp (elt->attribute_values[i], value) == 0))
        return TRUE;
    }

  return FALSE;
}

static gboolean
is_cdata_property (Element *element)
{
  if (g_str_equal (element->element_name, "property") &&
      has_attribute (element, "name", "bytes") &&
      element->parent != NULL &&
      g_str_equal (element->parent->element_name, "object") &&
      has_attribute (element->parent, "class", "GtkBuilderListItemFactory"))
    return TRUE;

  return FALSE;
}

static gboolean
is_pcdata_element (Element *element)
{
  /* elements that can contain text */
  const char *names[] = {
    "property",
    "attribute",
    "action-widget",
    "pattern",
    "mime-type",
    "col",
    "item",
    "mark",
    "lookup",
    NULL,
  };

  if (g_str_equal (element->element_name, "property") &&
      (g_strv_contains ((const char * const *)element->attribute_names, "bind-source") ||
       g_strv_contains ((const char * const *)element->attribute_names, "bind_source")))
    return FALSE;

  if (g_strv_contains (names, element->element_name))
    return TRUE;

  return FALSE;
}

static gboolean
is_container_element (Element *element)
{
  /* elements that just hold a list of things and
   * can be omitted when they have no children
   */
  const char *names[] = {
    "packing",
    "layout",
    "cell-packing",
    "attributes",
    "action-widgets",
    "patterns",
    "mime-types",
    "attributes",
    "row",
    "items",
    NULL
  };

  if (g_strv_contains (names, element->element_name))
    return TRUE;

  return FALSE;
}

static void
canonicalize_key (char *key)
{
  char *p;

  for (p = key; *p != 0; p++)
    {
      char c = *p;

      /* We may meet something like AtkObject::accessible-name */
      if (c == ':' && ((p > key && p[-1] == ':') || p[1] == ':'))
        continue;

      if (c != '-' &&
          (c < '0' || c > '9') &&
          (c < 'A' || c > 'Z') &&
          (c < 'a' || c > 'z'))
        *p = '-';
    }
}

static struct {
  const char *class;
  const char *layout_manager;
} layout_managers[] = {
  { "GtkBox", "GtkBoxLayout" },
  { "GtkGrid", "GtkGridLayout" },
  { "GtkFixed", "GtkFixedLayout" },
  { "GtkFileChooserButton", "GtkBinLayout" },
  { "GtkFileChooserWidget", "GtkBinLayout" },
  { "GtkOverlay", "GtkOverlayLayout" }
};

static GParamSpec *
get_property_pspec (MyParserData *data,
                    const char   *class_name,
                    const char   *property_name,
                    PropKind      kind)
{
  GType type;
  GObjectClass *class;
  GParamSpec *pspec;
  char *canonical_name;

  type = g_type_from_name (class_name);
  if (type == G_TYPE_INVALID)
    {
      type = gtk_builder_get_type_from_name (data->builder, class_name);
      if (type == G_TYPE_INVALID)
        return NULL;
    }

  class = g_type_class_ref (type);
  canonical_name = g_strdup (property_name);
  canonicalize_key (canonical_name);
  switch (kind)
    {
    case PROP_KIND_OBJECT:
      pspec = g_object_class_find_property (class, canonical_name);
      break;

    case PROP_KIND_PACKING:
      pspec = NULL;
      break;

    case PROP_KIND_CELL_PACKING:
      {
        GObjectClass *cell_class;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        /* We're just assuming that the cell layout is using a GtkCellAreaBox. */
        cell_class = g_type_class_ref (GTK_TYPE_CELL_AREA_BOX);
        pspec = gtk_cell_area_class_find_cell_property (GTK_CELL_AREA_CLASS (cell_class), canonical_name);
        g_type_class_unref (cell_class);
G_GNUC_END_IGNORE_DEPRECATIONS
      }
      break;

    case PROP_KIND_LAYOUT:
      {
        int i;
        const char *layout_manager = NULL;

        pspec = NULL;

        for (i = 0; i < G_N_ELEMENTS (layout_managers); i++)
          {
            if (g_str_equal (layout_managers[i].class, class_name))
              {
                layout_manager = layout_managers[i].layout_manager;
                break;
              }
          }

        if (layout_manager)
          {
            GtkLayoutManagerClass *layout_manager_class;

            layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (g_type_class_ref (g_type_from_name (layout_manager)));
            if (layout_manager_class->layout_child_type != G_TYPE_INVALID)
              {
                GObjectClass *layout_child_class;
                layout_child_class = g_type_class_ref (layout_manager_class->layout_child_type);
                pspec = g_object_class_find_property (layout_child_class, canonical_name);
                g_type_class_unref (layout_child_class);
              }
            g_type_class_unref (layout_manager_class);
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }
  g_free (canonical_name);
  g_type_class_unref (class);

  return pspec;
}

static const char *get_class_name (Element *element);

static gboolean
value_is_default (Element      *element,
                  MyParserData *data,
                  GParamSpec   *pspec,
                  const char   *value_string)
{
  GValue value = { 0, };
  gboolean ret;
  GError *error = NULL;

  if (g_type_is_a (G_PARAM_SPEC_VALUE_TYPE (pspec), G_TYPE_OBJECT))
    return FALSE;

  if (g_type_is_a (G_PARAM_SPEC_VALUE_TYPE (pspec), G_TYPE_BOXED))
    return FALSE;

  if (!value_string)
    return FALSE;

  if (!gtk_builder_value_from_string (data->builder, pspec, value_string, &value, &error))
    {
      g_printerr (_("%s:%d: Couldn’t parse value for property '%s': %s\n"), data->input_filename, element->line_number, pspec->name, error->message);
      g_error_free (error);
      ret = FALSE;
    }
  else
    {
      /* GtkWidget::visible has a 'smart' default */
      if (pspec->owner_type == GTK_TYPE_WIDGET &&
          g_str_equal (pspec->name, "visible"))
        {
          const char *class_name = get_class_name (element);
          GType type = g_type_from_name (class_name);
          gboolean default_value;

          if (g_type_is_a (type, GTK_TYPE_ROOT) ||
              g_type_is_a (type, GTK_TYPE_POPOVER))
            default_value = FALSE;
          else
            default_value = TRUE;

          ret = g_value_get_boolean (&value) == default_value;
        }
      else if (pspec->owner_type == GTK_TYPE_WINDOW &&
               (g_str_equal (pspec->name, "default-width") ||
                g_str_equal (pspec->name, "default-height")))
        {
          int default_size;

          default_size = g_value_get_int (&value);
          ret = default_size <= 0;
        }
      else
        ret = g_param_value_defaults (pspec, &value);
    }

  g_value_reset (&value);

  return ret;
}

static const char *
get_attribute_value (Element *element,
                     const char *name)
{
  int i;

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (g_str_equal (element->attribute_names[i], name))
        return element->attribute_values[i];
    }

  return "";
}

static void
set_attribute_value (Element *element,
                     const char *name,
                     const char *value)
{
  int i;
  int len;

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (g_str_equal (element->attribute_names[i], name))
        {
          g_free (element->attribute_values[i]);
          element->attribute_values[i] = g_strdup (value);
          return;
        }
    }

  len = g_strv_length (element->attribute_names);
  element->attribute_names = g_realloc_n (element->attribute_names, len + 2, sizeof (char *));
  element->attribute_values = g_realloc_n (element->attribute_values, len + 2, sizeof (char *));
  element->attribute_names[len] = g_strdup (name);
  element->attribute_values[len] = g_strdup (value);
  element->attribute_names[len + 1] = NULL;
  element->attribute_values[len + 1] = NULL;
}

static gboolean
element_is_object_or_template (Element *element)
{
  return g_str_equal (element->element_name, "object") ||
         g_str_equal (element->element_name, "template");
}

static const char *
get_class_name (Element *element)
{
  Element *parent = element->parent;

  if (element_is_object_or_template (element))
    parent = element;

  if (g_str_equal (parent->element_name, "packing"))
    parent = parent->parent->parent; /* child - object */
  else if (g_str_equal (parent->element_name, "layout"))
    parent = parent->parent->parent->parent; /* object - child - object */


  if (g_str_equal (parent->element_name, "object"))
    {
      return get_attribute_value (parent, "class");
    }
  else if (g_str_equal (parent->element_name, "template"))
    {
      if (get_attribute_value (parent, "parent"))
        return get_attribute_value (parent, "parent");
      else
        return get_attribute_value (parent, "class");
    }

  return "";
}

static gboolean
property_is_boolean (Element      *element,
                     MyParserData *data)
{
  GParamSpec *pspec = NULL;
  const char *class_name;
  const char *property_name;
  int i;
  PropKind kind;

  kind = get_prop_kind (element);
  class_name = get_class_name (element);
  property_name = "";

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "name") == 0)
        property_name = (const char *)element->attribute_values[i];
    }

  if (class_name && property_name)
    pspec = get_property_pspec (data, class_name, property_name, kind);
  if (pspec)
    return G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN;

  return FALSE;
}

static gboolean
property_is_enum (Element      *element,
                  MyParserData *data,
                  GType        *type)
{
  GParamSpec *pspec = NULL;
  const char *class_name;
  const char *property_name;
  int i;
  PropKind kind;

  kind = get_prop_kind (element);
  class_name = get_class_name (element);
  property_name = "";

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "name") == 0)
        property_name = (const char *)element->attribute_values[i];
    }

  if (class_name && property_name)
    pspec = get_property_pspec (data, class_name, property_name, kind);
  if (pspec && G_TYPE_IS_ENUM (G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
      *type = G_PARAM_SPEC_VALUE_TYPE (pspec);
      return TRUE;
    }

  *type = G_TYPE_NONE;
  return FALSE;
}

static char *
canonical_enum_value (MyParserData *data,
                      GType         type,
                      const char   *string)
{
  GValue value = G_VALUE_INIT;

  if (gtk_builder_value_from_string_type (data->builder, type, string, &value, NULL))
    {
      GEnumClass *eclass = g_type_class_ref (type);
      GEnumValue *evalue = g_enum_get_value (eclass, g_value_get_enum (&value));

      if (evalue)
        return g_strdup (evalue->value_nick);
      else
        return g_strdup_printf ("%d", g_value_get_enum (&value));
    }

  return NULL;
}

static void
warn_missing_property (Element      *element,
                       MyParserData *data,
                       const char   *class_name,
                       const char   *property_name,
                       PropKind      kind)
{
  char *name;
  char *msg;

  name = g_strconcat (class_name, "::", property_name, NULL);
  switch (kind)
    {
    case PROP_KIND_OBJECT:
      msg = g_strdup_printf (_("Property %s not found"), name);
      break;
    case PROP_KIND_PACKING:
      msg = g_strdup_printf (_("Packing property %s not found"), name);
      break;
    case PROP_KIND_CELL_PACKING:
      msg = g_strdup_printf (_("Cell property %s not found"), name);
      break;
    case PROP_KIND_LAYOUT:
      msg = g_strdup_printf (_("Layout property %s not found"), name);
      break;
    default:
      g_assert_not_reached ();
    }

  g_printerr ("%s:%d: %s\n", data->input_filename, element->line_number, msg);
  g_free (name);
  g_free (msg);
}

static gboolean
property_can_be_omitted (Element      *element,
                         MyParserData *data)
{
  int i;
  gboolean bound;
  gboolean translatable;
  const char *class_name;
  const char *property_name;
  const char *value_string;
  GParamSpec *pspec;
  PropKind kind;

  kind = get_prop_kind (element);
  class_name = get_class_name (element);
  property_name = "";
  value_string = element->data;

  bound = FALSE;
  translatable = FALSE;
  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "bind-source") == 0 ||
          strcmp (element->attribute_names[i], "bind_source") == 0)
        bound = TRUE;
      else if (strcmp (element->attribute_names[i], "translatable") == 0)
        translatable = TRUE;
      else if (strcmp (element->attribute_names[i], "name") == 0)
        property_name = (const char *)element->attribute_values[i];
    }

  if (translatable)
    return FALSE;

  if (bound)
    return FALSE;

  pspec = get_property_pspec (data, class_name, property_name, kind);
  if (pspec == NULL)
    {
      warn_missing_property (element, data, class_name, property_name, kind);
      return FALSE;
    }

  if (needs_explicit_setting (pspec, kind))
    return FALSE;

  return value_is_default (element, data, pspec, value_string);
}

static gboolean
property_has_been_removed (Element      *element,
                           MyParserData *data)
{
  const char *class_name;
  const char *property_name;
  struct _Prop {
    const char *class;
    const char *property;
    PropKind kind;
  } props[] = {
    { "GtkActionBar", "position", PROP_KIND_PACKING },
    { "GtkButtonBox", "secondary", PROP_KIND_PACKING },
    { "GtkButtonBox", "non-homogeneous", PROP_KIND_PACKING },
    { "GtkBox", "position", PROP_KIND_PACKING },
    { "GtkBox", "pack-type", PROP_KIND_PACKING },
    { "GtkHeaderBar", "position", PROP_KIND_PACKING },
    { "GtkPopoverMenu", "position",PROP_KIND_PACKING },
    { "GtkCheckButton", "draw-indicator", PROP_KIND_OBJECT },
  };
  char *canonical_name;
  gboolean found;
  int i, k;
  PropKind kind;

  kind = get_prop_kind (element);

  class_name = get_class_name (element);
  property_name = "";

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "name") == 0)
        property_name = (const char *)element->attribute_values[i];
    }

  canonical_name = g_strdup (property_name);
  g_strdelimit (canonical_name, "_", '-');

  found = FALSE;
  for (k = 0; k < G_N_ELEMENTS (props); k++)
    {
      if (strcmp (class_name, props[k].class) == 0 &&
          strcmp (canonical_name, props[k].property) == 0 &&
          kind == props[k].kind)
        {
          found = TRUE;
          break;
        }
    }

  g_free (canonical_name);

  return found;
}

static void
maybe_rename_property (Element *element, MyParserData *data)
{
  const char *class_name;
  const char *property_name;
  struct _Prop {
    const char *class;
    const char *property;
    GType type;
    PropKind kind;
    const char *new_name;
    const char *alt_names[3];
  } props[] = {
    /* the "replacement" property is placed *after* the "added" properties */
    { "GtkPopover", "modal", GTK_TYPE_POPOVER, PROP_KIND_OBJECT, "autohide", { NULL, NULL, NULL } },
    { "GtkWidget", "expand", GTK_TYPE_WIDGET, PROP_KIND_OBJECT, "vexpand", { "hexpand", NULL, NULL } },
    { "GtkWidget", "margin", GTK_TYPE_WIDGET, PROP_KIND_OBJECT, "margin-bottom", { "margin-start", "margin-end", "margin-top" } },
    { "GtkWidget", "margin-left", GTK_TYPE_WIDGET, PROP_KIND_OBJECT, "margin-start", { NULL, NULL, NULL } },
    { "GtkWidget", "margin-right", GTK_TYPE_WIDGET, PROP_KIND_OBJECT, "margin-end", { NULL, NULL, NULL } },
    { "GtkHeaderBar", "show-close-button", GTK_TYPE_HEADER_BAR, PROP_KIND_OBJECT, "show-title-buttons", { NULL, NULL, NULL } },
    { "GtkHeaderBar", "custom-title", GTK_TYPE_HEADER_BAR, PROP_KIND_OBJECT, "title-widget", { NULL, NULL, NULL } },
    { "GtkStack", "homogeneous", GTK_TYPE_STACK, PROP_KIND_OBJECT, "hhomogeneous", { "vhomogeneous", NULL, NULL } },
    { "GtkImage", "pixbuf", GTK_TYPE_IMAGE, PROP_KIND_OBJECT, "file", { NULL, NULL, NULL } },
    { "GtkWidget", "can-focus", GTK_TYPE_WIDGET, PROP_KIND_OBJECT, "focusable", { NULL, NULL, NULL } },
  };
  int i, k, l;
  PropKind kind;
  int prop_name_index = 0;
  GType type;
  char *canonical_name;

  kind = get_prop_kind (element);

  class_name = get_class_name (element);
  property_name = NULL;

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "name") == 0)
        {
          prop_name_index = i;
          property_name = (const char *)element->attribute_values[i];
        }
    }

  if (property_name == NULL)
    return;

  type = g_type_from_name (class_name);

  canonical_name = g_strdup (property_name);
  g_strdelimit (canonical_name, "_", '-');

  for (k = 0; k < G_N_ELEMENTS (props); k++)
    {
      if (g_type_is_a (type, props[k].type) &&
          strcmp (canonical_name, props[k].property) == 0 &&
          kind == props[k].kind)
        {
          g_free (element->attribute_values[prop_name_index]);
          element->attribute_values[prop_name_index] = g_strdup (props[k].new_name);
          for (l = 0; l < 3 && props[k].alt_names[l]; l++)
            {
              Element *elt;
              GList *sibling;

              elt = g_new0 (Element, 1);
              elt->parent = element->parent;
              elt->element_name = g_strdup (element->element_name);
              elt->attribute_names = g_strdupv ((char **) element->attribute_names);
              elt->attribute_values = g_strdupv ((char **) element->attribute_values);
              elt->data = g_strdup (element->data);

              g_free (elt->attribute_values[prop_name_index]);
              elt->attribute_values[prop_name_index] = g_strdup (props[k].alt_names[l]);

              sibling = g_list_find (element->parent->children, element);
              element->parent->children = g_list_insert_before (element->parent->children,
                                                                sibling,
                                                                elt);
            }
          break;
        }
    }

  g_free (canonical_name);
}

static Element *
rewrite_stack_child (Element *child, MyParserData *data)
{
  Element *object = NULL;
  Element *packing = NULL;
  Element *new_object;
  Element *prop;
  GList *l;

  if (!g_str_equal (child->element_name, "child"))
    return child;

  for (l = child->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "object"))
        object = elt;
      else if (g_str_equal (elt->element_name, "packing"))
        packing = elt;
    }

  if (!packing)
    return child;

  new_object = g_new0 (Element, 1);
  new_object->element_name = g_strdup ("object");
  new_object->attribute_names = g_new0 (char *, 2);
  new_object->attribute_names[0] = g_strdup ("class");
  new_object->attribute_values = g_new0 (char *, 2);
  new_object->attribute_values[0] = g_strdup ("GtkStackPage");
  new_object->children = packing->children;
  new_object->parent = child;
  packing->children = NULL;

  prop = g_new0 (Element, 1);
  prop->element_name = g_strdup ("property");
  prop->attribute_names = g_new0 (char *, 2);
  prop->attribute_names[0] = g_strdup ("name");
  prop->attribute_values = g_new0 (char *, 2);
  prop->attribute_values[0] = g_strdup ("child");
  prop->children = g_list_append (prop->children, object);
  prop->parent = new_object;
  new_object->children = g_list_append (new_object->children, prop);

  g_list_free (child->children);
  child->children = g_list_append (NULL, new_object);

  return child;
}

static void
rewrite_stack (Element      *element,
               MyParserData *data)
{
  GList *l, *new_children;

  new_children = NULL;
  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      new_children = g_list_append (new_children, rewrite_stack_child (child, data));
    }

  g_list_free (element->children);
  element->children = new_children;
}

static Element *
rewrite_assistant_child (Element *child, MyParserData *data)
{
  Element *object = NULL;
  Element *packing = NULL;
  Element *new_object;
  Element *prop;
  GList *l;

  if (!g_str_equal (child->element_name, "child"))
    return child;

  for (l = child->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "object"))
        object = elt;
      else if (g_str_equal (elt->element_name, "packing"))
        packing = elt;
    }

  if (!packing)
    return child;

  new_object = g_new0 (Element, 1);
  new_object->element_name = g_strdup ("object");
  new_object->attribute_names = g_new0 (char *, 2);
  new_object->attribute_names[0] = g_strdup ("class");
  new_object->attribute_values = g_new0 (char *, 2);
  new_object->attribute_values[0] = g_strdup ("GtkAssistantPage");
  new_object->children = packing->children;
  new_object->parent = child;
  packing->children = NULL;

  prop = g_new0 (Element, 1);
  prop->element_name = g_strdup ("property");
  prop->attribute_names = g_new0 (char *, 2);
  prop->attribute_names[0] = g_strdup ("name");
  prop->attribute_values = g_new0 (char *, 2);
  prop->attribute_values[0] = g_strdup ("child");
  prop->children = g_list_append (prop->children, object);
  prop->parent = new_object;
  new_object->children = g_list_append (new_object->children, prop);

  g_list_free (child->children);
  child->children = g_list_append (NULL, new_object);

  return child;
}

static void
rewrite_assistant (Element      *element,
                   MyParserData *data)
{
  GList *l, *new_children;

  new_children = NULL;
  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      new_children = g_list_append (new_children, rewrite_assistant_child (child, data));
    }

  g_list_free (element->children);
  element->children = new_children;
}

static Element *
rewrite_notebook_page (Element *child, Element *tab, MyParserData *data)
{
  Element *object = NULL;
  Element *tab_obj = NULL;
  Element *packing = NULL;
  Element *new_object;
  Element *prop;
  GList *l;

  if (!g_str_equal (child->element_name, "child"))
    return child;

  if (has_attribute (child, "type", "tab") ||
      has_attribute (child, "type", "action-start") ||
      has_attribute (child, "type", "action-end"))
    return child;

  for (l = child->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "object"))
        object = elt;
      else if (g_str_equal (elt->element_name, "packing"))
        packing = elt;
      else if (g_str_equal (elt->element_name, "placeholder"))
        return child;
    }

  if (!packing && !tab)
    return child;

  if (tab)
    {
      for (l = tab->children; l; l = l->next)
        {
          Element *elt = l->data;
          if (g_str_equal (elt->element_name, "object"))
            tab_obj = elt;
        }
    }

  new_object = g_new0 (Element, 1);
  new_object->element_name = g_strdup ("object");
  new_object->attribute_names = g_new0 (char *, 2);
  new_object->attribute_names[0] = g_strdup ("class");
  new_object->attribute_values = g_new0 (char *, 2);
  new_object->attribute_values[0] = g_strdup ("GtkNotebookPage");
  new_object->parent = child;
  if (packing)
    {
      new_object->children = packing->children;
      packing->children = NULL;
    }

  prop = g_new0 (Element, 1);
  prop->element_name = g_strdup ("property");
  prop->attribute_names = g_new0 (char *, 2);
  prop->attribute_names[0] = g_strdup ("name");
  prop->attribute_values = g_new0 (char *, 2);
  prop->attribute_values[0] = g_strdup ("child");
  prop->children = g_list_append (prop->children, object);
  prop->parent = new_object;
  new_object->children = g_list_append (new_object->children, prop);

  if (tab_obj)
    {
      prop = g_new0 (Element, 1);
      prop->element_name = g_strdup ("property");
      prop->attribute_names = g_new0 (char *, 2);
      prop->attribute_names[0] = g_strdup ("name");
      prop->attribute_values = g_new0 (char *, 2);
      prop->attribute_values[0] = g_strdup ("tab");
      prop->children = g_list_append (prop->children, tab_obj);
      prop->parent = new_object;
      new_object->children = g_list_append (new_object->children, prop);
    }

  g_list_free (child->children);
  child->children = g_list_append (NULL, new_object);

  return child;
}

static void
rewrite_notebook (Element      *element,
                  MyParserData *data)
{
  GList *l, *new_children;

  new_children = NULL;
  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      Element *tab = l->next ? l->next->data : NULL;

      if (tab && has_attribute (tab, "type", "tab"))
        {
          new_children = g_list_append (new_children, rewrite_notebook_page (child, tab, data));
          l = l->next; /* skip the tab */
        }
      else
        new_children = g_list_append (new_children, rewrite_notebook_page (child, NULL, data));
    }

  g_list_free (element->children);
  element->children = new_children;
}

static void
rewrite_pack_type_child (Element *element,
                         MyParserData *data)
{
  Element *pack_type = NULL;
  GList *l, *ll;

  if (!g_str_equal (element->element_name, "child"))
    return;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "packing"))
        {
          for (ll = elt->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "property") &&
                  has_attribute (elt2, "name", "pack-type"))
                {
                  pack_type = elt2;
                  elt->children = g_list_remove (elt->children, pack_type);
                  if (elt->children == NULL)
                    {
                      element->children = g_list_remove (element->children, elt);
                      free_element (elt);
                    }
                  break;
                }
            }
        }

      if (pack_type)
        break;
    }

  if (pack_type)
    {
      char **attnames = g_new0 (char *, g_strv_length (element->attribute_names) + 2);
      char **attvalues = g_new0 (char *, g_strv_length (element->attribute_names) + 2);
      int i;

      for (i = 0; element->attribute_names[i]; i++)
        {
          attnames[i] = g_strdup (element->attribute_names[i]);
          attvalues[i] = g_strdup (element->attribute_values[i]);
        }

      attnames[i] = g_strdup ("type");
      attvalues[i] = g_strdup (pack_type->data);

      g_strfreev (element->attribute_names);
      g_strfreev (element->attribute_values);

      element->attribute_names = attnames;
      element->attribute_values = attvalues;

      free_element (pack_type);
    }
}

static void
rewrite_pack_type (Element *element,
                   MyParserData *data)
{
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "child"))
        rewrite_pack_type_child (elt, data);
    }
}

static void
rewrite_paned_child (Element *element,
                     MyParserData *data,
                     Element *child,
                     const char *suffix)
{
  Element *resize = NULL;
  Element *shrink = NULL;
  GList *l, *ll;

  for (l = child->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "packing"))
        {
          for (ll = elt->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "property") &&
                  has_attribute (elt2, "name", "resize"))
                resize = elt2;
              else if (g_str_equal (elt2->element_name, "property") &&
                  has_attribute (elt2, "name", "shrink"))
                shrink = elt2;
            }
          if (resize)
            elt->children = g_list_remove (elt->children, resize);
          if (shrink)
            elt->children = g_list_remove (elt->children, shrink);
          if (elt->children == NULL)
            {
              child->children = g_list_remove (child->children, elt);
              free_element (elt);
            }
        }

      if (resize || shrink)
        break;
    }

  if (resize)
    {
      Element *elt;

      elt = g_new0 (Element, 1);
      elt->parent = element;
      elt->element_name = g_strdup ("property");
      elt->attribute_names = g_new0 (char *, 2);
      elt->attribute_names[0] = g_strdup ("name");
      elt->attribute_values = g_new0 (char *, 2);
      elt->attribute_values[0] = g_strconcat ("resize-", suffix, NULL);
      elt->data = g_strdup (resize->data);

      element->children = g_list_prepend (element->children, elt);

      free_element (resize);
    }

  if (shrink)
    {
      Element *elt;

      elt = g_new0 (Element, 1);
      elt->parent = element;
      elt->element_name = g_strdup ("property");
      elt->attribute_names = g_new0 (char *, 2);
      elt->attribute_names[0] = g_strdup ("name");
      elt->attribute_values = g_new0 (char *, 2);
      elt->attribute_values[0] = g_strconcat ("shrink-", suffix, NULL);
      elt->data = g_strdup (shrink->data);

      element->children = g_list_prepend (element->children, elt);

      free_element (shrink);
    }
}

static void
rewrite_paned (Element *element,
               MyParserData *data)
{
  Element *child1 = NULL;
  Element *child2 = NULL;
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "child"))
        {
          if (child1 == NULL)
            child1 = elt;
          else if (child2 == NULL)
            child2 = elt;
          else
            break;
        }
    }

  if (child1)
    rewrite_paned_child (element, data, child1, "start-child");

  if (child2)
    rewrite_paned_child (element, data, child2, "end-child");
}

static void
replace_child_by_property (Element *element,
                           Element *child,
                           const char *property,
                           MyParserData *data)
{
  Element *obj, *elt;

  obj = child->children->data;
  g_assert (obj && g_str_equal (obj->element_name, "object"));
  child->children = g_list_remove (child->children, obj);

  elt = g_new0 (Element, 1);
  elt->parent = element;
  elt->element_name = g_strdup ("property");
  elt->attribute_names = g_new0 (char *, 2);
  elt->attribute_names[0] = g_strdup ("name");
  elt->attribute_values = g_new0 (char *, 2);
  elt->attribute_values[0] = g_strdup (property);
  elt->children = g_list_prepend (NULL, obj);

  for (GList *l = element->children; l; l = l->next)
    {
      if (l->data == child)
        {
          l->data = elt;
          elt = NULL;
          free_element (child);
          break;
        }
     }

  g_assert (elt == NULL);
}

static void
rewrite_start_end_children (Element      *element,
                            MyParserData *data)
{
  Element *start_child = NULL;
  Element *end_child = NULL;
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (!g_str_equal (child->element_name, "child"))
        continue;

      if (has_attribute (child, "type", "start"))
        start_child = child;
      else if (has_attribute (child, "type", "end"))
        end_child = child;
      else if (start_child == NULL)
        start_child = child;
      else if (end_child == NULL)
        end_child = child;
      else
        g_warning ("%s only accepts two children", get_class_name (element));
    }

  if (start_child)
    replace_child_by_property (element, start_child, "start-child", data);

  if (end_child)
    replace_child_by_property (element, end_child, "end-child", data);
}

static void
rewrite_start_center_end_children (Element      *element,
                                   MyParserData *data)
{
  Element *start_child = NULL;
  Element *center_child = NULL;
  Element *end_child = NULL;
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (has_attribute (child, "type", "start"))
        start_child = child;
      else if (has_attribute (child, "type", "center"))
        center_child = child;
      else if (has_attribute (child, "type", "end"))
        end_child = child;
      else if (start_child == NULL)
        start_child = child;
      else if (center_child == NULL)
        center_child = child;
      else if (end_child == NULL)
        end_child = child;
      else
        g_warning (_("%s only accepts three children"), get_class_name (element));
    }

  if (start_child)
    replace_child_by_property (element, start_child, "start-widget", data);

  if (center_child)
    replace_child_by_property (element, center_child, "center-widget", data);

  if (end_child)
    replace_child_by_property (element, end_child, "end-widget", data);
}

static void
rewrite_dialog (Element *element,
               MyParserData *data)
{
  Element *content_area = NULL;
  Element *vbox = NULL;
  Element *action_area = NULL;
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "child") &&
          g_strcmp0 (get_attribute_value (elt, "internal-child"), "vbox") == 0)
        {
          content_area = elt;
          break;
        }
    }

  if (!content_area || !content_area->children)
    return;

  vbox = content_area->children->data;

  for (l = vbox->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "child") &&
          g_strcmp0 (get_attribute_value (elt, "internal-child"), "action_area") == 0)
        {
          action_area = elt;
          break;
        }
    }

  if (!action_area)
    return;

  set_attribute_value (content_area, "internal-child", "content_area");
  vbox->children = g_list_remove (vbox->children, action_area);
  action_area->parent = element;
  element->children = g_list_append (element->children, action_area);

  for (l = action_area->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "packing"))
        {
          action_area->children = g_list_remove (action_area->children, elt);
          free_element (elt);
          break;
        }
    }

}

static void
rewrite_grid_layout_prop (Element *element,
                          const char *attr_name,
                          const char *old_value,
                          const char *new_value)
{
  if (g_str_equal (element->element_name, "property"))
    {
      char *canonical_name;

      canonical_name = g_strdup (old_value);
      g_strdelimit (canonical_name, "_", '-');

      if (has_attribute (element, attr_name, old_value) ||
          has_attribute (element, attr_name, canonical_name))
        {
          set_attribute_value (element, attr_name, new_value);
        }

      g_free (canonical_name);
    }
}

static void
rewrite_grid_layout (Element *element,
                     MyParserData *data)
{
  struct _Prop {
    const char *attr_name;
    const char *old_value;
    const char *new_value;
  } props[] = {
    { "name", "left_attach", "column", },
    { "name", "top_attach", "row", },
    { "name", "width", "column-span", },
    { "name", "height", "row-span", },
  };
  GList *l, *ll;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "child"))
        {
          Element *object = NULL;
          Element *packing = NULL;

          for (ll = child->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "object"))
                object = elt2;

              if (g_str_equal (elt2->element_name, "packing"))
                packing = elt2;
            }

          if (object && packing)
            {
              int i;

              child->children = g_list_remove (child->children, packing);

              g_free (packing->element_name);
              packing->element_name = g_strdup ("layout");

              packing->parent = object;
              object->children = g_list_append (object->children, packing);

              for (ll = packing->children; ll; ll = ll->next)
                {
                  Element *elt = ll->data;

                  for (i = 0; i < G_N_ELEMENTS (props); i++)
                    rewrite_grid_layout_prop (elt,
                                              props[i].attr_name,
                                              props[i].old_value,
                                              props[i].new_value);
                }
            }
        }
    }
}

static Element *
add_element (Element    *parent,
             const char *element_name)
{
  Element *child;

  child = g_new0 (Element, 1);
  child->parent = parent;
  child->element_name = g_strdup (element_name);
  child->attribute_names = g_new0 (char *, 1);
  child->attribute_values = g_new0 (char *, 1);
  parent->children = g_list_prepend (parent->children, child);

  return child;
}

static Element *
write_box_prop (Element *element,
                Element *parent,
                const char *name,
                const char *value)
{

  if (element)
    g_free (element->data);
  else
    {
      element = add_element (parent, "property");
      set_attribute_value (element, "name", name);
    }
  element->data = g_strdup (value);

  return element;
}

static void
rewrite_box (Element *element,
             MyParserData *data)
{
  GList *l, *ll;
  GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL;

  if (g_str_equal (get_class_name (element), "GtkVBox"))
    write_box_prop (NULL, element, "orientation", "vertical");

  if (!g_str_equal (get_class_name (element), "GtkBox"))
    set_attribute_value (element, "class", "GtkBox");

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "property"))
        {
          if (has_attribute (child, "name", "orientation"))
            {
              GValue value = G_VALUE_INIT;

              if (gtk_builder_value_from_string_type (data->builder,
                                                      GTK_TYPE_ORIENTATION,
                                                      child->data,
                                                      &value,
                                                      NULL))
                orientation = g_value_get_enum (&value);
            }
        }
    }

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      if (g_str_equal (child->element_name, "child"))
        {
          Element *object = NULL;
          Element *packing = NULL;

          for (ll = child->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "object"))
                object = elt2;

              if (g_str_equal (elt2->element_name, "packing"))
                packing = elt2;
            }

          if (object && packing)
            {
              Element *halign = NULL;
              Element *hexpand = NULL;
              Element *valign = NULL;
              Element *vexpand = NULL;

              gboolean expand = FALSE;
              gboolean fill = TRUE;

              for (ll = object->children; ll; ll = ll->next)
                {
                  Element *elt = ll->data;
                  if (g_str_equal (elt->element_name, "property"))
                    {
                      if (has_attribute (elt, "name", "halign"))
                        halign = elt;
                      else if (has_attribute (elt, "name", "hexpand"))
                        hexpand = elt;
                      else if (has_attribute (elt, "name", "valign"))
                        valign = elt;
                      else if (has_attribute (elt, "name", "vexpand"))
                        vexpand = elt;
                    }
                }

              for (ll = packing->children; ll; ll = ll->next)
                {
                  Element *elt = ll->data;

                  if (has_attribute (elt, "name", "expand"))
                    {
                      GValue value = G_VALUE_INIT;

                      if (gtk_builder_value_from_string_type (data->builder,
                                                              G_TYPE_BOOLEAN,
                                                              elt->data,
                                                              &value,
                                                              NULL))
                        expand = g_value_get_boolean (&value);
                    }

                  if (has_attribute (elt, "name", "fill"))
                    {
                      GValue value = G_VALUE_INIT;

                      if (gtk_builder_value_from_string_type (data->builder,
                                                              G_TYPE_BOOLEAN,
                                                              elt->data,
                                                              &value,
                                                              NULL))
                        fill = g_value_get_boolean (&value);
                    }
                }

              if (orientation == GTK_ORIENTATION_HORIZONTAL)
                {
                  if (expand)
                    write_box_prop (hexpand, object, "hexpand", "1");
                  if (!fill)
                    write_box_prop (halign, object, "halign", "center");
                }
              else if (orientation == GTK_ORIENTATION_VERTICAL)
                {
                  if (expand)
                    write_box_prop (vexpand, object, "vexpand", "1");
                  if (!fill)
                    write_box_prop (valign, object, "valign", "center");
                }

              child->children = g_list_remove (child->children, packing);
              free_element (packing);
            }
        }
    }
}

static void
rewrite_bin_child (Element      *element,
                   MyParserData *data)
{
  GList *l, *ll;
  const char *class_name;
  GType type;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      Element *object = NULL;

      if (!g_str_equal (child->element_name, "child") ||
          has_attribute (child, "type", NULL))
        continue;

      for (ll = child->children; ll; ll = ll->next)
        {
          Element *elem = ll->data;

          if (!g_str_equal (elem->element_name, "object"))
            continue;

          class_name = get_attribute_value (elem, "class");
          if (!class_name)
            continue;

          type = g_type_from_name (class_name);
          if (!g_type_is_a (type, GTK_TYPE_WIDGET))
            continue;

          object = elem;
        }

      if (object)
        {
          g_free (child->element_name);
          g_strfreev (child->attribute_names);
          g_strfreev (child->attribute_values);
          child->element_name = g_strdup ("property");
          child->attribute_names = g_new0 (char *, 2);
          child->attribute_names[0] = g_strdup ("name");
          child->attribute_values = g_new0 (char *, 2);
          child->attribute_values[0] = g_strdup ("child");
          break;
        }
    }
}

static gboolean
remove_boolean_prop (Element      *element,
                     MyParserData *data,
                     const char   *prop_name,
                     gboolean     *value)
{
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "property") &&
          has_attribute (child, "name", prop_name))
        {
          *value = strcmp (canonical_boolean_value (data, child->data), "1") == 0;
          element->children = g_list_remove (element->children, child);
          free_element (child);
          return TRUE;
        }
    }

  return FALSE;
}

static void
rewrite_radio_button (Element      *element,
                      MyParserData *data)
{
  gboolean draw_indicator = TRUE;

  if (!remove_boolean_prop (element, data, "draw-indicator", &draw_indicator))
    remove_boolean_prop (element, data, "draw_indicator", &draw_indicator);

  if (draw_indicator)
    set_attribute_value (element, "class", "GtkCheckButton");
  else
    set_attribute_value (element, "class", "GtkToggleButton");

}

static gboolean
has_prop (Element      *element,
          MyParserData *data,
          const char   *prop_name)
{
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "property") &&
          has_attribute (child, "name", prop_name))
        return TRUE;
    }

  return FALSE;
}

static void
rewrite_scale (Element      *element,
               MyParserData *data)
{
  if (!has_prop (element, data, "draw-value") &&
      !has_prop (element, data, "draw_value"))
    {
      Element *child;
      child = add_element (element, "property");
      set_attribute_value (child, "name", "draw-value");
      child->data = g_strdup ("1");
    }
}

static void
rewrite_requires (Element      *element,
                  MyParserData *data)
{
  if (has_attribute (element, "lib", "gtk+"))
    {
      set_attribute_value (element, "lib", "gtk");
      set_attribute_value (element, "version", "4.0");
    }
}

static void
rewrite_overlay (Element      *element,
                 MyParserData *data)
{
  GList *l, *ll;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "child"))
        {
          Element *object = NULL;
          Element *packing = NULL;

          for (ll = child->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "object"))
                object = elt2;

              if (g_str_equal (elt2->element_name, "packing"))
                packing = elt2;
            }

          if (object && packing)
            {
              child->children = g_list_remove (child->children, packing);

              for (ll = packing->children; ll; ll = ll->next)
                {
                  Element *elt2 = ll->data;

                  if (g_str_equal (elt2->element_name, "property") &&
                      (has_attribute (elt2, "name", "pass-through") ||
                       has_attribute (elt2, "name", "pass_through")))
                    {
                      const char *b = canonical_boolean_value (data, elt2->data);
                      if (g_str_equal (b, "1"))
                        {
                          Element *new_prop;

                          new_prop = add_element (object, "property");
                          set_attribute_value (new_prop, "name", "can-target");
                          new_prop->data = g_strdup ("0");
                        }
                      break;
                    }
                }

              free_element (packing);
            }
        }
    }
}

static void
rewrite_toolbar (Element      *element,
                 MyParserData *data)
{
  GList *l, *ll;

  set_attribute_value (element, "class", "GtkBox");

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "property") &&
          (has_attribute (child, "name", "toolbar_style") ||
           has_attribute (child, "name", "toolbar-style")))
        {
          element->children = g_list_remove (element->children, child);
          free_element (child);
          break;
        }
    }

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      Element *object = NULL;
      Element *packing = NULL;

      if (!g_str_equal (child->element_name, "child"))
        continue;

      for (ll = child->children; ll; ll = ll->next)
        {
          Element *elt2 = ll->data;

          if (g_str_equal (elt2->element_name, "object"))
            object = elt2;

          if (g_str_equal (elt2->element_name, "packing"))
            packing = elt2;
        }

      if (object)
        {
          const char *class_name;

          class_name = get_class_name (object);

          if (g_str_equal (class_name, "GtkToolButton"))
            {
              set_attribute_value (object, "class", "GtkButton");
            }
          else if (g_str_equal (class_name, "GtkToggleToolButton") ||
                   g_str_equal (class_name, "GtkRadioToolButton"))
            {
              set_attribute_value (object, "class", "GtkToggleButton");
            }
          else if (g_str_equal (class_name, "GtkSeparatorToolItem"))
            {
              Element *prop;

              set_attribute_value (object, "class", "GtkSeparator");
              prop = add_element (object, "property");
              set_attribute_value (prop, "name", "orientation");
              prop->data = g_strdup ("vertical");
            }
        }

      if (packing)
        child->children = g_list_remove (child->children, packing);
    }

  {
    Element *child;

    child = add_element (element, "property");
    set_attribute_value (child, "name", "css-classes");
    child->data = g_strdup ("toolbar");
  }
}

static void
rewrite_fixed (Element      *element,
               MyParserData *data)
{
  GList *l, *ll;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "child"))
        {
          Element *object = NULL;
          Element *packing = NULL;

          for (ll = child->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "object"))
                object = elt2;

              if (g_str_equal (elt2->element_name, "packing"))
                packing = elt2;
            }

          if (object && packing)
            {
              int x = 0;
              int y = 0;
              Element *layout;
              Element *new_prop;
              GskTransform *transform;

              for (ll = packing->children; ll; ll = ll->next)
                {
                  Element *elt2 = ll->data;
                  GValue value = G_VALUE_INIT;

                  if (has_attribute (elt2, "name", "x"))
                    {
                      if (gtk_builder_value_from_string_type (data->builder, G_TYPE_INT, elt2->data, &value, NULL))
                        x = g_value_get_int (&value);
                    }
                  else if (has_attribute (elt2, "name", "y"))
                    {
                      if (gtk_builder_value_from_string_type (data->builder, G_TYPE_INT, elt2->data, &value, NULL))
                        y = g_value_get_int (&value);
                    }
                }

              child->children = g_list_remove (child->children, packing);
              free_element (packing);

              layout = add_element (object, "layout");
              new_prop = add_element (layout, "property");
              set_attribute_value (new_prop, "name", "transform");

              transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y));
              new_prop->data = gsk_transform_to_string (transform);
              gsk_transform_unref (transform);
            }
        }
    }
}

/* returns TRUE to remove the element from the parent */
static gboolean
simplify_element (Element      *element,
                  MyParserData *data)
{
  GList *l;
  GType type;

  if (!is_pcdata_element (element))
    {
      g_clear_pointer (&element->data, g_free);
    }
  else if (g_str_equal (element->element_name, "property"))
    {
      if (property_is_boolean (element, data))
        {
          const char *b = canonical_boolean_value (data, element->data);
          g_free (element->data);
          element->data = g_strdup (b);
        }
      else if (property_is_enum (element, data, &type))
        {
          char *e = canonical_enum_value (data, type, element->data);
          g_free (element->data);
          element->data = e;
        }

      for (int i = 0; element->attribute_names[i]; i++)
        {
          if (g_str_equal (element->attribute_names[i], "translatable"))
            {
              const char *b = canonical_boolean_value (data, element->attribute_values[i]);
              g_free (element->attribute_values[i]);
              element->attribute_values[i] = g_strdup (b);
              break;
            }
        }
     }

  l = element->children;
  while (l)
    {
      GList *next = l->next;
      Element *child = l->data;
      if (simplify_element (child, data))
        {
          element->children = g_list_remove (element->children, child);
          free_element (child);
        }
      l = next;
    }

  if (is_container_element (element) && element->children == NULL)
    return TRUE;

  if (g_str_equal (element->element_name, "property") &&
      property_can_be_omitted (element, data))
    return TRUE;

  if (g_str_equal (element->element_name, "binding"))
    {
      const char *property_name = get_attribute_value (element, "name");
      const char *class_name = get_class_name (element);
      if (!get_property_pspec (data, class_name, property_name, PROP_KIND_OBJECT))
        warn_missing_property (element, data, class_name, property_name, PROP_KIND_OBJECT);
    }

  return FALSE;
}

static void
simplify_tree (MyParserData *data)
{
  simplify_element (data->root, data);
}

static gboolean
rewrite_element_3to4 (Element      *element,
                      MyParserData *data)
{
  GList *l;

  l = element->children;
  while (l)
    {
      GList *next = l->next;
      Element *child = l->data;
      if (rewrite_element_3to4 (child, data))
        {
          element->children = g_list_remove (element->children, child);
          free_element (child);
        }
      l = next;
    }

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkStack"))
    rewrite_stack (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkAssistant"))
    rewrite_assistant (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkNotebook"))
    rewrite_notebook (element, data);

  if (element_is_object_or_template (element) &&
      (g_str_equal (get_class_name (element), "GtkActionBar") ||
       g_str_equal (get_class_name (element), "GtkHeaderBar")))
    rewrite_pack_type (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkToolbar"))
    rewrite_toolbar (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkPaned"))
    rewrite_paned (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkDialog"))
    rewrite_dialog (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkOverlay"))
    rewrite_overlay (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkGrid"))
    rewrite_grid_layout (element, data);

  if (element_is_object_or_template (element) &&
      (g_str_equal (get_class_name (element), "GtkHBox") ||
       g_str_equal (get_class_name (element), "GtkVBox") ||
       g_str_equal (get_class_name (element), "GtkBox")))
    rewrite_box (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkFixed"))
    rewrite_fixed (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkRadioButton"))
    rewrite_radio_button (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkScale"))
    rewrite_scale (element, data);

  if (g_str_equal (element->element_name, "property"))
    maybe_rename_property (element, data);

  if (g_str_equal (element->element_name, "property") &&
      property_has_been_removed (element, data))
    return TRUE;

  if (g_str_equal (element->element_name, "requires"))
    rewrite_requires (element, data);

  return FALSE;
}

static void
rewrite_tree_3to4 (MyParserData *data)
{
  rewrite_element_3to4 (data->root, data);
}

static gboolean
rewrite_element (Element      *element,
                 MyParserData *data)
{
  GList *l;

  l = element->children;
  while (l)
    {
      GList *next = l->next;
      Element *child = l->data;
      if (rewrite_element (child, data))
        {
          element->children = g_list_remove (element->children, child);
          free_element (child);
        }
      l = next;
    }

  if (element_is_object_or_template (element) &&
      (g_str_equal (get_class_name (element), "GtkAspectFrame") ||
       g_str_equal (get_class_name (element), "GtkComboBox") ||
       g_str_equal (get_class_name (element), "GtkComboBoxText") ||
       g_str_equal (get_class_name (element), "GtkFlowBoxChild") ||
       g_str_equal (get_class_name (element), "GtkFrame") ||
       g_str_equal (get_class_name (element), "GtkListBoxRow") ||
       g_str_equal (get_class_name (element), "GtkOverlay") ||
       g_str_equal (get_class_name (element), "GtkPopover") ||
       g_str_equal (get_class_name (element), "GtkPopoverMenu") ||
       g_str_equal (get_class_name (element), "GtkRevealer") ||
       g_str_equal (get_class_name (element), "GtkScrolledWindow") ||
       g_str_equal (get_class_name (element), "GtkSearchBar") ||
       g_str_equal (get_class_name (element), "GtkViewport") ||
       g_str_equal (get_class_name (element), "GtkWindow")))
    rewrite_bin_child (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkPaned"))
    rewrite_start_end_children (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkCenterBox"))
    rewrite_start_center_end_children (element, data);

  return FALSE;
}

static void
rewrite_tree (MyParserData *data)
{
  rewrite_element (data->root, data);
}

/* For properties which have changed their default
 * value between 3 and 4, we make sure that their
 * old default value is present in the tree before
 * simplifying it.
 *
 * So far, this is just GtkWidget::visible,
 * changing its default from 0 to 1.
 */
static void
add_old_default_properties (Element      *element,
                            MyParserData *data)
{
  const char *class_name;
  GType type;

  if (!g_str_equal (element->element_name, "object"))
    return;

  class_name = get_class_name (element);
  type = g_type_from_name (class_name);
  if (g_type_is_a (type, GTK_TYPE_WIDGET))
    {
      GList *l;
      gboolean has_visible = FALSE;

      for (l = element->children; l; l = l->next)
        {
          Element *prop = l->data;
          const char *name = get_attribute_value (prop, "name");

          if (g_str_equal (prop->element_name, "property") &&
              g_str_equal (name, "visible"))
            has_visible = TRUE;
        }

      if (!has_visible)
        {
          Element *new_prop;

          new_prop = add_element (element, "property");
          set_attribute_value (new_prop, "name", "visible");
          new_prop->data = g_strdup ("0");
        }
    }
}

static void
enhance_element (Element      *element,
                 MyParserData *data)
{
  GList *l;

  if (strcmp (element->element_name, "requires") == 0 &&
      has_attribute (element, "lib", "gtk+"))
    {
      data->has_gtk_requires = TRUE;
    }

  add_old_default_properties (element, data);

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      enhance_element (child, data);
    }

  if (element == data->root && !data->has_gtk_requires)
    {
      Element *requires = add_element (element, "requires");
      set_attribute_value (requires, "lib", "gtk+");
      set_attribute_value (requires, "version", "3.0");
    }
}

static void
enhance_tree (MyParserData *data)
{
  enhance_element (data->root, data);
}

static void
dump_element (Element *element,
              FILE    *output,
              int      indent)
{
  g_fprintf (output, "%*s<%s", indent, "", element->element_name);
  if (element->attribute_names[0])
    {
      int i;
      for (i = 0; element->attribute_names[i]; i++)
        {
          char *escaped = g_markup_escape_text (element->attribute_values[i], -1);
          g_fprintf (output, " %s=\"%s\"", element->attribute_names[i], escaped);
          g_free (escaped);
        }
    }
  if (element->children || element->data)
    {
      g_fprintf (output, ">");

      if (element->children)
        {
          GList *l;

          g_fprintf (output, "\n");
          for (l = element->children; l; l = l->next)
            {
              Element *child = l->data;
              dump_element (child, output, indent + 2);
            }
          g_fprintf (output, "%*s", indent, "");
        }
      else
        {
          if (is_cdata_property (element))
            {
              g_fprintf (output, "<![CDATA[");
              g_fprintf (output, "%s", element->data);
              g_fprintf (output, "]]>");
            }
          else
            {
              char *escaped = g_markup_escape_text (element->data, -1);
              g_fprintf (output, "%s", escaped);
              g_free (escaped);
            }
        }
      g_fprintf (output, "</%s>\n", element->element_name);
    }
  else
    g_fprintf (output, "/>\n");
}

static void
write_xml_declaration (FILE *output)
{
  g_fprintf (output, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
}

static void
dump_tree (MyParserData *data)
{
  write_xml_declaration (data->output);
  dump_element (data->root, data->output, 0);
}

static gboolean
simplify_file (const char *filename,
               gboolean    replace,
               gboolean    convert3to4)
{
  GMarkupParseContext *context;
  char *buffer;
  MyParserData data;
  GError *error = NULL;

  data.input_filename = filename;
  data.output_filename = NULL;
  data.convert3to4 = convert3to4;
  data.has_gtk_requires = FALSE;

  if (replace)
    {
      int fd;
      fd = g_file_open_tmp ("gtk-builder-tool-XXXXXX", &data.output_filename, NULL);
      data.output = fdopen (fd, "w");
    }
  else
    {
      data.output = stdout;
    }

  if (!g_file_get_contents (filename, &buffer, NULL, &error))
    {
      g_printerr (_("Can’t load “%s”: %s\n"), filename, error->message);
      return FALSE;
    }

  data.root = NULL;
  data.current = NULL;
  data.value = g_string_new ("");

  context = g_markup_parse_context_new (&parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &data, NULL);
  if (!g_markup_parse_context_parse (context, buffer, -1, &error))
    {
      g_printerr (_("Can’t parse “%s”: %s\n"), filename, error->message);
      return FALSE;
    }

  if (!g_markup_parse_context_end_parse (context, &error))
    {
      g_printerr (_("Can’t parse “%s”: %s\n"), filename, error->message);
      return FALSE;
    }

  if (data.root == NULL)
    {
      g_printerr (_("Can’t parse “%s”: %s\n"), filename, "");
      return FALSE;
    }

  data.builder = gtk_builder_new ();

  if (data.convert3to4)
    {
      enhance_tree (&data);
      rewrite_tree_3to4 (&data);
    }

  rewrite_tree (&data);
  simplify_tree (&data);

  dump_tree (&data);

  fclose (data.output);

  if (data.output_filename)
    {
      char *content;
      gsize length;

      if (!g_file_get_contents (data.output_filename, &content, &length, &error))
        {
          g_printerr (_("Failed to read “%s”: %s\n"), data.output_filename, error->message);
          return FALSE;
        }

      if (!g_file_set_contents (data.input_filename, content, length, &error))
        {
          g_printerr (_("Failed to write “%s”: “%s”\n"), data.input_filename, error->message);
          return FALSE;
        }
    }

  return TRUE;
}

void
do_simplify (int          *argc,
             const char ***argv)
{
  gboolean replace = FALSE;
  gboolean convert3to4 = FALSE;
  char **filenames = NULL;
  GOptionContext *context;
  const GOptionEntry entries[] = {
    { "replace", 0, 0, G_OPTION_ARG_NONE, &replace, N_("Replace the file"), NULL },
    { "3to4", 0, 0, G_OPTION_ARG_NONE, &convert3to4, N_("Convert from GTK 3 to GTK 4"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;
  int i;

  g_set_prgname ("gtk4-builder-tool simplify");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Simplify the file."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No .ui file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) > 1 && !replace)
    {
      g_printerr (_("Can only simplify a single .ui file without --replace\n"));
      exit (1);
    }

  for (i = 0; filenames[i]; i++)
    {
      if (!simplify_file (filenames[i], replace, convert3to4))
        exit (1);
    }

  g_strfreev (filenames);
}

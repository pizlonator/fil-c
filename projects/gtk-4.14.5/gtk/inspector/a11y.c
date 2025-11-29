/*
 * Copyright (c) 2020 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>

#include "a11y.h"
#include "window.h"

#include "gtktypebuiltins.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkbinlayout.h"
#include "gtkaccessibleprivate.h"
#include "gtkaccessiblevalueprivate.h"
#include "gtkatcontextprivate.h"
#include "gtkcolumnview.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitem.h"
#include "gtknoselection.h"
#include "gtkfilterlistmodel.h"
#include "gtkboolfilter.h"
#ifdef G_OS_UNIX
#include "a11y/gtkatspicontextprivate.h"
#endif

typedef enum {
  STATE,
  PROPERTY,
  RELATION
} AttributeKind;

typedef struct _AccessibleAttribute AccessibleAttribute;
typedef struct _AccessibleAttributeClass AccessibleAttributeClass;

struct _AccessibleAttribute
{
  GObject parent_instance;
  AttributeKind kind;
  int attribute;
  char *name;
  gboolean is_default;
  GtkAccessibleValue *value;
};

struct _AccessibleAttributeClass
{
  GObjectClass parent_class;
};

enum {
  PROP_KIND = 1,
  PROP_ATTRIBUTE,
  PROP_NAME,
  PROP_IS_DEFAULT,
  PROP_VALUE
};

static GType accessible_attribute_get_type (void);

G_DEFINE_TYPE (AccessibleAttribute, accessible_attribute, G_TYPE_OBJECT);

static void
accessible_attribute_init (AccessibleAttribute *object)
{
}

static void
accessible_attribute_finalize (GObject *object)
{
  AccessibleAttribute *self = (AccessibleAttribute *)object;

  g_free (self->name);
  gtk_accessible_value_unref (self->value);

  G_OBJECT_CLASS (accessible_attribute_parent_class)->finalize (object);
}

static void
accessible_attribute_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  AccessibleAttribute *self = (AccessibleAttribute *)object;

  switch (prop_id)
    {
    case PROP_KIND:
      self->kind = g_value_get_uint (value);
      break;

    case PROP_ATTRIBUTE:
      self->attribute = g_value_get_uint (value);
      break;

    case PROP_NAME:
      g_clear_pointer (&self->name, g_free);
      self->name = g_value_dup_string (value);
      break;

    case PROP_IS_DEFAULT:
      self->is_default = g_value_get_boolean (value);
      break;

    case PROP_VALUE:
      g_clear_pointer (&self->value, gtk_accessible_value_unref);
      self->value = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
accessible_attribute_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  AccessibleAttribute *self = (AccessibleAttribute *)object;

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_uint (value, self->kind);
      break;

    case PROP_ATTRIBUTE:
      g_value_set_uint (value, self->attribute);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_IS_DEFAULT:
      g_value_set_boolean (value, self->is_default);
      break;

    case PROP_VALUE:
      g_value_set_boxed (value, self->value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
accessible_attribute_class_init (AccessibleAttributeClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = accessible_attribute_finalize;
  object_class->set_property = accessible_attribute_set_property;
  object_class->get_property = accessible_attribute_get_property;

  g_object_class_install_property (object_class, PROP_KIND,
      g_param_spec_uint ("kind", NULL, NULL,
                         0, 2, 0,
                         G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_ATTRIBUTE,
      g_param_spec_uint ("attribute", NULL, NULL,
                         0, G_MAXUINT, 0,
                         G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_IS_DEFAULT,
      g_param_spec_boolean ("is-default", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_VALUE,
      g_param_spec_boxed ("value", NULL, NULL,
                          GTK_TYPE_ACCESSIBLE_VALUE,
                          G_PARAM_READWRITE));
}

struct _GtkInspectorA11y
{
  GtkWidget parent;

  GObject *object;

  GtkWidget *box;
  GtkWidget *role;
  GtkWidget *name;
  GtkWidget *description;
  GtkWidget *bounds;
  GtkWidget *path_label;
  GtkWidget *path;
  GtkWidget *attributes;
};

typedef struct _GtkInspectorA11yClass
{
  GtkWidgetClass parent_class;
} GtkInspectorA11yClass;

G_DEFINE_TYPE (GtkInspectorA11y, gtk_inspector_a11y, GTK_TYPE_WIDGET)

static void
update_role (GtkInspectorA11y *sl)
{
  GtkAccessibleRole role;
  GEnumClass *eclass;
  GEnumValue *value;

  role = gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (sl->object));

  eclass = g_type_class_ref (GTK_TYPE_ACCESSIBLE_ROLE);
  value = g_enum_get_value (eclass, role);
  gtk_label_set_label (GTK_LABEL (sl->role), value->value_nick);
  g_type_class_unref (eclass);
}

static void
update_name (GtkInspectorA11y *sl)
{
  GtkATContext *context;
  char *name;

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (sl->object));
  if (context == NULL)
    return;

  name = gtk_at_context_get_name (context);
  gtk_label_set_label (GTK_LABEL (sl->name), name);

  g_object_unref (context);
}

static void
update_description (GtkInspectorA11y *sl)
{
  GtkATContext *context;
  char *description;

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (sl->object));
  if (context == NULL)
    return;

  description = gtk_at_context_get_description (context);
  gtk_label_set_label (GTK_LABEL (sl->description), description);

  g_object_unref (context);
}

static void
update_path (GtkInspectorA11y *sl)
{
#ifdef G_OS_UNIX
  const char *path = NULL;
  GtkATContext *context;

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (sl->object));
  if (GTK_IS_AT_SPI_CONTEXT (context))
    {
      if (gtk_at_context_is_realized (context))
        path = gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (context));
      else
        path = "not realized";
    }

  if (path != NULL)
      gtk_label_set_label (GTK_LABEL (sl->path), path);

  gtk_widget_set_visible (sl->path, path != NULL);
  gtk_widget_set_visible (sl->path_label, path != NULL);

  g_clear_object (&context);
#endif
}

static void
update_bounds (GtkInspectorA11y *sl)
{
  int x, y, w, h;

  if (gtk_accessible_get_bounds (GTK_ACCESSIBLE (sl->object), &x, &y, &w, &h))
    {
      char *size_label = g_strdup_printf ("%d × %d +%d +%d", w, h, x, y);
      gtk_label_set_label (GTK_LABEL (sl->bounds), size_label);
      g_free (size_label);
    }
}

extern GType gtk_string_pair_get_type (void);

static void
update_attributes (GtkInspectorA11y *sl)
{
  GtkATContext *context;
  GListStore *store;
  GtkBoolFilter *filter;
  GtkFilterListModel *filter_model;
  GtkNoSelection *selection;
  GObject *obj;
  GEnumClass *eclass;
  guint i;
  const char *name;
  GtkAccessibleState state;
  GtkAccessibleProperty prop;
  GtkAccessibleRelation rel;
  GtkAccessibleValue *value;
  gboolean has_value;

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (sl->object));
  if (context == NULL)
    return;

  store = g_list_store_new (G_TYPE_OBJECT);

  eclass = g_type_class_ref (GTK_TYPE_ACCESSIBLE_STATE);

  for (i = 0; i < eclass->n_values; i++)
    {
      state = eclass->values[i].value;
      name = eclass->values[i].value_nick;
      has_value = gtk_at_context_has_accessible_state (context, state);
      value = gtk_at_context_get_accessible_state (context, state);
      obj = g_object_new (accessible_attribute_get_type (),
                          "kind", STATE,
                          "attribute", state,
                          "name", name,
                          "is-default", !has_value,
                          "value", value,
                          NULL);
      g_list_store_append (store, obj);
      g_object_unref (obj);
    }

  g_type_class_unref (eclass);

  eclass = g_type_class_ref (GTK_TYPE_ACCESSIBLE_PROPERTY);

  for (i = 0; i < eclass->n_values; i++)
    {
      prop = eclass->values[i].value;
      name = eclass->values[i].value_nick;
      has_value = gtk_at_context_has_accessible_property (context, prop);
      value = gtk_at_context_get_accessible_property (context, prop);
      obj = g_object_new (accessible_attribute_get_type (),
                          "kind", PROPERTY,
                          "attribute", prop,
                          "name", name,
                          "is-default", !has_value,
                          "value", value,
                          NULL);
      g_list_store_append (store, obj);
      g_object_unref (obj);
    }

  g_type_class_unref (eclass);

  eclass = g_type_class_ref (GTK_TYPE_ACCESSIBLE_RELATION);

  for (i = 0; i < eclass->n_values; i++)
    {
      rel = eclass->values[i].value;
      name = eclass->values[i].value_nick;
      has_value = gtk_at_context_has_accessible_relation (context, rel);
      value = gtk_at_context_get_accessible_relation (context, rel);
      obj = g_object_new (accessible_attribute_get_type (),
                          "kind", RELATION,
                          "attribute", rel,
                          "name", name,
                          "is-default", !has_value,
                          "value", value,
                          NULL);
      g_list_store_append (store, obj);
      g_object_unref (obj);
    }

  g_type_class_unref (eclass);

  filter = gtk_bool_filter_new (gtk_property_expression_new (accessible_attribute_get_type (), NULL, "is-default"));
  gtk_bool_filter_set_invert (filter, TRUE);

  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (store), GTK_FILTER (filter));
  selection = gtk_no_selection_new (G_LIST_MODEL (filter_model));
  gtk_column_view_set_model (GTK_COLUMN_VIEW (sl->attributes), GTK_SELECTION_MODEL (selection));
  g_object_unref (selection);

  gtk_widget_set_visible (sl->attributes, g_list_model_get_n_items (G_LIST_MODEL (filter_model)) > 0);

  g_object_unref (context);
}

static void
setup_cell_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 50);
  gtk_widget_set_margin_start (label, 6);
  gtk_widget_set_margin_end (label, 6);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_name_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  AccessibleAttribute *item;
  GtkWidget *label;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  if (item->is_default)
    gtk_widget_add_css_class (label, "dim-label");
  else
    gtk_widget_remove_css_class (label, "dim-label");

  gtk_label_set_label (GTK_LABEL (label), item->name);
}

static void
bind_value_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  AccessibleAttribute *item;
  GtkWidget *label;
  char *string;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  if (item->is_default)
    gtk_widget_add_css_class (label, "dim-label");
  else
    gtk_widget_remove_css_class (label, "dim-label");

  string = gtk_accessible_value_to_string (item->value);
  gtk_label_set_label (GTK_LABEL (label), string);
  g_free (string);
}

static void
refresh_all (GtkInspectorA11y *sl)
{
  update_role (sl);
  update_name (sl);
  update_description (sl);
  update_path (sl);
  update_attributes (sl);
}

void
gtk_inspector_a11y_set_object (GtkInspectorA11y *sl,
                               GObject          *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  GtkATContext *context;

  if (sl->object && GTK_IS_ACCESSIBLE (sl->object))
    {
      context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (sl->object));
      if (context != NULL)
        {
          g_signal_handlers_disconnect_by_func (context, refresh_all, sl);
          g_object_unref (context);
        }
    }

  g_set_object (&sl->object, object);

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));

  if (GTK_IS_ACCESSIBLE (sl->object))
    {
      context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (sl->object));
      if (context != NULL)
        {
          gtk_at_context_realize (context);
          g_signal_connect_swapped (context, "state-change", G_CALLBACK (refresh_all), sl);
          g_object_unref (context);
        }

      gtk_stack_page_set_visible (page, TRUE);
      refresh_all (sl);
      update_bounds (sl);
    }
  else
    {
      gtk_stack_page_set_visible (page, FALSE);
    }
}

static void
gtk_inspector_a11y_init (GtkInspectorA11y *sl)
{
  gtk_widget_init_template (GTK_WIDGET (sl));

#ifndef G_OS_UNIX
  gtk_widget_set_visible (sl->path, FALSE);
  gtk_widget_set_visible (sl->path_label, FALSE);
#endif
}

static void
dispose (GObject *o)
{
  GtkInspectorA11y *sl = GTK_INSPECTOR_A11Y (o);

  if (sl->object && GTK_IS_ACCESSIBLE (sl->object))
    {
      GtkATContext *context;

      context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (sl->object));
      if (context != NULL)
        {
          g_signal_handlers_disconnect_by_func (context, refresh_all, sl);
          g_object_unref (context);
        }
    }

  g_clear_object (&sl->object);

  gtk_widget_dispose_template (GTK_WIDGET (o), GTK_TYPE_INSPECTOR_A11Y);

  G_OBJECT_CLASS (gtk_inspector_a11y_parent_class)->dispose (o);
}

static void
gtk_inspector_a11y_class_init (GtkInspectorA11yClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/a11y.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorA11y, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorA11y, role);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorA11y, name);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorA11y, description);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorA11y, bounds);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorA11y, path_label);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorA11y, path);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorA11y, attributes);

  gtk_widget_class_bind_template_callback (widget_class, setup_cell_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_value_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

// vim: set et sw=2 ts=2:

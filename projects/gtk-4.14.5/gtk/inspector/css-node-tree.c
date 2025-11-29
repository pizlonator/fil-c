/*
 * Copyright (c) 2014 Benjamin Otte <otte@gnome.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicntnse,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright noticnt and this permission noticnt shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"

#include "css-node-tree.h"
#include "prop-editor.h"
#include "window.h"

#include "gtklabel.h"
#include "gtk/gtkwidgetprivate.h"
#include "gtkcssproviderprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtkcssselectorprivate.h"
#include "gtksettings.h"
#include "gtktypebuiltins.h"
#include "gtkstack.h"
#include "gtknoselection.h"
#include "gtksingleselection.h"
#include "gtkcolumnview.h"
#include "gtkcolumnviewcolumn.h"

#include <glib/gi18n-lib.h>
#include <gtk/css/gtkcss.h>

/* {{{ CssProperty object */

typedef struct _CssProperty CssProperty;

G_DECLARE_FINAL_TYPE (CssProperty, css_property, CSS, PROPERTY, GObject);

struct _CssProperty
{
  GObject parent;

  char *name;
  char *value;
  char *location;
};

enum {
  CSS_PROPERTY_PROP_NAME = 1,
  CSS_PROPERTY_PROP_VALUE,
  CSS_PROPERTY_PROP_LOCATION,
  Css_PROPERTY_NUM_PROPERTIES
};

G_DEFINE_TYPE (CssProperty, css_property, G_TYPE_OBJECT);

static void
css_property_init (CssProperty *self)
{
}

static void
css_property_finalize (GObject *object)
{
  CssProperty *self = CSS_PROPERTY (object);

  g_free (self->name);
  g_free (self->value);
  g_free (self->location);

  G_OBJECT_CLASS (css_property_parent_class)->finalize (object);
}

static void
css_property_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  CssProperty *self = CSS_PROPERTY (object);

  switch (property_id)
    {
    case CSS_PROPERTY_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case CSS_PROPERTY_PROP_VALUE:
      g_value_set_string (value, self->value);
      break;

    case CSS_PROPERTY_PROP_LOCATION:
      g_value_set_string (value, self->location);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
css_property_class_init (CssPropertyClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = css_property_finalize;
  object_class->get_property = css_property_get_property;

  pspec = g_param_spec_string ("name", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, CSS_PROPERTY_PROP_NAME, pspec);

  pspec = g_param_spec_string ("value", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, CSS_PROPERTY_PROP_VALUE, pspec);

  pspec = g_param_spec_string ("location", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, CSS_PROPERTY_PROP_LOCATION, pspec);
}

static CssProperty *
css_property_new (const char *name,
                  const char *value,
                  const char *location)
{
  CssProperty *self;

  self = g_object_new (css_property_get_type (), NULL);

  self->name = g_strdup (name);
  self->value = g_strdup (value);
  self->location = g_strdup (location);

  return self;
}

/* }}} */

enum
{
  PROP_0,
  PROP_NODE,

  N_PROPS
};

struct _GtkInspectorCssNodeTreePrivate
{
  GListStore *root_model;
  GtkTreeListModel *node_model;
  GtkSingleSelection *selection_model;
  GtkWidget *node_tree;
  GListStore *prop_model;
  GtkWidget *prop_tree;
  GtkCssNode *node;
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssNodeTree, gtk_inspector_css_node_tree, GTK_TYPE_BOX)

static void
gtk_inspector_css_node_tree_set_node (GtkInspectorCssNodeTree *cnt,
                                      GtkCssNode              *node);
static void
gtk_inspector_css_node_tree_unset_node (GtkInspectorCssNodeTree *cnt);

static void
selection_changed (GtkSelectionModel       *model,
                   GParamSpec              *pspec,
                   GtkInspectorCssNodeTree *cnt)
{
  if (gtk_single_selection_get_selected (cnt->priv->selection_model) != GTK_INVALID_LIST_POSITION)
    {
      GtkTreeListRow *row;
      GtkCssNode *node;

      row = gtk_single_selection_get_selected_item (cnt->priv->selection_model);
      node = gtk_tree_list_row_get_item (row);
      gtk_inspector_css_node_tree_set_node (cnt, node);
    }
  else
    gtk_inspector_css_node_tree_unset_node (cnt);
}

static void
gtk_inspector_css_node_tree_unset_node (GtkInspectorCssNodeTree *cnt)
{
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;

  if (priv->node)
    {
      g_signal_handlers_disconnect_matched (priv->node,
                                            G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL,
                                            cnt);
      g_object_unref (priv->node);
      priv->node = NULL;
    }
}

static void
gtk_inspector_css_node_tree_get_property (GObject    *object,
                                          guint       property_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GtkInspectorCssNodeTree *cnt = GTK_INSPECTOR_CSS_NODE_TREE (object);
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;

  switch (property_id)
    {
    case PROP_NODE:
      g_value_set_object (value, priv->node);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_inspector_css_node_tree_set_property (GObject      *object,
                                          guint         property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_inspector_css_node_tree_finalize (GObject *object)
{
  GtkInspectorCssNodeTree *cnt = GTK_INSPECTOR_CSS_NODE_TREE (object);

  gtk_inspector_css_node_tree_unset_node (cnt);

  G_OBJECT_CLASS (gtk_inspector_css_node_tree_parent_class)->finalize (object);
}

static void
gtk_inspector_css_node_tree_class_init (GtkInspectorCssNodeTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gtk_inspector_css_node_tree_set_property;
  object_class->get_property = gtk_inspector_css_node_tree_get_property;
  object_class->finalize = gtk_inspector_css_node_tree_finalize;

  properties[PROP_NODE] =
    g_param_spec_object ("node", NULL, NULL,
                         GTK_TYPE_CSS_NODE,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/css-node-tree.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, node_tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, prop_tree);
}

static int
sort_strv (gconstpointer a,
           gconstpointer b,
           gpointer      data)
{
  char **ap = (char **) a;
  char **bp = (char **) b;

  return g_ascii_strcasecmp (*ap, *bp);
}

static void
strv_sort (char **strv)
{
  g_qsort_with_data (strv,
		     g_strv_length (strv),
                     sizeof (char *),
                     sort_strv,
                     NULL);
}

static char *
format_state_flags (GtkStateFlags state)
{
  if (state)
    {
      GString *str;
      int i;
      gboolean first = TRUE;

      str = g_string_new ("");

      for (i = 0; i < 31; i++)
        {
          if (state & (1 << i))
            {
              if (!first)
                g_string_append (str, " | ");
              first = FALSE;
              g_string_append (str, gtk_css_pseudoclass_name (1 << i));
            }
        }
      return g_string_free (str, FALSE);
    }

 return g_strdup ("");
}

static void
setup_label (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.);
  gtk_list_item_set_child (list_item, label);
}

static void
setup_value (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 20);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_name (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item)
{
  GtkWidget *label;
  CssProperty *property;

  property = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), property->name);
}

static void
bind_value (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item)
{
  GtkWidget *label;
  CssProperty *property;

  property = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), property->value);
}

static void
bind_location (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *label;
  CssProperty *property;

  property = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), property->location);
}

static GListModel *
create_model_for_node (gpointer object,
                       gpointer user_data)
{
  return gtk_css_node_observe_children (GTK_CSS_NODE (object));
}

static void
setup_node_label (GtkSignalListItemFactory *factory,
                  GtkListItem              *list_item)
{
  GtkWidget *expander;
  GtkWidget *label;

  expander = gtk_tree_expander_new ();
  label = gtk_editable_label_new ("");
  gtk_editable_set_width_chars (GTK_EDITABLE (label), 5);
  gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), label);
  gtk_list_item_set_child (list_item, expander);
}

static void
setup_editable_label (GtkSignalListItemFactory *factory,
                      GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_editable_label_new ("");
  gtk_editable_set_width_chars (GTK_EDITABLE (label), 5);
  gtk_list_item_set_child (list_item, label);
}

static void
name_changed (GtkEditable *editable,
              GParamSpec  *pspec,
              GtkCssNode  *node)
{
  gtk_css_node_set_name (node, g_quark_from_string (gtk_editable_get_text (editable)));
}

static void
bind_node_name (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GtkWidget *expander;
  GtkWidget *label;
  GtkTreeListRow *row;
  GtkCssNode *node;
  const char *text;

  row = gtk_list_item_get_item (list_item);
  node = gtk_tree_list_row_get_item (row);

  expander = gtk_list_item_get_child (list_item);
  gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (expander), row);

  label = gtk_tree_expander_get_child (GTK_TREE_EXPANDER (expander));
  text = g_quark_to_string (gtk_css_node_get_name (node));
  if (!text)
    text = "";
  gtk_editable_set_text (GTK_EDITABLE (label), text);

  g_signal_connect (label, "notify::text", G_CALLBACK (name_changed), node);
}

static void
unbind_node_name (GtkSignalListItemFactory *factory,
                  GtkListItem              *list_item)
{
  GtkWidget *expander;
  GtkWidget *label;
  GtkTreeListRow *row;
  GtkCssNode *node;

  row = gtk_list_item_get_item (list_item);
  node = gtk_tree_list_row_get_item (row);

  expander = gtk_list_item_get_child (list_item);
  label = gtk_tree_expander_get_child (GTK_TREE_EXPANDER (expander));

  g_signal_handlers_disconnect_by_func (label, G_CALLBACK (name_changed), node);
}

static void
id_changed (GtkEditable *editable,
              GParamSpec  *pspec,
              GtkCssNode  *node)
{
  gtk_css_node_set_id (node, g_quark_from_string (gtk_editable_get_text (editable)));
}

static void
bind_node_id (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GtkWidget *label;
  GtkTreeListRow *row;
  GtkCssNode *node;
  const char *text;

  row = gtk_list_item_get_item (list_item);
  node = gtk_tree_list_row_get_item (row);

  label = gtk_list_item_get_child (list_item);
  text = g_quark_to_string (gtk_css_node_get_id (node));
  if (!text)
    text = "";
  gtk_editable_set_text (GTK_EDITABLE (label), text);

  g_signal_connect (label, "notify::text", G_CALLBACK (id_changed), node);
}

static void
unbind_node_id (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GtkWidget *label;
  GtkTreeListRow *row;
  GtkCssNode *node;

  row = gtk_list_item_get_item (list_item);
  node = gtk_tree_list_row_get_item (row);

  label = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (label, G_CALLBACK (id_changed), node);
}

static void
classes_changed (GtkEditable *editable,
                 GParamSpec  *pspec,
                 GtkCssNode  *node)
{
  const char *text;
  char **classes;

  text = gtk_editable_get_text (editable);
  classes = g_strsplit (text, " ", -1);
  gtk_css_node_set_classes (node, (const char **)classes);
  g_strfreev (classes);
}

static void
bind_node_classes (GtkSignalListItemFactory *factory,
                   GtkListItem              *list_item)
{
  GtkWidget *label;
  GtkTreeListRow *row;
  GtkCssNode *node;
  char **classes;
  char *text;

  row = gtk_list_item_get_item (list_item);
  node = gtk_tree_list_row_get_item (row);

  label = gtk_list_item_get_child (list_item);
  classes = gtk_css_node_get_classes (node);
  strv_sort (classes);
  text = g_strjoinv (" ", classes);
  gtk_editable_set_text (GTK_EDITABLE (label), text);
  g_strfreev (classes);
  g_free (text);

  g_signal_connect (label, "notify::text", G_CALLBACK (classes_changed), node);
}

static void
unbind_node_classes (GtkSignalListItemFactory *factory,
                     GtkListItem              *list_item)
{
  GtkWidget *label;
  GtkTreeListRow *row;
  GtkCssNode *node;

  row = gtk_list_item_get_item (list_item);
  node = gtk_tree_list_row_get_item (row);

  label = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (label, G_CALLBACK (classes_changed), node);
}

static void
bind_node_state (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  GtkWidget *label;
  GtkTreeListRow *row;
  GtkCssNode *node;
  char *text;

  row = gtk_list_item_get_item (list_item);
  node = gtk_tree_list_row_get_item (row);

  label = gtk_list_item_get_child (list_item);
  text = format_state_flags (gtk_css_node_get_state (node));
  gtk_label_set_text (GTK_LABEL (label), text);
  g_free (text);
}

static void
gtk_inspector_css_node_tree_init (GtkInspectorCssNodeTree *cnt)
{
  GtkInspectorCssNodeTreePrivate *priv;
  int i;
  GtkListItemFactory *factory;
  GtkColumnViewColumn *column;
  GtkSorter *sorter;
  GtkSortListModel *sort_model;
  GtkSelectionModel *selection_model;

  cnt->priv = gtk_inspector_css_node_tree_get_instance_private (cnt);
  gtk_widget_init_template (GTK_WIDGET (cnt));
  priv = cnt->priv;

  priv->root_model = g_list_store_new (gtk_css_node_get_type ());
  priv->node_model = gtk_tree_list_model_new (G_LIST_MODEL (priv->root_model),
                                              FALSE, FALSE,
                                              create_model_for_node,
                                              NULL, NULL);

  priv->selection_model = gtk_single_selection_new (G_LIST_MODEL (priv->node_model));
  g_signal_connect (priv->selection_model, "notify::selected", G_CALLBACK (selection_changed), cnt);

  gtk_column_view_set_model (GTK_COLUMN_VIEW (priv->node_tree), GTK_SELECTION_MODEL (priv->selection_model));
  g_object_unref (priv->selection_model);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->node_tree)), 0);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_node_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_node_name), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_node_name), NULL);
  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->node_tree)), 1);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_editable_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_node_id), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_node_id), NULL);
  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->node_tree)), 2);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_editable_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_node_classes), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_node_classes), NULL);
  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->node_tree)), 3);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_node_state), NULL);
  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  priv->prop_model = g_list_store_new (css_property_get_type ());

  sort_model = gtk_sort_list_model_new (G_LIST_MODEL (priv->prop_model),
                                        g_object_ref (gtk_column_view_get_sorter (GTK_COLUMN_VIEW (priv->prop_tree))));

  selection_model = GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (sort_model)));
  gtk_column_view_set_model (GTK_COLUMN_VIEW (priv->prop_tree), selection_model);
  g_object_unref (selection_model);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->prop_tree)), 0);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_name), NULL);
  gtk_column_view_column_set_factory (column, factory);
  sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (css_property_get_type (), NULL, "name")));
  gtk_column_view_column_set_sorter (column, sorter);
  gtk_column_view_sort_by_column (GTK_COLUMN_VIEW (priv->prop_tree), column, GTK_SORT_ASCENDING);
  g_object_unref (sorter);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->prop_tree)), 1);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_value), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_value), NULL);
  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->prop_tree)), 2);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_location), NULL);
  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssStyleProperty *prop;
      const char *name;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

      g_list_store_append (priv->prop_model, css_property_new (name, NULL, NULL));
    }
}

void
gtk_inspector_css_node_tree_set_object (GtkInspectorCssNodeTree *cnt,
                                        GObject                 *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  GtkCssNode *root;
  GList *nodes = NULL;
  GList *l;
  int i;

  g_return_if_fail (GTK_INSPECTOR_IS_CSS_NODE_TREE (cnt));

  stack = gtk_widget_get_parent (GTK_WIDGET (cnt));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (cnt));

  if (!GTK_IS_WIDGET (object))
    {
      g_object_set (page, "visible", FALSE, NULL);
      return;
    }

  g_object_set (page, "visible", TRUE, NULL);

  root = gtk_widget_get_css_node (GTK_WIDGET (object));
  nodes = g_list_prepend (nodes, root);
  while (gtk_css_node_get_parent (root))
    {
      root = gtk_css_node_get_parent (root);
      nodes = g_list_prepend (nodes, root);
    }

  g_list_store_remove_all (cnt->priv->root_model);
  g_list_store_append (cnt->priv->root_model, root);

  i = 0;
  for (l = nodes; l; l = l->next)
    {
      GtkCssNode *node = l->data;

      for (; i < g_list_model_get_n_items (G_LIST_MODEL (cnt->priv->node_model)); i++)
        {
          GtkTreeListRow *row = g_list_model_get_item (G_LIST_MODEL (cnt->priv->node_model), i);
          g_object_unref (row);
          if (gtk_tree_list_row_get_item (row) == node)
            {
              gtk_tree_list_row_set_expanded (row, TRUE);
              break;
            }
        }
    }

 gtk_single_selection_set_selected (cnt->priv->selection_model, i);

 g_list_free (nodes);
}

static void
gtk_inspector_css_node_tree_update_style (GtkInspectorCssNodeTree *cnt,
                                          GtkCssStyle             *new_style)
{
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;
  int i;

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssStyleProperty *prop;
      const char *name;
      GtkCssSection *section;
      char *location;
      char *value;
      CssProperty *property;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

      if (new_style)
        {
          value = _gtk_css_value_to_string (gtk_css_style_get_value (new_style, i));

          section = gtk_css_style_get_section (new_style, i);
          if (section)
            location = gtk_css_section_to_string (section);
          else
            location = NULL;
        }
      else
        {
          value = NULL;
          location = NULL;
        }

      property = css_property_new (name, value, location);
      g_list_store_splice (priv->prop_model, i, 1, (gpointer *)&property, 1);

      g_free (location);
      g_free (value);
    }
}

static void
gtk_inspector_css_node_tree_update_style_cb (GtkCssNode              *node,
                                             GtkCssStyleChange       *change,
                                             GtkInspectorCssNodeTree *cnt)
{
  gtk_inspector_css_node_tree_update_style (cnt, gtk_css_style_change_get_new_style (change));
}

static void
gtk_inspector_css_node_tree_set_node (GtkInspectorCssNodeTree *cnt,
                                      GtkCssNode              *node)
{
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;

  if (priv->node == node)
    return;

  if (node)
    g_object_ref (node);

  gtk_inspector_css_node_tree_update_style (cnt, node ? gtk_css_node_get_style (node) : NULL);

  gtk_inspector_css_node_tree_unset_node (cnt);

  priv->node = node;

  if (node)
    g_signal_connect (node, "style-changed", G_CALLBACK (gtk_inspector_css_node_tree_update_style_cb), cnt);

  g_object_notify_by_pspec (G_OBJECT (cnt), properties[PROP_NODE]);
}

GtkCssNode *
gtk_inspector_css_node_tree_get_node (GtkInspectorCssNodeTree *cnt)
{
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;

  return priv->node;
}

void
gtk_inspector_css_node_tree_set_display (GtkInspectorCssNodeTree *cnt,
                                         GdkDisplay *display)
{
  GtkSettings *settings;
  char *theme_name;

  settings = gtk_settings_get_for_display (display);
  g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
  g_object_set (settings, "gtk-theme-name", theme_name, NULL);
  g_free (theme_name);
}

/* vim:set foldmethod=marker expandtab: */

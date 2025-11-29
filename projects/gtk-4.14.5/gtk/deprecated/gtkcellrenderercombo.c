/* GtkCellRendererCombo
 * Copyright (C) 2004 Lorenzo Gil Sanchez
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

#include "gtkentry.h"
#include "deprecated/gtkcelllayout.h"
#include "gtkcellrenderercombo.h"
#include "deprecated/gtkcellrenderertext.h"
#include "deprecated/gtkcombobox.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkCellRendererCombo:
 *
 * Renders a combobox in a cell
 *
 * `GtkCellRendererCombo` renders text in a cell like `GtkCellRendererText` from
 * which it is derived. But while `GtkCellRendererText` offers a simple entry to
 * edit the text, `GtkCellRendererCombo` offers a `GtkComboBox`
 * widget to edit the text. The values to display in the combo box are taken from
 * the tree model specified in the `GtkCellRendererCombo`:model property.
 *
 * The combo cell renderer takes care of adding a text cell renderer to the combo
 * box and sets it to display the column specified by its
 * `GtkCellRendererCombo`:text-column property. Further properties of the combo box
 * can be set in a handler for the `GtkCellRenderer::editing-started` signal.
 *
 * Deprecated: 4.10: List views use widgets to display their contents. You
 *   should use [class@Gtk.DropDown] instead
 */

typedef struct _GtkCellRendererComboPrivate GtkCellRendererComboPrivate;
typedef struct _GtkCellRendererComboClass   GtkCellRendererComboClass;

struct _GtkCellRendererCombo
{
  GtkCellRendererText parent;
};

struct _GtkCellRendererComboClass
{
  GtkCellRendererTextClass parent;
};


struct _GtkCellRendererComboPrivate
{
  GtkTreeModel *model;

  GtkWidget *combo;

  gboolean has_entry;

  int text_column;

  gulong focus_out_id;
};


static void gtk_cell_renderer_combo_finalize     (GObject      *object);
static void gtk_cell_renderer_combo_get_property (GObject      *object,
						  guint         prop_id,
						  GValue       *value,
						  GParamSpec   *pspec);

static void gtk_cell_renderer_combo_set_property (GObject      *object,
						  guint         prop_id,
						  const GValue *value,
						  GParamSpec   *pspec);

static GtkCellEditable *gtk_cell_renderer_combo_start_editing (GtkCellRenderer     *cell,
                                                               GdkEvent            *event,
                                                               GtkWidget           *widget,
                                                               const char          *path,
                                                               const GdkRectangle  *background_area,
                                                               const GdkRectangle  *cell_area,
                                                               GtkCellRendererState flags);

enum {
  PROP_0,
  PROP_MODEL,
  PROP_TEXT_COLUMN,
  PROP_HAS_ENTRY
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint cell_renderer_combo_signals[LAST_SIGNAL] = { 0, };

#define GTK_CELL_RENDERER_COMBO_PATH "gtk-cell-renderer-combo-path"

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererCombo, gtk_cell_renderer_combo, GTK_TYPE_CELL_RENDERER_TEXT)

static void
gtk_cell_renderer_combo_class_init (GtkCellRendererComboClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->finalize = gtk_cell_renderer_combo_finalize;
  object_class->get_property = gtk_cell_renderer_combo_get_property;
  object_class->set_property = gtk_cell_renderer_combo_set_property;

  cell_class->start_editing = gtk_cell_renderer_combo_start_editing;

  /**
   * GtkCellRendererCombo:model:
   *
   * Holds a tree model containing the possible values for the combo box.
   * Use the text_column property to specify the column holding the values.
   */
  g_object_class_install_property (object_class,
				   PROP_MODEL,
				   g_param_spec_object ("model", NULL, NULL,
							GTK_TYPE_TREE_MODEL,
							GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererCombo:text-column:
   *
   * Specifies the model column which holds the possible values for the
   * combo box.
   *
   * Note that this refers to the model specified in the model property,
   * not the model backing the tree view to which
   * this cell renderer is attached.
   *
   * `GtkCellRendererCombo` automatically adds a text cell renderer for
   * this column to its combo box.
   */
  g_object_class_install_property (object_class,
                                   PROP_TEXT_COLUMN,
                                   g_param_spec_int ("text-column", NULL, NULL,
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererCombo:has-entry:
   *
   * If %TRUE, the cell renderer will include an entry and allow to enter
   * values other than the ones in the popup list.
   */
  g_object_class_install_property (object_class,
                                   PROP_HAS_ENTRY,
                                   g_param_spec_boolean ("has-entry", NULL, NULL,
							 TRUE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkCellRendererCombo::changed:
   * @combo: the object on which the signal is emitted
   * @path_string: a string of the path identifying the edited cell
   *               (relative to the tree view model)
   * @new_iter: the new iter selected in the combo box
   *            (relative to the combo box model)
   *
   * This signal is emitted each time after the user selected an item in
   * the combo box, either by using the mouse or the arrow keys.  Contrary
   * to GtkComboBox, GtkCellRendererCombo::changed is not emitted for
   * changes made to a selected item in the entry.  The argument @new_iter
   * corresponds to the newly selected item in the combo box and it is relative
   * to the GtkTreeModel set via the model property on GtkCellRendererCombo.
   *
   * Note that as soon as you change the model displayed in the tree view,
   * the tree view will immediately cease the editing operating.  This
   * means that you most probably want to refrain from changing the model
   * until the combo cell renderer emits the edited or editing_canceled signal.
   */
  cell_renderer_combo_signals[CHANGED] =
    g_signal_new (I_("changed"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  _gtk_marshal_VOID__STRING_BOXED,
		  G_TYPE_NONE, 2,
		  G_TYPE_STRING,
		  GTK_TYPE_TREE_ITER);
  g_signal_set_va_marshaller (cell_renderer_combo_signals[CHANGED],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_VOID__STRING_BOXEDv);
}

static void
gtk_cell_renderer_combo_init (GtkCellRendererCombo *self)
{
  GtkCellRendererComboPrivate *priv = gtk_cell_renderer_combo_get_instance_private (self);

  priv->model = NULL;
  priv->text_column = -1;
  priv->has_entry = TRUE;
  priv->focus_out_id = 0;
}

/**
 * gtk_cell_renderer_combo_new:
 *
 * Creates a new `GtkCellRendererCombo`.
 * Adjust how text is drawn using object properties.
 * Object properties can be set globally (with g_object_set()).
 * Also, with `GtkTreeViewColumn`, you can bind a property to a value
 * in a `GtkTreeModel`. For example, you can bind the “text” property
 * on the cell renderer to a string value in the model, thus rendering
 * a different string in each row of the `GtkTreeView`.
 *
 * Returns: the new cell renderer
 *
 * Deprecated: 4.10
 */
GtkCellRenderer *
gtk_cell_renderer_combo_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_COMBO, NULL);
}

static void
gtk_cell_renderer_combo_finalize (GObject *object)
{
  GtkCellRendererCombo *cell = GTK_CELL_RENDERER_COMBO (object);
  GtkCellRendererComboPrivate *priv = gtk_cell_renderer_combo_get_instance_private (cell);

  if (priv->model)
    {
      g_object_unref (priv->model);
      priv->model = NULL;
    }

  G_OBJECT_CLASS (gtk_cell_renderer_combo_parent_class)->finalize (object);
}

static void
gtk_cell_renderer_combo_get_property (GObject    *object,
				      guint       prop_id,
				      GValue     *value,
				      GParamSpec *pspec)
{
  GtkCellRendererCombo *cell = GTK_CELL_RENDERER_COMBO (object);
  GtkCellRendererComboPrivate *priv = gtk_cell_renderer_combo_get_instance_private (cell);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, priv->model);
      break;
    case PROP_TEXT_COLUMN:
      g_value_set_int (value, priv->text_column);
      break;
    case PROP_HAS_ENTRY:
      g_value_set_boolean (value, priv->has_entry);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_renderer_combo_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GtkCellRendererCombo *cell = GTK_CELL_RENDERER_COMBO (object);
  GtkCellRendererComboPrivate *priv = gtk_cell_renderer_combo_get_instance_private (cell);

  switch (prop_id)
    {
    case PROP_MODEL:
      {
        if (priv->model)
          g_object_unref (priv->model);
        priv->model = GTK_TREE_MODEL (g_value_get_object (value));
        if (priv->model)
          g_object_ref (priv->model);
        break;
      }
    case PROP_TEXT_COLUMN:
      if (priv->text_column != g_value_get_int (value))
        {
          priv->text_column = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_HAS_ENTRY:
      if (priv->has_entry != g_value_get_boolean (value))
        {
          priv->has_entry = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_renderer_combo_changed (GtkComboBox *combo,
				 gpointer     data)
{
  GtkTreeIter iter;
  GtkCellRendererCombo *cell;

  cell = GTK_CELL_RENDERER_COMBO (data);

  if (gtk_combo_box_get_active_iter (combo, &iter))
    {
      const char *path;

      path = g_object_get_data (G_OBJECT (combo), GTK_CELL_RENDERER_COMBO_PATH);
      g_signal_emit (cell, cell_renderer_combo_signals[CHANGED], 0,
		     path, &iter);
    }
}

static void
gtk_cell_renderer_combo_editing_done (GtkCellEditable *combo,
				      gpointer         data)
{
  GtkCellRendererCombo *cell = GTK_CELL_RENDERER_COMBO (data);
  GtkCellRendererComboPrivate *priv = gtk_cell_renderer_combo_get_instance_private (cell);
  const char *path;
  char *new_text = NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkEntry *entry;
  gboolean canceled;

  if (priv->focus_out_id > 0)
    {
      g_signal_handler_disconnect (combo, priv->focus_out_id);
      priv->focus_out_id = 0;
    }

  g_object_get (combo,
                "editing-canceled", &canceled,
                NULL);
  gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (data), canceled);
  if (canceled)
    {
      priv->combo = NULL;
      return;
    }

  if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (combo)))
    {
      entry = GTK_ENTRY (gtk_combo_box_get_child (GTK_COMBO_BOX (combo)));
      new_text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
    }
  else
    {
      model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

      if (model
          && gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
        gtk_tree_model_get (model, &iter, priv->text_column, &new_text, -1);
    }

  path = g_object_get_data (G_OBJECT (combo), GTK_CELL_RENDERER_COMBO_PATH);
  g_signal_emit_by_name (cell, "edited", path, new_text);

  priv->combo = NULL;

  g_free (new_text);
}

static void
gtk_cell_renderer_combo_focus_change (GtkWidget  *widget,
                                      GParamSpec *pspec,
                                      gpointer    data)
{
  if (!gtk_widget_has_focus (widget))
    gtk_cell_renderer_combo_editing_done (GTK_CELL_EDITABLE (widget), data);
}

typedef struct
{
  GtkCellRendererCombo *cell;
  gboolean found;
  GtkTreeIter iter;
} SearchData;

static gboolean
find_text (GtkTreeModel *model,
	   GtkTreePath  *path,
	   GtkTreeIter  *iter,
	   gpointer      data)
{
  SearchData *search_data = (SearchData *)data;
  GtkCellRendererComboPrivate *priv = gtk_cell_renderer_combo_get_instance_private (search_data->cell);
  char *text, *cell_text;

  gtk_tree_model_get (model, iter, priv->text_column, &text, -1);
  g_object_get (GTK_CELL_RENDERER_TEXT (search_data->cell),
                "text", &cell_text,
                NULL);
  if (text && cell_text && g_strcmp0 (text, cell_text) == 0)
    {
      search_data->iter = *iter;
      search_data->found = TRUE;
    }

  g_free (cell_text);
  g_free (text);

  return search_data->found;
}

static GtkCellEditable *
gtk_cell_renderer_combo_start_editing (GtkCellRenderer     *cell,
                                       GdkEvent            *event,
                                       GtkWidget           *widget,
                                       const char          *path,
                                       const GdkRectangle  *background_area,
                                       const GdkRectangle  *cell_area,
                                       GtkCellRendererState flags)
{
  GtkCellRendererCombo *cell_combo = GTK_CELL_RENDERER_COMBO (cell);
  GtkCellRendererComboPrivate *priv = gtk_cell_renderer_combo_get_instance_private (cell_combo);
  GtkWidget *combo;
  SearchData search_data;
  gboolean editable;
  char *text;

  g_object_get (cell, "editable", &editable, NULL);
  if (editable == FALSE)
    return NULL;

  if (priv->text_column < 0)
    return NULL;

  if (priv->has_entry)
    {
      combo = g_object_new (GTK_TYPE_COMBO_BOX, "has-entry", TRUE, NULL);

      if (priv->model)
        gtk_combo_box_set_model (GTK_COMBO_BOX (combo), priv->model);
      gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (combo),
                                           priv->text_column);

      g_object_get (cell, "text", &text, NULL);
      if (text)
	gtk_editable_set_text (GTK_EDITABLE (gtk_combo_box_get_child (GTK_COMBO_BOX (combo))), text);
      g_free (text);
    }
  else
    {
      cell = gtk_cell_renderer_text_new ();

      combo = gtk_combo_box_new ();
      if (priv->model)
        gtk_combo_box_set_model (GTK_COMBO_BOX (combo), priv->model);

      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo),
				      cell, "text", priv->text_column,
				      NULL);

      /* determine the current value */
      if (priv->model)
        {
          search_data.cell = cell_combo;
          search_data.found = FALSE;
          gtk_tree_model_foreach (priv->model, find_text, &search_data);
          if (search_data.found)
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo),
                                           &(search_data.iter));
        }
    }

  g_object_set (combo, "has-frame", FALSE, NULL);
  g_object_set_data_full (G_OBJECT (combo),
			  I_(GTK_CELL_RENDERER_COMBO_PATH),
			  g_strdup (path), g_free);

  gtk_widget_show (combo);

  g_signal_connect (GTK_CELL_EDITABLE (combo), "editing-done",
		    G_CALLBACK (gtk_cell_renderer_combo_editing_done),
		    cell_combo);
  g_signal_connect (GTK_CELL_EDITABLE (combo), "changed",
		    G_CALLBACK (gtk_cell_renderer_combo_changed),
		    cell_combo);
  priv->focus_out_id = g_signal_connect (combo, "notify::has-focus",
                                         G_CALLBACK (gtk_cell_renderer_combo_focus_change),
                                         cell_combo);

  priv->combo = combo;

  return GTK_CELL_EDITABLE (combo);
}

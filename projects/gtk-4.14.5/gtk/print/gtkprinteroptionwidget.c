/* GtkPrinterOptionWidget
 * Copyright (C) 2006 Alexander Larsson  <alexl@redhat.com>
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <glib/gi18n-lib.h>

#include "gtkprinteroptionwidgetprivate.h"

/* This defines the max file length that the file chooser
 * button should display. The total length will be
 * FILENAME_LENGTH_MAX+3 because the truncated name is prefixed
 * with “...”.
 */
#define FILENAME_LENGTH_MAX 27

static void gtk_printer_option_widget_finalize (GObject *object);

static void deconstruct_widgets (GtkPrinterOptionWidget *widget);
static void construct_widgets   (GtkPrinterOptionWidget *widget);
static void update_widgets      (GtkPrinterOptionWidget *widget);

static char *trim_long_filename (const char *filename);

struct GtkPrinterOptionWidgetPrivate
{
  GtkPrinterOption *source;
  gulong source_changed_handler;

  GtkWidget *check;
  GtkWidget *combo;
  GtkWidget *entry;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *info_label;
  GtkWidget *box;
  GtkWidget *button;

  /* the last location for save to file, that the user selected */
  GFile *last_location;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SOURCE
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkPrinterOptionWidget, gtk_printer_option_widget, GTK_TYPE_BOX)

static void gtk_printer_option_widget_set_property (GObject      *object,
						    guint         prop_id,
						    const GValue *value,
						    GParamSpec   *pspec);
static void gtk_printer_option_widget_get_property (GObject      *object,
						    guint         prop_id,
						    GValue       *value,
						    GParamSpec   *pspec);
static gboolean gtk_printer_option_widget_mnemonic_activate (GtkWidget *widget,
							     gboolean   group_cycling);

static void
gtk_printer_option_widget_class_init (GtkPrinterOptionWidgetClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  object_class->finalize = gtk_printer_option_widget_finalize;
  object_class->set_property = gtk_printer_option_widget_set_property;
  object_class->get_property = gtk_printer_option_widget_get_property;

  widget_class->mnemonic_activate = gtk_printer_option_widget_mnemonic_activate;

  signals[CHANGED] =
    g_signal_new ("changed",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrinterOptionWidgetClass, changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_SOURCE,
                                   g_param_spec_object ("source", NULL, NULL,
							GTK_TYPE_PRINTER_OPTION,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

}

static void
gtk_printer_option_widget_init (GtkPrinterOptionWidget *widget)
{
  widget->priv = gtk_printer_option_widget_get_instance_private (widget);

  gtk_box_set_spacing (GTK_BOX (widget), 12);
}

static void
gtk_printer_option_widget_finalize (GObject *object)
{
  GtkPrinterOptionWidget *widget = GTK_PRINTER_OPTION_WIDGET (object);
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  if (priv->source)
    {
      g_signal_handler_disconnect (priv->source,
				   priv->source_changed_handler);
      g_object_unref (priv->source);
      priv->source = NULL;
    }

  G_OBJECT_CLASS (gtk_printer_option_widget_parent_class)->finalize (object);
}

static void
gtk_printer_option_widget_set_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec)
{
  GtkPrinterOptionWidget *widget;

  widget = GTK_PRINTER_OPTION_WIDGET (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      gtk_printer_option_widget_set_source (widget, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_printer_option_widget_get_property (GObject    *object,
					guint       prop_id,
					GValue     *value,
					GParamSpec *pspec)
{
  GtkPrinterOptionWidget *widget = GTK_PRINTER_OPTION_WIDGET (object);
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, priv->source);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_printer_option_widget_mnemonic_activate (GtkWidget *widget,
					     gboolean   group_cycling)
{
  GtkPrinterOptionWidget *powidget = GTK_PRINTER_OPTION_WIDGET (widget);
  GtkPrinterOptionWidgetPrivate *priv = powidget->priv;

  if (priv->check)
    return gtk_widget_mnemonic_activate (priv->check, group_cycling);
  if (priv->combo)
    return gtk_widget_mnemonic_activate (priv->combo, group_cycling);
  if (priv->entry)
    return gtk_widget_mnemonic_activate (priv->entry, group_cycling);
  if (priv->button)
    return gtk_widget_mnemonic_activate (priv->button, group_cycling);

  return FALSE;
}

static void
emit_changed (GtkPrinterOptionWidget *widget)
{
  g_signal_emit (widget, signals[CHANGED], 0);
}

GtkWidget *
gtk_printer_option_widget_new (GtkPrinterOption *source)
{
  return g_object_new (GTK_TYPE_PRINTER_OPTION_WIDGET, "source", source, NULL);
}

static void
source_changed_cb (GtkPrinterOption *source,
		   GtkPrinterOptionWidget  *widget)
{
  update_widgets (widget);
  emit_changed (widget);
}

void
gtk_printer_option_widget_set_source (GtkPrinterOptionWidget *widget,
				      GtkPrinterOption       *source)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  if (source)
    g_object_ref (source);

  if (priv->source)
    {
      g_signal_handler_disconnect (priv->source,
				   priv->source_changed_handler);
      g_object_unref (priv->source);
    }

  priv->source = source;

  if (source)
    priv->source_changed_handler =
      g_signal_connect (source, "changed", G_CALLBACK (source_changed_cb), widget);

  construct_widgets (widget);
  update_widgets (widget);

  g_object_notify (G_OBJECT (widget), "source");
}

#define GTK_TYPE_STRING_PAIR (gtk_string_pair_get_type ())
G_DECLARE_FINAL_TYPE (GtkStringPair, gtk_string_pair, GTK, STRING_PAIR, GObject)

struct _GtkStringPair {
  GObject parent_instance;
  char *id;
  char *string;
};

enum {
  PROP_ID = 1,
  PROP_STRING,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringPair, gtk_string_pair, G_TYPE_OBJECT);

static void
gtk_string_pair_init (GtkStringPair *pair)
{
}

static void
gtk_string_pair_finalize (GObject *object)
{
  GtkStringPair *pair = GTK_STRING_PAIR (object);

  g_free (pair->id);
  g_free (pair->string);

  G_OBJECT_CLASS (gtk_string_pair_parent_class)->finalize (object);
}

static void
gtk_string_pair_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkStringPair *pair = GTK_STRING_PAIR (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_free (pair->string);
      pair->string = g_value_dup_string (value);
      break;

    case PROP_ID:
      g_free (pair->id);
      pair->id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_string_pair_get_property (GObject      *object,
                              guint         property_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  GtkStringPair *pair = GTK_STRING_PAIR (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_value_set_string (value, pair->string);
      break;

    case PROP_ID:
      g_value_set_string (value, pair->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_string_pair_class_init (GtkStringPairClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = gtk_string_pair_finalize;
  object_class->set_property = gtk_string_pair_set_property;
  object_class->get_property = gtk_string_pair_get_property;

  pspec = g_param_spec_string ("string", NULL, NULL,
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_STRING, pspec);

  pspec = g_param_spec_string ("id", NULL, NULL,
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_ID, pspec);
}

static GtkStringPair *
gtk_string_pair_new (const char *id,
                     const char *string)
{
  return g_object_new (GTK_TYPE_STRING_PAIR,
                       "id", id,
                       "string", string,
                       NULL);
}

static const char *
gtk_string_pair_get_string (GtkStringPair *pair)
{
  return pair->string;
}

static const char *
gtk_string_pair_get_id (GtkStringPair *pair)
{
  return pair->id;
}

static void
combo_box_set_model (GtkWidget *combo_box)
{
  GListStore *store;

  store = g_list_store_new (GTK_TYPE_STRING_PAIR);
  gtk_drop_down_set_model (GTK_DROP_DOWN (combo_box), G_LIST_MODEL (store));
  g_object_unref (store);
}

static void
setup_no_item (GtkSignalListItemFactory *factory,
               GtkListItem              *item)
{
}

static void
setup_list_item (GtkSignalListItemFactory *factory,
                 GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_list_item_set_child (item, label);
}

static void
bind_list_item (GtkSignalListItemFactory *factory,
                GtkListItem              *item)
{
  GtkStringPair *pair;
  GtkWidget *label;

  pair = gtk_list_item_get_item (item);
  label = gtk_list_item_get_child (item);

  gtk_label_set_text (GTK_LABEL (label), gtk_string_pair_get_string (pair));
}

static void
combo_box_set_view (GtkWidget *combo_box)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_list_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_list_item), NULL);
  gtk_drop_down_set_factory (GTK_DROP_DOWN (combo_box), factory);
  g_object_unref (factory);
}

static void
selected_changed (GtkDropDown *dropdown,
                  GParamSpec *pspec,
                  gpointer data)
{
  GListModel *model;
  guint selected;
  GtkStringPair *pair;
  GtkWidget *entry = data;

  model = gtk_drop_down_get_model (dropdown);
  selected = gtk_drop_down_get_selected (dropdown);

  pair = g_list_model_get_item (model, selected);
  if (pair)
    {
      gtk_editable_set_text (GTK_EDITABLE (entry), gtk_string_pair_get_string (pair));
      g_object_unref (pair);
    }
  else
    gtk_editable_set_text (GTK_EDITABLE (entry), "");

}

static GtkWidget *
combo_box_entry_new (void)
{
  GtkWidget *hbox, *entry, *button;
  GtkListItemFactory *factory;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (hbox, "linked");

  entry = gtk_entry_new ();
  button = gtk_drop_down_new (NULL, NULL);
  combo_box_set_model (button);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_no_item), NULL);
  gtk_drop_down_set_factory (GTK_DROP_DOWN (button), factory);
  g_object_unref (factory);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_list_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_list_item), NULL);
  gtk_drop_down_set_list_factory (GTK_DROP_DOWN (button), factory);
  g_object_unref (factory);

  g_signal_connect (button, "notify::selected", G_CALLBACK (selected_changed), entry);

  gtk_box_append (GTK_BOX (hbox), entry);
  gtk_box_append (GTK_BOX (hbox), button);

  return hbox;
}

static GtkWidget *
combo_box_new (void)
{
  GtkWidget *combo_box;

  combo_box = gtk_drop_down_new (NULL, NULL);

  combo_box_set_model (combo_box);
  combo_box_set_view (combo_box);

  return combo_box;
}

static void
combo_box_append (GtkWidget   *combo,
                  const char *display_text,
                  const char *value)
{
  GtkWidget *dropdown;
  GListModel *model;
  GtkStringPair *object;

  if (GTK_IS_DROP_DOWN (combo))
    dropdown = combo;
  else
    dropdown = gtk_widget_get_last_child (combo);

  model = gtk_drop_down_get_model (GTK_DROP_DOWN (dropdown));

  object = gtk_string_pair_new (value, display_text);
  g_list_store_append (G_LIST_STORE (model), object);
  g_object_unref (object);
}

static void
combo_box_set (GtkWidget   *combo,
               const char *value)
{
  GtkWidget *dropdown;
  GListModel *model;
  guint i;

  if (GTK_IS_DROP_DOWN (combo))
    dropdown = combo;
  else
    dropdown = gtk_widget_get_last_child (combo);

  model = gtk_drop_down_get_model (GTK_DROP_DOWN (dropdown));

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      GtkStringPair *item = g_list_model_get_item (model, i);
      if (strcmp (value, gtk_string_pair_get_id (item)) == 0)
        {
          gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), i);
          g_object_unref (item);
          break;
        }
      g_object_unref (item);
    }
}

static char *
combo_box_get (GtkWidget *combo, gboolean *custom)
{
  GtkWidget *dropdown;
  GListModel *model;
  guint selected;
  gpointer item;
  const char *id;
  const char *string;

  if (GTK_IS_DROP_DOWN (combo))
    dropdown = combo;
  else
    dropdown = gtk_widget_get_last_child (combo);

  model = gtk_drop_down_get_model (GTK_DROP_DOWN (dropdown));
  selected = gtk_drop_down_get_selected (GTK_DROP_DOWN (dropdown));
  item = g_list_model_get_item (model, selected);
  if (item)
    {
      id = gtk_string_pair_get_id (item);
      string = gtk_string_pair_get_string (item);
      g_object_unref (item);
    }
  else
    {
      id = "";
      string = NULL;
    }

  if (dropdown == combo) // no entry
    {
      *custom = FALSE;
      return g_strdup (id);
    }
  else
    {
      const char *text;

      text = gtk_editable_get_text (GTK_EDITABLE (gtk_widget_get_first_child (combo)));
      if (g_strcmp0 (text, string) == 0)
        {
          *custom = FALSE;
          return g_strdup (id);
        }
      else
        {
          *custom = TRUE;
          return g_strdup (text);
        }
    }
}

static void
deconstruct_widgets (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  g_clear_pointer (&priv->check, gtk_widget_unparent);
  g_clear_pointer (&priv->combo, gtk_widget_unparent);
  g_clear_pointer (&priv->entry, gtk_widget_unparent);
  g_clear_pointer (&priv->image, gtk_widget_unparent);
  g_clear_pointer (&priv->label, gtk_widget_unparent);
  g_clear_pointer (&priv->info_label, gtk_widget_unparent);
}

static void
check_toggled_cb (GtkCheckButton         *check_button,
		  GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  g_signal_handler_block (priv->source, priv->source_changed_handler);
  gtk_printer_option_set_boolean (priv->source,
                                  gtk_check_button_get_active (check_button));
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
}

static void
dialog_response_callback (GObject *source,
                          GAsyncResult *result,
                          gpointer data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GtkPrinterOptionWidget *widget = data;
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  GFile *new_location = NULL;
  char *uri = NULL;

  new_location = gtk_file_dialog_save_finish (dialog, result, NULL);
  if (new_location)
    {
      GFileInfo *info;

      info = g_file_query_info (new_location,
                                "standard::display-name",
                                0,
                                NULL,
                                NULL);
      if (info != NULL)
        {
          const char *filename_utf8 = g_file_info_get_display_name (info);

          char *filename_short = trim_long_filename (filename_utf8);
          gtk_button_set_label (GTK_BUTTON (priv->button), filename_short);

          g_free (filename_short);
          g_object_unref (info);
        }
      else
        {
          const char *path = g_file_peek_path (new_location);
          char *filename_utf8 = g_utf8_make_valid (path, -1);

          char *filename_short = trim_long_filename (filename_utf8);
          gtk_button_set_label (GTK_BUTTON (priv->button), filename_short);

          g_free (filename_short);
          g_free (filename_utf8);
        }
    }

  if (new_location)
    uri = g_file_get_uri (new_location);
  else
    uri = g_file_get_uri (priv->last_location);

  if (uri != NULL)
    {
      gtk_printer_option_set (priv->source, uri);
      emit_changed (widget);
      g_free (uri);
    }

  g_clear_object (&new_location);
  g_clear_object (&priv->last_location);

  /* unblock the handler which was blocked in the filesave_choose_cb function */
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
}

static void
filesave_choose_cb (GtkWidget              *button,
                    GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  GtkFileDialog *dialog;

  /* this will be unblocked in the dialog_response_callback function */
  g_signal_handler_block (priv->source, priv->source_changed_handler);

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Select a filename"));

  /* select the current filename in the dialog */
  if (priv->source != NULL && priv->source->value != NULL)
    {
      priv->last_location = g_file_new_for_uri (priv->source->value);
      if (priv->last_location)
        {
          if (g_file_query_file_type (priv->last_location, 0, NULL) == G_FILE_TYPE_DIRECTORY)
            gtk_file_dialog_set_initial_folder (dialog, priv->last_location);
          else
            gtk_file_dialog_set_initial_file (dialog, priv->last_location);
        }
    }

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (widget))),
                        NULL,
                        dialog_response_callback, widget);
}

static char *
filter_numeric (const char *val,
                gboolean     allow_neg,
		gboolean     allow_dec,
                gboolean    *changed_out)
{
  char *filtered_val;
  int i, j;
  int len = strlen (val);
  gboolean dec_set = FALSE;

  filtered_val = g_malloc (len + 1);

  for (i = 0, j = 0; i < len; i++)
    {
      if (isdigit (val[i]))
        {
          filtered_val[j] = val[i];
	  j++;
	}
      else if (allow_dec && !dec_set &&
               (val[i] == '.' || val[i] == ','))
        {
	  /* allow one period or comma
	   * we should be checking locals
	   * but this is good enough for now
	   */
          filtered_val[j] = val[i];
	  dec_set = TRUE;
	  j++;
	}
      else if (allow_neg && i == 0 && val[0] == '-')
        {
          filtered_val[0] = val[0];
	  j++;
	}
    }

  filtered_val[j] = '\0';
  *changed_out = !(i == j);

  return filtered_val;
}

static void
combo_changed_cb (GtkWidget              *combo,
                  GParamSpec             *pspec,
		  GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  char *value;
  char *filtered_val = NULL;
  gboolean changed;
  gboolean custom = TRUE;

  g_signal_handler_block (priv->source, priv->source_changed_handler);

  value = combo_box_get (priv->combo, &custom);

  /* Handle constraints if the user entered a custom value. */
  if (custom)
    {
      switch (priv->source->type)
        {
        case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE:
          filtered_val = filter_numeric (value, FALSE, FALSE, &changed);
          break;
        case GTK_PRINTER_OPTION_TYPE_PICKONE_INT:
          filtered_val = filter_numeric (value, TRUE, FALSE, &changed);
          break;
        case GTK_PRINTER_OPTION_TYPE_PICKONE_REAL:
          filtered_val = filter_numeric (value, TRUE, TRUE, &changed);
          break;
        case GTK_PRINTER_OPTION_TYPE_BOOLEAN:
        case GTK_PRINTER_OPTION_TYPE_PICKONE:
        case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD:
        case GTK_PRINTER_OPTION_TYPE_PICKONE_STRING:
        case GTK_PRINTER_OPTION_TYPE_ALTERNATIVE:
        case GTK_PRINTER_OPTION_TYPE_STRING:
        case GTK_PRINTER_OPTION_TYPE_FILESAVE:
        case GTK_PRINTER_OPTION_TYPE_INFO:
        default:
          break;
        }
    }

  if (filtered_val)
    {
      g_free (value);

      if (changed)
        {
          GtkWidget *entry = gtk_widget_get_first_child (priv->combo);
          gtk_editable_set_text (GTK_EDITABLE (entry), filtered_val);
        }
      value = filtered_val;
    }

  if (value)
    gtk_printer_option_set (priv->source, value);
  g_free (value);
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
}

static void
entry_changed_cb (GtkWidget              *entry,
		  GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  const char *value;

  g_signal_handler_block (priv->source, priv->source_changed_handler);
  value = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (value)
    gtk_printer_option_set (priv->source, value);
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
}


static void
radio_changed_cb (GtkWidget              *button,
		  GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  char *value;

  g_signal_handler_block (priv->source, priv->source_changed_handler);
  value = g_object_get_data (G_OBJECT (button), "value");
  if (value)
    gtk_printer_option_set (priv->source, value);
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
}

static void
alternative_set (GtkWidget   *box,
                 const char *value)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (box);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      char *v = g_object_get_data (G_OBJECT (child), "value");

      if (strcmp (value, v) == 0)
        {
          gtk_check_button_set_active (GTK_CHECK_BUTTON (child), TRUE);
          break;
        }
    }
}

static void
alternative_append (GtkWidget              *box,
		    const char             *label,
                    const char             *value,
		    GtkPrinterOptionWidget *widget,
		    GtkWidget              **group)
{
  GtkWidget *button;

  button = gtk_check_button_new_with_label (label);
  if (*group)
    gtk_check_button_set_group (GTK_CHECK_BUTTON (button), GTK_CHECK_BUTTON (*group));
  else
    *group = button;

  gtk_widget_set_valign (button, GTK_ALIGN_BASELINE_FILL);
  gtk_box_append (GTK_BOX (box), button);

  g_object_set_data (G_OBJECT (button), "value", (gpointer)value);
  g_signal_connect (button, "toggled", G_CALLBACK (radio_changed_cb), widget);
}

static void
construct_widgets (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  GtkPrinterOption *source;
  char *text;
  int i;
  GtkWidget *group;

  source = priv->source;

  deconstruct_widgets (widget);

  gtk_widget_set_sensitive (GTK_WIDGET (widget), TRUE);

  if (source == NULL)
    {
      const char * strings[2];
      strings[0] = _("Not available");
      strings[1] = NULL;
      priv->combo = gtk_drop_down_new_from_strings (strings);
      gtk_drop_down_set_selected (GTK_DROP_DOWN (priv->combo), 0);
      gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
      gtk_box_append (GTK_BOX (widget), priv->combo);
    }
  else switch (source->type)
    {
    case GTK_PRINTER_OPTION_TYPE_BOOLEAN:
      priv->check = gtk_check_button_new_with_mnemonic (source->display_text);
      g_signal_connect (priv->check, "toggled", G_CALLBACK (check_toggled_cb), widget);
      gtk_box_append (GTK_BOX (widget), priv->check);
      break;
    case GTK_PRINTER_OPTION_TYPE_PICKONE:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_REAL:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_INT:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_STRING:
      if (source->type == GTK_PRINTER_OPTION_TYPE_PICKONE)
        {
          priv->combo = combo_box_new ();
        }
      else
        {
          priv->combo = combo_box_entry_new ();

          if (source->type == GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD ||
              source->type == GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE)
            {
              GtkWidget *entry = gtk_widget_get_first_child (priv->combo);
              gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
            }
        }

      for (i = 0; i < source->num_choices; i++)
        combo_box_append (priv->combo,
                          source->choices_display[i],
                          source->choices[i]);
      gtk_box_append (GTK_BOX (widget), priv->combo);
      if (GTK_IS_DROP_DOWN (priv->combo))
        g_signal_connect (priv->combo, "notify::selected", G_CALLBACK (combo_changed_cb),widget);
      else
        g_signal_connect (gtk_widget_get_last_child (priv->combo), "notify::selected",G_CALLBACK (combo_changed_cb), widget);

      text = g_strdup_printf ("%s:", source->display_text);
      priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);
      break;

    case GTK_PRINTER_OPTION_TYPE_ALTERNATIVE:
      group = NULL;
      priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_widget_set_valign (priv->box, GTK_ALIGN_BASELINE_FILL);
      gtk_box_append (GTK_BOX (widget), priv->box);
      for (i = 0; i < source->num_choices; i++)
        {
          alternative_append (priv->box,
                              source->choices_display[i],
                              source->choices[i],
                              widget,
                              &group);
          /* for mnemonic activation */
          if (i == 0)
            priv->button = group;
        }

      if (source->display_text)
	{
	  text = g_strdup_printf ("%s:", source->display_text);
	  priv->label = gtk_label_new_with_mnemonic (text);
          gtk_widget_set_valign (priv->label, GTK_ALIGN_BASELINE_FILL);
	  g_free (text);
	}
      break;

    case GTK_PRINTER_OPTION_TYPE_STRING:
      priv->entry = gtk_entry_new ();
      gtk_entry_set_activates_default (GTK_ENTRY (priv->entry),
                                       gtk_printer_option_get_activates_default (source));
      gtk_box_append (GTK_BOX (widget), priv->entry);
      g_signal_connect (priv->entry, "changed", G_CALLBACK (entry_changed_cb), widget);

      text = g_strdup_printf ("%s:", source->display_text);
      priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);

      break;

    case GTK_PRINTER_OPTION_TYPE_FILESAVE:
      priv->button = gtk_button_new ();
      gtk_box_append (GTK_BOX (widget), priv->button);
      g_signal_connect (priv->button, "clicked", G_CALLBACK (filesave_choose_cb), widget);

      text = g_strdup_printf ("%s:", source->display_text);
      priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);

      break;

    case GTK_PRINTER_OPTION_TYPE_INFO:
      priv->info_label = gtk_label_new (NULL);
      gtk_label_set_selectable (GTK_LABEL (priv->info_label), TRUE);
      gtk_box_append (GTK_BOX (widget), priv->info_label);

      text = g_strdup_printf ("%s:", source->display_text);
      priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);

      break;

    default:
      break;
    }

  priv->image = gtk_image_new_from_icon_name ("dialog-warning");
  gtk_box_append (GTK_BOX (widget), priv->image);
}

/*
 * If the filename exceeds FILENAME_LENGTH_MAX, then trim it and replace
 * the first three letters with three dots.
 */
static char *
trim_long_filename (const char *filename)
{
  const char *home;
  int len, offset;
  char *result;

  home = g_get_home_dir ();
  if (g_str_has_prefix (filename, home))
    {
      char *homeless_filename;

      offset = g_utf8_strlen (home, -1);
      len = g_utf8_strlen (filename, -1);
      homeless_filename = g_utf8_substring (filename, offset, len);
      result = g_strconcat ("~", homeless_filename, NULL);
      g_free (homeless_filename);
    }
  else
    result = g_strdup (filename);

  len = g_utf8_strlen (result, -1);
  if (len > FILENAME_LENGTH_MAX)
    {
      char *suffix;

      suffix = g_utf8_substring (result, len - FILENAME_LENGTH_MAX, len);
      g_free (result);
      result = g_strconcat ("...", suffix, NULL);
      g_free (suffix);
    }

  return result;
}

static void
update_widgets (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  GtkPrinterOption *source;

  source = priv->source;

  if (source == NULL)
    {
      gtk_widget_set_visible (priv->image, FALSE);
      return;
    }

  switch (source->type)
    {
    case GTK_PRINTER_OPTION_TYPE_BOOLEAN:
      if (g_ascii_strcasecmp (source->value, "True") == 0)
	gtk_check_button_set_active (GTK_CHECK_BUTTON (priv->check), TRUE);
      else
	gtk_check_button_set_active (GTK_CHECK_BUTTON (priv->check), FALSE);
      break;
    case GTK_PRINTER_OPTION_TYPE_PICKONE:
      combo_box_set (priv->combo, source->value);
      break;
    case GTK_PRINTER_OPTION_TYPE_ALTERNATIVE:
      alternative_set (priv->box, source->value);
      break;
    case GTK_PRINTER_OPTION_TYPE_STRING:
      gtk_editable_set_text (GTK_EDITABLE (priv->entry), source->value);
      break;
    case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_REAL:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_INT:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_STRING:
      {
        GtkWidget *entry = gtk_widget_get_first_child (priv->combo);
        if (gtk_printer_option_has_choice (source, source->value))
          combo_box_set (priv->combo, source->value);
        else
          gtk_editable_set_text (GTK_EDITABLE (entry), source->value);

        break;
      }
    case GTK_PRINTER_OPTION_TYPE_FILESAVE:
      {
        char *text;
        char *filename;

        filename = g_filename_from_uri (source->value, NULL, NULL);
        if (filename != NULL)
          {
            text = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
            if (text != NULL)
              {
                char *short_filename;

                short_filename = trim_long_filename (text);
                gtk_button_set_label (GTK_BUTTON (priv->button), short_filename);
                g_free (short_filename);
              }

            g_free (text);
            g_free (filename);
          }
        else
          gtk_button_set_label (GTK_BUTTON (priv->button), source->value);
        break;
      }
    case GTK_PRINTER_OPTION_TYPE_INFO:
      gtk_label_set_text (GTK_LABEL (priv->info_label), source->value);
      break;
    default:
      break;
    }

  gtk_widget_set_visible (priv->image, source->has_conflict);
}

gboolean
gtk_printer_option_widget_has_external_label (GtkPrinterOptionWidget *widget)
{
  return widget->priv->label != NULL;
}

GtkWidget *
gtk_printer_option_widget_get_external_label (GtkPrinterOptionWidget  *widget)
{
  return widget->priv->label;
}

const char *
gtk_printer_option_widget_get_value (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  if (priv->source)
    return priv->source->value;

  return "";
}

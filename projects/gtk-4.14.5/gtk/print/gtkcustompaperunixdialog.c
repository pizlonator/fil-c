/* GtkCustomPaperUnixDialog
 * Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
 * Copyright © 2006, 2007, 2008 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */


#include "config.h"
#include <string.h>
#include <locale.h>

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
#include <langinfo.h>
#endif

#include <glib/gi18n-lib.h>

#include "deprecated/gtkdialogprivate.h"

#include "gtkcustompaperunixdialog.h"
#include "gtkprintbackendprivate.h"
#include "gtkprintutilsprivate.h"

#define LEGACY_CUSTOM_PAPER_FILENAME ".gtk-custom-papers"
#define CUSTOM_PAPER_FILENAME "custom-papers"


typedef struct
{
  GtkUnit    display_unit;
  GtkWidget *spin_button;
} UnitWidget;

struct _GtkCustomPaperUnixDialog
{
  GtkDialog parent_instance;

  GtkWidget *listview;
  GtkWidget *values_box;
  GtkWidget *printer_combo;
  GtkWidget *width_widget;
  GtkWidget *height_widget;
  GtkWidget *top_widget;
  GtkWidget *bottom_widget;
  GtkWidget *left_widget;
  GtkWidget *right_widget;

  gulong printer_inserted_tag;

  guint request_details_tag;
  GtkPrinter *request_details_printer;

  guint non_user_change : 1;

  GListStore *custom_paper_list;
  GListModel *printer_list;

  GList *print_backends;
};

typedef struct _GtkCustomPaperUnixDialogClass GtkCustomPaperUnixDialogClass;

struct _GtkCustomPaperUnixDialogClass
{
  GtkDialogClass parent_class;
};


enum {
  PRINTER_LIST_COL_NAME,
  PRINTER_LIST_COL_PRINTER,
  PRINTER_LIST_N_COLS
};


G_DEFINE_TYPE (GtkCustomPaperUnixDialog, gtk_custom_paper_unix_dialog, GTK_TYPE_DIALOG)


static void gtk_custom_paper_unix_dialog_constructed (GObject *object);
static void gtk_custom_paper_unix_dialog_finalize  (GObject                *object);
static void populate_dialog                        (GtkCustomPaperUnixDialog *dialog);



GtkUnit
_gtk_print_get_default_user_units (void)
{
  /* Translate to the default units to use for presenting
   * lengths to the user. Translate to default:inch if you
   * want inches, otherwise translate to default:mm.
   * Do *not* translate it to "predefinito:mm", if it
   * it isn't default:mm or default:inch it will not work
   */
  char *e = _("default:mm");

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
  char *imperial = NULL;

  imperial = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
  if (imperial && imperial[0] == 2 )
    return GTK_UNIT_INCH;  /* imperial */
  if (imperial && imperial[0] == 1 )
    return GTK_UNIT_MM;  /* metric */
#endif

  if (strcmp (e, "default:inch")==0)
    return GTK_UNIT_INCH;
  else if (strcmp (e, "default:mm"))
    g_warning ("Whoever translated default:mm did so wrongly.");
  return GTK_UNIT_MM;
}

static char *
custom_paper_get_legacy_filename (void)
{
  char *filename;

  filename = g_build_filename (g_get_home_dir (),
                               LEGACY_CUSTOM_PAPER_FILENAME, NULL);
  g_assert (filename != NULL);
  return filename;
}

static char *
custom_paper_get_filename (void)
{
  char *filename;

  filename = g_build_filename (g_get_user_config_dir (),
                               "gtk-4.0",
                               CUSTOM_PAPER_FILENAME, NULL);
  g_assert (filename != NULL);
  return filename;
}

GList *
_gtk_load_custom_papers (void)
{
  GKeyFile *keyfile;
  char *filename;
  char **groups;
  gsize n_groups, i;
  gboolean load_ok;
  GList *result = NULL;

  filename = custom_paper_get_filename ();

  keyfile = g_key_file_new ();
  load_ok = g_key_file_load_from_file (keyfile, filename, 0, NULL);
  g_free (filename);
  if (!load_ok)
    {
      /* try legacy file */
      filename = custom_paper_get_legacy_filename ();
      load_ok = g_key_file_load_from_file (keyfile, filename, 0, NULL);
      g_free (filename);
    }
  if (!load_ok)
    {
      g_key_file_free (keyfile);
      return NULL;
    }

  groups = g_key_file_get_groups (keyfile, &n_groups);
  for (i = 0; i < n_groups; ++i)
    {
      GtkPageSetup *page_setup;

      page_setup = gtk_page_setup_new_from_key_file (keyfile, groups[i], NULL);
      if (!page_setup)
        continue;

      result = g_list_prepend (result, page_setup);
    }

  g_strfreev (groups);
  g_key_file_free (keyfile);

  return g_list_reverse (result);
}

void
gtk_print_load_custom_papers (GListStore *store)
{
  GList *papers, *p;
  GtkPageSetup *page_setup;

  g_list_store_remove_all (store);

  papers = _gtk_load_custom_papers ();
  for (p = papers; p; p = p->next)
    {
      page_setup = p->data;
      g_list_store_append (store, page_setup);
      g_object_unref (page_setup);
    }

  g_list_free (papers);
}

static void
gtk_print_save_custom_papers (GListStore *store)
{
  GKeyFile *keyfile;
  char *filename, *data, *parentdir;
  gsize len;
  int i = 0;

  keyfile = g_key_file_new ();

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++)
    {
      GtkPageSetup *page_setup;
      char group[32];

      page_setup = g_list_model_get_item (G_LIST_MODEL (store), i);
      g_snprintf (group, sizeof (group), "Paper%u", i);
      gtk_page_setup_to_key_file (page_setup, keyfile, group);
      g_object_unref (page_setup);
    }

  filename = custom_paper_get_filename ();
  parentdir = g_build_filename (g_get_user_config_dir (),
                                "gtk-4.0",
                                NULL);
  if (g_mkdir_with_parents (parentdir, 0700) == 0)
    {
      data = g_key_file_to_data (keyfile, &len, NULL);
      g_file_set_contents (filename, data, len, NULL);
      g_free (data);
    }
  g_free (parentdir);
  g_free (filename);
}

static void
gtk_custom_paper_unix_dialog_class_init (GtkCustomPaperUnixDialogClass *class)
{
  G_OBJECT_CLASS (class)->constructed = gtk_custom_paper_unix_dialog_constructed;
  G_OBJECT_CLASS (class)->finalize = gtk_custom_paper_unix_dialog_finalize;
}

static void
custom_paper_dialog_response_cb (GtkDialog *dialog,
                                 int        response,
                                 gpointer   user_data)
{
  GtkCustomPaperUnixDialog *self = GTK_CUSTOM_PAPER_UNIX_DIALOG (dialog);

  gtk_print_save_custom_papers (self->custom_paper_list);
}

static gboolean
match_func (gpointer item, gpointer user_data)
{
  return !gtk_printer_is_virtual (GTK_PRINTER (item));
}

static void
gtk_custom_paper_unix_dialog_init (GtkCustomPaperUnixDialog *dialog)
{
  GtkPrinter *printer;
  GListStore *printer_list;
  GListStore *printer_list_list;
  GListModel *full_list;
  GtkFilter *filter;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (dialog));
G_GNUC_END_IGNORE_DEPRECATIONS

  dialog->print_backends = NULL;

  dialog->request_details_printer = NULL;
  dialog->request_details_tag = 0;

  printer_list_list = g_list_store_new (G_TYPE_LIST_MODEL);
  printer_list = g_list_store_new (GTK_TYPE_PRINTER);
  printer = gtk_printer_new (_("Margins from Printer…"), NULL, FALSE);
  g_list_store_append (printer_list, printer);
  g_object_unref (printer);
  g_list_store_append (printer_list_list, printer_list);
  g_object_unref (printer_list);

  full_list = G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (printer_list_list)));

  filter = GTK_FILTER (gtk_custom_filter_new (match_func, NULL, NULL));
  dialog->printer_list = G_LIST_MODEL (gtk_filter_list_model_new (full_list, filter));

  dialog->custom_paper_list = g_list_store_new (GTK_TYPE_PAGE_SETUP);
  gtk_print_load_custom_papers (dialog->custom_paper_list);

  populate_dialog (dialog);

  g_signal_connect (dialog, "response", G_CALLBACK (custom_paper_dialog_response_cb), NULL);
}

static void
gtk_custom_paper_unix_dialog_constructed (GObject *object)
{
  gboolean use_header;

  G_OBJECT_CLASS (gtk_custom_paper_unix_dialog_parent_class)->constructed (object);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_object_get (object, "use-header-bar", &use_header, NULL);
  if (!use_header)
    {
      gtk_dialog_add_buttons (GTK_DIALOG (object),
                              _("_Close"), GTK_RESPONSE_CLOSE,
                              NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (object), GTK_RESPONSE_CLOSE);
    }
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_custom_paper_unix_dialog_finalize (GObject *object)
{
  GtkCustomPaperUnixDialog *dialog = GTK_CUSTOM_PAPER_UNIX_DIALOG (object);
  GList *node;

  if (dialog->printer_list)
    {
      g_signal_handler_disconnect (dialog->printer_list, dialog->printer_inserted_tag);
      g_object_unref (dialog->printer_list);
      dialog->printer_list = NULL;
    }

  if (dialog->request_details_tag)
    {
      g_signal_handler_disconnect (dialog->request_details_printer,
                                   dialog->request_details_tag);
      g_object_unref (dialog->request_details_printer);
      dialog->request_details_printer = NULL;
      dialog->request_details_tag = 0;
    }

  g_clear_object (&dialog->custom_paper_list);

  for (node = dialog->print_backends; node; node = node->next)
    gtk_print_backend_destroy (GTK_PRINT_BACKEND (node->data));
  g_list_free_full (dialog->print_backends, g_object_unref);
  dialog->print_backends = NULL;

  G_OBJECT_CLASS (gtk_custom_paper_unix_dialog_parent_class)->finalize (object);
}

/**
 * gtk_custom_paper_unix_dialog_new:
 * @title: (nullable): the title of the dialog
 * @parent: (nullable): transient parent of the dialog
 *
 * Creates a new custom paper dialog.
 *
 * Returns: the new `GtkCustomPaperUnixDialog`
 */
GtkWidget *
_gtk_custom_paper_unix_dialog_new (GtkWindow   *parent,
                                   const char *title)
{
  GtkWidget *result;

  if (title == NULL)
    title = _("Manage Custom Sizes");

  result = g_object_new (GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG,
                         "title", title,
                         "transient-for", parent,
                         "modal", parent != NULL,
                         "destroy-with-parent", TRUE,
                         "resizable", FALSE,
                         NULL);

  return result;
}

static void
load_print_backends (GtkCustomPaperUnixDialog *dialog)
{
  GListModel *full_list;
  GListStore *printer_list_list;
  GList *node;

  full_list = gtk_filter_list_model_get_model (GTK_FILTER_LIST_MODEL (dialog->printer_list));
  printer_list_list = G_LIST_STORE (gtk_flatten_list_model_get_model (GTK_FLATTEN_LIST_MODEL (full_list)));

  if (g_module_supported ())
    dialog->print_backends = gtk_print_backend_load_modules ();

  for (node = dialog->print_backends; node != NULL; node = node->next)
    {
      GtkPrintBackend *backend = GTK_PRINT_BACKEND (node->data);
      g_list_store_append (printer_list_list, gtk_print_backend_get_printers (backend));
    }
}

static void unit_widget_changed (GtkCustomPaperUnixDialog *dialog);

static GtkWidget *
new_unit_widget (GtkCustomPaperUnixDialog *dialog,
                 GtkUnit                   unit,
                 GtkWidget                *mnemonic_label)
{
  GtkWidget *hbox, *button, *label;
  UnitWidget *data;

  data = g_new0 (UnitWidget, 1);
  data->display_unit = unit;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  button = gtk_spin_button_new_with_range (0.0, 9999.0, 1);
  gtk_widget_set_valign (button, GTK_ALIGN_BASELINE_FILL);
  if (unit == GTK_UNIT_INCH)
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (button), 2);
  else
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (button), 1);

  gtk_box_append (GTK_BOX (hbox), button);

  data->spin_button = button;

  g_signal_connect_swapped (button, "value-changed",
                            G_CALLBACK (unit_widget_changed), dialog);

  if (unit == GTK_UNIT_INCH)
    label = gtk_label_new (_("inch"));
  else
    label = gtk_label_new (_("mm"));
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);

  gtk_box_append (GTK_BOX (hbox), label);
  gtk_label_set_mnemonic_widget (GTK_LABEL (mnemonic_label), button);

  g_object_set_data_full (G_OBJECT (hbox), "unit-data", data, g_free);

  return hbox;
}

static double
unit_widget_get (GtkWidget *unit_widget)
{
  UnitWidget *data = g_object_get_data (G_OBJECT (unit_widget), "unit-data");
  return _gtk_print_convert_to_mm (gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->spin_button)),
                                   data->display_unit);
}

static void
unit_widget_set (GtkWidget *unit_widget,
                 double     value)
{
  UnitWidget *data;

  data = g_object_get_data (G_OBJECT (unit_widget), "unit-data");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (data->spin_button),
                             _gtk_print_convert_from_mm (value, data->display_unit));
}

static void
update_combo_sensitivity_from_printers (GtkCustomPaperUnixDialog *dialog)
{
  gboolean sensitive = FALSE;

  if (g_list_model_get_n_items (dialog->printer_list) > 1)
    sensitive = TRUE;

  gtk_widget_set_sensitive (dialog->printer_combo, sensitive);
}

static void
update_custom_widgets_from_list (GtkCustomPaperUnixDialog *dialog)
{
  GListModel *model;
  GtkPageSetup *page_setup;

  model = G_LIST_MODEL (gtk_list_view_get_model (GTK_LIST_VIEW (dialog->listview)));
  page_setup = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (model));

  dialog->non_user_change = TRUE;
  if (page_setup != NULL)
    {
      unit_widget_set (dialog->width_widget,
                       gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM));
      unit_widget_set (dialog->height_widget,
                       gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM));
      unit_widget_set (dialog->top_widget,
                       gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_MM));
      unit_widget_set (dialog->bottom_widget,
                       gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_MM));
      unit_widget_set (dialog->left_widget,
                       gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_MM));
      unit_widget_set (dialog->right_widget,
                       gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_MM));

      gtk_widget_set_sensitive (dialog->values_box, TRUE);
    }
  else
    {
      gtk_widget_set_sensitive (dialog->values_box, FALSE);
    }

  if (dialog->printer_list)
    update_combo_sensitivity_from_printers (dialog);
  dialog->non_user_change = FALSE;
}

static void
selected_custom_paper_changed (GObject                  *list,
                               GParamSpec               *pspec,
                               GtkCustomPaperUnixDialog *dialog)
{
  update_custom_widgets_from_list (dialog);
}

static void
unit_widget_changed (GtkCustomPaperUnixDialog *dialog)
{
  double w, h, top, bottom, left, right;
  GListModel *model;
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;

  if (dialog->non_user_change)
    return;

  model = G_LIST_MODEL (gtk_list_view_get_model (GTK_LIST_VIEW (dialog->listview)));
  page_setup = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (model));

  if (page_setup != NULL)
    {
      w = unit_widget_get (dialog->width_widget);
      h = unit_widget_get (dialog->height_widget);

      paper_size = gtk_page_setup_get_paper_size (page_setup);
      gtk_paper_size_set_size (paper_size, w, h, GTK_UNIT_MM);

      top = unit_widget_get (dialog->top_widget);
      bottom = unit_widget_get (dialog->bottom_widget);
      left = unit_widget_get (dialog->left_widget);
      right = unit_widget_get (dialog->right_widget);

      gtk_page_setup_set_top_margin (page_setup, top, GTK_UNIT_MM);
      gtk_page_setup_set_bottom_margin (page_setup, bottom, GTK_UNIT_MM);
      gtk_page_setup_set_left_margin (page_setup, left, GTK_UNIT_MM);
      gtk_page_setup_set_right_margin (page_setup, right, GTK_UNIT_MM);
    }
}

static gboolean
custom_paper_name_used (GtkCustomPaperUnixDialog *dialog,
                        const char               *name)
{
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;
  guint i;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (dialog->custom_paper_list)); i++)
    {
      page_setup = g_list_model_get_item (G_LIST_MODEL (dialog->custom_paper_list), i);
      paper_size = gtk_page_setup_get_paper_size (page_setup);
      if (strcmp (name, gtk_paper_size_get_name (paper_size)) == 0)
        {
          g_object_unref (page_setup);
          return TRUE;
        }
      g_object_unref (page_setup);
    }

  return FALSE;
}

static void
add_custom_paper (GtkCustomPaperUnixDialog *dialog)
{
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;
  char *name;
  int i;

  i = 1;
  name = NULL;
  do
    {
      g_free (name);
      name = g_strdup_printf (_("Custom Size %d"), i);
      i++;
    } while (custom_paper_name_used (dialog, name));

  page_setup = gtk_page_setup_new ();
  paper_size = gtk_paper_size_new_custom (name, name,
                                          gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM),
                                          gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM),
                                          GTK_UNIT_MM);
  gtk_page_setup_set_paper_size (page_setup, paper_size);
  gtk_paper_size_free (paper_size);

  g_list_store_append (dialog->custom_paper_list, page_setup);
  g_object_unref (page_setup);

  /* FIXME start editing */

  g_free (name);
}

static void
remove_custom_paper (GtkCustomPaperUnixDialog *dialog)
{
  GListModel *model;
  guint selected;

  model = G_LIST_MODEL (gtk_list_view_get_model (GTK_LIST_VIEW (dialog->listview)));
  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));
  if (selected != GTK_INVALID_LIST_POSITION)
    g_list_store_remove (dialog->custom_paper_list, selected);
}

static void
set_margins_from_printer (GtkCustomPaperUnixDialog *dialog,
                          GtkPrinter               *printer)
{
  double top, bottom, left, right;

  top = bottom = left = right = 0;
  if (!gtk_printer_get_hard_margins (printer, &top, &bottom, &left, &right))
    return;

  dialog->non_user_change = TRUE;
  unit_widget_set (dialog->top_widget, _gtk_print_convert_to_mm (top, GTK_UNIT_POINTS));
  unit_widget_set (dialog->bottom_widget, _gtk_print_convert_to_mm (bottom, GTK_UNIT_POINTS));
  unit_widget_set (dialog->left_widget, _gtk_print_convert_to_mm (left, GTK_UNIT_POINTS));
  unit_widget_set (dialog->right_widget, _gtk_print_convert_to_mm (right, GTK_UNIT_POINTS));
  dialog->non_user_change = FALSE;

  /* Only send one change */
  unit_widget_changed (dialog);
}

static void
get_margins_finished_callback (GtkPrinter               *printer,
                               gboolean                  success,
                               GtkCustomPaperUnixDialog *dialog)
{
  g_signal_handler_disconnect (dialog->request_details_printer,
                               dialog->request_details_tag);
  g_object_unref (dialog->request_details_printer);
  dialog->request_details_tag = 0;
  dialog->request_details_printer = NULL;

  if (success)
    set_margins_from_printer (dialog, printer);

  gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->printer_combo), 0);
}

static void
margins_from_printer_changed (GtkCustomPaperUnixDialog *dialog)
{
  guint selected;
  GtkPrinter *printer;

  if (dialog->request_details_tag)
    {
      g_signal_handler_disconnect (dialog->request_details_printer,
                                   dialog->request_details_tag);
      g_object_unref (dialog->request_details_printer);
      dialog->request_details_printer = NULL;
      dialog->request_details_tag = 0;
    }

  selected = gtk_drop_down_get_selected (GTK_DROP_DOWN (dialog->printer_combo));
  if (selected != 0)
    {
      GListModel *model;

      model = gtk_drop_down_get_model (GTK_DROP_DOWN (dialog->printer_combo));
      printer = g_list_model_get_item (model, selected);

      if (printer)
        {
          if (gtk_printer_has_details (printer))
            {
              set_margins_from_printer (dialog, printer);
              gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->printer_combo), 0);
            }
          else
            {
              dialog->request_details_printer = g_object_ref (printer);
              dialog->request_details_tag =
                g_signal_connect (printer, "details-acquired",
                                  G_CALLBACK (get_margins_finished_callback), dialog);
              gtk_printer_request_details (printer);
            }

          g_object_unref (printer);
        }
    }
}

static GtkWidget *
wrap_in_frame (const char *label,
               GtkWidget   *child)
{
  GtkWidget *frame, *label_widget;
  char *bold_text;

  label_widget = gtk_label_new (NULL);
  gtk_widget_set_halign (label_widget, GTK_ALIGN_START);
  gtk_widget_set_valign (label_widget, GTK_ALIGN_CENTER);

  bold_text = g_markup_printf_escaped ("<b>%s</b>", label);
  gtk_label_set_markup (GTK_LABEL (label_widget), bold_text);
  g_free (bold_text);

  frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_append (GTK_BOX (frame), label_widget);

  gtk_widget_set_margin_start (child, 12);
  gtk_widget_set_halign (child, GTK_ALIGN_FILL);
  gtk_widget_set_valign (child, GTK_ALIGN_FILL);

  gtk_box_append (GTK_BOX (frame), child);

  return frame;
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *item)
{
  gtk_list_item_set_child (item, gtk_editable_label_new (""));
}

static void
label_changed (GObject     *object,
               GParamSpec  *pspec,
               GtkListItem *item)
{
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;
  const char *new_text;

  page_setup = gtk_list_item_get_item (item);

  new_text = gtk_editable_get_text (GTK_EDITABLE (object));

  paper_size = gtk_paper_size_new_custom (new_text, new_text,
                                          gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM),
                                          gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM), GTK_UNIT_MM);
  gtk_page_setup_set_paper_size (page_setup, paper_size);
  gtk_paper_size_free (paper_size);
}

static void
state_changed (GtkWidget     *item,
               GtkStateFlags  old_state,
               GtkWidget     *label)
{
  gboolean selected;

  selected = (gtk_widget_get_state_flags (item) & GTK_STATE_FLAG_SELECTED) != 0;
  gtk_editable_set_editable (GTK_EDITABLE (label), selected);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *item)
{
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;
  GtkWidget *label;

  page_setup = gtk_list_item_get_item (item);
  label = gtk_list_item_get_child (item);

  paper_size = gtk_page_setup_get_paper_size (page_setup);
  gtk_editable_set_text (GTK_EDITABLE (label),
                         gtk_paper_size_get_display_name (paper_size));
  g_signal_connect (label, "notify::text",
                    G_CALLBACK (label_changed), item);
  g_signal_connect (gtk_widget_get_parent (label), "state-flags-changed",
                    G_CALLBACK (state_changed), label);
}

static void
unbind_item (GtkSignalListItemFactory *factory,
             GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_list_item_get_child (item);
  g_signal_handlers_disconnect_by_func (label, label_changed, item);
  g_signal_handlers_disconnect_by_func (gtk_widget_get_parent (label), state_changed, label);
}

static void
setup_printer_item (GtkSignalListItemFactory *factory,
                    GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_list_item_set_child (item, label);
}

static void
bind_printer_item (GtkSignalListItemFactory *factory,
                   GtkListItem              *item)
{
  GtkPrinter *printer;
  GtkWidget *label;

  printer = gtk_list_item_get_item (item);
  label = gtk_list_item_get_child (item);

  gtk_label_set_label (GTK_LABEL (label), gtk_printer_get_name (printer));
}

static void
populate_dialog (GtkCustomPaperUnixDialog *dialog)
{
  GtkDialog *cpu_dialog = GTK_DIALOG (dialog);
  GtkWidget *content_area;
  GtkWidget *grid, *label, *widget, *frame, *combo;
  GtkWidget *hbox, *vbox, *listview, *scrolled, *toolbar, *button;
  GtkUnit user_units;
  GtkSelectionModel *model;
  GtkListItemFactory *factory;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  content_area = gtk_dialog_get_content_area (cpu_dialog);
G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 18);
  gtk_widget_set_margin_start (hbox, 20);
  gtk_widget_set_margin_end (hbox, 20);
  gtk_widget_set_margin_top (hbox, 20);
  gtk_widget_set_margin_bottom (hbox, 20);
  gtk_box_append (GTK_BOX (content_area), hbox);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append (GTK_BOX (hbox), vbox);

  scrolled = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scrolled, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scrolled), TRUE);
  gtk_box_append (GTK_BOX (vbox), scrolled);

  model = GTK_SELECTION_MODEL (gtk_single_selection_new (g_object_ref (G_LIST_MODEL (dialog->custom_paper_list))));
  g_signal_connect (model, "notify::selected", G_CALLBACK (selected_custom_paper_changed), dialog);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_item), NULL);

  listview = gtk_list_view_new (model, factory);
  gtk_widget_set_size_request (listview, 140, -1);

  dialog->listview = listview;

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), listview);

  toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_widget_add_css_class (toolbar, "linked");

  gtk_box_append (GTK_BOX (vbox), toolbar);

  button = gtk_button_new_from_icon_name ("list-add-symbolic");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (add_custom_paper), dialog);

  gtk_box_append (GTK_BOX (toolbar), button);

  button = gtk_button_new_from_icon_name ("list-remove-symbolic");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (remove_custom_paper), dialog);

  gtk_box_append (GTK_BOX (toolbar), button);

  user_units = _gtk_print_get_default_user_units ();

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
  dialog->values_box = vbox;
  gtk_box_append (GTK_BOX (hbox), vbox);

  grid = gtk_grid_new ();

  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  label = gtk_label_new_with_mnemonic (_("_Width:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  widget = new_unit_widget (dialog, user_units, label);
  dialog->width_widget = widget;
  gtk_grid_attach (GTK_GRID (grid), widget, 1, 0, 1, 1);

  label = gtk_label_new_with_mnemonic (_("_Height:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  widget = new_unit_widget (dialog, user_units, label);
  dialog->height_widget = widget;
  gtk_grid_attach (GTK_GRID (grid), widget, 1, 1, 1, 1);

  frame = wrap_in_frame (_("Paper Size"), grid);
  gtk_box_append (GTK_BOX (vbox), frame);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  label = gtk_label_new_with_mnemonic (_("_Top:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  widget = new_unit_widget (dialog, user_units, label);
  dialog->top_widget = widget;
  gtk_grid_attach (GTK_GRID (grid), widget, 1, 0, 1, 1);

  label = gtk_label_new_with_mnemonic (_("_Bottom:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  widget = new_unit_widget (dialog, user_units, label);
  dialog->bottom_widget = widget;
  gtk_grid_attach (GTK_GRID (grid), widget, 1, 1, 1, 1);

  label = gtk_label_new_with_mnemonic (_("_Left:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

  widget = new_unit_widget (dialog, user_units, label);
  dialog->left_widget = widget;
  gtk_grid_attach (GTK_GRID (grid), widget, 1, 2, 1, 1);

  label = gtk_label_new_with_mnemonic (_("_Right:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);

  widget = new_unit_widget (dialog, user_units, label);
  dialog->right_widget = widget;
  gtk_grid_attach (GTK_GRID (grid), widget, 1, 3, 1, 1);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_grid_attach (GTK_GRID (grid), hbox, 0, 4, 2, 1);

  combo = gtk_drop_down_new (g_object_ref (dialog->printer_list), NULL);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_printer_item), dialog);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_printer_item), dialog);
  gtk_drop_down_set_factory (GTK_DROP_DOWN (combo), factory);
  g_object_unref (factory);

  dialog->printer_combo = combo;

  dialog->printer_inserted_tag =
    g_signal_connect_swapped (dialog->printer_list, "items-changed",
                              G_CALLBACK (update_combo_sensitivity_from_printers), dialog);
  update_combo_sensitivity_from_printers (dialog);

  gtk_drop_down_set_selected (GTK_DROP_DOWN (combo), 0);
  gtk_box_append (GTK_BOX (hbox), combo);

  g_signal_connect_swapped (combo, "notify::selected",
                            G_CALLBACK (margins_from_printer_changed), dialog);

  frame = wrap_in_frame (_("Paper Margins"), grid);
  gtk_box_append (GTK_BOX (vbox), frame);

  update_custom_widgets_from_list (dialog);

  if (g_list_model_get_n_items (G_LIST_MODEL (dialog->custom_paper_list)) == 0)
    add_custom_paper (dialog);

  load_print_backends (dialog);
}

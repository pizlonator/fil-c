/* GtkPageSetupUnixDialog
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */


#include "config.h"
#include <string.h>
#include <locale.h>

#include <glib/gi18n-lib.h>

#include "deprecated/gtkdialogprivate.h"

#include "gtkpagesetupunixdialog.h"
#include "gtkcustompaperunixdialog.h"
#include "gtkprintbackendprivate.h"
#include "gtkpapersize.h"
#include "gtkprintutilsprivate.h"

/**
 * GtkPageSetupUnixDialog:
 *
 * `GtkPageSetupUnixDialog` implements a page setup dialog for platforms
 * which don’t provide a native page setup dialog, like Unix.
 *
 * ![An example GtkPageSetupUnixDialog](pagesetupdialog.png)
 *
 * It can be used very much like any other GTK dialog, at the
 * cost of the portability offered by the high-level printing
 * API in [class@Gtk.PrintOperation].
 *
 * ## CSS nodes
 *
 * `GtkPageSetupUnixDialog` has a single CSS node with the name `window` and
 * style class `.pagesetup`.
 */

typedef struct _GtkPageSetupUnixDialogClass    GtkPageSetupUnixDialogClass;

struct _GtkPageSetupUnixDialog
{
  GtkDialog parent_instance;

  GListModel *printer_list;
  GListStore *page_setup_list;
  GListStore *custom_paper_list;
  GListStore *manage_papers_list;
  GListStore *paper_size_list;

  GList *print_backends;

  GtkWidget *printer_combo;
  GtkWidget *paper_size_combo;
  GtkWidget *paper_size_label;

  GtkWidget *portrait_radio;
  GtkWidget *reverse_portrait_radio;
  GtkWidget *landscape_radio;
  GtkWidget *reverse_landscape_radio;

  gulong request_details_tag;
  GtkPrinter *request_details_printer;

  GtkPrintSettings *print_settings;

  gboolean internal_change;

  /* Save last setup so we can re-set it after selecting manage custom sizes */
  GtkPageSetup *last_setup;
};

struct _GtkPageSetupUnixDialogClass
{
  GtkDialogClass parent_class;
};


/* Keep these in line with GtkListStores defined in gtkpagesetupunixprintdialog.ui */
enum {
  PRINTER_LIST_COL_NAME,
  PRINTER_LIST_COL_PRINTER,
  PRINTER_LIST_N_COLS
};

enum {
  PAGE_SETUP_LIST_COL_PAGE_SETUP,
  PAGE_SETUP_LIST_COL_IS_SEPARATOR,
  PAGE_SETUP_LIST_N_COLS
};

G_DEFINE_TYPE (GtkPageSetupUnixDialog, gtk_page_setup_unix_dialog, GTK_TYPE_DIALOG)

static void gtk_page_setup_unix_dialog_finalize  (GObject                *object);
static void fill_paper_sizes_from_printer        (GtkPageSetupUnixDialog *dialog,
                                                  GtkPrinter             *printer);
static void printer_changed_callback             (GtkDropDown            *combo_box,
                                                  GParamSpec             *pspec,
                                                  GtkPageSetupUnixDialog *dialog);
static void paper_size_changed                   (GtkDropDown            *combo_box,
                                                  GParamSpec             *pspec,
                                                  GtkPageSetupUnixDialog *dialog);
static void load_print_backends                  (GtkPageSetupUnixDialog *dialog);


static const char common_paper_sizes[][16] = {
  "na_letter",
  "na_legal",
  "iso_a4",
  "iso_a5",
  "roc_16k",
  "iso_b5",
  "jis_b5",
  "na_number-10",
  "iso_dl",
  "jpn_chou3",
  "na_ledger",
  "iso_a3"
};


static void
gtk_page_setup_unix_dialog_class_init (GtkPageSetupUnixDialogClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_page_setup_unix_dialog_finalize;

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/print/ui/gtkpagesetupunixdialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkPageSetupUnixDialog, printer_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkPageSetupUnixDialog, paper_size_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkPageSetupUnixDialog, paper_size_label);
  gtk_widget_class_bind_template_child (widget_class, GtkPageSetupUnixDialog, portrait_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPageSetupUnixDialog, reverse_portrait_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPageSetupUnixDialog, landscape_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPageSetupUnixDialog, reverse_landscape_radio);

  gtk_widget_class_bind_template_callback (widget_class, printer_changed_callback);
  gtk_widget_class_bind_template_callback (widget_class, paper_size_changed);
}

static void
setup_paper_size_item (GtkSignalListItemFactory *factory,
                       GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_list_item_set_child (item, label);
}

static void
bind_paper_size_list_item (GtkSignalListItemFactory *factory,
                           GtkListItem              *item,
                           GtkPageSetupUnixDialog   *self)
{
  GtkPageSetup *page_setup;
  GtkWidget *label;
  guint pos;
  GListModel *papers;
  GListModel *model;
  gpointer first;

  page_setup = gtk_list_item_get_item (item);
  label = gtk_list_item_get_child (item);

  pos = gtk_list_item_get_position (item);
  papers = gtk_drop_down_get_model (GTK_DROP_DOWN (self->paper_size_combo));
  model = gtk_flatten_list_model_get_model_for_item (GTK_FLATTEN_LIST_MODEL (papers), pos);
  if (model != G_LIST_MODEL (self->manage_papers_list))
    {
      GtkPaperSize *paper_size = gtk_page_setup_get_paper_size (page_setup);
      gtk_label_set_text (GTK_LABEL (label), gtk_paper_size_get_display_name (paper_size));
    }
  else
    gtk_label_set_text (GTK_LABEL (label), _("Manage Custom Sizes…"));

  first = g_list_model_get_item (model, 0);
  g_object_unref (first);
  if (pos != 0 &&
      page_setup == GTK_PAGE_SETUP (first))
    gtk_widget_add_css_class (gtk_widget_get_parent (label), "separator");
  else
    gtk_widget_remove_css_class (gtk_widget_get_parent (label), "separator");
}

static void
bind_paper_size_item (GtkSignalListItemFactory *factory,
                      GtkListItem              *item,
                      GtkPageSetupUnixDialog   *self)
{
  GtkWidget *label;

  bind_paper_size_list_item (factory, item, self);

  label = gtk_list_item_get_child (item);
  gtk_widget_remove_css_class (label, "separator-before");
}

static gboolean
match_func (gpointer item, gpointer user_data)
{
  return !gtk_printer_is_virtual (GTK_PRINTER (item));
}

static void
setup_printer_item (GtkSignalListItemFactory *factory,
                    GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (item, label);
}

static void
bind_printer_item (GtkSignalListItemFactory *factory,
                   GtkListItem              *item,
                   GtkPageSetupUnixDialog   *self)
{
  GtkPrinter *printer;
  GtkWidget *label;
  const char *location;
  const char *name;
  char *str;

  printer = gtk_list_item_get_item (item);
  label = gtk_list_item_get_child (item);

  name = gtk_printer_get_name (printer);
  location = gtk_printer_get_location (printer);
  str = g_strdup_printf ("<b>%s</b>\n%s", name, location ? location : "");
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
}

static void
gtk_page_setup_unix_dialog_init (GtkPageSetupUnixDialog *dialog)
{
  GtkListItemFactory *factory;
  GListStore *store;
  GListModel *paper_size_list;
  GtkPrinter *printer;
  GListStore *printer_list;
  GListStore *printer_list_list;
  GListModel *full_list;
  GtkFilter *filter;
  GtkPageSetup *page_setup;

  dialog->internal_change = TRUE;
  dialog->print_backends = NULL;

  gtk_widget_init_template (GTK_WIDGET (dialog));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (dialog));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("_Apply"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
G_GNUC_END_IGNORE_DEPRECATIONS

  dialog->page_setup_list = g_list_store_new (GTK_TYPE_PAGE_SETUP);
  dialog->custom_paper_list = g_list_store_new (GTK_TYPE_PAGE_SETUP);
  dialog->manage_papers_list = g_list_store_new (GTK_TYPE_PAGE_SETUP);
  page_setup = gtk_page_setup_new ();
  g_list_store_append (dialog->manage_papers_list, page_setup);
  g_object_unref (page_setup);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_paper_size_item), dialog);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_paper_size_item), dialog);
  gtk_drop_down_set_factory (GTK_DROP_DOWN (dialog->paper_size_combo), factory);
  g_object_unref (factory);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_paper_size_item), dialog);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_paper_size_list_item), dialog);
  gtk_drop_down_set_list_factory (GTK_DROP_DOWN (dialog->paper_size_combo), factory);
  g_object_unref (factory);

  store = g_list_store_new (G_TYPE_LIST_MODEL);
  g_list_store_append (store, dialog->page_setup_list);
  g_list_store_append (store, dialog->custom_paper_list);
  g_list_store_append (store, dialog->manage_papers_list);
  paper_size_list = G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (store)));
  gtk_drop_down_set_model (GTK_DROP_DOWN (dialog->paper_size_combo), paper_size_list);
  g_object_unref (paper_size_list);

  /* Do this in code, we want the translatable strings without the markup */
  printer_list_list = g_list_store_new (G_TYPE_LIST_MODEL);
  printer_list = g_list_store_new (GTK_TYPE_PRINTER);
  printer = gtk_printer_new (_("Any Printer"), NULL, FALSE);
  gtk_printer_set_location (printer, _("For portable documents"));
  g_list_store_append (printer_list, printer);
  g_object_unref (printer);
  g_list_store_append (printer_list_list, printer_list);
  g_object_unref (printer_list);

  full_list = G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (printer_list_list)));

  filter = GTK_FILTER (gtk_custom_filter_new (match_func, NULL, NULL));
  dialog->printer_list = G_LIST_MODEL (gtk_filter_list_model_new (full_list, filter));

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_printer_item), dialog);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_printer_item), dialog);
  gtk_drop_down_set_factory (GTK_DROP_DOWN (dialog->printer_combo), factory);
  g_object_unref (factory);

  gtk_drop_down_set_model (GTK_DROP_DOWN (dialog->printer_combo), dialog->printer_list);
  printer_changed_callback (GTK_DROP_DOWN (dialog->printer_combo), NULL, dialog);

  /* Load data */
  gtk_print_load_custom_papers (dialog->custom_paper_list);
  load_print_backends (dialog);
  dialog->internal_change = FALSE;
}

static void
gtk_page_setup_unix_dialog_finalize (GObject *object)
{
  GtkPageSetupUnixDialog *dialog = GTK_PAGE_SETUP_UNIX_DIALOG (object);
  GList *node;

  if (dialog->request_details_tag)
    {
      g_signal_handler_disconnect (dialog->request_details_printer,
                                   dialog->request_details_tag);
      g_object_unref (dialog->request_details_printer);
      dialog->request_details_printer = NULL;
      dialog->request_details_tag = 0;
    }

  g_clear_object (&dialog->printer_list);
  g_clear_object (&dialog->page_setup_list);
  g_clear_object (&dialog->custom_paper_list);
  g_clear_object (&dialog->manage_papers_list);

  if (dialog->print_settings)
    {
      g_object_unref (dialog->print_settings);
      dialog->print_settings = NULL;
    }

  for (node = dialog->print_backends; node != NULL; node = node->next)
    gtk_print_backend_destroy (GTK_PRINT_BACKEND (node->data));
  g_list_free_full (dialog->print_backends, g_object_unref);
  dialog->print_backends = NULL;

  G_OBJECT_CLASS (gtk_page_setup_unix_dialog_parent_class)->finalize (object);
}

static void
load_print_backends (GtkPageSetupUnixDialog *dialog)
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
      GtkPrintBackend *backend = node->data;
      g_list_store_append (printer_list_list, gtk_print_backend_get_printers (backend));
    }
}

static GtkPageSetup *
get_current_page_setup (GtkPageSetupUnixDialog *dialog)
{
  guint selected;
  GListModel *model;

  selected = gtk_drop_down_get_selected (GTK_DROP_DOWN (dialog->paper_size_combo));
  model = gtk_drop_down_get_model (GTK_DROP_DOWN (dialog->paper_size_combo));
  if (selected != GTK_INVALID_LIST_POSITION)
    return g_list_model_get_item (model, selected);

  return gtk_page_setup_new ();
}

static gboolean
page_setup_is_equal (GtkPageSetup *a,
                     GtkPageSetup *b)
{
  return
    gtk_paper_size_is_equal (gtk_page_setup_get_paper_size (a),
                             gtk_page_setup_get_paper_size (b)) &&
    gtk_page_setup_get_top_margin (a, GTK_UNIT_MM) == gtk_page_setup_get_top_margin (b, GTK_UNIT_MM) &&
    gtk_page_setup_get_bottom_margin (a, GTK_UNIT_MM) == gtk_page_setup_get_bottom_margin (b, GTK_UNIT_MM) &&
    gtk_page_setup_get_left_margin (a, GTK_UNIT_MM) == gtk_page_setup_get_left_margin (b, GTK_UNIT_MM) &&
    gtk_page_setup_get_right_margin (a, GTK_UNIT_MM) == gtk_page_setup_get_right_margin (b, GTK_UNIT_MM);
}

static gboolean
page_setup_is_same_size (GtkPageSetup *a,
                         GtkPageSetup *b)
{
  return gtk_paper_size_is_equal (gtk_page_setup_get_paper_size (a),
                                  gtk_page_setup_get_paper_size (b));
}

static gboolean
set_paper_size (GtkPageSetupUnixDialog *dialog,
                GtkPageSetup           *page_setup,
                gboolean                size_only,
                gboolean                add_item)
{
  GListModel *model;
  GtkPageSetup *list_page_setup;
  guint i;

  if (page_setup == NULL)
    return FALSE;

  model = gtk_drop_down_get_model (GTK_DROP_DOWN (dialog->paper_size_combo));
  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      list_page_setup = g_list_model_get_item (model, i);
      if (list_page_setup == NULL)
        continue;

      if ((size_only && page_setup_is_same_size (page_setup, list_page_setup)) ||
          (!size_only && page_setup_is_equal (page_setup, list_page_setup)))
        {
          gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->paper_size_combo), i);
          g_object_unref (list_page_setup);
          return TRUE;
        }

      g_object_unref (list_page_setup);
    }

  if (add_item)
    {
      i = g_list_model_get_n_items (model);
      g_list_store_append (dialog->page_setup_list, page_setup);
      gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->paper_size_combo), i);
      return TRUE;
    }

  return FALSE;
}

static void
fill_paper_sizes_from_printer (GtkPageSetupUnixDialog *dialog,
                               GtkPrinter             *printer)
{
  GList *list, *l;
  GtkPageSetup *current_page_setup, *page_setup;
  GtkPaperSize *paper_size;
  int i;

  g_list_store_remove_all (dialog->page_setup_list);

  if (printer == NULL)
    {
      for (i = 0; i < G_N_ELEMENTS (common_paper_sizes); i++)
        {
          page_setup = gtk_page_setup_new ();
          paper_size = gtk_paper_size_new (common_paper_sizes[i]);
          gtk_page_setup_set_paper_size_and_default_margins (page_setup, paper_size);
          gtk_paper_size_free (paper_size);

          g_list_store_append (dialog->page_setup_list, page_setup);
          g_object_unref (page_setup);
        }
    }
  else
    {
      list = gtk_printer_list_papers (printer);
      /* TODO: We should really sort this list so interesting size
         are at the top */
      for (l = list; l != NULL; l = l->next)
        {
          page_setup = l->data;
          g_list_store_append (dialog->page_setup_list, page_setup);
          g_object_unref (page_setup);
        }
      g_list_free (list);
    }

  current_page_setup = NULL;

  /* When selecting a different printer, select its default paper size */
  if (printer != NULL)
    current_page_setup = gtk_printer_get_default_page_size (printer);

  if (current_page_setup == NULL)
    current_page_setup = get_current_page_setup (dialog);

  if (!set_paper_size (dialog, current_page_setup, FALSE, FALSE))
    set_paper_size (dialog, current_page_setup, TRUE, TRUE);

  if (current_page_setup)
    g_object_unref (current_page_setup);
}

static void
printer_changed_finished_callback (GtkPrinter             *printer,
                                   gboolean                success,
                                   GtkPageSetupUnixDialog *dialog)
{
  g_signal_handler_disconnect (dialog->request_details_printer,
                               dialog->request_details_tag);
  g_object_unref (dialog->request_details_printer);
  dialog->request_details_tag = 0;
  dialog->request_details_printer = NULL;

  if (success)
    fill_paper_sizes_from_printer (dialog, printer);

}

static void
printer_changed_callback (GtkDropDown            *combo_box,
                          GParamSpec             *pspec,
                          GtkPageSetupUnixDialog *dialog)
{
  GtkPrinter *printer;
  guint selected;

  if (dialog->request_details_tag)
    {
      g_signal_handler_disconnect (dialog->request_details_printer,
                                   dialog->request_details_tag);
      g_object_unref (dialog->request_details_printer);
      dialog->request_details_printer = NULL;
      dialog->request_details_tag = 0;
    }

  selected = gtk_drop_down_get_selected (GTK_DROP_DOWN (dialog->printer_combo));
  if (selected != GTK_INVALID_LIST_POSITION)
    {
      GListModel *model;

      model = gtk_drop_down_get_model (GTK_DROP_DOWN (dialog->printer_combo));
      printer = g_list_model_get_item (model, selected);
      if (strcmp (gtk_printer_get_name (printer), _("Any Printer")) == 0)
        g_clear_object (&printer);

      if (printer == NULL ||
          gtk_printer_has_details (printer))
        fill_paper_sizes_from_printer (dialog, printer);
      else
        {
          dialog->request_details_printer = g_object_ref (printer);
          dialog->request_details_tag =
            g_signal_connect (printer, "details-acquired",
                              G_CALLBACK (printer_changed_finished_callback), dialog);
          gtk_printer_request_details (printer);
        }

      if (printer)
        g_object_unref (printer);

      if (dialog->print_settings)
        {
          const char *name = NULL;

          if (printer)
            name = gtk_printer_get_name (printer);

          gtk_print_settings_set (dialog->print_settings,
                                  "format-for-printer", name);
        }
    }
}

/* We do this munging because we don't want to show zero digits
   after the decimal point, and not to many such digits if they
   are nonzero. I wish printf let you specify max precision for %f... */
static char *
double_to_string (double d,
                  GtkUnit unit)
{
  char *val, *p;
  struct lconv *locale_data;
  const char *decimal_point;
  int decimal_point_len;

  locale_data = localeconv ();
  decimal_point = locale_data->decimal_point;
  decimal_point_len = strlen (decimal_point);

  /* Max two decimal digits for inch, max one for mm */
  if (unit == GTK_UNIT_INCH)
    val = g_strdup_printf ("%.2f", d);
  else
    val = g_strdup_printf ("%.1f", d);

  if (strstr (val, decimal_point))
    {
      p = val + strlen (val) - 1;
      while (*p == '0')
        p--;
      if (p - val + 1 >= decimal_point_len &&
          strncmp (p - (decimal_point_len - 1), decimal_point, decimal_point_len) == 0)
        p -= decimal_point_len;
      p[1] = '\0';
    }

  return val;
}


static void
custom_paper_dialog_response_cb (GtkDialog *custom_paper_dialog,
                                 int        response_id,
                                 gpointer   user_data)
{
  GtkPageSetupUnixDialog *dialog = GTK_PAGE_SETUP_UNIX_DIALOG (user_data);
  GtkPageSetup *last_page_setup;

  dialog->internal_change = TRUE;
  gtk_print_load_custom_papers (dialog->custom_paper_list);
  printer_changed_callback (GTK_DROP_DOWN (dialog->printer_combo), NULL, dialog);
  dialog->internal_change = FALSE;

  if (dialog->last_setup)
    last_page_setup = g_object_ref (dialog->last_setup);
  else
    last_page_setup = gtk_page_setup_new (); /* "good" default */
  set_paper_size (dialog, last_page_setup, FALSE, TRUE);
  g_object_unref (last_page_setup);

  gtk_window_destroy (GTK_WINDOW (custom_paper_dialog));
}

static void
paper_size_changed (GtkDropDown            *combo_box,
                    GParamSpec             *pspec,
                    GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetup *page_setup, *last_page_setup;
  guint selected;
  GtkUnit unit;
  char *str, *w, *h;
  char *top, *bottom, *left, *right;
  GtkLabel *label;
  const char *unit_str;

  if (dialog->internal_change)
    return;

  label = GTK_LABEL (dialog->paper_size_label);

  selected = gtk_drop_down_get_selected (GTK_DROP_DOWN (combo_box));
  if (selected != GTK_INVALID_LIST_POSITION)
    {
      GListModel *papers, *model;

      papers = gtk_drop_down_get_model (GTK_DROP_DOWN (dialog->paper_size_combo));
      page_setup = g_list_model_get_item (papers, selected);
      model = gtk_flatten_list_model_get_model_for_item (GTK_FLATTEN_LIST_MODEL (papers), selected);

      if (model == G_LIST_MODEL (dialog->manage_papers_list))
        {
          GtkWidget *custom_paper_dialog;

          /* Change from "manage" menu item to last value */
          if (dialog->last_setup)
            last_page_setup = g_object_ref (dialog->last_setup);
          else
            last_page_setup = gtk_page_setup_new (); /* "good" default */
          set_paper_size (dialog, last_page_setup, FALSE, TRUE);
          g_object_unref (last_page_setup);

          /* And show the custom paper dialog */
          custom_paper_dialog = _gtk_custom_paper_unix_dialog_new (GTK_WINDOW (dialog), NULL);
          g_signal_connect (custom_paper_dialog, "response", G_CALLBACK (custom_paper_dialog_response_cb), dialog);
          gtk_window_present (GTK_WINDOW (custom_paper_dialog));

          g_object_unref (page_setup);

          return;
        }

      if (dialog->last_setup)
        g_object_unref (dialog->last_setup);

      dialog->last_setup = g_object_ref (page_setup);

      unit = _gtk_print_get_default_user_units ();

      if (unit == GTK_UNIT_MM)
        unit_str = _("mm");
      else
        unit_str = _("inch");

      w = double_to_string (gtk_page_setup_get_paper_width (page_setup, unit),
                            unit);
      h = double_to_string (gtk_page_setup_get_paper_height (page_setup, unit),
                            unit);
      str = g_strdup_printf ("%s × %s %s", w, h, unit_str);
      g_free (w);
      g_free (h);

      gtk_label_set_text (label, str);
      g_free (str);

      top = double_to_string (gtk_page_setup_get_top_margin (page_setup, unit), unit);
      bottom = double_to_string (gtk_page_setup_get_bottom_margin (page_setup, unit), unit);
      left = double_to_string (gtk_page_setup_get_left_margin (page_setup, unit), unit);
      right = double_to_string (gtk_page_setup_get_right_margin (page_setup, unit), unit);

      str = g_strdup_printf (_("Margins:\n"
                               " Left: %s %s\n"
                               " Right: %s %s\n"
                               " Top: %s %s\n"
                               " Bottom: %s %s"
                               ),
                             left, unit_str,
                             right, unit_str,
                             top, unit_str,
                             bottom, unit_str);
      g_free (top);
      g_free (bottom);
      g_free (left);
      g_free (right);

      gtk_widget_set_tooltip_text (dialog->paper_size_label, str);
      g_free (str);

      g_object_unref (page_setup);
    }
  else
    {
      gtk_label_set_text (label, "");
      gtk_widget_set_tooltip_text (dialog->paper_size_label, NULL);
      if (dialog->last_setup)
        g_object_unref (dialog->last_setup);
      dialog->last_setup = NULL;
    }
}

/**
 * gtk_page_setup_unix_dialog_new:
 * @title: (nullable): the title of the dialog
 * @parent: (nullable): transient parent of the dialog
 *
 * Creates a new page setup dialog.
 *
 * Returns: the new `GtkPageSetupUnixDialog`
 */
GtkWidget *
gtk_page_setup_unix_dialog_new (const char *title,
                                GtkWindow   *parent)
{
  GtkWidget *result;

  if (title == NULL)
    title = _("Page Setup");

  result = g_object_new (GTK_TYPE_PAGE_SETUP_UNIX_DIALOG,
                         "title", title,
                         NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);

  return result;
}

static GtkPageOrientation
get_orientation (GtkPageSetupUnixDialog *dialog)
{
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->portrait_radio)))
    return GTK_PAGE_ORIENTATION_PORTRAIT;
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->landscape_radio)))
    return GTK_PAGE_ORIENTATION_LANDSCAPE;
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->reverse_landscape_radio)))
    return GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE;
  return GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT;
}

static void
set_orientation (GtkPageSetupUnixDialog *dialog,
                 GtkPageOrientation      orientation)
{
  switch (orientation)
    {
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
      gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->reverse_portrait_radio), TRUE);
      break;
    case GTK_PAGE_ORIENTATION_PORTRAIT:
      gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->portrait_radio), TRUE);
      break;
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
      gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->landscape_radio), TRUE);
      break;
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
      gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->reverse_landscape_radio), TRUE);
      break;
    default:
      break;
    }
}

/**
 * gtk_page_setup_unix_dialog_set_page_setup:
 * @dialog: a `GtkPageSetupUnixDialog`
 * @page_setup: a `GtkPageSetup`
 *
 * Sets the `GtkPageSetup` from which the page setup
 * dialog takes its values.
 */
void
gtk_page_setup_unix_dialog_set_page_setup (GtkPageSetupUnixDialog *dialog,
                                           GtkPageSetup           *page_setup)
{
  if (page_setup)
    {
      set_paper_size (dialog, page_setup, FALSE, TRUE);
      set_orientation (dialog, gtk_page_setup_get_orientation (page_setup));
    }
}

/**
 * gtk_page_setup_unix_dialog_get_page_setup:
 * @dialog: a `GtkPageSetupUnixDialog`
 *
 * Gets the currently selected page setup from the dialog.
 *
 * Returns: (transfer none): the current page setup
 */
GtkPageSetup *
gtk_page_setup_unix_dialog_get_page_setup (GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetup *page_setup;

  page_setup = get_current_page_setup (dialog);

  gtk_page_setup_set_orientation (page_setup, get_orientation (dialog));

  return page_setup;
}

static gboolean
set_active_printer (GtkPageSetupUnixDialog *dialog,
                    const char             *printer_name)
{
  guint i, n;
  GtkPrinter *printer;

  if (!printer_name)
    return FALSE;

  for (i = 0, n = g_list_model_get_n_items (dialog->printer_list); i < n; i++)
    {
      printer = g_list_model_get_item (dialog->printer_list, i);

      if (strcmp (gtk_printer_get_name (printer), printer_name) == 0)
        {
          gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->printer_combo), i);
          g_object_unref (printer);

          return TRUE;
        }

      g_object_unref (printer);
    }

  return FALSE;
}

/**
 * gtk_page_setup_unix_dialog_set_print_settings:
 * @dialog: a `GtkPageSetupUnixDialog`
 * @print_settings: (nullable): a `GtkPrintSettings`
 *
 * Sets the `GtkPrintSettings` from which the page setup dialog
 * takes its values.
 */
void
gtk_page_setup_unix_dialog_set_print_settings (GtkPageSetupUnixDialog *dialog,
                                               GtkPrintSettings       *print_settings)
{
  const char *format_for_printer;

  if (dialog->print_settings == print_settings) return;

  if (dialog->print_settings)
    g_object_unref (dialog->print_settings);

  dialog->print_settings = print_settings;

  if (print_settings)
    {
      g_object_ref (print_settings);

      format_for_printer = gtk_print_settings_get (print_settings, "format-for-printer");

      /* Set printer if in list, otherwise set when
       * that printer is added
       */
      set_active_printer (dialog, format_for_printer);
    }
}

/**
 * gtk_page_setup_unix_dialog_get_print_settings:
 * @dialog: a `GtkPageSetupUnixDialog`
 *
 * Gets the current print settings from the dialog.
 *
 * Returns: (transfer none) (nullable): the current print settings
 **/
GtkPrintSettings *
gtk_page_setup_unix_dialog_get_print_settings (GtkPageSetupUnixDialog *dialog)
{
  return dialog->print_settings;
}

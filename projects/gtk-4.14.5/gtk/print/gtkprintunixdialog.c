/* GtkPrintUnixDialog
 * Copyright (C) 2006 John (J5) Palmieri  <johnp@redhat.com>
 * Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
 * Copyright © 2006, 2007 Christian Persch
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
#include <ctype.h>
#include <stdio.h>
#include <math.h>

#include <glib/gi18n-lib.h>

#include "gtkmarshalers.h"
#include "deprecated/gtkdialogprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkcsscolorvalueprivate.h"

#include "gtkprintunixdialog.h"

#include "gtkcustompaperunixdialog.h"
#include "gtkprintbackendprivate.h"
#include "gtkprinterprivate.h"
#include "gtkprinteroptionwidgetprivate.h"
#include "gtkprintutilsprivate.h"
#include "gtkpagethumbnailprivate.h"


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkPrintUnixDialog:
 *
 * `GtkPrintUnixDialog` implements a print dialog for platforms
 * which don’t provide a native print dialog, like Unix.
 *
 * ![An example GtkPrintUnixDialog](printdialog.png)
 *
 * It can be used very much like any other GTK dialog, at the cost of
 * the portability offered by the high-level printing API with
 * [class@Gtk.PrintOperation].
 *
 * In order to print something with `GtkPrintUnixDialog`, you need to
 * use [method@Gtk.PrintUnixDialog.get_selected_printer] to obtain a
 * [class@Gtk.Printer] object and use it to construct a [class@Gtk.PrintJob]
 * using [ctor@Gtk.PrintJob.new].
 *
 * `GtkPrintUnixDialog` uses the following response values:
 *
 * - %GTK_RESPONSE_OK: for the “Print” button
 * - %GTK_RESPONSE_APPLY: for the “Preview” button
 * - %GTK_RESPONSE_CANCEL: for the “Cancel” button
 *
 * # GtkPrintUnixDialog as GtkBuildable
 *
 * The `GtkPrintUnixDialog` implementation of the `GtkBuildable` interface
 * exposes its @notebook internal children with the name “notebook”.
 *
 * An example of a `GtkPrintUnixDialog` UI definition fragment:
 *
 * ```xml
 * <object class="GtkPrintUnixDialog" id="dialog1">
 *   <child internal-child="notebook">
 *     <object class="GtkNotebook" id="notebook">
 *       <child>
 *         <object type="GtkNotebookPage">
 *           <property name="tab_expand">False</property>
 *           <property name="tab_fill">False</property>
 *           <property name="tab">
 *             <object class="GtkLabel" id="tablabel">
 *               <property name="label">Tab label</property>
 *             </object>
 *           </property>
 *           <property name="child">
 *             <object class="GtkLabel" id="tabcontent">
 *               <property name="label">Content on notebook tab</property>
 *             </object>
 *           </property>
 *         </object>
 *       </child>
 *     </object>
 *   </child>
 * </object>
 * ```
 *
 * # CSS nodes
 *
 * `GtkPrintUnixDialog` has a single CSS node with name window. The style classes
 * dialog and print are added.
 */


#define EXAMPLE_PAGE_AREA_SIZE 110
#define RULER_DISTANCE 7.5
#define RULER_RADIUS 2


static void     gtk_print_unix_dialog_constructed  (GObject            *object);
static void     gtk_print_unix_dialog_dispose      (GObject            *object);
static void     gtk_print_unix_dialog_finalize     (GObject            *object);
static void     gtk_print_unix_dialog_set_property (GObject            *object,
                                                    guint               prop_id,
                                                    const GValue       *value,
                                                    GParamSpec         *pspec);
static void     gtk_print_unix_dialog_get_property (GObject            *object,
                                                    guint               prop_id,
                                                    GValue             *value,
                                                    GParamSpec         *pspec);
static void     unschedule_idle_mark_conflicts     (GtkPrintUnixDialog *dialog);
static void     selected_printer_changed           (GtkPrintUnixDialog *dialog);
static void     clear_per_printer_ui               (GtkPrintUnixDialog *dialog);
static void     printer_added_cb                   (GListModel         *model,
                                                    guint               position,
                                                    guint               removed,
                                                    guint               added,
                                                    GtkPrintUnixDialog *dialog);
static void     printer_status_cb                  (GtkPrintBackend    *backend,
                                                    GtkPrinter         *printer,
                                                    GtkPrintUnixDialog *dialog);
static void     update_collate_icon                (GtkToggleButton    *toggle_button,
                                                    GtkPrintUnixDialog *dialog);
static void     error_dialogs                      (GtkPrintUnixDialog *print_dialog,
						    int                 print_dialog_response_id,
						    gpointer            data);
static gboolean page_range_entry_focus_changed     (GtkWidget          *entry,
                                                    GParamSpec         *pspec,
                                                    GtkPrintUnixDialog *dialog);
static void     update_page_range_entry_sensitivity(GtkWidget          *button,
						    GtkPrintUnixDialog *dialog);
static void     update_print_at_entry_sensitivity  (GtkWidget          *button,
						    GtkPrintUnixDialog *dialog);
static void     update_print_at_option             (GtkPrintUnixDialog *dialog);
static void     update_dialog_from_capabilities    (GtkPrintUnixDialog *dialog);
static gboolean is_printer_active                  (gpointer            item,
                                                    gpointer            data);
static  int     default_printer_list_sort_func     (gconstpointer        a,
                                                    gconstpointer        b,
						    gpointer             user_data);
static void     update_number_up_layout            (GtkPrintUnixDialog  *dialog);
static void     draw_page                          (GtkDrawingArea      *da,
						    cairo_t             *cr,
                                                    int                  width,
                                                    int                  height,
                                                    gpointer             data);


static gboolean dialog_get_collate                 (GtkPrintUnixDialog *dialog);
static gboolean dialog_get_reverse                 (GtkPrintUnixDialog *dialog);
static int      dialog_get_n_copies                (GtkPrintUnixDialog *dialog);

static gboolean set_active_printer                 (GtkPrintUnixDialog *dialog,
                                                    const char         *printer_name);
static void redraw_page_layout_preview             (GtkPrintUnixDialog *dialog);
static GListModel *load_print_backends             (GtkPrintUnixDialog *dialog);

/* GtkBuildable */
static void gtk_print_unix_dialog_buildable_init                    (GtkBuildableIface *iface);
static GObject *gtk_print_unix_dialog_buildable_get_internal_child  (GtkBuildable *buildable,
                                                                     GtkBuilder   *builder,
                                                                     const char   *childname);

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
  "iso_a3",
};

/* Keep in line with liststore defined in gtkprintunixdialog.ui */
enum {
  PAGE_SETUP_LIST_COL_PAGE_SETUP,
  PAGE_SETUP_LIST_COL_IS_SEPARATOR,
  PAGE_SETUP_LIST_N_COLS
};

/* Keep in line with liststore defined in gtkprintunixdialog.ui */
enum {
  PRINTER_LIST_COL_ICON,
  PRINTER_LIST_COL_NAME,
  PRINTER_LIST_COL_STATE,
  PRINTER_LIST_COL_JOBS,
  PRINTER_LIST_COL_LOCATION,
  PRINTER_LIST_COL_PRINTER_OBJ,
  PRINTER_LIST_N_COLS
};

enum {
  PROP_0,
  PROP_PAGE_SETUP,
  PROP_CURRENT_PAGE,
  PROP_PRINT_SETTINGS,
  PROP_SELECTED_PRINTER,
  PROP_MANUAL_CAPABILITIES,
  PROP_SUPPORT_SELECTION,
  PROP_HAS_SELECTION,
  PROP_EMBED_PAGE_SETUP
};

typedef struct _GtkPrintUnixDialogClass    GtkPrintUnixDialogClass;

struct _GtkPrintUnixDialog
{
  GtkDialog parent_instance;

  GtkWidget *notebook;

  GtkWidget *printer_list;

  GtkPrintCapabilities manual_capabilities;
  GtkPrintCapabilities printer_capabilities;

  GtkPageSetup *page_setup;
  gboolean page_setup_set;
  gboolean embed_page_setup;
  GListStore *page_setup_list;
  GListStore *custom_paper_list;
  GListStore *manage_papers_list;

  gboolean support_selection;
  gboolean has_selection;

  GtkWidget *all_pages_radio;
  GtkWidget *current_page_radio;
  GtkWidget *selection_radio;
  GtkWidget *range_table;
  GtkWidget *page_range_radio;
  GtkWidget *page_range_entry;

  GtkWidget *copies_spin;
  GtkWidget *collate_check;
  GtkWidget *reverse_check;
  GtkWidget *page_collate_preview;
  GtkWidget *page_a1;
  GtkWidget *page_a2;
  GtkWidget *page_b1;
  GtkWidget *page_b2;
  GtkWidget *page_layout_preview;
  GtkWidget *scale_spin;
  GtkWidget *page_set_combo;
  GtkWidget *print_now_radio;
  GtkWidget *print_at_radio;
  GtkWidget *print_at_entry;
  GtkWidget *print_hold_radio;
  GtkWidget *paper_size_combo;
  GtkWidget *orientation_combo;
  gboolean internal_page_setup_change;
  gboolean updating_print_at;
  GtkPrinterOptionWidget *pages_per_sheet;
  GtkPrinterOptionWidget *duplex;
  GtkPrinterOptionWidget *paper_type;
  GtkPrinterOptionWidget *paper_source;
  GtkPrinterOptionWidget *output_tray;
  GtkPrinterOptionWidget *job_prio;
  GtkPrinterOptionWidget *billing_info;
  GtkPrinterOptionWidget *cover_before;
  GtkPrinterOptionWidget *cover_after;
  GtkPrinterOptionWidget *number_up_layout;

  GtkWidget *conflicts_widget;

  GtkWidget *job_page;
  GtkWidget *finishing_table;
  GtkWidget *finishing_page;
  GtkWidget *image_quality_table;
  GtkWidget *image_quality_page;
  GtkWidget *color_table;
  GtkWidget *color_page;

  GtkWidget *advanced_vbox;
  GtkWidget *advanced_page;

  GtkWidget *extension_point;

  /* These are set initially on selected printer (either default printer,
   * printer taken from set settings, or user-selected), but when any
   * setting is changed by the user it is cleared.
   */
  GtkPrintSettings *initial_settings;

  GtkPrinterOption *number_up_layout_n_option;
  GtkPrinterOption *number_up_layout_2_option;

  /* This is the initial printer set by set_settings. We look for it in
   * the added printers. We clear this whenever the user manually changes
   * to another printer, when the user changes a setting or when we find
   * this printer.
   */
  char *waiting_for_printer;
  gboolean internal_printer_change;

  GList *print_backends;

  GtkPrinter *current_printer;
  GtkPrinter *request_details_printer;
  gulong request_details_tag;
  GtkPrinterOptionSet *options;
  gulong options_changed_handler;
  gulong mark_conflicts_id;

  char *format_for_printer;

  int current_page;
  GtkCssNode *collate_paper_node;
  GtkCssNode *page_layout_paper_node;
};

struct _GtkPrintUnixDialogClass
{
  GtkDialogClass parent_class;
};

G_DEFINE_TYPE_WITH_CODE (GtkPrintUnixDialog, gtk_print_unix_dialog, GTK_TYPE_DIALOG,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_print_unix_dialog_buildable_init))

static GtkBuildableIface *parent_buildable_iface;

static gboolean
is_default_printer (GtkPrintUnixDialog *dialog,
                    GtkPrinter         *printer)
{
  if (dialog->format_for_printer)
    return strcmp (dialog->format_for_printer,
                   gtk_printer_get_name (printer)) == 0;
 else
   return gtk_printer_is_default (printer);
}

static const char *css_data = ""
  "page-thumbnail {\n"
  "  border: 1px solid #e6e5e4;\n"
  "  background: white;\n"
  "}\n"
  "page-thumbnail > label {\n"
  "  font-family: Sans;\n"
  "  font-size: 9pt;\n"
  "  color: #2e3436;\n"
  "}\n";

static void
ensure_fallback_style (void)
{
  GdkDisplay *display;
  GtkCssProvider *provider;

  display = gdk_display_get_default ();
  if (!display)
    return;

  provider = gtk_css_provider_new ();

  gtk_css_provider_load_from_string (provider, css_data);

  gtk_style_context_add_provider_for_display (display,
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

  g_object_unref (provider);
}

static void
gtk_print_unix_dialog_class_init (GtkPrintUnixDialogClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  ensure_fallback_style ();

  g_type_ensure (GTK_TYPE_PAGE_THUMBNAIL);

  object_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  object_class->constructed = gtk_print_unix_dialog_constructed;
  object_class->finalize = gtk_print_unix_dialog_finalize;
  object_class->dispose = gtk_print_unix_dialog_dispose;
  object_class->set_property = gtk_print_unix_dialog_set_property;
  object_class->get_property = gtk_print_unix_dialog_get_property;

  /**
   * GtkPrintUnixDialog:page-setup: (attributes org.gtk.Property.get=gtk_print_unix_dialog_get_page_setup org.gtk.Property.set=gtk_print_unix_dialog_set_page_setup)
   *
   * The `GtkPageSetup` object to use.
   */
  g_object_class_install_property (object_class,
                                   PROP_PAGE_SETUP,
                                   g_param_spec_object ("page-setup", NULL, NULL,
                                                        GTK_TYPE_PAGE_SETUP,
                                                        G_PARAM_READWRITE));

  /**
   * GtkPrintUnixDialog:current-page: (attributes org.gtk.Property.get=gtk_print_unix_dialog_get_current_page org.gtk.Property.set=gtk_print_unix_dialog_set_current_page)
   *
   * The current page in the document.
   */
  g_object_class_install_property (object_class,
                                   PROP_CURRENT_PAGE,
                                   g_param_spec_int ("current-page", NULL, NULL,
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READWRITE));

  /**
   * GtkPrintUnixDialog:print-settings: (attributes org.gtk.Property.get=gtk_print_unix_dialog_get_settings org.gtk.Property.set=gtk_print_unix_dialog_set_settings)
   *
   * The `GtkPrintSettings` object used for this dialog.
   */
  g_object_class_install_property (object_class,
                                   PROP_PRINT_SETTINGS,
                                   g_param_spec_object ("print-settings", NULL, NULL,
                                                        GTK_TYPE_PRINT_SETTINGS,
                                                        G_PARAM_READWRITE));

  /**
   * GtkPrintUnixDialog:selected-printer: (attributes org.gtk.Property.get=gtk_print_unix_dialog_get_selected_printer)
   *
   * The `GtkPrinter` which is selected.
   */
  g_object_class_install_property (object_class,
                                   PROP_SELECTED_PRINTER,
                                   g_param_spec_object ("selected-printer", NULL, NULL,
                                                        GTK_TYPE_PRINTER,
                                                        G_PARAM_READABLE));

  /**
   * GtkPrintUnixDialog:manual-capabilities: (attributes org.gtk.Property.get=gtk_print_unix_dialog_get_manual_capabilities org.gtk.Property.set=gtk_print_unix_dialog_set_manual_capabilities)
   *
   * Capabilities the application can handle.
   */
  g_object_class_install_property (object_class,
                                   PROP_MANUAL_CAPABILITIES,
                                   g_param_spec_flags ("manual-capabilities", NULL, NULL,
                                                       GTK_TYPE_PRINT_CAPABILITIES,
                                                       0,
                                                       G_PARAM_READWRITE));

  /**
   * GtkPrintUnixDialog:support-selection: (attributes org.gtk.Property.get=gtk_print_unix_dialog_get_support_selection org.gtk.Property.set=gtk_print_unix_dialog_set_support_selection)
   *
   * Whether the dialog supports selection.
   */
  g_object_class_install_property (object_class,
                                   PROP_SUPPORT_SELECTION,
                                   g_param_spec_boolean ("support-selection", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * GtkPrintUnixDialog:has-selection: (attributes org.gtk.Property.get=gtk_print_unix_dialog_get_has_selection org.gtk.Property.set=gtk_print_unix_dialog_set_has_selection)
   *
   * Whether the application has a selection.
   */
  g_object_class_install_property (object_class,
                                   PROP_HAS_SELECTION,
                                   g_param_spec_boolean ("has-selection", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

   /**
    * GtkPrintUnixDialog:embed-page-setup: (attributes org.gtk.Property.get=gtk_print_unix_dialog_get_embed_page_setup org.gtk.Property.set=gtk_print_unix_dialog_set_embed_page_setup)
    *
    * %TRUE if the page setup controls are embedded.
    */
   g_object_class_install_property (object_class,
                                   PROP_EMBED_PAGE_SETUP,
                                   g_param_spec_boolean ("embed-page-setup", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/print/ui/gtkprintunixdialog.ui");

  /* GtkTreeView / GtkTreeModel */
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, printer_list);

  /* General Widgetry */
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, notebook);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, all_pages_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, all_pages_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, current_page_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, selection_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, range_table);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_range_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_range_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, copies_spin);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, collate_check);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, reverse_check);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_collate_preview);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_a1);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_a2);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_b1);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_b2);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_layout_preview);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, scale_spin);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, page_set_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, print_now_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, print_at_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, print_at_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, print_hold_radio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, paper_size_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, orientation_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, conflicts_widget);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, job_page);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, finishing_table);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, finishing_page);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, image_quality_table);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, image_quality_page);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, color_table);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, color_page);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, advanced_vbox);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, advanced_page);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, extension_point);

  /* GtkPrinterOptionWidgets... */
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, pages_per_sheet);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, duplex);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, paper_type);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, paper_source);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, output_tray);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, job_prio);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, billing_info);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, cover_before);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, cover_after);
  gtk_widget_class_bind_template_child (widget_class, GtkPrintUnixDialog, number_up_layout);

  /* Callbacks handled in the UI */
  gtk_widget_class_bind_template_callback (widget_class, redraw_page_layout_preview);
  gtk_widget_class_bind_template_callback (widget_class, error_dialogs);
  gtk_widget_class_bind_template_callback (widget_class, page_range_entry_focus_changed);
  gtk_widget_class_bind_template_callback (widget_class, update_page_range_entry_sensitivity);
  gtk_widget_class_bind_template_callback (widget_class, update_print_at_entry_sensitivity);
  gtk_widget_class_bind_template_callback (widget_class, update_print_at_option);
  gtk_widget_class_bind_template_callback (widget_class, update_dialog_from_capabilities);
  gtk_widget_class_bind_template_callback (widget_class, update_collate_icon);
  gtk_widget_class_bind_template_callback (widget_class, redraw_page_layout_preview);
  gtk_widget_class_bind_template_callback (widget_class, update_number_up_layout);
  gtk_widget_class_bind_template_callback (widget_class, redraw_page_layout_preview);
}

/* Returns a toplevel GtkWindow, or NULL if none */
static GtkWindow *
get_toplevel (GtkWidget *widget)
{
  GtkWidget *toplevel = NULL;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  if (GTK_IS_WINDOW (toplevel))
    return GTK_WINDOW (toplevel);
  else
    return NULL;
}

static void
set_busy_cursor (GtkPrintUnixDialog *dialog,
                 gboolean            busy)
{
  GtkWidget *widget;
  GtkWindow *toplevel;

  toplevel = get_toplevel (GTK_WIDGET (dialog));
  widget = GTK_WIDGET (toplevel);

  if (!toplevel || !gtk_widget_get_realized (widget))
    return;

  if (busy)
    gtk_widget_set_cursor_from_name (widget, "progress");
  else
    gtk_widget_set_cursor (widget, NULL);
}

typedef struct {
  GMainLoop *loop;
  int response;
} ConfirmationData;

static void
on_confirmation_dialog_response (GtkWidget *dialog,
                                 int        response,
                                 gpointer   user_data)
{
  ConfirmationData *data = user_data;

  data->response = response;

  g_main_loop_quit (data->loop);

  gtk_window_destroy (GTK_WINDOW (dialog));
}

/* This function handles error messages before printing.
 */
static void
error_dialogs (GtkPrintUnixDialog *dialog,
               int                 dialog_response_id,
               gpointer            data)
{
  if (dialog != NULL && dialog_response_id == GTK_RESPONSE_OK)
    {
      GtkPrinter *printer = gtk_print_unix_dialog_get_selected_printer (dialog);

      if (printer != NULL)
        {
          if (dialog->request_details_tag || !gtk_printer_is_accepting_jobs (printer))
            {
              g_signal_stop_emission_by_name (dialog, "response");
              return;
            }

          /* Shows overwrite confirmation dialog in the case of printing
           * to file which already exists.
           */
          if (gtk_printer_is_virtual (printer))
            {
              GtkPrinterOption *option =
                gtk_printer_option_set_lookup (dialog->options,
                                               "gtk-main-page-custom-input");

              if (option != NULL &&
                  option->type == GTK_PRINTER_OPTION_TYPE_FILESAVE)
                {
                  GFile *file = g_file_new_for_uri (option->value);

                  if (g_file_query_exists (file, NULL))
                    {
                      GtkWidget *message_dialog;
                      GtkWindow *toplevel;
                      char *basename;
                      char *dirname;
                      GFile *parent;

                      toplevel = get_toplevel (GTK_WIDGET (dialog));

                      basename = g_file_get_basename (file);
                      parent = g_file_get_parent (file);
                      dirname = g_file_get_parse_name (parent);
                      g_object_unref (parent);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                      message_dialog = gtk_message_dialog_new (toplevel,
                                                               GTK_DIALOG_MODAL |
                                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                                               GTK_MESSAGE_QUESTION,
                                                               GTK_BUTTONS_NONE,
                                                               _("A file named “%s” already exists.  Do you want to replace it?"),
                                                               basename);

                      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message_dialog),
                                                                _("The file already exists in “%s”.  Replacing it will "
                                                                "overwrite its contents."),
                                                                dirname);

                      gtk_dialog_add_button (GTK_DIALOG (message_dialog),
                                             _("_Cancel"),
                                             GTK_RESPONSE_CANCEL);
                      gtk_dialog_add_button (GTK_DIALOG (message_dialog),
                                             _("_Replace"),
                                             GTK_RESPONSE_ACCEPT);
                      gtk_dialog_set_default_response (GTK_DIALOG (message_dialog),
                                                       GTK_RESPONSE_ACCEPT);
G_GNUC_END_IGNORE_DEPRECATIONS

                      if (gtk_window_has_group (toplevel))
                        gtk_window_group_add_window (gtk_window_get_group (toplevel),
                                                     GTK_WINDOW (message_dialog));

                      gtk_window_present (GTK_WINDOW (message_dialog));

                      /* Block on the confirmation dialog until we have a response,
                       * so that we can stop the "response" signal emission on the
                       * print dialog
                       */
                      ConfirmationData cdata;

                      cdata.loop = g_main_loop_new (NULL, FALSE);
                      cdata.response = 0;

                      g_signal_connect (message_dialog, "response",
                                        G_CALLBACK (on_confirmation_dialog_response),
                                        &cdata);

                      g_main_loop_run (cdata.loop);
                      g_main_loop_unref (cdata.loop);

                      g_free (dirname);
                      g_free (basename);

                      if (cdata.response != GTK_RESPONSE_ACCEPT)
                        g_signal_stop_emission_by_name (dialog, "response");
                    }

                  g_object_unref (file);
                }
            }
        }
    }
}

static char *
get_printer_key (GtkPrinter *printer)
{
  return g_strconcat ("", gtk_printer_get_name (printer), " ", gtk_printer_get_location (printer), NULL);
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
                           GtkPrintUnixDialog       *self)
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
                      GtkPrintUnixDialog       *self)
{
  GtkWidget *label;

  bind_paper_size_list_item (factory, item, self);

  label = gtk_list_item_get_child (item);
  gtk_widget_remove_css_class (label, "separator-before");
}

static void
gtk_print_unix_dialog_init (GtkPrintUnixDialog *dialog)
{
  GtkWidget *widget;
  GListModel *model;
  GListModel *sorted;
  GListModel *filtered;
  GListModel *selection;
  GtkSorter *sorter;
  GtkFilter *filter;
  GtkStringFilter *filter1;
  GtkCustomFilter *filter2;
  GtkListItemFactory *factory;
  GListStore *store;
  GListModel *paper_size_list;
  GtkPageSetup *page_setup;

  dialog->print_backends = NULL;
  dialog->current_page = -1;
  dialog->number_up_layout_n_option = NULL;
  dialog->number_up_layout_2_option = NULL;

  dialog->page_setup = gtk_page_setup_new ();
  dialog->page_setup_set = FALSE;
  dialog->embed_page_setup = FALSE;
  dialog->internal_page_setup_change = FALSE;
  dialog->page_setup_list = g_list_store_new (GTK_TYPE_PAGE_SETUP);
  dialog->custom_paper_list = g_list_store_new (GTK_TYPE_PAGE_SETUP);
  dialog->manage_papers_list = g_list_store_new (GTK_TYPE_PAGE_SETUP);
  page_setup = gtk_page_setup_new ();
  g_list_store_append (dialog->manage_papers_list, page_setup);
  g_object_unref (page_setup);

  dialog->support_selection = FALSE;
  dialog->has_selection = FALSE;

  g_type_ensure (GTK_TYPE_PRINTER);
  g_type_ensure (GTK_TYPE_PRINTER_OPTION);
  g_type_ensure (GTK_TYPE_PRINTER_OPTION_SET);
  g_type_ensure (GTK_TYPE_PRINTER_OPTION_WIDGET);

  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_widget_add_css_class (GTK_WIDGET (dialog), "print");

  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (dialog));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Pre_view"), GTK_RESPONSE_APPLY,
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("_Print"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  widget = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_widget_set_sensitive (widget, FALSE);

  gtk_widget_set_visible (dialog->selection_radio, FALSE);
  gtk_widget_set_visible (dialog->conflicts_widget, FALSE);

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

  /* Load backends */
  model = load_print_backends (dialog);
  sorter = GTK_SORTER (gtk_custom_sorter_new (default_printer_list_sort_func, NULL, NULL));
  sorted = G_LIST_MODEL (gtk_sort_list_model_new (model, sorter));

  filter = GTK_FILTER (gtk_every_filter_new ());

  filter1 = gtk_string_filter_new (
                gtk_cclosure_expression_new (G_TYPE_STRING,
                                             NULL, 0, NULL,
                                             G_CALLBACK (get_printer_key),
                                             NULL, NULL));
  gtk_string_filter_set_match_mode (filter1, GTK_STRING_FILTER_MATCH_MODE_SUBSTRING);
  gtk_string_filter_set_ignore_case (filter1, TRUE);
  gtk_multi_filter_append (GTK_MULTI_FILTER (filter), GTK_FILTER (filter1));

  filter2 = gtk_custom_filter_new (is_printer_active, dialog, NULL);
  gtk_multi_filter_append (GTK_MULTI_FILTER (filter), GTK_FILTER (filter2));

  filtered = G_LIST_MODEL (gtk_filter_list_model_new (sorted, filter));

  selection = G_LIST_MODEL (gtk_single_selection_new (filtered));
  gtk_single_selection_set_autoselect (GTK_SINGLE_SELECTION (selection), FALSE);
  gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (selection), GTK_INVALID_LIST_POSITION);
  gtk_column_view_set_model (GTK_COLUMN_VIEW (dialog->printer_list), GTK_SELECTION_MODEL (selection));
  g_signal_connect (selection, "items-changed", G_CALLBACK (printer_added_cb), dialog);
  g_signal_connect_swapped (selection, "notify::selected", G_CALLBACK (selected_printer_changed), dialog);
  g_object_unref (selection);

  gtk_print_load_custom_papers (dialog->custom_paper_list);

  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (dialog->page_layout_preview),
                                  draw_page,
                                  dialog, NULL);
  gtk_css_node_set_name (gtk_widget_get_css_node (dialog->page_layout_preview), g_quark_from_static_string ("drawing"));

  dialog->collate_paper_node = gtk_css_node_new();
  gtk_css_node_set_name (dialog->collate_paper_node, g_quark_from_static_string ("paper"));
  g_object_unref (dialog->collate_paper_node);

  dialog->page_layout_paper_node = gtk_css_node_new();
  gtk_css_node_set_name (dialog->page_layout_paper_node, g_quark_from_static_string ("paper"));
  gtk_css_node_set_parent (dialog->page_layout_paper_node,
                           gtk_widget_get_css_node (dialog->page_layout_preview));
  g_object_unref (dialog->page_layout_paper_node);
}

static void
gtk_print_unix_dialog_constructed (GObject *object)
{
  gboolean use_header;

  G_OBJECT_CLASS (gtk_print_unix_dialog_parent_class)->constructed (object);

  g_object_get (object, "use-header-bar", &use_header, NULL);
  if (use_header)
    {
       /* Reorder the preview button */
       GtkWidget *button, *parent;
       button = gtk_dialog_get_widget_for_response (GTK_DIALOG (object), GTK_RESPONSE_APPLY);
       g_object_ref (button);
       parent = gtk_widget_get_ancestor (button, GTK_TYPE_HEADER_BAR);
       gtk_box_remove (GTK_BOX (gtk_widget_get_parent (button)), button);
       gtk_header_bar_pack_end (GTK_HEADER_BAR (parent), button);
       g_object_unref (button);
    }

  update_dialog_from_capabilities (GTK_PRINT_UNIX_DIALOG (object));
}

static void
gtk_print_unix_dialog_dispose (GObject *object)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (object);

  /* Make sure we don't destroy custom widgets owned by the backends */
  clear_per_printer_ui (dialog);

  G_OBJECT_CLASS (gtk_print_unix_dialog_parent_class)->dispose (object);
}

static void
disconnect_printer_details_request (GtkPrintUnixDialog *dialog,
                                    gboolean            details_failed)
{
  if (dialog->request_details_tag)
    {
      g_signal_handler_disconnect (dialog->request_details_printer,
                                   dialog->request_details_tag);
      dialog->request_details_tag = 0;
      set_busy_cursor (dialog, FALSE);
      if (details_failed)
        gtk_printer_set_state_message (dialog->request_details_printer, _("Getting printer information failed"));
      g_clear_object (&dialog->request_details_printer);
    }
}

static void
gtk_print_unix_dialog_finalize (GObject *object)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (object);
  GList *iter;

  unschedule_idle_mark_conflicts (dialog);
  disconnect_printer_details_request (dialog, FALSE);

  g_clear_object (&dialog->current_printer);
  g_clear_object (&dialog->options);

  if (dialog->number_up_layout_2_option)
    {
      dialog->number_up_layout_2_option->choices[0] = NULL;
      dialog->number_up_layout_2_option->choices[1] = NULL;
      g_free (dialog->number_up_layout_2_option->choices_display[0]);
      g_free (dialog->number_up_layout_2_option->choices_display[1]);
      dialog->number_up_layout_2_option->choices_display[0] = NULL;
      dialog->number_up_layout_2_option->choices_display[1] = NULL;
      g_object_unref (dialog->number_up_layout_2_option);
      dialog->number_up_layout_2_option = NULL;
    }

  g_clear_object (&dialog->number_up_layout_n_option);
  g_clear_object (&dialog->page_setup);
  g_clear_object (&dialog->initial_settings);
  g_clear_pointer (&dialog->waiting_for_printer, (GDestroyNotify)g_free);
  g_clear_pointer (&dialog->format_for_printer, (GDestroyNotify)g_free);

  for (iter = dialog->print_backends; iter != NULL; iter = iter->next)
    gtk_print_backend_destroy (GTK_PRINT_BACKEND (iter->data));
  g_list_free_full (dialog->print_backends, g_object_unref);
  dialog->print_backends = NULL;

  g_clear_object (&dialog->page_setup_list);
  g_clear_object (&dialog->custom_paper_list);
  g_clear_object (&dialog->manage_papers_list);

  G_OBJECT_CLASS (gtk_print_unix_dialog_parent_class)->finalize (object);
}

static void
gtk_print_unix_dialog_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->get_internal_child = gtk_print_unix_dialog_buildable_get_internal_child;
}

static GObject *
gtk_print_unix_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                                                    GtkBuilder   *builder,
                                                    const char   *childname)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (buildable);

  if (strcmp (childname, "notebook") == 0)
    return G_OBJECT (dialog->notebook);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void
printer_status_cb (GtkPrintBackend    *backend,
                   GtkPrinter         *printer,
                   GtkPrintUnixDialog *dialog)
{
  GListModel *model;

  /* When the pause state change then we need to update sensitive property
   * of GTK_RESPONSE_OK button inside of selected_printer_changed function.
   */
  selected_printer_changed (dialog);

  model = G_LIST_MODEL (gtk_column_view_get_model (GTK_COLUMN_VIEW (dialog->printer_list)));

  if (gtk_print_backend_printer_list_is_done (backend) &&
      gtk_printer_is_default (printer) &&
      gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model)) == GTK_INVALID_LIST_POSITION)
    set_active_printer (dialog, gtk_printer_get_name (printer));
}

static void
printer_added_cb (GListModel         *model,
                  guint               position,
                  guint               removed,
                  guint               added,
                  GtkPrintUnixDialog *dialog)
{
  guint i;

  for (i = position; i < position + added; i++)
    {
      GtkPrinter *printer = g_list_model_get_item (model, i);

      if (dialog->waiting_for_printer != NULL &&
          strcmp (gtk_printer_get_name (printer), dialog->waiting_for_printer) == 0)
        {
          gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (model), i);
          g_free (dialog->waiting_for_printer);
          dialog->waiting_for_printer = NULL;
          g_object_unref (printer);
          return;
        }
      else if (is_default_printer (dialog, printer) &&
               gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model)) == GTK_INVALID_LIST_POSITION)
        {
          gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (model), i);
          g_object_unref (printer);
          return;
        }

      g_object_unref (printer);
    }
}

static GListModel *
load_print_backends (GtkPrintUnixDialog *dialog)
{
  GList *node;
  GListStore *lists;

  lists = g_list_store_new (G_TYPE_LIST_MODEL);

  if (g_module_supported ())
    dialog->print_backends = gtk_print_backend_load_modules ();

  for (node = dialog->print_backends; node != NULL; node = node->next)
    {
      GtkPrintBackend *backend = node->data;

      g_signal_connect_object (backend, "printer-status-changed",
                               G_CALLBACK (printer_status_cb), G_OBJECT (dialog), 0);
      g_list_store_append (lists, gtk_print_backend_get_printers (backend));
    }

  return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (lists)));
}

static void
gtk_print_unix_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)

{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (object);

  switch (prop_id)
    {
    case PROP_PAGE_SETUP:
      gtk_print_unix_dialog_set_page_setup (dialog, g_value_get_object (value));
      break;
    case PROP_CURRENT_PAGE:
      gtk_print_unix_dialog_set_current_page (dialog, g_value_get_int (value));
      break;
    case PROP_PRINT_SETTINGS:
      gtk_print_unix_dialog_set_settings (dialog, g_value_get_object (value));
      break;
    case PROP_MANUAL_CAPABILITIES:
      gtk_print_unix_dialog_set_manual_capabilities (dialog, g_value_get_flags (value));
      break;
    case PROP_SUPPORT_SELECTION:
      gtk_print_unix_dialog_set_support_selection (dialog, g_value_get_boolean (value));
      break;
    case PROP_HAS_SELECTION:
      gtk_print_unix_dialog_set_has_selection (dialog, g_value_get_boolean (value));
      break;
    case PROP_EMBED_PAGE_SETUP:
      gtk_print_unix_dialog_set_embed_page_setup (dialog, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_unix_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (object);

  switch (prop_id)
    {
    case PROP_PAGE_SETUP:
      g_value_set_object (value, dialog->page_setup);
      break;
    case PROP_CURRENT_PAGE:
      g_value_set_int (value, dialog->current_page);
      break;
    case PROP_PRINT_SETTINGS:
      g_value_take_object (value, gtk_print_unix_dialog_get_settings (dialog));
      break;
    case PROP_SELECTED_PRINTER:
      g_value_set_object (value, dialog->current_printer);
      break;
    case PROP_MANUAL_CAPABILITIES:
      g_value_set_flags (value, dialog->manual_capabilities);
      break;
    case PROP_SUPPORT_SELECTION:
      g_value_set_boolean (value, dialog->support_selection);
      break;
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, dialog->has_selection);
      break;
    case PROP_EMBED_PAGE_SETUP:
      g_value_set_boolean (value, dialog->embed_page_setup);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
is_printer_active (gpointer item, gpointer data)
{
  GtkPrinter *printer = item;
  GtkPrintUnixDialog *dialog = data;
  gboolean result;

  result = gtk_printer_is_active (printer);

  if (result &&
      dialog->manual_capabilities & (GTK_PRINT_CAPABILITY_GENERATE_PDF |
                                   GTK_PRINT_CAPABILITY_GENERATE_PS))
    {
       /* Check that the printer can handle at least one of the data
        * formats that the application supports.
        */
       result = ((dialog->manual_capabilities & GTK_PRINT_CAPABILITY_GENERATE_PDF) &&
                 gtk_printer_accepts_pdf (printer)) ||
                ((dialog->manual_capabilities & GTK_PRINT_CAPABILITY_GENERATE_PS) &&
                 gtk_printer_accepts_ps (printer));
    }

  return result;
}

static int
default_printer_list_sort_func (gconstpointer a,
                                gconstpointer b,
                                gpointer      user_data)
{
  GtkPrinter *a_printer = (gpointer)a;
  GtkPrinter *b_printer = (gpointer)b;
  const char *a_name;
  const char *b_name;

  if (a_printer == NULL && b_printer == NULL)
    return 0;
  else if (a_printer == NULL)
   return 1;
  else if (b_printer == NULL)
   return -1;

  if (gtk_printer_is_virtual (a_printer) && gtk_printer_is_virtual (b_printer))
    return 0;
  else if (gtk_printer_is_virtual (a_printer) && !gtk_printer_is_virtual (b_printer))
    return -1;
  else if (!gtk_printer_is_virtual (a_printer) && gtk_printer_is_virtual (b_printer))
    return 1;

  a_name = gtk_printer_get_name (a_printer);
  b_name = gtk_printer_get_name (b_printer);

  if (a_name == NULL && b_name == NULL)
    return  0;
  else if (a_name == NULL && b_name != NULL)
    return  1;
  else if (a_name != NULL && b_name == NULL)
    return -1;

  return g_ascii_strcasecmp (a_name, b_name);
}

static GtkWidget *
wrap_in_frame (const char *label,
               GtkWidget   *child)
{
  GtkWidget *box, *label_widget;
  char *bold_text;

  label_widget = gtk_label_new (NULL);
  gtk_widget_set_halign (label_widget, GTK_ALIGN_START);
  gtk_widget_set_valign (label_widget, GTK_ALIGN_CENTER);

  bold_text = g_markup_printf_escaped ("<b>%s</b>", label);
  gtk_label_set_markup (GTK_LABEL (label_widget), bold_text);
  g_free (bold_text);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_append (GTK_BOX (box), label_widget);

  gtk_widget_set_margin_start (child, 12);
  gtk_widget_set_halign (child, GTK_ALIGN_FILL);
  gtk_widget_set_valign (child, GTK_ALIGN_FILL);

  gtk_box_append (GTK_BOX (box), child);

  return box;
}

static gboolean
setup_option (GtkPrintUnixDialog     *dialog,
              const char             *option_name,
              GtkPrinterOptionWidget *widget)
{
  GtkPrinterOption *option;

  option = gtk_printer_option_set_lookup (dialog->options, option_name);
  gtk_printer_option_widget_set_source (widget, option);

  return option != NULL;
}

static void
add_option_to_extension_point (GtkPrinterOption *option,
                               gpointer          data)
{
  GtkWidget *extension_point = data;
  GtkWidget *widget;

  widget = gtk_printer_option_widget_new (option);

  if (gtk_printer_option_widget_has_external_label (GTK_PRINTER_OPTION_WIDGET (widget)))
    {
      GtkWidget *label, *hbox;

      gtk_widget_set_valign (widget, GTK_ALIGN_BASELINE_FILL);

      label = gtk_printer_option_widget_get_external_label (GTK_PRINTER_OPTION_WIDGET (widget));
      gtk_widget_set_visible (label, TRUE);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_widget_set_valign (hbox, GTK_ALIGN_BASELINE_FILL);
      gtk_box_append (GTK_BOX (hbox), label);
      gtk_box_append (GTK_BOX (hbox), widget);

      gtk_box_append (GTK_BOX (extension_point), hbox);
    }
  else
    gtk_box_append (GTK_BOX (extension_point), widget);
}

static int
grid_rows (GtkGrid *table)
{
  int t0, t1, l, t, w, h;
  GtkWidget *c;
  gboolean first;

  t0 = t1 = 0;
  for (c = gtk_widget_get_first_child (GTK_WIDGET (table)), first = TRUE;
       c != NULL;
       c  = gtk_widget_get_next_sibling (GTK_WIDGET (c)), first = FALSE)
    {
      gtk_grid_query_child (table, c, &l, &t, &w, &h);
      if (first)
        {
          t0 = t;
          t1 = t + h;
        }
      else
        {
          if (t < t0)
            t0 = t;
          if (t + h > t1)
            t1 = t + h;
        }
    }

  return t1 - t0;
}

static void
add_option_to_table (GtkPrinterOption *option,
                     gpointer          user_data)
{
  GtkGrid *table;
  GtkWidget *label, *widget;
  guint row;

  table = GTK_GRID (user_data);

  if (g_str_has_prefix (option->name, "gtk-"))
    return;

  row = grid_rows (table);

  widget = gtk_printer_option_widget_new (option);

  if (gtk_printer_option_widget_has_external_label (GTK_PRINTER_OPTION_WIDGET (widget)))
    {
      label = gtk_printer_option_widget_get_external_label (GTK_PRINTER_OPTION_WIDGET (widget));
      gtk_widget_set_visible (label, TRUE);

      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

      gtk_grid_attach (table, label, 0, row - 1, 1, 1);
      gtk_grid_attach (table, widget, 1, row - 1, 1, 1);
    }
  else
    gtk_grid_attach (table, widget, 0, row - 1, 2, 1);
}

static void
setup_page_table (GtkPrinterOptionSet *options,
                  const char          *group,
                  GtkWidget           *table,
                  GtkWidget           *page)
{
  int nrows;

  gtk_printer_option_set_foreach_in_group (options, group,
                                           add_option_to_table,
                                           table);

  nrows = grid_rows (GTK_GRID (table));
  gtk_widget_set_visible (page, nrows > 0);
}

static void
update_print_at_option (GtkPrintUnixDialog *dialog)
{
  GtkPrinterOption *option;

  option = gtk_printer_option_set_lookup (dialog->options, "gtk-print-time");

  if (option == NULL)
    return;

  if (dialog->updating_print_at)
    return;

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->print_at_radio)))
    gtk_printer_option_set (option, "at");
  else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->print_hold_radio)))
    gtk_printer_option_set (option, "on-hold");
  else
    gtk_printer_option_set (option, "now");

  option = gtk_printer_option_set_lookup (dialog->options, "gtk-print-time-text");
  if (option != NULL)
    {
      const char *text;

      text = gtk_editable_get_text (GTK_EDITABLE (dialog->print_at_entry));
      gtk_printer_option_set (option, text);
    }
}


static gboolean
setup_print_at (GtkPrintUnixDialog *dialog)
{
  GtkPrinterOption *option;

  option = gtk_printer_option_set_lookup (dialog->options, "gtk-print-time");

  if (option == NULL)
    {
      gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->print_now_radio), TRUE);
      gtk_widget_set_sensitive (dialog->print_at_radio, FALSE);
      gtk_widget_set_sensitive (dialog->print_at_entry, FALSE);
      gtk_widget_set_sensitive (dialog->print_hold_radio, FALSE);
      gtk_editable_set_text (GTK_EDITABLE (dialog->print_at_entry), "");
      return FALSE;
    }

  dialog->updating_print_at = TRUE;

  gtk_widget_set_sensitive (dialog->print_at_entry, FALSE);
  gtk_widget_set_sensitive (dialog->print_at_radio,
                            gtk_printer_option_has_choice (option, "at"));

  gtk_widget_set_sensitive (dialog->print_hold_radio,
                            gtk_printer_option_has_choice (option, "on-hold"));

  update_print_at_option (dialog);

  if (strcmp (option->value, "at") == 0)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->print_at_radio), TRUE);
  else if (strcmp (option->value, "on-hold") == 0)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->print_hold_radio), TRUE);
  else
    gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->print_now_radio), TRUE);

  option = gtk_printer_option_set_lookup (dialog->options, "gtk-print-time-text");
  if (option != NULL)
    gtk_editable_set_text (GTK_EDITABLE (dialog->print_at_entry), option->value);

  dialog->updating_print_at = FALSE;

  return TRUE;
}

static void
update_dialog_from_settings (GtkPrintUnixDialog *dialog)
{
  GList *groups, *l;
  char *group;
  GtkWidget *table, *frame;
  gboolean has_advanced, has_job;
  guint nrows;
  GtkWidget *child;

  if (dialog->current_printer == NULL)
    {
       clear_per_printer_ui (dialog);
       gtk_widget_set_visible (dialog->job_page, FALSE);
       gtk_widget_set_visible (dialog->advanced_page, FALSE);
       gtk_widget_set_visible (dialog->image_quality_page, FALSE);
       gtk_widget_set_visible (dialog->finishing_page, FALSE);
       gtk_widget_set_visible (dialog->color_page, FALSE);
       gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);

       return;
    }

  setup_option (dialog, "gtk-n-up", dialog->pages_per_sheet);
  setup_option (dialog, "gtk-n-up-layout", dialog->number_up_layout);
  setup_option (dialog, "gtk-duplex", dialog->duplex);
  setup_option (dialog, "gtk-paper-type", dialog->paper_type);
  setup_option (dialog, "gtk-paper-source", dialog->paper_source);
  setup_option (dialog, "gtk-output-tray", dialog->output_tray);

  has_job = FALSE;
  has_job |= setup_option (dialog, "gtk-job-prio", dialog->job_prio);
  has_job |= setup_option (dialog, "gtk-billing-info", dialog->billing_info);
  has_job |= setup_option (dialog, "gtk-cover-before", dialog->cover_before);
  has_job |= setup_option (dialog, "gtk-cover-after", dialog->cover_after);
  has_job |= setup_print_at (dialog);

  gtk_widget_set_visible (dialog->job_page, has_job);

  setup_page_table (dialog->options,
                    "ImageQualityPage",
                    dialog->image_quality_table,
                    dialog->image_quality_page);

  setup_page_table (dialog->options,
                    "FinishingPage",
                    dialog->finishing_table,
                    dialog->finishing_page);

  setup_page_table (dialog->options,
                    "ColorPage",
                    dialog->color_table,
                    dialog->color_page);

  gtk_printer_option_set_foreach_in_group (dialog->options,
                                           "GtkPrintDialogExtension",
                                           add_option_to_extension_point,
                                           dialog->extension_point);

  /* A bit of a hack, keep the last option flush right.
   * This keeps the file format radios from moving as the
   * filename changes.
   */
  child = gtk_widget_get_last_child (dialog->extension_point);
  if (child && child != gtk_widget_get_first_child (dialog->extension_point))
    gtk_widget_set_halign (child, GTK_ALIGN_END);

  /* Put the rest of the groups in the advanced page */
  groups = gtk_printer_option_set_get_groups (dialog->options);

  has_advanced = FALSE;
  for (l = groups; l != NULL; l = l->next)
    {
      group = l->data;

      if (group == NULL)
        continue;

      if (strcmp (group, "ImageQualityPage") == 0 ||
          strcmp (group, "ColorPage") == 0 ||
          strcmp (group, "FinishingPage") == 0 ||
          strcmp (group, "GtkPrintDialogExtension") == 0)
        continue;

      table = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (table), 6);
      gtk_grid_set_column_spacing (GTK_GRID (table), 12);

      gtk_printer_option_set_foreach_in_group (dialog->options,
                                               group,
                                               add_option_to_table,
                                               table);

      nrows = grid_rows (GTK_GRID (table));
      if (nrows == 0)
        {
          g_object_unref (g_object_ref_sink (table));
        }
      else
        {
          has_advanced = TRUE;
          frame = wrap_in_frame (group, table);
          gtk_box_append (GTK_BOX (dialog->advanced_vbox), frame);
        }
    }

  gtk_widget_set_visible (dialog->advanced_page, has_advanced);

  g_list_free_full (groups, g_free);
}

static void
update_dialog_from_capabilities (GtkPrintUnixDialog *dialog)
{
  GtkPrintCapabilities caps;
  gboolean can_collate;
  const char *copies;
  GtkWidget *button;

  copies = gtk_editable_get_text (GTK_EDITABLE (dialog->copies_spin));
  can_collate = (*copies != '\0' && atoi (copies) > 1);

  caps = dialog->manual_capabilities | dialog->printer_capabilities;

  gtk_widget_set_sensitive (dialog->page_set_combo,
                            caps & GTK_PRINT_CAPABILITY_PAGE_SET);
  gtk_widget_set_sensitive (dialog->copies_spin,
                            caps & GTK_PRINT_CAPABILITY_COPIES);
  gtk_widget_set_sensitive (dialog->collate_check,
                            can_collate &&
                            (caps & GTK_PRINT_CAPABILITY_COLLATE));
  gtk_widget_set_sensitive (dialog->reverse_check,
                            caps & GTK_PRINT_CAPABILITY_REVERSE);
  gtk_widget_set_sensitive (dialog->scale_spin,
                            caps & GTK_PRINT_CAPABILITY_SCALE);
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->pages_per_sheet),
                            caps & GTK_PRINT_CAPABILITY_NUMBER_UP);

  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_APPLY);
  gtk_widget_set_visible (button, (caps & GTK_PRINT_CAPABILITY_PREVIEW) != 0);

  update_collate_icon (NULL, dialog);
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
set_paper_size (GtkPrintUnixDialog *dialog,
                GtkPageSetup       *page_setup,
                gboolean            size_only,
                gboolean            add_item)
{
  GListModel *model;
  GtkPageSetup *list_page_setup;
  guint i;

  if (!dialog->internal_page_setup_change)
    return TRUE;

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
          gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->orientation_combo),
                                      gtk_page_setup_get_orientation (page_setup));
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
      gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->orientation_combo),
                                  gtk_page_setup_get_orientation (page_setup));
      return TRUE;
    }

  return FALSE;
}

static void
fill_custom_paper_sizes (GtkPrintUnixDialog *dialog)
{
  g_list_store_remove_all (dialog->custom_paper_list);
  gtk_print_load_custom_papers (dialog->custom_paper_list);
}

static void
fill_paper_sizes (GtkPrintUnixDialog *dialog,
                  GtkPrinter         *printer)
{
  GList *list, *l;
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;
  int i;

  g_list_store_remove_all (dialog->page_setup_list);

  if (printer == NULL || (list = gtk_printer_list_papers (printer)) == NULL)
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
      for (l = list; l != NULL; l = l->next)
        {
          page_setup = l->data;
          g_list_store_append (dialog->page_setup_list, page_setup);
          g_object_unref (page_setup);
        }
      g_list_free (list);
    }
}

static void
update_paper_sizes (GtkPrintUnixDialog *dialog)
{
  GtkPageSetup *current_page_setup = NULL;
  GtkPrinter   *printer;

  printer = gtk_print_unix_dialog_get_selected_printer (dialog);

  fill_paper_sizes (dialog, printer);
  fill_custom_paper_sizes (dialog);

  current_page_setup = gtk_page_setup_copy (gtk_print_unix_dialog_get_page_setup (dialog));

  if (current_page_setup)
    {
      if (!set_paper_size (dialog, current_page_setup, FALSE, FALSE))
        set_paper_size (dialog, current_page_setup, TRUE, TRUE);

      g_object_unref (current_page_setup);
    }
}

static void
mark_conflicts (GtkPrintUnixDialog *dialog)
{
  GtkPrinter *printer;
  gboolean have_conflict;

  have_conflict = FALSE;

  printer = dialog->current_printer;

  if (printer)
    {
      g_signal_handler_block (dialog->options, dialog->options_changed_handler);

      gtk_printer_option_set_clear_conflicts (dialog->options);
      have_conflict = _gtk_printer_mark_conflicts (printer, dialog->options);

      g_signal_handler_unblock (dialog->options, dialog->options_changed_handler);
    }

  gtk_widget_set_visible (dialog->conflicts_widget, have_conflict);
}

static gboolean
mark_conflicts_callback (gpointer data)
{
  GtkPrintUnixDialog *dialog = data;

  dialog->mark_conflicts_id = 0;

  mark_conflicts (dialog);

  return FALSE;
}

static void
unschedule_idle_mark_conflicts (GtkPrintUnixDialog *dialog)
{
  if (dialog->mark_conflicts_id != 0)
    {
      g_source_remove (dialog->mark_conflicts_id);
      dialog->mark_conflicts_id = 0;
    }
}

static void
schedule_idle_mark_conflicts (GtkPrintUnixDialog *dialog)
{
  if (dialog->mark_conflicts_id != 0)
    return;

  dialog->mark_conflicts_id = g_idle_add (mark_conflicts_callback, dialog);
  g_source_set_static_name (g_main_context_find_source_by_id (NULL, dialog->mark_conflicts_id),
                            "[gtk] mark_conflicts_callback");
}

static void
options_changed_cb (GtkPrintUnixDialog *dialog)
{
  schedule_idle_mark_conflicts (dialog);

  g_free (dialog->waiting_for_printer);
  dialog->waiting_for_printer = NULL;
}

static void
clear_per_printer_ui (GtkPrintUnixDialog *dialog)
{
  GtkWidget *child;

  if (dialog->finishing_table == NULL)
    return;

  while ((child = gtk_widget_get_first_child (dialog->finishing_table)))
    gtk_grid_remove (GTK_GRID (dialog->finishing_table), child);
  while ((child = gtk_widget_get_first_child (dialog->image_quality_table)))
    gtk_grid_remove (GTK_GRID (dialog->image_quality_table), child);
  while ((child = gtk_widget_get_first_child (dialog->color_table)))
    gtk_grid_remove (GTK_GRID (dialog->color_table), child);
  while ((child = gtk_widget_get_first_child (dialog->advanced_vbox)))
    gtk_box_remove (GTK_BOX (dialog->advanced_vbox), child);
  while ((child = gtk_widget_get_first_child (dialog->extension_point)))
    gtk_box_remove (GTK_BOX (dialog->extension_point), child);
}

static void
printer_details_acquired (GtkPrinter         *printer,
                          gboolean            success,
                          GtkPrintUnixDialog *dialog)
{
  disconnect_printer_details_request (dialog, !success);

  if (success)
    selected_printer_changed (dialog);
}

static void
selected_printer_changed (GtkPrintUnixDialog *dialog)
{
  GListModel *model = G_LIST_MODEL (gtk_column_view_get_model (GTK_COLUMN_VIEW (dialog->printer_list)));
  GtkPrinter *printer;

  /* Whenever the user selects a printer we stop looking for
   * the printer specified in the initial settings
   */
  if (dialog->waiting_for_printer &&
      !dialog->internal_printer_change)
    {
      g_free (dialog->waiting_for_printer);
      dialog->waiting_for_printer = NULL;
    }

  disconnect_printer_details_request (dialog, FALSE);

  printer = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (model));

  /* sets GTK_RESPONSE_OK button sensitivity depending on whether the printer
   * accepts/rejects jobs
   */
  if (printer != NULL)
    {
      if (!gtk_printer_is_accepting_jobs (printer))
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
      else if (dialog->current_printer == printer && gtk_printer_has_details (printer))
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
    }

  if (printer != NULL && !gtk_printer_has_details (printer))
    {
      gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
      dialog->request_details_tag = g_signal_connect (printer, "details-acquired",
                                                      G_CALLBACK (printer_details_acquired), dialog);

      dialog->request_details_printer = g_object_ref (printer);
      set_busy_cursor (dialog, TRUE);
      gtk_printer_set_state_message (printer, _("Getting printer information…"));
      gtk_printer_request_details (printer);
      return;
    }

  if (printer == dialog->current_printer)
    return;

  if (dialog->options)
    {
      g_clear_object (&dialog->options);
      clear_per_printer_ui (dialog);
    }

  g_clear_object (&dialog->current_printer);
  dialog->printer_capabilities = 0;

  if (printer != NULL && gtk_printer_is_accepting_jobs (printer))
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
  dialog->current_printer = g_object_ref (printer);

  if (printer != NULL)
    {
      if (!dialog->page_setup_set)
        {
          /* if no explicit page setup has been set, use the printer default */
          GtkPageSetup *page_setup;

          page_setup = gtk_printer_get_default_page_size (printer);

          if (!page_setup)
            page_setup = gtk_page_setup_new ();

          if (page_setup && dialog->page_setup)
            gtk_page_setup_set_orientation (page_setup, gtk_page_setup_get_orientation (dialog->page_setup));

          g_clear_object (&dialog->page_setup);
          dialog->page_setup = page_setup; /* transfer ownership */
        }

      dialog->printer_capabilities = gtk_printer_get_capabilities (printer);
      dialog->options = _gtk_printer_get_options (printer,
                                                dialog->initial_settings,
                                                dialog->page_setup,
                                                dialog->manual_capabilities);

      dialog->options_changed_handler =
        g_signal_connect_swapped (dialog->options, "changed", G_CALLBACK (options_changed_cb), dialog);
      schedule_idle_mark_conflicts (dialog);
    }

  update_dialog_from_settings (dialog);
  update_dialog_from_capabilities (dialog);

  dialog->internal_page_setup_change = TRUE;
  update_paper_sizes (dialog);
  dialog->internal_page_setup_change = FALSE;

  g_object_notify (G_OBJECT (dialog), "selected-printer");
}

static void
update_collate_icon (GtkToggleButton    *toggle_button,
                     GtkPrintUnixDialog *dialog)
{
  gboolean collate;
  gboolean reverse;
  int copies;

  collate = dialog_get_collate (dialog);
  reverse = dialog_get_reverse (dialog);
  copies = dialog_get_n_copies (dialog);

  if (collate)
    {
      gtk_page_thumbnail_set_page_num (GTK_PAGE_THUMBNAIL (dialog->page_a1), reverse ? 1 : 2);
      gtk_page_thumbnail_set_page_num (GTK_PAGE_THUMBNAIL (dialog->page_a2), reverse ? 2 : 1);
      gtk_page_thumbnail_set_page_num (GTK_PAGE_THUMBNAIL (dialog->page_b1), reverse ? 1 : 2);
      gtk_page_thumbnail_set_page_num (GTK_PAGE_THUMBNAIL (dialog->page_b2), reverse ? 2 : 1);
    }
  else
    {
      gtk_page_thumbnail_set_page_num (GTK_PAGE_THUMBNAIL (dialog->page_a1), reverse ? 2 : 1);
      gtk_page_thumbnail_set_page_num (GTK_PAGE_THUMBNAIL (dialog->page_a2), reverse ? 2 : 1);
      gtk_page_thumbnail_set_page_num (GTK_PAGE_THUMBNAIL (dialog->page_b1), reverse ? 1 : 2);
      gtk_page_thumbnail_set_page_num (GTK_PAGE_THUMBNAIL (dialog->page_b2), reverse ? 1 : 2);
    }

  gtk_widget_set_visible (dialog->page_b1, copies > 1);
  gtk_widget_set_visible (dialog->page_b2, copies > 1);
}

static gboolean
page_range_entry_focus_changed (GtkWidget          *entry,
                                GParamSpec         *pspec,
                                GtkPrintUnixDialog *dialog)
{
  if (gtk_widget_has_focus (entry))
    gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->page_range_radio), TRUE);

  return FALSE;
}

static void
update_page_range_entry_sensitivity (GtkWidget *button,
				     GtkPrintUnixDialog *dialog)
{
  gboolean active;

  active = gtk_check_button_get_active (GTK_CHECK_BUTTON (button));

  if (active)
    gtk_widget_grab_focus (dialog->page_range_entry);
}

static void
update_print_at_entry_sensitivity (GtkWidget *button,
				   GtkPrintUnixDialog *dialog)
{
  gboolean active;

  active = gtk_check_button_get_active (GTK_CHECK_BUTTON (button));

  gtk_widget_set_sensitive (dialog->print_at_entry, active);

  if (active)
    gtk_widget_grab_focus (dialog->print_at_entry);
}

static gboolean
is_range_separator (char c)
{
  return (c == ',' || c == ';' || c == ':');
}

static GtkPageRange *
dialog_get_page_ranges (GtkPrintUnixDialog *dialog,
                        int                *n_ranges_out)
{
  int i, n_ranges;
  const char *text, *p;
  char *next;
  GtkPageRange *ranges;
  int start, end;

  text = gtk_editable_get_text (GTK_EDITABLE (dialog->page_range_entry));

  if (*text == 0)
    {
      *n_ranges_out = 0;
      return NULL;
    }

  n_ranges = 1;
  p = text;
  while (*p)
    {
      if (is_range_separator (*p))
        n_ranges++;
      p++;
    }

  ranges = g_new0 (GtkPageRange, n_ranges);

  i = 0;
  p = text;
  while (*p)
    {
      while (isspace (*p)) p++;

      if (*p == '-')
        {
          /* a half-open range like -2 */
          start = 1;
        }
      else
        {
          start = (int)strtol (p, &next, 10);
          if (start < 1)
            start = 1;
          p = next;
        }

      end = start;

      while (isspace (*p)) p++;

      if (*p == '-')
        {
          p++;
          end = (int)strtol (p, &next, 10);
          if (next == p) /* a half-open range like 2- */
            end = 0;
          else if (end < start)
            end = start;
        }

      ranges[i].start = start - 1;
      ranges[i].end = end - 1;
      i++;

      /* Skip until end or separator */
      while (*p && !is_range_separator (*p))
        p++;

      /* if not at end, skip separator */
      if (*p)
        p++;
    }

  *n_ranges_out = i;

  return ranges;
}

static void
dialog_set_page_ranges (GtkPrintUnixDialog *dialog,
                        GtkPageRange       *ranges,
                        int                 n_ranges)
{
  int i;
  GString *s = g_string_new (NULL);

  for (i = 0; i < n_ranges; i++)
    {
      g_string_append_printf (s, "%d", ranges[i].start + 1);
      if (ranges[i].end > ranges[i].start)
        g_string_append_printf (s, "-%d", ranges[i].end + 1);
      else if (ranges[i].end == -1)
        g_string_append (s, "-");

      if (i != n_ranges - 1)
        g_string_append (s, ",");
    }

  gtk_editable_set_text (GTK_EDITABLE (dialog->page_range_entry), s->str);

  g_string_free (s, TRUE);
}

static GtkPrintPages
dialog_get_print_pages (GtkPrintUnixDialog *dialog)
{
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->all_pages_radio)))
    return GTK_PRINT_PAGES_ALL;
  else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->current_page_radio)))
    return GTK_PRINT_PAGES_CURRENT;
  else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->selection_radio)))
    return GTK_PRINT_PAGES_SELECTION;
  else
    return GTK_PRINT_PAGES_RANGES;
}

static void
dialog_set_print_pages (GtkPrintUnixDialog *dialog,
                        GtkPrintPages       pages)
{
  if (pages == GTK_PRINT_PAGES_RANGES)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->page_range_radio), TRUE);
  else if (pages == GTK_PRINT_PAGES_CURRENT)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->current_page_radio), TRUE);
  else if (pages == GTK_PRINT_PAGES_SELECTION)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->selection_radio), TRUE);
  else
    gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->all_pages_radio), TRUE);
}

static double
dialog_get_scale (GtkPrintUnixDialog *dialog)
{
  if (gtk_widget_is_sensitive (dialog->scale_spin))
    return gtk_spin_button_get_value (GTK_SPIN_BUTTON (dialog->scale_spin));
  else
    return 100.0;
}

static void
dialog_set_scale (GtkPrintUnixDialog *dialog,
                  double              val)
{
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->scale_spin), val);
}

static GtkPageSet
dialog_get_page_set (GtkPrintUnixDialog *dialog)
{
  if (gtk_widget_is_sensitive (dialog->page_set_combo))
    return (GtkPageSet)gtk_drop_down_get_selected (GTK_DROP_DOWN (dialog->page_set_combo));
  else
    return GTK_PAGE_SET_ALL;
}

static void
dialog_set_page_set (GtkPrintUnixDialog *dialog,
                     GtkPageSet          val)
{
  gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->page_set_combo), (guint)val);
}

static int
dialog_get_n_copies (GtkPrintUnixDialog *dialog)
{
  GtkAdjustment *adjustment;
  const char *text;
  char *endptr = NULL;
  int n_copies;

  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (dialog->copies_spin));

  text = gtk_editable_get_text (GTK_EDITABLE (dialog->copies_spin));
  n_copies = g_ascii_strtoull (text, &endptr, 0);

  if (gtk_widget_is_sensitive (dialog->copies_spin))
    {
      if (n_copies != 0 && endptr != text && (endptr != NULL && endptr[0] == '\0') &&
          n_copies >= gtk_adjustment_get_lower (adjustment) &&
          n_copies <= gtk_adjustment_get_upper (adjustment))
        {
          return n_copies;
        }

      return gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (dialog->copies_spin));
    }

  return 1;
}

static void
dialog_set_n_copies (GtkPrintUnixDialog *dialog,
                     int                 n_copies)
{
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->copies_spin), n_copies);
}

static gboolean
dialog_get_collate (GtkPrintUnixDialog *dialog)
{
  if (gtk_widget_is_sensitive (dialog->collate_check))
    return gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->collate_check));
  return TRUE;
}

static void
dialog_set_collate (GtkPrintUnixDialog *dialog,
                    gboolean            collate)
{
  gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->collate_check), collate);
}

static gboolean
dialog_get_reverse (GtkPrintUnixDialog *dialog)
{
  if (gtk_widget_is_sensitive (dialog->reverse_check))
    return gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->reverse_check));
  return FALSE;
}

static void
dialog_set_reverse (GtkPrintUnixDialog *dialog,
                    gboolean            reverse)
{
  gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->reverse_check), reverse);
}

static int
dialog_get_pages_per_sheet (GtkPrintUnixDialog *dialog)
{
  const char *val;
  int num;

  val = gtk_printer_option_widget_get_value (dialog->pages_per_sheet);

  num = 1;

  if (val)
    {
      num = atoi(val);
      if (num < 1)
        num = 1;
    }

  return num;
}

static GtkNumberUpLayout
dialog_get_number_up_layout (GtkPrintUnixDialog *dialog)
{
  GtkPrintCapabilities       caps;
  GtkNumberUpLayout          layout;
  const char                *val;
  GEnumClass                *enum_class;
  GEnumValue                *enum_value;

  val = gtk_printer_option_widget_get_value (dialog->number_up_layout);

  caps = dialog->manual_capabilities | dialog->printer_capabilities;

  if ((caps & GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT) == 0)
    return GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;

  if (gtk_widget_get_direction (GTK_WIDGET (dialog)) == GTK_TEXT_DIR_LTR)
    layout = GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;
  else
    layout = GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM;

  if (val == NULL)
    return layout;

  if (val[0] == '\0' && dialog->options)
    {
      GtkPrinterOption *option = gtk_printer_option_set_lookup (dialog->options, "gtk-n-up-layout");
      if (option)
        val = option->value;
    }

  enum_class = g_type_class_ref (GTK_TYPE_NUMBER_UP_LAYOUT);
  enum_value = g_enum_get_value_by_nick (enum_class, val);
  if (enum_value)
    layout = enum_value->value;
  g_type_class_unref (enum_class);

  return layout;
}

static void
draw_page (GtkDrawingArea *da,
           cairo_t        *cr,
           int             width,
           int             height,
           gpointer        data)
{
  GtkWidget *widget = GTK_WIDGET (da);
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (data);
  GtkCssStyle *style;
  double ratio;
  int w, h, tmp;
  int pages_x, pages_y, i, x, y, layout_w, layout_h;
  double page_width, page_height;
  GtkPageOrientation orientation;
  gboolean landscape;
  PangoLayout *layout;
  PangoFontDescription *font;
  char *text;
  GdkRGBA color;
  GtkNumberUpLayout number_up_layout;
  int start_x, end_x, start_y, end_y;
  int dx, dy;
  gboolean horizontal;
  GtkPageSetup *page_setup;
  double paper_width, paper_height;
  double pos_x, pos_y;
  int pages_per_sheet;
  gboolean ltr = TRUE;
  GtkSnapshot *snapshot;
  GtkCssBoxes boxes;
  GskRenderNode *node;

  orientation = gtk_page_setup_get_orientation (dialog->page_setup);
  landscape =
    (orientation == GTK_PAGE_ORIENTATION_LANDSCAPE) ||
    (orientation == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE);

  number_up_layout = dialog_get_number_up_layout (dialog);

  cairo_save (cr);

  page_setup = gtk_print_unix_dialog_get_page_setup (dialog);

  if (page_setup != NULL)
    {
      if (!landscape)
        {
          paper_width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM);
          paper_height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM);
        }
      else
        {
          paper_width = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM);
          paper_height = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM);
        }

      if (paper_width < paper_height)
        {
          h = EXAMPLE_PAGE_AREA_SIZE - 3;
          w = (paper_height != 0) ? h * paper_width / paper_height : 0;
        }
      else
        {
          w = EXAMPLE_PAGE_AREA_SIZE - 3;
          h = (paper_width != 0) ? w * paper_height / paper_width : 0;
        }

      if (paper_width == 0)
        w = 0;

      if (paper_height == 0)
        h = 0;
    }
  else
    {
      ratio = G_SQRT2;
      w = (EXAMPLE_PAGE_AREA_SIZE - 3) / ratio;
      h = EXAMPLE_PAGE_AREA_SIZE - 3;
    }

  pages_per_sheet = dialog_get_pages_per_sheet (dialog);
  switch (pages_per_sheet)
    {
    default:
    case 1:
      pages_x = 1; pages_y = 1;
      break;
    case 2:
      landscape = !landscape;
      pages_x = 1; pages_y = 2;
      break;
    case 4:
      pages_x = 2; pages_y = 2;
      break;
    case 6:
      landscape = !landscape;
      pages_x = 2; pages_y = 3;
      break;
    case 9:
      pages_x = 3; pages_y = 3;
      break;
    case 16:
      pages_x = 4; pages_y = 4;
      break;
    }

  if (landscape)
    {
      tmp = w;
      w = h;
      h = tmp;

      tmp = pages_x;
      pages_x = pages_y;
      pages_y = tmp;
    }

  style = gtk_css_node_get_style (dialog->page_layout_paper_node);
  color = *gtk_css_color_value_get_rgba (style->core->color);

  pos_x = (width - w) / 2;
  pos_y = (height - h) / 2 - 10;
  cairo_translate (cr, pos_x, pos_y);

  snapshot = gtk_snapshot_new ();
  gtk_css_boxes_init_border_box (&boxes, style, 1, 1, w, h);
  gtk_css_style_snapshot_background (&boxes, snapshot);
  gtk_css_style_snapshot_border (&boxes, snapshot);

  node = gtk_snapshot_free_to_node (snapshot);
  if (node)
    {
      gsk_render_node_draw (node, cr);
      gsk_render_node_unref (node);
    }

  cairo_set_line_width (cr, 1.0);

  i = 1;

  page_width = (double)w / pages_x;
  page_height = (double)h / pages_y;

  layout  = pango_cairo_create_layout (cr);

  font = pango_font_description_new ();
  pango_font_description_set_family (font, "sans");

  if (page_height > 0)
    pango_font_description_set_absolute_size (font, page_height * 0.4 * PANGO_SCALE);
  else
    pango_font_description_set_absolute_size (font, 1);

  pango_layout_set_font_description (layout, font);
  pango_font_description_free (font);

  pango_layout_set_width (layout, page_width * PANGO_SCALE);
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

  switch (number_up_layout)
    {
      default:
      case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM:
        start_x = 0;
        end_x = pages_x - 1;
        start_y = 0;
        end_y = pages_y - 1;
        dx = 1;
        dy = 1;
        horizontal = TRUE;
        break;
      case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP:
        start_x = 0;
        end_x = pages_x - 1;
        start_y = pages_y - 1;
        end_y = 0;
        dx = 1;
        dy = - 1;
        horizontal = TRUE;
        break;
      case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM:
        start_x = pages_x - 1;
        end_x = 0;
        start_y = 0;
        end_y = pages_y - 1;
        dx = - 1;
        dy = 1;
        horizontal = TRUE;
        break;
      case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP:
        start_x = pages_x - 1;
        end_x = 0;
        start_y = pages_y - 1;
        end_y = 0;
        dx = - 1;
        dy = - 1;
        horizontal = TRUE;
        break;
      case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT:
        start_x = 0;
        end_x = pages_x - 1;
        start_y = 0;
        end_y = pages_y - 1;
        dx = 1;
        dy = 1;
        horizontal = FALSE;
        break;
      case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT:
        start_x = pages_x - 1;
        end_x = 0;
        start_y = 0;
        end_y = pages_y - 1;
        dx = - 1;
        dy = 1;
        horizontal = FALSE;
        break;
      case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT:
        start_x = 0;
        end_x = pages_x - 1;
        start_y = pages_y - 1;
        end_y = 0;
        dx = 1;
        dy = - 1;
        horizontal = FALSE;
        break;
      case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT:
        start_x = pages_x - 1;
        end_x = 0;
        start_y = pages_y - 1;
        end_y = 0;
        dx = - 1;
        dy = - 1;
        horizontal = FALSE;
        break;
    }

  gdk_cairo_set_source_rgba (cr, &color);
  if (horizontal)
    for (y = start_y; y != end_y + dy; y += dy)
      {
        for (x = start_x; x != end_x + dx; x += dx)
          {
            text = g_strdup_printf ("%d", i++);
            pango_layout_set_text (layout, text, -1);
            g_free (text);
            pango_layout_get_size (layout, &layout_w, &layout_h);
            cairo_save (cr);
            cairo_translate (cr,
                             x * page_width,
                             y * page_height + (page_height - layout_h / 1024.0) / 2);

            pango_cairo_show_layout (cr, layout);
            cairo_restore (cr);
          }
      }
  else
    for (x = start_x; x != end_x + dx; x += dx)
      {
        for (y = start_y; y != end_y + dy; y += dy)
          {
            text = g_strdup_printf ("%d", i++);
            pango_layout_set_text (layout, text, -1);
            g_free (text);
            pango_layout_get_size (layout, &layout_w, &layout_h);
            cairo_save (cr);
            cairo_translate (cr,
                             x * page_width,
                             y * page_height + (page_height - layout_h / 1024.0) / 2);

            pango_cairo_show_layout (cr, layout);
            cairo_restore (cr);
          }
      }

  g_object_unref (layout);

  gtk_widget_get_color (widget, &color);

  if (page_setup != NULL)
    {
      PangoContext *pango_c = NULL;
      PangoFontDescription *pango_f = NULL;
      int font_size = 12 * PANGO_SCALE;

      pos_x += 1;
      pos_y += 1;

      if (pages_per_sheet == 2 || pages_per_sheet == 6)
        {
          paper_width = gtk_page_setup_get_paper_height (page_setup, _gtk_print_get_default_user_units ());
          paper_height = gtk_page_setup_get_paper_width (page_setup, _gtk_print_get_default_user_units ());
        }
      else
        {
          paper_width = gtk_page_setup_get_paper_width (page_setup, _gtk_print_get_default_user_units ());
          paper_height = gtk_page_setup_get_paper_height (page_setup, _gtk_print_get_default_user_units ());
        }

      cairo_restore (cr);
      cairo_save (cr);

      layout = pango_cairo_create_layout (cr);

      font = pango_font_description_new ();
      pango_font_description_set_family (font, "sans");

      pango_c = gtk_widget_get_pango_context (widget);
      if (pango_c != NULL)
        {
          pango_f = pango_context_get_font_description (pango_c);
          if (pango_f != NULL)
            font_size = pango_font_description_get_size (pango_f);
        }

      pango_font_description_set_size (font, font_size);
      pango_layout_set_font_description (layout, font);
      pango_font_description_free (font);

      pango_layout_set_width (layout, -1);
      pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

      if (_gtk_print_get_default_user_units () == GTK_UNIT_MM)
        text = g_strdup_printf ("%.1f mm", paper_height);
      else
        text = g_strdup_printf ("%.2f inch", paper_height);

      pango_layout_set_text (layout, text, -1);
      g_free (text);
      pango_layout_get_size (layout, &layout_w, &layout_h);

      ltr = gtk_widget_get_direction (GTK_WIDGET (dialog)) == GTK_TEXT_DIR_LTR;

      if (ltr)
        cairo_translate (cr, pos_x - layout_w / PANGO_SCALE - 2 * RULER_DISTANCE,
                             (height - layout_h / PANGO_SCALE) / 2);
      else
        cairo_translate (cr, pos_x + w + 2 * RULER_DISTANCE,
                             (height - layout_h / PANGO_SCALE) / 2);

      gdk_cairo_set_source_rgba (cr, &color);
      pango_cairo_show_layout (cr, layout);

      cairo_restore (cr);
      cairo_save (cr);

      if (_gtk_print_get_default_user_units () == GTK_UNIT_MM)
        text = g_strdup_printf ("%.1f mm", paper_width);
      else
        text = g_strdup_printf ("%.2f inch", paper_width);

      pango_layout_set_text (layout, text, -1);
      g_free (text);
      pango_layout_get_size (layout, &layout_w, &layout_h);

      cairo_translate (cr, (width - layout_w / PANGO_SCALE) / 2,
                           pos_y + h + 2 * RULER_DISTANCE);

      gdk_cairo_set_source_rgba (cr, &color);
      pango_cairo_show_layout (cr, layout);

      g_object_unref (layout);

      cairo_restore (cr);

      cairo_set_line_width (cr, 1);

      gdk_cairo_set_source_rgba (cr, &color);

      if (ltr)
        {
          cairo_move_to (cr, pos_x - RULER_DISTANCE, pos_y);
          cairo_line_to (cr, pos_x - RULER_DISTANCE, pos_y + h);
          cairo_stroke (cr);

          cairo_move_to (cr, pos_x - RULER_DISTANCE - RULER_RADIUS, pos_y - 0.5);
          cairo_line_to (cr, pos_x - RULER_DISTANCE + RULER_RADIUS, pos_y - 0.5);
          cairo_stroke (cr);

          cairo_move_to (cr, pos_x - RULER_DISTANCE - RULER_RADIUS, pos_y + h + 0.5);
          cairo_line_to (cr, pos_x - RULER_DISTANCE + RULER_RADIUS, pos_y + h + 0.5);
          cairo_stroke (cr);
        }
      else
        {
          cairo_move_to (cr, pos_x + w + RULER_DISTANCE, pos_y);
          cairo_line_to (cr, pos_x + w + RULER_DISTANCE, pos_y + h);
          cairo_stroke (cr);

          cairo_move_to (cr, pos_x + w + RULER_DISTANCE - RULER_RADIUS, pos_y - 0.5);
          cairo_line_to (cr, pos_x + w + RULER_DISTANCE + RULER_RADIUS, pos_y - 0.5);
          cairo_stroke (cr);

          cairo_move_to (cr, pos_x + w + RULER_DISTANCE - RULER_RADIUS, pos_y + h + 0.5);
          cairo_line_to (cr, pos_x + w + RULER_DISTANCE + RULER_RADIUS, pos_y + h + 0.5);
          cairo_stroke (cr);
        }

      cairo_move_to (cr, pos_x, pos_y + h + RULER_DISTANCE);
      cairo_line_to (cr, pos_x + w, pos_y + h + RULER_DISTANCE);
      cairo_stroke (cr);

      cairo_move_to (cr, pos_x - 0.5, pos_y + h + RULER_DISTANCE - RULER_RADIUS);
      cairo_line_to (cr, pos_x - 0.5, pos_y + h + RULER_DISTANCE + RULER_RADIUS);
      cairo_stroke (cr);

      cairo_move_to (cr, pos_x + w + 0.5, pos_y + h + RULER_DISTANCE - RULER_RADIUS);
      cairo_line_to (cr, pos_x + w + 0.5, pos_y + h + RULER_DISTANCE + RULER_RADIUS);
      cairo_stroke (cr);
    }
}

static void
redraw_page_layout_preview (GtkPrintUnixDialog *dialog)
{
  if (dialog->page_layout_preview)
    gtk_widget_queue_draw (dialog->page_layout_preview);
}

static void
update_number_up_layout (GtkPrintUnixDialog *dialog)
{
  GtkPrintCapabilities       caps;
  GtkPrinterOptionSet       *set;
  GtkNumberUpLayout          layout;
  GtkPrinterOption          *option;
  GtkPrinterOption          *old_option;
  GtkPageOrientation         page_orientation;

  set = dialog->options;

  caps = dialog->manual_capabilities | dialog->printer_capabilities;

  if (caps & GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT)
    {
      if (dialog->number_up_layout_n_option == NULL)
        {
          dialog->number_up_layout_n_option = gtk_printer_option_set_lookup (set, "gtk-n-up-layout");
          if (dialog->number_up_layout_n_option == NULL)
            {
              const char *n_up_layout[] = { "lrtb", "lrbt", "rltb", "rlbt", "tblr", "tbrl", "btlr", "btrl" };
               /* Translators: These strings name the possible arrangements of
                * multiple pages on a sheet when printing (same as in gtkprintbackendcups.c)
                */
              const char *n_up_layout_display[] = { N_("Left to right, top to bottom"), N_("Left to right, bottom to top"),
                                                    N_("Right to left, top to bottom"), N_("Right to left, bottom to top"),
                                                    N_("Top to bottom, left to right"), N_("Top to bottom, right to left"),
                                                    N_("Bottom to top, left to right"), N_("Bottom to top, right to left") };
              int i;

              dialog->number_up_layout_n_option = gtk_printer_option_new ("gtk-n-up-layout",
                                                                        _("Page Ordering"),
                                                                        GTK_PRINTER_OPTION_TYPE_PICKONE);
              gtk_printer_option_allocate_choices (dialog->number_up_layout_n_option, 8);

              for (i = 0; i < G_N_ELEMENTS (n_up_layout_display); i++)
                {
                  dialog->number_up_layout_n_option->choices[i] = g_strdup (n_up_layout[i]);
                  dialog->number_up_layout_n_option->choices_display[i] = g_strdup (_(n_up_layout_display[i]));
                }
            }
          g_object_ref (dialog->number_up_layout_n_option);

          dialog->number_up_layout_2_option = gtk_printer_option_new ("gtk-n-up-layout",
                                                                    _("Page Ordering"),
                                                                    GTK_PRINTER_OPTION_TYPE_PICKONE);
          gtk_printer_option_allocate_choices (dialog->number_up_layout_2_option, 2);
        }

      page_orientation = gtk_page_setup_get_orientation (dialog->page_setup);
      if (page_orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
          page_orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
        {
          if (! (dialog->number_up_layout_2_option->choices[0] == dialog->number_up_layout_n_option->choices[0] &&
                 dialog->number_up_layout_2_option->choices[1] == dialog->number_up_layout_n_option->choices[2]))
            {
              g_free (dialog->number_up_layout_2_option->choices_display[0]);
              g_free (dialog->number_up_layout_2_option->choices_display[1]);
              dialog->number_up_layout_2_option->choices[0] = dialog->number_up_layout_n_option->choices[0];
              dialog->number_up_layout_2_option->choices[1] = dialog->number_up_layout_n_option->choices[2];
              dialog->number_up_layout_2_option->choices_display[0] = g_strdup ( _("Left to right"));
              dialog->number_up_layout_2_option->choices_display[1] = g_strdup ( _("Right to left"));
            }
        }
      else
        {
          if (! (dialog->number_up_layout_2_option->choices[0] == dialog->number_up_layout_n_option->choices[0] &&
                 dialog->number_up_layout_2_option->choices[1] == dialog->number_up_layout_n_option->choices[1]))
            {
              g_free (dialog->number_up_layout_2_option->choices_display[0]);
              g_free (dialog->number_up_layout_2_option->choices_display[1]);
              dialog->number_up_layout_2_option->choices[0] = dialog->number_up_layout_n_option->choices[0];
              dialog->number_up_layout_2_option->choices[1] = dialog->number_up_layout_n_option->choices[1];
              dialog->number_up_layout_2_option->choices_display[0] = g_strdup ( _("Top to bottom"));
              dialog->number_up_layout_2_option->choices_display[1] = g_strdup ( _("Bottom to top"));
            }
        }

      layout = dialog_get_number_up_layout (dialog);

      old_option = gtk_printer_option_set_lookup (set, "gtk-n-up-layout");
      if (old_option != NULL)
        gtk_printer_option_set_remove (set, old_option);

      if (dialog_get_pages_per_sheet (dialog) != 1)
        {
          GEnumClass *enum_class;
          GEnumValue *enum_value;
          enum_class = g_type_class_ref (GTK_TYPE_NUMBER_UP_LAYOUT);

          if (dialog_get_pages_per_sheet (dialog) == 2)
            {
              option = dialog->number_up_layout_2_option;

              switch (layout)
                {
                case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM:
                case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT:
                  enum_value = g_enum_get_value (enum_class, GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM);
                  break;

                case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP:
                case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT:
                  enum_value = g_enum_get_value (enum_class, GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP);
                  break;

                case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM:
                case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT:
                  enum_value = g_enum_get_value (enum_class, GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM);
                  break;

                case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP:
                case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT:
                  enum_value = g_enum_get_value (enum_class, GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP);
                  break;

                default:
                  g_assert_not_reached();
                  enum_value = NULL;
                }
            }
          else
            {
              option = dialog->number_up_layout_n_option;

              enum_value = g_enum_get_value (enum_class, layout);
            }

          g_assert (enum_value != NULL);
          gtk_printer_option_set (option, enum_value->value_nick);
          g_type_class_unref (enum_class);

          gtk_printer_option_set_add (set, option);
        }
    }

  setup_option (dialog, "gtk-n-up-layout", dialog->number_up_layout);

  if (dialog->number_up_layout != NULL)
    gtk_widget_set_sensitive (GTK_WIDGET (dialog->number_up_layout),
                              (caps & GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT) &&
                              (dialog_get_pages_per_sheet (dialog) > 1));
}

static void
custom_paper_dialog_response_cb (GtkDialog *custom_paper_dialog,
                                 int        response_id,
                                 gpointer   user_data)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (user_data);

  dialog->internal_page_setup_change = TRUE;
  gtk_print_load_custom_papers (dialog->custom_paper_list);
  update_paper_sizes (dialog);
  dialog->internal_page_setup_change = FALSE;

  if (dialog->page_setup_set)
    {
      GListModel *model;
      guint n, i;

      model = G_LIST_MODEL (dialog->custom_paper_list);
      n = g_list_model_get_n_items (model);
      for (i = 0; i < n; i++)
        {
          GtkPageSetup *page_setup = g_list_model_get_item (model, i);

          if (g_strcmp0 (gtk_paper_size_get_display_name (gtk_page_setup_get_paper_size (page_setup)),
                         gtk_paper_size_get_display_name (gtk_page_setup_get_paper_size (dialog->page_setup))) == 0)
            gtk_print_unix_dialog_set_page_setup (dialog, page_setup);

          g_clear_object (&page_setup);
        }
    }

  gtk_window_destroy (GTK_WINDOW (custom_paper_dialog));
}

static void
orientation_changed (GObject            *object,
                     GParamSpec         *pspec,
                     GtkPrintUnixDialog *dialog)
{
  GtkPageOrientation orientation;
  GtkPageSetup *page_setup;

  if (dialog->internal_page_setup_change)
    return;

  orientation = (GtkPageOrientation) gtk_drop_down_get_selected (GTK_DROP_DOWN (dialog->orientation_combo));

  if (dialog->page_setup)
    {
      page_setup = gtk_page_setup_copy (dialog->page_setup);
      if (page_setup)
        gtk_page_setup_set_orientation (page_setup, orientation);

      gtk_print_unix_dialog_set_page_setup (dialog, page_setup);
    }

  redraw_page_layout_preview (dialog);
}

static void
paper_size_changed (GtkDropDown *combo_box,
                    GParamSpec *pspec,
                    GtkPrintUnixDialog *dialog)
{
  GtkPageSetup *page_setup, *last_page_setup;
  GtkPageOrientation orientation;
  guint selected;

  if (dialog->internal_page_setup_change)
    return;

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
          if (dialog->page_setup)
            last_page_setup = g_object_ref (dialog->page_setup);
          else
            last_page_setup = gtk_page_setup_new (); /* "good" default */

          if (!set_paper_size (dialog, last_page_setup, FALSE, FALSE))
            set_paper_size (dialog, last_page_setup, TRUE, TRUE);
          g_object_unref (last_page_setup);

          /* And show the custom paper dialog */
          custom_paper_dialog = _gtk_custom_paper_unix_dialog_new (GTK_WINDOW (dialog), _("Manage Custom Sizes"));
          g_signal_connect (custom_paper_dialog, "response", G_CALLBACK (custom_paper_dialog_response_cb), dialog);
          gtk_window_present (GTK_WINDOW (custom_paper_dialog));

          g_object_unref (page_setup);

          return;
        }

      if (dialog->page_setup)
        orientation = gtk_page_setup_get_orientation (dialog->page_setup);
      else
        orientation = GTK_PAGE_ORIENTATION_PORTRAIT;

      gtk_page_setup_set_orientation (page_setup, orientation);
      gtk_print_unix_dialog_set_page_setup (dialog, page_setup);

      g_object_unref (page_setup);
    }

  redraw_page_layout_preview (dialog);
}

/**
 * gtk_print_unix_dialog_new:
 * @title: (nullable): Title of the dialog
 * @parent: (nullable): Transient parent of the dialog
 *
 * Creates a new `GtkPrintUnixDialog`.
 *
 * Returns: a new `GtkPrintUnixDialog`
 */
GtkWidget *
gtk_print_unix_dialog_new (const char *title,
                           GtkWindow   *parent)
{
  GtkWidget *result;

  result = g_object_new (GTK_TYPE_PRINT_UNIX_DIALOG,
                         "transient-for", parent,
                         "title", title ? title : _("Print"),
                         NULL);

  return result;
}

/**
 * gtk_print_unix_dialog_get_selected_printer: (attributes org.gtk.Method.get_property=selected-printer)
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets the currently selected printer.
 *
 * Returns: (transfer none) (nullable): the currently selected printer
 */
GtkPrinter *
gtk_print_unix_dialog_get_selected_printer (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), NULL);

  return dialog->current_printer;
}

/**
 * gtk_print_unix_dialog_set_page_setup: (attributes org.gtk.Method.set_property=page-setup)
 * @dialog: a `GtkPrintUnixDialog`
 * @page_setup: a `GtkPageSetup`
 *
 * Sets the page setup of the `GtkPrintUnixDialog`.
 */
void
gtk_print_unix_dialog_set_page_setup (GtkPrintUnixDialog *dialog,
                                      GtkPageSetup       *page_setup)
{
  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));
  g_return_if_fail (GTK_IS_PAGE_SETUP (page_setup));

  if (dialog->page_setup != page_setup)
    {
      g_clear_object (&dialog->page_setup);
      dialog->page_setup = g_object_ref (page_setup);

      dialog->page_setup_set = TRUE;

      g_object_notify (G_OBJECT (dialog), "page-setup");
    }
}

/**
 * gtk_print_unix_dialog_get_page_setup: (attributes org.gtk.Method.get_property=page-setup)
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets the page setup that is used by the `GtkPrintUnixDialog`.
 *
 * Returns: (transfer none): the page setup of @dialog.
 */
GtkPageSetup *
gtk_print_unix_dialog_get_page_setup (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), NULL);

  return dialog->page_setup;
}

/**
 * gtk_print_unix_dialog_get_page_setup_set:
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets whether a page setup was set by the user.
 *
 * Returns: whether a page setup was set by user.
 */
gboolean
gtk_print_unix_dialog_get_page_setup_set (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->page_setup_set;
}

/**
 * gtk_print_unix_dialog_set_current_page: (attributes org.gtk.Method.set_property=current-page)
 * @dialog: a `GtkPrintUnixDialog`
 * @current_page: the current page number.
 *
 * Sets the current page number.
 *
 * If @current_page is not -1, this enables the current page choice
 * for the range of pages to print.
 */
void
gtk_print_unix_dialog_set_current_page (GtkPrintUnixDialog *dialog,
                                        int                 current_page)
{
  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));

  if (dialog->current_page != current_page)
    {
      dialog->current_page = current_page;

      if (dialog->current_page_radio)
        gtk_widget_set_sensitive (dialog->current_page_radio, current_page != -1);

      g_object_notify (G_OBJECT (dialog), "current-page");
    }
}

/**
 * gtk_print_unix_dialog_get_current_page: (attributes org.gtk.Method.get_property=current-page)
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets the current page of the `GtkPrintUnixDialog`.
 *
 * Returns: the current page of @dialog
 */
int
gtk_print_unix_dialog_get_current_page (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), -1);

  return dialog->current_page;
}

static gboolean
set_active_printer (GtkPrintUnixDialog *dialog,
                    const char         *printer_name)
{
  GListModel *model;
  GtkPrinter *printer;
  guint i;

  model = G_LIST_MODEL (gtk_column_view_get_model (GTK_COLUMN_VIEW (dialog->printer_list)));

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      printer = g_list_model_get_item (model, i);

      if (strcmp (gtk_printer_get_name (printer), printer_name) == 0)
        {
          gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (model), i);

          g_free (dialog->waiting_for_printer);
          dialog->waiting_for_printer = NULL;

          g_object_unref (printer);
          return TRUE;
        }

      g_object_unref (printer);
    }

  return FALSE;
}

/**
 * gtk_print_unix_dialog_set_settings: (attributes org.gtk.Method.set_property=print-settings)
 * @dialog: a `GtkPrintUnixDialog`
 * @settings: (nullable): a `GtkPrintSettings`
 *
 * Sets the `GtkPrintSettings` for the `GtkPrintUnixDialog`.
 *
 * Typically, this is used to restore saved print settings
 * from a previous print operation before the print dialog
 * is shown.
 */
void
gtk_print_unix_dialog_set_settings (GtkPrintUnixDialog *dialog,
                                    GtkPrintSettings   *settings)
{
  const char *printer;
  GtkPageRange *ranges;
  int num_ranges;

  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));
  g_return_if_fail (settings == NULL || GTK_IS_PRINT_SETTINGS (settings));

  if (settings != NULL)
    {
      dialog_set_collate (dialog, gtk_print_settings_get_collate (settings));
      dialog_set_reverse (dialog, gtk_print_settings_get_reverse (settings));
      dialog_set_n_copies (dialog, gtk_print_settings_get_n_copies (settings));
      dialog_set_scale (dialog, gtk_print_settings_get_scale (settings));
      dialog_set_page_set (dialog, gtk_print_settings_get_page_set (settings));
      dialog_set_print_pages (dialog, gtk_print_settings_get_print_pages (settings));
      ranges = gtk_print_settings_get_page_ranges (settings, &num_ranges);
      if (ranges)
        {
          dialog_set_page_ranges (dialog, ranges, num_ranges);
          g_free (ranges);
        }

      dialog->format_for_printer =
        g_strdup (gtk_print_settings_get (settings, "format-for-printer"));
    }

  if (dialog->initial_settings)
    g_object_unref (dialog->initial_settings);

  dialog->initial_settings = settings;

  g_free (dialog->waiting_for_printer);
  dialog->waiting_for_printer = NULL;

  if (settings)
    {
      g_object_ref (settings);

      printer = gtk_print_settings_get_printer (settings);

      if (printer && !set_active_printer (dialog, printer))
        dialog->waiting_for_printer = g_strdup (printer);
    }

  g_object_notify (G_OBJECT (dialog), "print-settings");
}

/**
 * gtk_print_unix_dialog_get_settings: (attributes org.gtk.Method.set_property=print-settings)
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets a new `GtkPrintSettings` object that represents the
 * current values in the print dialog.
 *
 * Note that this creates a new object, and you need to unref
 * it if don’t want to keep it.
 *
 * Returns: (transfer full): a new `GtkPrintSettings` object with the values from @dialog
 */
GtkPrintSettings *
gtk_print_unix_dialog_get_settings (GtkPrintUnixDialog *dialog)
{
  GtkPrintSettings *settings;
  GtkPrintPages print_pages;
  GtkPageRange *ranges;
  int n_ranges;

  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), NULL);

  settings = gtk_print_settings_new ();

  if (dialog->current_printer)
    gtk_print_settings_set_printer (settings,
                                    gtk_printer_get_name (dialog->current_printer));
  else
    gtk_print_settings_set_printer (settings, "default");

  gtk_print_settings_set (settings, "format-for-printer",
                          dialog->format_for_printer);

  gtk_print_settings_set_collate (settings,
                                  dialog_get_collate (dialog));

  gtk_print_settings_set_reverse (settings,
                                  dialog_get_reverse (dialog));

  gtk_print_settings_set_n_copies (settings,
                                   dialog_get_n_copies (dialog));

  gtk_print_settings_set_scale (settings,
                                dialog_get_scale (dialog));

  gtk_print_settings_set_page_set (settings,
                                   dialog_get_page_set (dialog));

  print_pages = dialog_get_print_pages (dialog);
  gtk_print_settings_set_print_pages (settings, print_pages);

  ranges = dialog_get_page_ranges (dialog, &n_ranges);
  if (ranges)
    {
      gtk_print_settings_set_page_ranges (settings, ranges, n_ranges);
      g_free (ranges);
    }

  /* TODO: print when. How to handle? */

  if (dialog->current_printer)
    _gtk_printer_get_settings_from_options (dialog->current_printer,
                                            dialog->options,
                                            settings);

  return settings;
}

/**
 * gtk_print_unix_dialog_add_custom_tab:
 * @dialog: a `GtkPrintUnixDialog`
 * @child: the widget to put in the custom tab
 * @tab_label: the widget to use as tab label
 *
 * Adds a custom tab to the print dialog.
 */
void
gtk_print_unix_dialog_add_custom_tab (GtkPrintUnixDialog *dialog,
                                      GtkWidget          *child,
                                      GtkWidget          *tab_label)
{
  gtk_notebook_insert_page (GTK_NOTEBOOK (dialog->notebook),
                            child, tab_label, 2);
  gtk_widget_set_visible (child, TRUE);
  gtk_widget_set_visible (tab_label, TRUE);
}

/**
 * gtk_print_unix_dialog_set_manual_capabilities: (attributes org.gtk.Method.set_property=manual-capabilities)
 * @dialog: a `GtkPrintUnixDialog`
 * @capabilities: the printing capabilities of your application
 *
 * This lets you specify the printing capabilities your application
 * supports.
 *
 * For instance, if you can handle scaling the output then you pass
 * %GTK_PRINT_CAPABILITY_SCALE. If you don’t pass that, then the dialog
 * will only let you select the scale if the printing system automatically
 * handles scaling.
 */
void
gtk_print_unix_dialog_set_manual_capabilities (GtkPrintUnixDialog   *dialog,
                                               GtkPrintCapabilities  capabilities)
{
  if (dialog->manual_capabilities != capabilities)
    {
      dialog->manual_capabilities = capabilities;
      update_dialog_from_capabilities (dialog);

      if (dialog->current_printer)
        {
          g_clear_object (&dialog->current_printer);
          selected_printer_changed (dialog);
       }

      g_object_notify (G_OBJECT (dialog), "manual-capabilities");
    }
}

/**
 * gtk_print_unix_dialog_get_manual_capabilities: (attributes org.gtk.Method.get_property=manual-capabilities)
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets the capabilities that have been set on this `GtkPrintUnixDialog`.
 *
 * Returns: the printing capabilities
 */
GtkPrintCapabilities
gtk_print_unix_dialog_get_manual_capabilities (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->manual_capabilities;
}

/**
 * gtk_print_unix_dialog_set_support_selection: (attributes org.gtk.Method.set_property=support-selection)
 * @dialog: a `GtkPrintUnixDialog`
 * @support_selection: %TRUE to allow print selection
 *
 * Sets whether the print dialog allows user to print a selection.
 */
void
gtk_print_unix_dialog_set_support_selection (GtkPrintUnixDialog *dialog,
                                             gboolean            support_selection)
{
  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));

  support_selection = support_selection != FALSE;
  if (dialog->support_selection != support_selection)
    {
      dialog->support_selection = support_selection;

      if (dialog->selection_radio)
        {
          gtk_widget_set_visible (dialog->selection_radio, support_selection);
          gtk_widget_set_sensitive (dialog->selection_radio, support_selection && dialog->has_selection);
        }

      g_object_notify (G_OBJECT (dialog), "support-selection");
    }
}

/**
 * gtk_print_unix_dialog_get_support_selection: (attributes org.gtk.Method.get_property=support-selection)
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets whether the print dialog allows user to print a selection.
 *
 * Returns: whether the application supports print of selection
 */
gboolean
gtk_print_unix_dialog_get_support_selection (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->support_selection;
}

/**
 * gtk_print_unix_dialog_set_has_selection: (attributes org.gtk.Method.set_property=has-selection)
 * @dialog: a `GtkPrintUnixDialog`
 * @has_selection: %TRUE indicates that a selection exists
 *
 * Sets whether a selection exists.
 */
void
gtk_print_unix_dialog_set_has_selection (GtkPrintUnixDialog *dialog,
                                         gboolean            has_selection)
{
  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));

  has_selection = has_selection != FALSE;
  if (dialog->has_selection != has_selection)
    {
      dialog->has_selection = has_selection;

      if (dialog->selection_radio)
        {
          if (dialog->support_selection)
            gtk_widget_set_sensitive (dialog->selection_radio, has_selection);
          else
            gtk_widget_set_sensitive (dialog->selection_radio, FALSE);
        }

      g_object_notify (G_OBJECT (dialog), "has-selection");
    }
}

/**
 * gtk_print_unix_dialog_get_has_selection: (attributes org.gtk.Method.get_property=has-selection)
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets whether there is a selection.
 *
 * Returns: whether there is a selection
 */
gboolean
gtk_print_unix_dialog_get_has_selection (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->has_selection;
}

/**
 * gtk_print_unix_dialog_set_embed_page_setup: (attributes org.gtk.Method.set_property=embed-page-setup)
 * @dialog: a `GtkPrintUnixDialog`
 * @embed: embed page setup selection
 *
 * Embed page size combo box and orientation combo box into page setup page.
 */
void
gtk_print_unix_dialog_set_embed_page_setup (GtkPrintUnixDialog *dialog,
                                            gboolean            embed)
{
  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));

  embed = embed != FALSE;
  if (dialog->embed_page_setup != embed)
    {
      dialog->embed_page_setup = embed;

      gtk_widget_set_sensitive (dialog->paper_size_combo, dialog->embed_page_setup);
      gtk_widget_set_sensitive (dialog->orientation_combo, dialog->embed_page_setup);

      if (dialog->embed_page_setup)
        {
          if (dialog->paper_size_combo != NULL)
            g_signal_connect (dialog->paper_size_combo, "notify::selected", G_CALLBACK (paper_size_changed), dialog);

          if (dialog->orientation_combo)
            g_signal_connect (dialog->orientation_combo, "notify::selected", G_CALLBACK (orientation_changed), dialog);
        }
      else
        {
          if (dialog->paper_size_combo != NULL)
            g_signal_handlers_disconnect_by_func (dialog->paper_size_combo, G_CALLBACK (paper_size_changed), dialog);

          if (dialog->orientation_combo)
            g_signal_handlers_disconnect_by_func (dialog->orientation_combo, G_CALLBACK (orientation_changed), dialog);
        }

      dialog->internal_page_setup_change = TRUE;
      update_paper_sizes (dialog);
      dialog->internal_page_setup_change = FALSE;
    }
}

/**
 * gtk_print_unix_dialog_get_embed_page_setup: (attributes org.gtk.Method.get_property=embed-page-setup)
 * @dialog: a `GtkPrintUnixDialog`
 *
 * Gets whether to embed the page setup.
 *
 * Returns: whether to embed the page setup
 */
gboolean
gtk_print_unix_dialog_get_embed_page_setup (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->embed_page_setup;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-options-dialog.h"
#include "pp-maintenance-command.h"
#include "pp-ppd-option-widget.h"
#include "pp-ipp-option-widget.h"
#include "pp-utils.h"
#include "pp-printer.h"

struct _PpOptionsDialog {
  AdwWindow    parent_instance;

  GtkTreeSelection *categories_selection;
  GtkTreeView      *categories_treeview;
  GtkWidget        *main_box;
  GtkNotebook      *notebook;
  GtkSpinner       *spinner;
  GtkStack         *stack;

  gchar       *printer_name;

  gchar       *ppd_filename;
  gboolean     ppd_filename_set;

  cups_dest_t *destination;
  gboolean     destination_set;

  GHashTable  *ipp_attributes;
  gboolean     ipp_attributes_set;

  gboolean sensitive;
};

G_DEFINE_TYPE (PpOptionsDialog, pp_options_dialog, ADW_TYPE_WINDOW)

enum
{
  CATEGORY_IDS_COLUMN = 0,
  CATEGORY_NAMES_COLUMN
};

/* These lists come from Gtk+ */
/* TODO: Only "Resolution" currently has a context to disambiguate it from
 *       the display settings. All of these should have contexts, but it
 *       was late in the release cycle and this partial solution was
 *       preferable. See:
 *       https://gitlab.gnome.org/GNOME/gnome-control-center/merge_requests/414#note_446778
 */
static const struct {
  const char *keyword;
  const char *translation_context;
  const char *translation;
} ppd_option_translations[] = {
  { "Duplex", NULL, N_("Two Sided") },
  { "MediaType", NULL, N_("Paper Type") },
  { "InputSlot", NULL, N_("Paper Source") },
  { "OutputBin", NULL, N_("Output Tray") },
  { "Resolution", "printing option", NC_("printing option", "Resolution") },
  { "PreFilter", NULL, N_("GhostScript pre-filtering") },
};

/* keep sorted when changing */
static const char *allowed_page_setup_options[] = {
  "InputSlot",
  "MediaType",
  "OutputBin",
  "PageSize",
};

/* keep sorted when changing */
static const char *allowed_color_options[] = {
  "BRColorEnhancement",
  "BRColorMatching",
  "BRColorMatching",
  "BRColorMode",
  "BRGammaValue",
  "BRImprovedGray",
  "BlackSubstitution",
  "ColorModel",
  "HPCMYKInks",
  "HPCSGraphics",
  "HPCSImages",
  "HPCSText",
  "HPColorSmart",
  "RPSBlackMode",
  "RPSBlackOverPrint",
  "Rcmyksimulation",
};

/* keep sorted when changing */
static const char *allowed_color_groups[] = {
  "Color",
  "Color1",
  "Color2",
  "ColorBalance",
  "ColorPage",
  "ColorSettings1",
  "ColorSettings2",
  "ColorSettings3",
  "ColorSettings4",
  "EPColorSettings",
  "FPColorWise1",
  "FPColorWise2",
  "FPColorWise3",
  "FPColorWise4",
  "FPColorWise5",
  "HPCMYKInksPanel",
  "HPColorOptions",
  "HPColorOptionsPanel",
  "HPColorQualityOptionsPanel",
  "ManualColor",
};

/* keep sorted when changing */
static const char *allowed_image_quality_options[] = {
  "BRDocument",
  "BRHalfTonePattern",
  "BRNormalPrt",
  "BRPrintQuality",
  "BitsPerPixel",
  "Darkness",
  "Dithering",
  "EconoMode",
  "Economode",
  "HPEconoMode",
  "HPEdgeControl",
  "HPGraphicsHalftone",
  "HPHalftone",
  "HPImagingOptions",
  "HPLJDensity",
  "HPPhotoHalftone",
  "HPPrintQualityOptions",
  "HPResolutionOptions",
  "OutputMode",
  "REt",
  "RPSBitsPerPixel",
  "RPSDitherType",
  "Resolution",
  "ScreenLock",
  "Smoothing",
  "TonerSaveMode",
  "UCRGCRForImage",
};

/* keep sorted when changing */
static const char *allowed_image_quality_groups[] = {
  "EPQualitySettings",
  "FPImageQuality1",
  "FPImageQuality2",
  "FPImageQuality3",
  "ImageQualityPage",
  "Quality",
};

/* keep sorted when changing */
static const char * allowed_finishing_options[] = {
  "BindColor",
  "BindEdge",
  "BindType",
  "BindWhen",
  "Booklet",
  "FoldType",
  "FoldWhen",
  "HPStaplerOptions",
  "Jog",
  "Slipsheet",
  "Sorter",
  "StapleLocation",
  "StapleOrientation",
  "StapleWhen",
  "StapleX",
  "StapleY",
};

/* keep sorted when changing */
static const char *allowed_job_groups[] = {
  "JobHandling",
  "JobLog",
};

/* keep sorted when changing */
static const char *allowed_finishing_groups[] = {
  "Booklet",
  "BookletCover",
  "BookletModeOptions",
  "FPFinishing1",
  "FPFinishing2",
  "FPFinishing3",
  "FPFinishing4",
  "Finishing",
  "FinishingOptions",
  "FinishingPage",
  "HPBookletPanel",
  "HPFinishing",
  "HPFinishingOptions",
  "HPFinishingPanel",
};

/* keep sorted when changing */
static const char *allowed_installable_options_groups[] = {
  "InstallableOptions",
};

/* keep sorted when changing */
static const char *allowed_page_setup_groups[] = {
  "HPMarginAndLayout",
  "OutputControl",
  "PaperHandling",
  "Paper",
  "Source",
};

/* keep sorted when changing */
static const char *disallowed_ppd_options[] = {
  "Collate",
  "Copies",
  "Duplex",
  "HPManualDuplexOrientation",
  "HPManualDuplexSwitch",
  "OutputOrder",
  "PageRegion"
};

static int
strptr_cmp (const void *a,
	    const void *b)
{
  char **aa = (char **)a;
  char **bb = (char **)b;
  return strcmp (*aa, *bb);
}

static gboolean
string_in_table (gchar       *str,
		 const gchar *table[],
		 gint         table_len)
{
  return bsearch (&str, table, table_len, sizeof (char *), (void *)strptr_cmp) != NULL;
}

#define STRING_IN_TABLE(_str, _table) (string_in_table (_str, _table, G_N_ELEMENTS (_table)))

static const gchar *
ppd_option_name_translate (ppd_option_t *option)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (ppd_option_translations); i++)
    {
      if (g_strcmp0 (ppd_option_translations[i].keyword, option->keyword) == 0)
        {
          if (ppd_option_translations[i].translation_context)
            return g_dpgettext2(NULL, ppd_option_translations[i].translation_context, ppd_option_translations[i].translation);
          else
            return _(ppd_option_translations[i].translation);
        }
    }

  return option->text;
}

static gint
grid_get_height (GtkWidget *grid)
{
  GtkWidget *child;
  gint   height = 0;
  gint   row = 0;
  gint   max = 0;

  for (child = gtk_widget_get_first_child (grid);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_grid_query_child (GTK_GRID (grid),
                            child,
                            NULL, &row,
                            NULL, &height);

      if (height + row > max)
        max = height + row;
    }

  return max;
}

static gboolean
grid_is_empty (GtkWidget *grid)
{
  return gtk_widget_get_first_child (grid) == NULL;
}

static GtkWidget *
ipp_option_add (IPPAttribute *attr_supported,
                IPPAttribute *attr_default,
                const gchar  *option_name,
                const gchar  *option_display_name,
                const gchar  *printer_name,
                GtkWidget    *grid,
                gboolean      sensitive)
{
  GtkWidget       *widget;
  GtkWidget       *label;
  gint             position;

  widget = (GtkWidget *) pp_ipp_option_widget_new (attr_supported,
                                                   attr_default,
                                                   option_name,
                                                   printer_name);
  if (widget)
    {
      gtk_widget_set_visible (widget, TRUE);
      gtk_widget_set_sensitive (widget, sensitive);
      position = grid_get_height (grid);

      label = gtk_label_new (option_display_name);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
      gtk_widget_add_css_class (label, "dim-label");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_margin_start (label, 10);
      gtk_grid_attach (GTK_GRID (grid), label, 0, position, 1, 1);

      gtk_widget_set_margin_start (widget, 20);
      gtk_grid_attach (GTK_GRID (grid), widget, 1, position, 1, 1);
    }

  return widget;
}

static GtkWidget *
ppd_option_add (ppd_option_t  option,
                const gchar  *printer_name,
                GtkWidget    *grid,
                gboolean      sensitive)
{
  GtkWidget       *widget;
  GtkWidget       *label;
  gint             position;

  widget = (GtkWidget *) pp_ppd_option_widget_new (&option, printer_name);
  if (widget)
    {
      gtk_widget_set_visible (widget, TRUE);
      gtk_widget_set_sensitive (widget, sensitive);
      position = grid_get_height (grid);

      label = gtk_label_new (ppd_option_name_translate (&option));
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
      gtk_widget_add_css_class (label, "dim-label");
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_margin_start (label, 10);
      gtk_grid_attach (GTK_GRID (grid), label, 0, position, 1, 1);

      gtk_widget_set_margin_start (widget, 20);
      gtk_grid_attach (GTK_GRID (grid), widget, 1, position, 1, 1);
    }

  return widget;
}

static GtkWidget *
tab_grid_new ()
{
  GtkWidget *grid;

  grid = gtk_grid_new ();
  gtk_widget_set_margin_start (grid, 20);
  gtk_widget_set_margin_end (grid, 20);
  gtk_widget_set_margin_top (grid, 20);
  gtk_widget_set_margin_bottom (grid, 20);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 15);

  return grid;
}

static void
tab_add (PpOptionsDialog *self,
         const gchar     *tab_name,
         GtkWidget       *grid)
{
  GtkListStore *store;
  GtkTreeIter   iter;
  GtkWidget    *scrolled_window;
  gboolean      unref_store = FALSE;
  gint          id;

  if (!grid_is_empty (grid))
    {
      scrolled_window = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), grid);

      id = gtk_notebook_append_page (self->notebook,
                                     scrolled_window,
                                     NULL);

      if (id >= 0)
        {
          store = GTK_LIST_STORE (gtk_tree_view_get_model (self->categories_treeview));
          if (!store)
            {
              store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
              unref_store = TRUE;
            }

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              CATEGORY_IDS_COLUMN, id,
                              CATEGORY_NAMES_COLUMN, tab_name,
                              -1);

          if (unref_store)
            {
              gtk_tree_view_set_model (self->categories_treeview, GTK_TREE_MODEL (store));
              g_object_unref (store);
            }
        }
    }
  else
    {
      g_object_ref_sink (grid);
      g_object_unref (grid);
    }
}

static void
category_selection_changed_cb (PpOptionsDialog *self)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gint          id = -1;

  if (gtk_tree_selection_get_selected (self->categories_selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  CATEGORY_IDS_COLUMN, &id,
			  -1);
    }

  if (id >= 0)
    {
      gtk_notebook_set_current_page (self->notebook, id);
    }
}

static void
populate_options_real (PpOptionsDialog *self)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  ppd_file_t   *ppd_file;
  GtkWidget    *grid;
  GtkWidget    *general_tab_grid = tab_grid_new ();
  GtkWidget    *page_setup_tab_grid = tab_grid_new ();
  GtkWidget    *installable_options_tab_grid = tab_grid_new ();
  GtkWidget    *job_tab_grid = tab_grid_new ();
  GtkWidget    *image_quality_tab_grid = tab_grid_new ();
  GtkWidget    *color_tab_grid = tab_grid_new ();
  GtkWidget    *finishing_tab_grid = tab_grid_new ();
  GtkWidget    *advanced_tab_grid = tab_grid_new ();
  gint          i, j;

  gtk_spinner_stop (self->spinner);

  gtk_stack_set_visible_child (self->stack, self->main_box);

  if (self->ipp_attributes)
    {
      /* Add number-up option to Page Setup tab */
      ipp_option_add (g_hash_table_lookup (self->ipp_attributes,
                                           "number-up-supported"),
                      g_hash_table_lookup (self->ipp_attributes,
                                           "number-up-default"),
                      "number-up",
                      /* Translators: This option sets number of pages printed on one sheet */
                      _("Pages per side"),
                      self->printer_name,
                      page_setup_tab_grid,
                      self->sensitive);

      /* Add sides option to Page Setup tab */
      ipp_option_add (g_hash_table_lookup (self->ipp_attributes,
                                           "sides-supported"),
                      g_hash_table_lookup (self->ipp_attributes,
                                           "sides-default"),
                      "sides",
                      /* Translators: This option sets whether to print on both sides of paper */
                      _("Two-sided"),
                      self->printer_name,
                      page_setup_tab_grid,
                      self->sensitive);

      /* Add orientation-requested option to Page Setup tab */
      ipp_option_add (g_hash_table_lookup (self->ipp_attributes,
                                           "orientation-requested-supported"),
                      g_hash_table_lookup (self->ipp_attributes,
                                           "orientation-requested-default"),
                      "orientation-requested",
                      /* Translators: This option sets orientation of print (portrait, landscape...) */
                      _("Orientation"),
                      self->printer_name,
                      page_setup_tab_grid,
                      self->sensitive);
    }

  if (self->destination && self->ppd_filename)
    {
      ppd_file = ppdOpenFile (self->ppd_filename);
      ppdLocalize (ppd_file);

      if (ppd_file)
        {
          ppdMarkDefaults (ppd_file);
          cupsMarkOptions (ppd_file,
                           self->destination->num_options,
                           self->destination->options);

          for (i = 0; i < ppd_file->num_groups; i++)
            {
              for (j = 0; j < ppd_file->groups[i].num_options; j++)
                {
                  grid = NULL;

                  if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                       allowed_color_groups))
                    grid = color_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            allowed_image_quality_groups))
                    grid = image_quality_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            allowed_job_groups))
                    grid = job_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            allowed_finishing_groups))
                    grid = finishing_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            allowed_installable_options_groups))
                    grid = installable_options_tab_grid;
                  else if (STRING_IN_TABLE (ppd_file->groups[i].name,
                                            allowed_page_setup_groups))
                    grid = page_setup_tab_grid;

                  if (!STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                        disallowed_ppd_options))
                    {
                      if (!grid && STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                                    allowed_color_options))
                        grid = color_tab_grid;
                      else if (!grid && STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                                         allowed_image_quality_options))
                        grid = image_quality_tab_grid;
                      else if (!grid && STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                                         allowed_finishing_options))
                        grid = finishing_tab_grid;
                      else if (!grid && STRING_IN_TABLE (ppd_file->groups[i].options[j].keyword,
                                                         allowed_page_setup_options))
                        grid = page_setup_tab_grid;

                      if (!grid)
                        grid = advanced_tab_grid;

                      ppd_option_add (ppd_file->groups[i].options[j],
                                      self->printer_name,
                                      grid,
                                      self->sensitive);
                    }
                }
            }

          ppdClose (ppd_file);
        }
    }

  self->ppd_filename_set = FALSE;
  if (self->ppd_filename)
    {
      g_unlink (self->ppd_filename);
      g_free (self->ppd_filename);
      self->ppd_filename = NULL;
    }

  self->destination_set = FALSE;
  if (self->destination)
    {
      cupsFreeDests (1, self->destination);
      self->destination = NULL;
    }

  self->ipp_attributes_set = FALSE;
  if (self->ipp_attributes)
    {
      g_hash_table_unref (self->ipp_attributes);
      self->ipp_attributes = NULL;
    }

  /* Translators: "General" tab contains general printer options */
  tab_add (self, C_("Printer Option Group", "General"), general_tab_grid);

  /* Translators: "Page Setup" tab contains settings related to pages (page size, paper source, etc.) */
  tab_add (self, C_("Printer Option Group", "Page Setup"), page_setup_tab_grid);

  /* Translators: "Installable Options" tab contains settings of presence of installed options (amount of RAM, duplex unit, etc.) */
  tab_add (self, C_("Printer Option Group", "Installable Options"), installable_options_tab_grid);

  /* Translators: "Job" tab contains settings for jobs */
  tab_add (self, C_("Printer Option Group", "Job"), job_tab_grid);

  /* Translators: "Image Quality" tab contains settings for quality of output print (e.g. resolution) */
  tab_add (self, C_("Printer Option Group", "Image Quality"), image_quality_tab_grid);

  /* Translators: "Color" tab contains color settings (e.g. color printing) */
  tab_add (self, C_("Printer Option Group", "Color"), color_tab_grid);

  /* Translators: "Finishing" tab contains finishing settings (e.g. booklet printing) */
  tab_add (self, C_("Printer Option Group", "Finishing"), finishing_tab_grid);

  /* Translators: "Advanced" tab contains all others settings */
  tab_add (self, C_("Printer Option Group", "Advanced"), advanced_tab_grid);

  /* Select the first option group */
  if ((model = gtk_tree_view_get_model (self->categories_treeview)) != NULL &&
      gtk_tree_model_get_iter_first (model, &iter))
    gtk_tree_selection_select_iter (self->categories_selection, &iter);
}

static void
printer_get_ppd_cb (const gchar *ppd_filename,
                    gpointer     user_data)
{
  PpOptionsDialog *self = (PpOptionsDialog *) user_data;

  if (self->ppd_filename)
    {
      g_unlink (self->ppd_filename);
      g_free (self->ppd_filename);
    }

  self->ppd_filename = g_strdup (ppd_filename);
  self->ppd_filename_set = TRUE;

  if (self->destination_set &&
      self->ipp_attributes_set)
    {
      populate_options_real (self);
    }
}

static void
get_named_dest_cb (cups_dest_t *dest,
                   gpointer     user_data)
{
  PpOptionsDialog *self = (PpOptionsDialog *) user_data;

  if (self->destination)
    cupsFreeDests (1, self->destination);

  self->destination = dest;
  self->destination_set = TRUE;

  if (self->ppd_filename_set &&
      self->ipp_attributes_set)
    {
      populate_options_real (self);
    }
}

static void
get_ipp_attributes_cb (GHashTable *table,
                       gpointer    user_data)
{
  PpOptionsDialog *self = (PpOptionsDialog *) user_data;

  if (self->ipp_attributes)
    g_hash_table_unref (self->ipp_attributes);

  self->ipp_attributes = g_hash_table_ref (table);
  self->ipp_attributes_set = TRUE;

  if (self->ppd_filename_set &&
      self->destination_set)
    {
      populate_options_real (self);
    }
}

static void
populate_options (PpOptionsDialog *self)
{
  GtkTreeViewColumn  *column;
  GtkCellRenderer    *renderer;
  /*
   * Options which we need to obtain through an IPP request
   * to be able to fill the options dialog.
   * *-supported - possible values of the option
   * *-default - actual value of the option
   */
  const gchar        *attributes[] =
    { "number-up-supported",
      "number-up-default",
      "sides-supported",
      "sides-default",
      "orientation-requested-supported",
      "orientation-requested-default",
      NULL};

  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->spinner));

  renderer = gtk_cell_renderer_text_new ();

  column = gtk_tree_view_column_new_with_attributes ("Categories", renderer,
                                                     "text", CATEGORY_NAMES_COLUMN, NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (self->categories_treeview, column);

  gtk_spinner_start (self->spinner);

  printer_get_ppd_async (self->printer_name,
                         NULL,
                         0,
                         printer_get_ppd_cb,
                         self);

  get_named_dest_async (self->printer_name,
                        get_named_dest_cb,
                        self);

  get_ipp_attributes_async (self->printer_name,
                            (gchar **) attributes,
                            get_ipp_attributes_cb,
                            self);
}

static void
pp_maintenance_command_execute_cb (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  pp_maintenance_command_execute_finish (PP_MAINTENANCE_COMMAND(source_object), res, NULL);
}

static gchar *
get_testprint_filename (const gchar *datadir)
{
  const gchar *testprint[] = { "/data/testprint",
                               "/data/testprint.ps",
                               NULL };
  gchar       *filename = NULL;
  gint         i;

  for (i = 0; testprint[i] != NULL; i++)
    {
      filename = g_strconcat (datadir, testprint[i], NULL);
      if (g_access (filename, R_OK) == 0)
        break;

      g_clear_pointer (&filename, g_free);
    }

  return filename;
}

static void
print_test_page_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  pp_printer_print_file_finish (PP_PRINTER (source_object),
                                result, NULL);
}

static void
test_page_cb (PpOptionsDialog *self)
{
  gint i;

  if (self->printer_name)
    {
      const gchar      *const dirs[] = { "/usr/share/cups",
                                         "/usr/local/share/cups",
                                         NULL };
      const gchar      *datadir = NULL;
      g_autofree gchar *filename = NULL;

      datadir = getenv ("CUPS_DATADIR");
      if (datadir != NULL)
        {
          filename = get_testprint_filename (datadir);
        }
      else
        {
          for (i = 0; dirs[i] != NULL && filename == NULL; i++)
            filename = get_testprint_filename (dirs[i]);
        }

      if (filename != NULL)
        {
          g_autoptr(PpPrinter) printer = NULL;

          printer = pp_printer_new (self->printer_name);
          pp_printer_print_file_async (printer,
                                       filename,
          /* Translators: Name of job which makes printer to print test page */
                                       _("Test Page"),
                                       NULL,
                                       print_test_page_cb,
                                       NULL);
        }
      else
        {
          g_autoptr(PpMaintenanceCommand) command = NULL;

          command = pp_maintenance_command_new (self->printer_name,
                                                "PrintSelfTestPage",
                                                NULL,
          /* Translators: Name of job which makes printer to print test page */
                                                _("Test page"));

          pp_maintenance_command_execute_async (command, NULL, pp_maintenance_command_execute_cb, NULL);
        }
    }
}

PpOptionsDialog *
pp_options_dialog_new (gchar   *printer_name,
                       gboolean sensitive)
{
  PpOptionsDialog *self;

  self = g_object_new (pp_options_dialog_get_type (), NULL);

  self->printer_name = g_strdup (printer_name);

  self->sensitive = sensitive;

  gtk_window_set_title (GTK_WINDOW (self), printer_name);

  populate_options (self);

  return self;
}

static void
pp_options_dialog_dispose (GObject *object)
{
  PpOptionsDialog *self = PP_OPTIONS_DIALOG (object);

  g_free (self->printer_name);
  self->printer_name = NULL;

  if (self->ppd_filename)
    {
      g_unlink (self->ppd_filename);
      g_free (self->ppd_filename);
      self->ppd_filename = NULL;
    }

  if (self->destination)
    {
      cupsFreeDests (1, self->destination);
      self->destination = NULL;
    }

  if (self->ipp_attributes)
    {
      g_hash_table_unref (self->ipp_attributes);
      self->ipp_attributes = NULL;
    }

  G_OBJECT_CLASS (pp_options_dialog_parent_class)->dispose (object);
}

void
pp_options_dialog_class_init (PpOptionsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pp_options_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/pp-options-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PpOptionsDialog, categories_selection);
  gtk_widget_class_bind_template_child (widget_class, PpOptionsDialog, categories_treeview);
  gtk_widget_class_bind_template_child (widget_class, PpOptionsDialog, main_box);
  gtk_widget_class_bind_template_child (widget_class, PpOptionsDialog, notebook);
  gtk_widget_class_bind_template_child (widget_class, PpOptionsDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, PpOptionsDialog, stack);

  gtk_widget_class_bind_template_callback (widget_class, category_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, test_page_cb);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

void
pp_options_dialog_init (PpOptionsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

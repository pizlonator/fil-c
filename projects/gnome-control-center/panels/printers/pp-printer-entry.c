/*
 * Copyright 2017 Red Hat, Inc
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
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#include <config.h>

#include "pp-printer-entry.h"
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "pp-details-dialog.h"
#include "pp-maintenance-command.h"
#include "pp-options-dialog.h"
#include "pp-jobs-dialog.h"
#include "pp-printer.h"
#include "pp-utils.h"

#define SUPPLY_BAR_HEIGHT 8

typedef struct
{
  gchar *marker_names;
  gchar *marker_levels;
  gchar *marker_colors;
  gchar *marker_types;
} InkLevelData;

struct _PpPrinterEntry
{
  GtkListBoxRow parent;

  gchar    *printer_name;
  gboolean  is_accepting_jobs;
  gchar    *printer_make_and_model;
  gchar    *printer_location;
  gchar    *printer_hostname;
  gboolean  is_authorized;
  gint      printer_state;
  InkLevelData *inklevel;

  /* Maintenance commands */
  PpMaintenanceCommand *clean_command;
  GCancellable *check_clean_heads_cancellable;

  /* Widgets */
  GtkOrientable  *header_box;
  GtkLabel       *printer_status;
  GtkLabel       *printer_name_label;
  GtkLabel       *printer_model_label;
  GtkLabel       *printer_model;
  GtkLabel       *printer_location_label;
  GtkLabel       *printer_location_address_label;
  GtkLabel       *printer_inklevel_label;
  GtkFrame       *supply_frame;
  GtkDrawingArea *supply_drawing_area;
  GtkWidget      *show_jobs_dialog_button;
  GtkBox         *printer_error;
  GtkLabel       *error_status;

  gboolean        is_default;
  gboolean        compact;

  /* Dialogs */
  PpJobsDialog    *pp_jobs_dialog;

  GCancellable *get_jobs_cancellable;
};

struct _PpPrinterEntryClass
{
  GtkListBoxRowClass parent_class;

  void (*printer_changed) (PpPrinterEntry *printer_entry);
  void (*printer_delete)  (PpPrinterEntry *printer_entry);
  void (*printer_renamed) (PpPrinterEntry *printer_entry, const gchar *new_name);
};

G_DEFINE_TYPE (PpPrinterEntry, pp_printer_entry, GTK_TYPE_LIST_BOX_ROW)

enum {
  IS_DEFAULT_PRINTER,
  PRINTER_DELETE,
  PRINTER_RENAMED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_0,
  PROP_DEFAULT,
  PROP_COMPACT,
};

static InkLevelData *
ink_level_data_new (void)
{
  return g_slice_new0 (InkLevelData);
}

static void
ink_level_data_free (InkLevelData *data)
{
  g_clear_pointer (&data->marker_names, g_free);
  g_clear_pointer (&data->marker_levels, g_free);
  g_clear_pointer (&data->marker_colors, g_free);
  g_clear_pointer (&data->marker_types, g_free);
  g_slice_free (InkLevelData, data);
}

static void
pp_printer_entry_init (PpPrinterEntry *self)
{
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "printer.clean-heads", FALSE);

  gtk_widget_init_template (GTK_WIDGET (self));
  self->inklevel = ink_level_data_new ();
}

typedef struct {
  gchar *color;
  gchar *type;
  gchar *name;
  gint   level;
} MarkerItem;

static gint
markers_cmp (gconstpointer a,
             gconstpointer b)
{
  MarkerItem *x = (MarkerItem*) a;
  MarkerItem *y = (MarkerItem*) b;

  if (x->level < y->level)
    return 1;
  else if (x->level == y->level)
    return 0;
  else
    return -1;
}

static gchar *
sanitize_printer_model (const gchar *printer_make_and_model)
{
  gchar            *breakpoint = NULL, *tmp2 = NULL;
  g_autofree gchar *tmp = NULL;
  gchar             backup;
  size_t            length = 0;
  gchar            *forbidden[] = {
    "foomatic",
    ",",
    "hpijs",
    "hpcups",
    "(recommended)",
    "postscript (recommended)",
    NULL };
  int     i;

  tmp = g_ascii_strdown (printer_make_and_model, -1);

  for (i = 0; i < g_strv_length (forbidden); i++)
    {
      tmp2 = g_strrstr (tmp, forbidden[i]);
      if (breakpoint == NULL ||
         (tmp2 != NULL && tmp2 < breakpoint))
           breakpoint = tmp2;
    }

  if (breakpoint)
    {
      backup = *breakpoint;
      *breakpoint = '\0';
      length = strlen (tmp);
      *breakpoint = backup;

      if (length > 0)
        return g_strndup (printer_make_and_model, length);
    }
  else
    return g_strdup (printer_make_and_model);

  return NULL;
}

static gboolean
supply_level_is_empty (PpPrinterEntry *self)
{
    return !((self->inklevel->marker_levels != NULL) &&
             (self->inklevel->marker_colors != NULL) &&
             (self->inklevel->marker_names != NULL) &&
             (self->inklevel->marker_types != NULL));
}

/* To tone down the colors in the supply level bar
 * we shade them by darkening the hue.
 *
 * Obs.: we don't know whether the color is already
 * shaded.
 *
 */
static void
tone_down_color (GdkRGBA *color,
                 gdouble  hue_ratio,
                 gdouble  saturation_ratio,
                 gdouble  value_ratio)
{
  gfloat h, s, v;

  gtk_rgb_to_hsv (color->red, color->green, color->blue,
                  &h, &s, &v);
  gtk_hsv_to_rgb (h * hue_ratio, s * saturation_ratio, v * value_ratio,
                  &color->red, &color->green, &color->blue);
}

static void
supply_levels_draw_cb (GtkDrawingArea *drawing_area,
                       cairo_t        *cr,
                       int             width,
                       int             height,
                       gpointer        user_data)
{
  PpPrinterEntry         *self = PP_PRINTER_ENTRY (user_data);
  GtkStyleContext        *context;
  gboolean                is_empty = TRUE;
  g_autofree gchar       *tooltip_text = NULL;
  int                     i;

  context = gtk_widget_get_style_context (GTK_WIDGET (self->supply_drawing_area));

  gtk_render_background (context, cr, 0, 0, width, height);

  if (!supply_level_is_empty (self))
    {
      GSList   *markers = NULL;
      GSList   *tmp_list = NULL;
      gchar   **marker_levelsv = NULL;
      gchar   **marker_colorsv = NULL;
      gchar   **marker_namesv = NULL;
      gchar   **marker_typesv = NULL;

      gtk_style_context_save (context);

      marker_levelsv = g_strsplit (self->inklevel->marker_levels, ",", -1);
      marker_colorsv = g_strsplit (self->inklevel->marker_colors, ",", -1);
      marker_namesv = g_strsplit (self->inklevel->marker_names, ",", -1);
      marker_typesv = g_strsplit (self->inklevel->marker_types, ",", -1);

      if (g_strv_length (marker_levelsv) == g_strv_length (marker_colorsv) &&
          g_strv_length (marker_colorsv) == g_strv_length (marker_namesv) &&
          g_strv_length (marker_namesv) == g_strv_length (marker_typesv))
        {
          for (i = 0; i < g_strv_length (marker_levelsv); i++)
            {
              MarkerItem *marker;

              if (g_strcmp0 (marker_typesv[i], "ink") == 0 ||
                  g_strcmp0 (marker_typesv[i], "toner") == 0 ||
                  g_strcmp0 (marker_typesv[i], "inkCartridge") == 0 ||
                  g_strcmp0 (marker_typesv[i], "tonerCartridge") == 0)
                {
                  marker = g_new0 (MarkerItem, 1);
                  marker->type = g_strdup (marker_typesv[i]);
                  marker->name = g_strdup (marker_namesv[i]);
                  marker->color = g_strdup (marker_colorsv[i]);
                  marker->level = atoi (marker_levelsv[i]);

                  markers = g_slist_prepend (markers, marker);
                }
            }

            markers = g_slist_sort (markers, markers_cmp);

            for (tmp_list = markers; tmp_list; tmp_list = tmp_list->next)
              {
                GdkRGBA color = {0.0, 0.0, 0.0, 1.0};
                double  display_value;
                int     value;

                value = ((MarkerItem*) tmp_list->data)->level;

                gdk_rgba_parse (&color, ((MarkerItem*) tmp_list->data)->color);
                tone_down_color (&color, 1.0, 0.6, 0.9);

                if (value > 0)
                  {
                    display_value = value / 100.0 * (width - 3.0);
                    gdk_cairo_set_source_rgba (cr, &color);
                    cairo_rectangle (cr, 2.0, 2.0, display_value, SUPPLY_BAR_HEIGHT);
                    cairo_fill (cr);

                    tone_down_color (&color, 1.0, 1.0, 0.85);
                    gdk_cairo_set_source_rgba (cr, &color);
                    cairo_set_line_width (cr, 1.0);
                    cairo_rectangle (cr, 1.5, 1.5, display_value, SUPPLY_BAR_HEIGHT + 1);
                    cairo_stroke (cr);

                    is_empty = FALSE;
                  }

                if (tooltip_text)
                  {
                    g_autofree gchar *old_tooltip_text = g_steal_pointer (&tooltip_text);
                    tooltip_text = g_strdup_printf ("%s\n%s",
                                                    old_tooltip_text,
                                                    ((MarkerItem*) tmp_list->data)->name);
                  }
                else
                  tooltip_text = g_strdup_printf ("%s",
                                                  ((MarkerItem*) tmp_list->data)->name);
              }

            gtk_render_frame (context, cr, 1, 1, width - 1, SUPPLY_BAR_HEIGHT);

            for (tmp_list = markers; tmp_list; tmp_list = tmp_list->next)
              {
                g_free (((MarkerItem*) tmp_list->data)->name);
                g_free (((MarkerItem*) tmp_list->data)->type);
                g_free (((MarkerItem*) tmp_list->data)->color);
              }
            g_slist_free_full (markers, g_free);
          }

        gtk_style_context_restore (context);

    if (tooltip_text)
      {
        gtk_widget_set_tooltip_text (GTK_WIDGET (self->supply_drawing_area), tooltip_text);
      }
    else
      {
        gtk_widget_set_tooltip_text (GTK_WIDGET (self->supply_drawing_area), NULL);
        gtk_widget_set_has_tooltip (GTK_WIDGET (self->supply_drawing_area), FALSE);
      }
    }

  gtk_widget_set_visible (GTK_WIDGET (self->printer_inklevel_label), !is_empty);
  gtk_widget_set_visible (GTK_WIDGET (self->supply_frame), !is_empty);
}

static void
on_printer_rename_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  PpPrinterEntry *self = user_data;

  if (!pp_printer_rename_finish (PP_PRINTER (source_object), result, NULL))
    return;

  g_signal_emit_by_name (self, "printer-renamed", pp_printer_get_name (PP_PRINTER (source_object)));
}

static gboolean
show_printer_details_response_cb (PpPrinterEntry  *self,
                                  PpDetailsDialog *dialog)
{
  const gchar *new_name;
  const gchar *new_location;

  new_location = pp_details_dialog_get_printer_location (dialog);
  if (g_strcmp0 (self->printer_location, new_location) != 0)
    printer_set_location (self->printer_name, new_location);

  new_name = pp_details_dialog_get_printer_name (dialog);
  if (g_strcmp0 (self->printer_name, new_name) != 0 && printer_name_is_valid (new_name))
    {
      g_autoptr(PpPrinter) printer = pp_printer_new (self->printer_name);

      pp_printer_rename_async (printer,
                               new_name,
                               NULL,
                               on_printer_rename_cb,
                               self);
    }

  g_signal_emit_by_name (self, "printer-changed");

  return GDK_EVENT_PROPAGATE;
}

static void
printer_details_cb (PpPrinterEntry *self)
{
  PpDetailsDialog *dialog = pp_details_dialog_new (self->printer_name,
                                                   self->printer_location,
                                                   self->printer_hostname,
                                                   self->printer_make_and_model,
                                                   self->is_authorized);

  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));

  g_signal_connect_swapped (dialog, "close-request", G_CALLBACK (show_printer_details_response_cb), self);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
printer_options_cb (PpPrinterEntry *self)
{
  PpOptionsDialog *dialog;

  dialog = pp_options_dialog_new (self->printer_name, self->is_authorized);

  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
set_as_default_printer (PpPrinterEntry *self)
{
  if (self->is_default)
    return;

  printer_set_default (self->printer_name);

  self->is_default = TRUE;

  g_object_notify (G_OBJECT (self), "default");

  g_signal_emit_by_name (self, "printer-changed");
}

static void
check_clean_heads_maintenance_command_cb (GObject      *source_object,
                                          GAsyncResult *res,
                                          gpointer      user_data)
{
  PpPrinterEntry       *self = user_data;
  gboolean              is_supported = FALSE;
  g_autoptr(GError)     error = NULL;

  is_supported = pp_maintenance_command_is_supported_finish (PP_MAINTENANCE_COMMAND (source_object), res, &error);
  if (error != NULL)
    {
      g_debug ("Could not check 'Clean' maintenance command: %s", error->message);
      return;
    }

  if (is_supported)
    {
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "printer.clean-heads", TRUE);
    }
}

static void
check_clean_heads_maintenance_command (PpPrinterEntry *self)
{
  if (self->clean_command == NULL)
    return;

  g_object_ref (self->clean_command);
  self->check_clean_heads_cancellable = g_cancellable_new ();

  pp_maintenance_command_is_supported_async (self->clean_command,
                                             self->check_clean_heads_cancellable,
                                             check_clean_heads_maintenance_command_cb,
                                             self);
}

static void
clean_heads_maintenance_command_cb (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  PpPrinterEntry *self = user_data;
  g_autoptr(GError) error = NULL;

  if (!pp_maintenance_command_execute_finish (PP_MAINTENANCE_COMMAND (source_object), res, &error))
    g_warning ("Error cleaning print heads for %s: %s", self->printer_name, error->message);
}

static void
printer_clean_heads_cb (PpPrinterEntry *self)
{
  if (self->clean_command == NULL)
    return;

  g_object_ref (self->clean_command);
  pp_maintenance_command_execute_async (self->clean_command,
                                        NULL,
                                        clean_heads_maintenance_command_cb,
                                        self);
}

static void
printer_remove_cb (PpPrinterEntry *self)
{
  g_signal_emit_by_name (self, "printer-delete");
}

static void
get_jobs_cb (GObject      *source_object,
             GAsyncResult *result,
             gpointer      user_data)
{
  PpPrinterEntry      *self = user_data;
  g_autoptr(GError)    error = NULL;
  g_autoptr(GPtrArray) jobs = NULL;
  g_autofree gchar    *button_label = NULL;

  jobs = pp_printer_get_jobs_finish (PP_PRINTER (source_object), result, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not get jobs: %s", error->message);
        }

      return;
    }

  if (jobs->len == 0)
    {
      /* Translators: This is the label of the button that opens the Jobs Dialog. */
      button_label = g_strdup (_("No Active Jobs"));
    }
  else
    {
      /* Translators: This is the label of the button that opens the Jobs Dialog. */
      button_label = g_strdup_printf (ngettext ("%u Job", "%u Jobs", jobs->len), jobs->len);
    }

  gtk_button_set_label (GTK_BUTTON (self->show_jobs_dialog_button), button_label);
  gtk_widget_set_sensitive (self->show_jobs_dialog_button, jobs->len > 0);

  if (self->pp_jobs_dialog != NULL)
    {
      pp_jobs_dialog_update (self->pp_jobs_dialog);
    }

  g_clear_object (&self->get_jobs_cancellable);
}

void
pp_printer_entry_update_jobs_count (PpPrinterEntry *self)
{
  g_autoptr(PpPrinter) printer = NULL;

  g_cancellable_cancel (self->get_jobs_cancellable);
  g_clear_object (&self->get_jobs_cancellable);

  self->get_jobs_cancellable = g_cancellable_new ();

  printer = pp_printer_new (self->printer_name);
  pp_printer_get_jobs_async (printer,
                             TRUE,
                             CUPS_WHICHJOBS_ACTIVE,
                             self->get_jobs_cancellable,
                             get_jobs_cb,
                             self);
}

static gboolean
jobs_dialog_close_request_cb (PpPrinterEntry *self)
{
  if (self->pp_jobs_dialog != NULL)
    {
      gtk_window_destroy (GTK_WINDOW (self->pp_jobs_dialog));
      self->pp_jobs_dialog = NULL;
    }

  return GDK_EVENT_STOP;
}

void
pp_printer_entry_show_jobs_dialog (PpPrinterEntry *self)
{
  if (self->pp_jobs_dialog == NULL)
    {
      self->pp_jobs_dialog = pp_jobs_dialog_new (self->printer_name);
      g_signal_connect_object (self->pp_jobs_dialog, "close-request", G_CALLBACK (jobs_dialog_close_request_cb), self, G_CONNECT_SWAPPED);
      gtk_window_set_transient_for (GTK_WINDOW (self->pp_jobs_dialog), GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
      gtk_window_present (GTK_WINDOW (self->pp_jobs_dialog));
    }
}

void
pp_printer_entry_authenticate_jobs (PpPrinterEntry *self)
{
  pp_printer_entry_show_jobs_dialog (self);
  pp_jobs_dialog_authenticate_jobs (self->pp_jobs_dialog);
}

static void
show_jobs_dialog (GtkButton *button,
                  gpointer   user_data)
{
  pp_printer_entry_show_jobs_dialog (PP_PRINTER_ENTRY (user_data));
}

enum
{
  PRINTER_READY = 3,
  PRINTER_PROCESSING,
  PRINTER_STOPPED
};

static void
restart_printer (GtkButton      *button,
                 PpPrinterEntry *self)
{
  if (self->printer_state == PRINTER_STOPPED)
    printer_set_enabled (self->printer_name, TRUE);

  if (!self->is_accepting_jobs)
    printer_set_accepting_jobs (self->printer_name, TRUE, NULL);

  g_signal_emit_by_name (self, "printer-changed");
}

GSList *
pp_printer_entry_get_size_group_widgets (PpPrinterEntry *self)
{
  GSList *widgets = NULL;

  widgets = g_slist_prepend (widgets, self->printer_location_label);
  widgets = g_slist_prepend (widgets, self->printer_model_label);
  widgets = g_slist_prepend (widgets, self->printer_inklevel_label);

  return widgets;
}

PpPrinterEntry *
pp_printer_entry_new (cups_dest_t  printer,
                      gboolean     is_authorized)
{
  PpPrinterEntry *self;

  self = g_object_new (PP_PRINTER_ENTRY_TYPE, NULL);

  self->printer_name = g_strdup (printer.name);

  self->clean_command = pp_maintenance_command_new (self->printer_name,
                                                    "Clean",
                                                    "all",
                                                    /* Translators: Name of job which makes printer to clean its heads */
                                                    _("Clean print heads"));
  check_clean_heads_maintenance_command (self);

  gtk_drawing_area_set_draw_func (self->supply_drawing_area,
                                  supply_levels_draw_cb,
                                  self,
                                  NULL);

  pp_printer_entry_update (self, printer, is_authorized);

  return self;
}

const gchar *
pp_printer_entry_get_name (PpPrinterEntry *self)
{
  g_return_val_if_fail (PP_IS_PRINTER_ENTRY (self), NULL);
  return self->printer_name;
}

const gchar *
pp_printer_entry_get_location (PpPrinterEntry *self)
{
  g_return_val_if_fail (PP_IS_PRINTER_ENTRY (self), NULL);
  return self->printer_location;
}

void
pp_printer_entry_update (PpPrinterEntry *self,
                         cups_dest_t     printer,
                         gboolean        is_authorized)
{
  cups_ptype_t      printer_type = 0;
  gboolean          is_accepting_jobs = TRUE;
  gboolean          ink_supply_is_empty;
  g_autofree gchar *instance = NULL;
  const gchar      *printer_uri = NULL;
  const gchar      *device_uri = NULL;
  const gchar      *location = NULL;
  const gchar      *printer_make_and_model = NULL;
  const gchar      *reason = NULL;
  gchar           **printer_reasons = NULL;
  g_autofree gchar *status = NULL;
  g_autofree gchar *printer_status = NULL;
  int               i, j;
  static const char * const reasons[] =
    {
      "toner-low",
      "toner-empty",
      "developer-low",
      "developer-empty",
      "marker-supply-low",
      "marker-supply-empty",
      "cover-open",
      "door-open",
      "media-low",
      "media-empty",
      "offline",
      "paused",
      "marker-waste-almost-full",
      "marker-waste-full",
      "opc-near-eol",
      "opc-life-over"
    };
  static const char * statuses[] =
    {
      /* Translators: The printer is low on toner */
      N_("Low on toner"),
      /* Translators: The printer has no toner left */
      N_("Out of toner"),
      /* Translators: "Developer" is a chemical for photo development,
       * http://en.wikipedia.org/wiki/Photographic_developer */
      N_("Low on developer"),
      /* Translators: "Developer" is a chemical for photo development,
       * http://en.wikipedia.org/wiki/Photographic_developer */
      N_("Out of developer"),
      /* Translators: "marker" is one color bin of the printer */
      N_("Low on a marker supply"),
      /* Translators: "marker" is one color bin of the printer */
      N_("Out of a marker supply"),
      /* Translators: One or more covers on the printer are open */
      N_("Open cover"),
      /* Translators: One or more doors on the printer are open */
      N_("Open door"),
      /* Translators: At least one input tray is low on media */
      N_("Low on paper"),
      /* Translators: At least one input tray is empty */
      N_("Out of paper"),
      /* Translators: The printer is offline */
      NC_("printer state", "Offline"),
      /* Translators: Someone has stopped the Printer */
      NC_("printer state", "Stopped"),
      /* Translators: The printer marker supply waste receptacle is almost full */
      N_("Waste receptacle almost full"),
      /* Translators: The printer marker supply waste receptacle is full */
      N_("Waste receptacle full"),
      /* Translators: Optical photo conductors are used in laser printers */
      N_("The optical photo conductor is near end of life"),
      /* Translators: Optical photo conductors are used in laser printers */
      N_("The optical photo conductor is no longer functioning")
    };

  if (printer.instance)
    {
      instance = g_strdup_printf ("%s / %s", printer.name, printer.instance);
    }
  else
    {
      instance = g_strdup (printer.name);
    }

  self->printer_state = PRINTER_READY;

  for (i = 0; i < printer.num_options; i++)
    {
      if (g_strcmp0 (printer.options[i].name, "device-uri") == 0)
        device_uri = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "printer-uri-supported") == 0)
        printer_uri = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "printer-type") == 0)
        printer_type = atoi (printer.options[i].value);
      else if (g_strcmp0 (printer.options[i].name, "printer-location") == 0)
        location = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "printer-state-reasons") == 0)
        reason = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "marker-names") == 0)
        {
          g_free (self->inklevel->marker_names);
          self->inklevel->marker_names = g_strcompress (g_strdup (printer.options[i].value));
        }
      else if (g_strcmp0 (printer.options[i].name, "marker-levels") == 0)
        {
          g_free (self->inklevel->marker_levels);
          self->inklevel->marker_levels = g_strdup (printer.options[i].value);
        }
      else if (g_strcmp0 (printer.options[i].name, "marker-colors") == 0)
        {
          g_free (self->inklevel->marker_colors);
          self->inklevel->marker_colors = g_strdup (printer.options[i].value);
        }
      else if (g_strcmp0 (printer.options[i].name, "marker-types") == 0)
        {
          g_free (self->inklevel->marker_types);
          self->inklevel->marker_types = g_strdup (printer.options[i].value);
        }
      else if (g_strcmp0 (printer.options[i].name, "printer-make-and-model") == 0)
        printer_make_and_model = printer.options[i].value;
      else if (g_strcmp0 (printer.options[i].name, "printer-state") == 0)
        self->printer_state = atoi (printer.options[i].value);
      else if (g_strcmp0 (printer.options[i].name, "printer-is-accepting-jobs") == 0)
        {
          if (g_strcmp0 (printer.options[i].value, "true") == 0)
            is_accepting_jobs = TRUE;
          else
            is_accepting_jobs = FALSE;
        }
    }

  /* Find the first of the most severe reasons
   * and show it in the status field
   */
  if (reason && !g_str_equal (reason, "none"))
    {
      int errors = 0, warnings = 0, reports = 0;
      int error_index = -1, warning_index = -1, report_index = -1;

      printer_reasons = g_strsplit (reason, ",", -1);
      for (i = 0; i < g_strv_length (printer_reasons); i++)
        {
          for (j = 0; j < G_N_ELEMENTS (reasons); j++)
            if (strncmp (printer_reasons[i], reasons[j], strlen (reasons[j])) == 0)
                {
                  if (g_str_has_suffix (printer_reasons[i], "-report"))
                    {
                      if (reports == 0)
                        report_index = j;
                      reports++;
                    }
                  else if (g_str_has_suffix (printer_reasons[i], "-warning"))
                    {
                      if (warnings == 0)
                        warning_index = j;
                      warnings++;
                    }
                  else
                    {
                      if (errors == 0)
                        error_index = j;
                      errors++;
                    }
                }
        }
      g_strfreev (printer_reasons);

      if (error_index >= 0)
        status = g_strdup (_(statuses[error_index]));
      else if (warning_index >= 0)
        status = g_strdup (_(statuses[warning_index]));
      else if (report_index >= 0)
        status = g_strdup (_(statuses[report_index]));
    }

  if ((self->printer_state == PRINTER_STOPPED || !is_accepting_jobs) &&
      status != NULL && status[0] != '\0')
    {
      gtk_label_set_label (self->error_status, status);
      gtk_widget_set_visible (GTK_WIDGET (self->printer_error), TRUE);
    }
  else
    {
      gtk_label_set_label (self->error_status, "");
      gtk_widget_set_visible (GTK_WIDGET (self->printer_error), FALSE);
    }

  switch (self->printer_state)
    {
      case PRINTER_READY:
        if (is_accepting_jobs)
          {
            /* Translators: Printer's state (can start new job without waiting) */
            printer_status = g_strdup ( C_("printer state", "Ready"));
          }
        else
          {
            /* Translators: Printer's state (printer is ready but doesn't accept new jobs) */
            printer_status = g_strdup ( C_("printer state", "Does not accept jobs"));
          }
        break;
      case PRINTER_PROCESSING:
        /* Translators: Printer's state (jobs are processing) */
        printer_status = g_strdup ( C_("printer state", "Processing"));
        break;
      case PRINTER_STOPPED:
        /* Translators: Printer's state (no jobs can be processed) */
        printer_status = g_strdup ( C_("printer state", "Stopped"));
        break;
    }

  g_free (self->printer_location);
  self->printer_location = g_strdup (location);

  self->is_accepting_jobs = is_accepting_jobs;
  self->is_authorized = is_authorized;

  g_free (self->printer_hostname);
  self->printer_hostname = printer_get_hostname (printer_type, device_uri, printer_uri);

  gtk_label_set_text (self->printer_status, printer_status);
  gtk_label_set_text (self->printer_name_label, instance);
  self->is_default = printer.is_default;
  g_object_notify (G_OBJECT (self), "default");

  self->printer_make_and_model = sanitize_printer_model (printer_make_and_model);

  if (self->printer_make_and_model == NULL || self->printer_make_and_model[0] == '\0')
    {
      gtk_widget_set_visible (GTK_WIDGET (self->printer_model_label), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->printer_model), FALSE);
    }
  else
    {
      gtk_label_set_text (self->printer_model, self->printer_make_and_model);
    }

  if (location != NULL && location[0] == '\0')
    {
      gtk_widget_set_visible (GTK_WIDGET (self->printer_location_label), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->printer_location_address_label), FALSE);
    }
  else
    {
      gtk_label_set_text (self->printer_location_address_label, location);
    }

  ink_supply_is_empty = supply_level_is_empty (self);
  gtk_widget_set_visible (GTK_WIDGET (self->printer_inklevel_label), !ink_supply_is_empty);
  gtk_widget_set_visible (GTK_WIDGET (self->supply_frame), !ink_supply_is_empty);

  pp_printer_entry_update_jobs_count (self);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "printer.default", self->is_authorized);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "printer.remove", self->is_authorized);
}

static void
set_compact (PpPrinterEntry *self,
             gboolean        compact)
{
  compact = !!compact;

  if (self->compact == compact)
    return;

  self->compact = compact;

  if (compact)
    gtk_orientable_set_orientation (self->header_box, GTK_ORIENTATION_VERTICAL);
  else
    gtk_orientable_set_orientation (self->header_box, GTK_ORIENTATION_HORIZONTAL);
}

static void
pp_printer_entry_dispose (GObject *object)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (object);

  g_cancellable_cancel (self->get_jobs_cancellable);
  g_cancellable_cancel (self->check_clean_heads_cancellable);

  g_clear_pointer (&self->printer_name, g_free);
  g_clear_pointer (&self->printer_location, g_free);
  g_clear_pointer (&self->printer_make_and_model, g_free);
  g_clear_pointer (&self->printer_hostname, g_free);
  g_clear_pointer (&self->inklevel, ink_level_data_free);
  g_clear_object (&self->get_jobs_cancellable);
  g_clear_object (&self->check_clean_heads_cancellable);
  g_clear_object (&self->clean_command);

  G_OBJECT_CLASS (pp_printer_entry_parent_class)->dispose (object);
}

static void
pp_printer_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (object);

  switch (prop_id)
    {
    case PROP_DEFAULT:
      g_value_set_boolean (value, self->is_default);
      break;
    case PROP_COMPACT:
      g_value_set_boolean (value, self->compact);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pp_printer_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PpPrinterEntry *self = PP_PRINTER_ENTRY (object);

  switch (prop_id)
    {
    case PROP_DEFAULT:
      set_as_default_printer (self);
      break;
    case PROP_COMPACT:
      set_compact (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pp_printer_entry_class_init (PpPrinterEntryClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/printer-entry.ui");

  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, header_box);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_name_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_status);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_model_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_model);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_location_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_location_address_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_inklevel_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, supply_frame);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, supply_drawing_area);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, show_jobs_dialog_button);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, error_status);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterEntry, printer_error);

  gtk_widget_class_bind_template_callback (widget_class, show_jobs_dialog);
  gtk_widget_class_bind_template_callback (widget_class, restart_printer);

  object_class->dispose = pp_printer_entry_dispose;
  object_class->get_property = pp_printer_entry_get_property;
  object_class->set_property = pp_printer_entry_set_property;

  g_object_class_install_property (object_class,
                                   PROP_DEFAULT,
                                   g_param_spec_boolean ("default",
                                                         "default",
                                                         "default",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_COMPACT,
                                   g_param_spec_boolean ("compact",
                                                         "compact",
                                                         "compact",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  signals[IS_DEFAULT_PRINTER] =
    g_signal_new ("printer-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[PRINTER_DELETE] =
    g_signal_new ("printer-delete",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[PRINTER_RENAMED] =
    g_signal_new ("printer-renamed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  gtk_widget_class_install_action (widget_class, "printer.options", NULL,
                                   (GtkWidgetActionActivateFunc) printer_options_cb);
  gtk_widget_class_install_action (widget_class, "printer.details", NULL,
                                   (GtkWidgetActionActivateFunc) printer_details_cb);
  gtk_widget_class_install_property_action (widget_class, "printer.default", "default");
  gtk_widget_class_install_action (widget_class, "printer.clean-heads", NULL,
                                   (GtkWidgetActionActivateFunc) printer_clean_heads_cb);
  gtk_widget_class_install_action (widget_class, "printer.remove", NULL,
                                   (GtkWidgetActionActivateFunc) printer_remove_cb);
}
